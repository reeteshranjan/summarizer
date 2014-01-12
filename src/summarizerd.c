/*
 * summarizer.c
 */

#include "header.h"
#include <sys/wait.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <pthread.h>
#include "daemon.h"

/* MACROS */

#define DEFAULT_LOG_FILE      "/var/log/summarizerd.log"
#define DEFAULT_PID_FILE      "/var/log/summarizerd.pid"
#define DEFAULT_LOG_LEVEL     LL_ERROR
#define DEFAULT_CLIENTS       32
#define MAX_CLIENTS           32
#define DEFAULT_WORKERS       4
#define MAX_WORKERS           4
#define CLIENT_WAIT_TIME      500000 /* 0.5s */
#define MAX_STACK_SIZE        65536
#define EXPECTED_CLI_PER_WORKER  DEFAULT_CLIENTS

#define TERMSIGCASES   case SIGTERM: case SIGINT: case SIGKILL: case SIGUSR1
#define CRASHSIGCASES  case SIGABRT: case SIGSEGV: case SIGILL: case SIGFPE: case SIGBUS: case SIGQUIT
#define BLOCKCASES     case EAGAIN

/* TYPES */

typedef enum {
    LL_NONE = 0,
    LL_FATAL,
    LL_CRIT,
    LL_ERROR,
    LL_WARN,
    LL_NOTICE,
    LL_INFO,
    LL_DEBUG
} loglevel_t;

typedef enum {
    EXIT_OK = -1,
    EXIT_CANT_RECOVER = -2,
    EXIT_CRASH = -3,
} exit_status_t;

typedef enum {
    PROTO_PEER_LOST = -4,
    PROTO_INVALID = -5,
    PROTO_INTERNAL_ERROR = -6
} proto_status_t;

typedef enum {
    SOCK_READ = 0,
    SOCK_WRITE
} sock_status_t;

typedef struct {
    int                sock;
    int                req_offset;
    request_header_t   reqhdr;
    response_type_t    rep_type;
    sock_status_t      status;
    char               filename[MAX_FILENAME_LEN];
} sock_context_t;

typedef struct {
    pthread_mutex_t    mutex;
    pthread_cond_t     cond;
    array_t          * sock_contexts;
    int                max_fds;
} worker_context_t;

/* GLOBALS */

static uint16_t   g_port = SUMMARIZERD_PORT;
static loglevel_t g_log_level = DEFAULT_LOG_LEVEL;
static int        g_num_cli = DEFAULT_CLIENTS;
static bool_t     g_is_daemon = SMRZR_TRUE;
static literal_t  g_pid_file = DEFAULT_PID_FILE;
static int        g_pid_fd = -1;
static int        g_num_workers = DEFAULT_WORKERS;

static FILE*      g_log;

static int        g_dev_null = -1;
static int        g_sig = -1;

static int        g_main_sock = -1;
static pid_t      g_pid;
static int        g_err = 0, g_exiting = 0, g_to_fork = 0, g_to_exit = 0;

static worker_context_t g_worker_contexts[MAX_WORKERS] = {
    { PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, NULL, 0 },
    { PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, NULL, 0 },
    { PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, NULL, 0 },
    { PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, NULL, 0 }
};

static pthread_t        g_workers[MAX_WORKERS];

static const char* g_level_strs[] = {
    "none    ", /* LL_NONE = 0 */
    "fatal   ", /* LL_FATAL */
    "critical", /* LL_CRIT */
    "error   ", /* LL_ERROR */
    "warning ", /* LL_WARN */
    "notice  ", /* LL_NOTICE */
    "info    ", /* LL_INFO */
    "debug   ", /* LL_DEBUG */
};

/* PROTOTYPES */

static void usage(const char*);
static void handle_fork(void);
static void setup_watchdog_signal_handlers(void);
static void sighnd_handle_fork(int);
static void setup_signal_handlers(void);
static int  setup_one_signal_handler(int sig, struct sigaction* sa,
                                     void (*handler)(int));
static void sighnd_note(int);
static void sighnd_crash(int);
static void main_proc(void);
static void check_existing_process(void);
static void handle_io_streams(void);
static void register_pid(void);
static void init_workers(void);
static int  file_lock_ex(int);
static void setup_socket(void);
static void quit(int);
static int  file_lock_un(int);
static int  handle_accept_nb(void);
static int  assign_to_worker(int sock);
static void* worker(void*);
static void initiate_quit(int);
static int  worker_loop(worker_context_t*, lang_t*, article_t*);
static int  read_summary_request(sock_context_t* ctxt);
static int  read_nb(int sock, void* buf, size_t len);
static int  write_summary_response(int sock, article_t*);
static int  write_error_response(int sock, int err);
static int  write_nb(int sock, const void* buf, size_t len);
static int  handle_select_error(void);
static int  handle_read_error(int, worker_context_t*, sock_context_t*);
static int  handle_write_error(int, worker_context_t*, sock_context_t*);
static int  close_peer(worker_context_t *ctxt, int sock);
static relation_t comp_sock_context(const elem_t, const elem_t);

/* FUNCTIONS */

int
main(int argc, char** argv)
{
    status_t   status;
    int        opt;
    pid_t      pid;
    literal_t  log_file = DEFAULT_LOG_FILE;

    if(argc <= 1) {
        usage(argv[0]);
    }

    while(-1 != (opt = getopt(argc, argv, "l:p:v:n:i:w:fh"))) {
        switch(opt) {
            case 'l': log_file = optarg; break;
            case 'v': g_log_level = (loglevel_t)atoi(optarg); break;
            case 'p': g_port = atoi(optarg); break;
            case 'n': g_num_cli = atoi(optarg); break;
            case 'i': g_pid_file = optarg; break;
            case 'f': g_is_daemon = SMRZR_FALSE; break;
            case 'w': g_num_workers = atoi(optarg); break;
            case 'h': usage(argv[0]);
            default: usage(argv[0]);
        }
    }

    if(g_log_level < 1 || g_log_level > 7) {
        fprintf(stderr, "Invalid log level value %d\n", g_log_level);
        usage(argv[0]);
    }

    if(g_num_cli > MAX_CLIENTS) {
        fprintf(stderr, "Maximum %d clients supported, required %u\n",
                        MAX_CLIENTS, g_num_cli);
        usage(argv[0]);
    }

    if(g_num_workers > MAX_WORKERS) {
        fprintf(stderr, "Maximum %d workers supported, suggested %u\n",
                        MAX_WORKERS, g_num_workers);
        usage(argv[0]);
    }

    if(!g_num_cli)      g_num_cli     = DEFAULT_CLIENTS;
    if(!g_num_workers)  g_num_workers = DEFAULT_WORKERS;
    if(!g_port)         g_port        = SUMMARIZERD_PORT;
    if(!g_pid_file)     g_pid_file    = DEFAULT_PID_FILE;
    if(!log_file)       log_file      = DEFAULT_LOG_FILE;

    if(NULL == (g_log = fopen(log_file, "a"))) {
        fprintf(stderr, "Failed to open the log file '%s'\n", log_file);
        return(1);
    }

    LOG(LL_INFO, "Summarizer Daemon Config: logfile '%s', logging level '%d', "
                 "port '%u', clients listened '%u', daemon mode '%s'",
        log_file, g_log_level, g_port, g_num_cli,
        ((SMRZR_TRUE == g_is_daemon) ? "Y" : "N"));

    /* Do the initial inits common to all children */
    status = init_globals();
    if(SMRZR_OK != status) {
        fprintf(stderr, "Failed to init globals\nExiting...\n");
        return(1);
    }

    if(SMRZR_TRUE == g_is_daemon) {
        if(-1 == (pid = fork())) {
            perror("Failed to create a child process: ");
            return(1);
        }
        else
        if(0 == pid) { /* child of 1st fork */
            handle_fork();
        }
        /* else parent quits */
    } else {
        setup_signal_handlers();
        main_proc();
    }

    return(0);
}

void
usage(const char* prog)
{
    fprintf(stderr, "Usage:\n%s -p <port> -l <logfile> -v <verbosity> -n <numclients> -i <pidfile> -w <numworkers> [-f]\n", prog);
    fprintf(stderr, "%s -h (prints this help)\n\n", prog);
    fprintf(stderr, "logfile    : logging file [/var/log/summarizerd.log]\n");
    fprintf(stderr, "pidfile    : pid file [/var/log/summarizerd.pid]\n");
    fprintf(stderr, "port       : port on which to listen [9872]\n");
    fprintf(stderr, "numclients : number of clients to listen for [32] (<=32)\n");
    fprintf(stderr, "numworkers : number of workers to use [4] (<=4)\n");
    fprintf(stderr, "        -f : run summarizerd in foreground\n");
    fprintf(stderr, "verbosity  : verbosity of logging, a number in 1-7 [3]\n");
    fprintf(stderr, "                1-fatal, 2-crit, 3-error, 4-warn, 5-notice, 6-info, 7-debug\n");
    exit(EXIT_OK);
}

void
handle_fork(void)
{
    pid_t pid;
    assert(SMRZR_TRUE == g_is_daemon);

    while(1) {

        if(-1 == (pid = fork())) {
            LOG(LL_FATAL, "[%d] Failed to fork the daemon process", getpid());
            exit(EXIT_CANT_RECOVER);
        }
        else
        if(0 == pid) { /* double forked child */
            setup_signal_handlers();
            main_proc();
        }
        else { /* waits and re-forks on child term */
            g_pid = pid;
            setup_watchdog_signal_handlers();
            while(1) {
                usleep(100000);
                if(g_to_exit) {
                    LOG(LL_INFO, "Watchdog exiting...");
                    fclose(g_log);
                    exit(0);
                }
                if(g_to_fork) {
                    g_to_fork = 0;
                    LOG(LL_INFO, "Watchdog would fork a new child...");
                    break; /* go fork again */
                }
            }
        }
    }
}

void
setup_watchdog_signal_handlers(void)
{
    struct sigaction sa;
    int              res = 0;

    sigfillset(&sa.sa_mask );
    sa.sa_flags = SA_NOCLDSTOP;

    res = setup_one_signal_handler(SIGCHLD, &sa, sighnd_handle_fork);

    if(0 != res) {
        LOG(LL_FATAL, "Can't setup signal handlers: %s", strerror(errno));
        exit(EXIT_CANT_RECOVER);
    }
}

void
sighnd_handle_fork(int sig)
{
    pid_t pid;
    int   child_st, exit_st;

    if(0 < (pid = waitpid(g_pid, &child_st, WNOHANG))) {

        g_to_fork = 1; g_to_exit = 0;

        if(WIFEXITED(child_st)) {
            LOG(LL_NOTICE, "Child exited with status - %d", WEXITSTATUS(child_st));
            if(EXIT_CRASH != (exit_st = WEXITSTATUS(child_st))) {
                /* exited ok, or can't recover condition. let the parent
                   watchdog exit as well */
                LOG(LL_INFO, "Marking watchdog parent to exit");
                g_to_exit = 1;
            }
        } else
        if(WIFSIGNALED(child_st)) {
            LOG(LL_INFO, "Child sig'd with - %d", WTERMSIG(child_st));
            switch(WTERMSIG(child_st)) {
                TERMSIGCASES:
                    /* child killed by an exit-intent signal. let the parent
                       watchdog exit as well */
                    LOG(LL_INFO, "Child term-sig'd. Marking watchdog parent to exit");
                    g_to_exit = 1; /* no break intentional */
                default: break;
            }
        } else if(WIFSTOPPED(child_st) || WIFCONTINUED(child_st)) {
            LOG(LL_DEBUG, "Ignoring cont/stop signals sent to child");
            g_to_fork = 0;
        }
    }

    if(pid < 0) {
        LOG(LL_WARN, "Failed to wait for child\n");
    }
}

void
setup_signal_handlers(void)
{
    struct sigaction sa;
    int              res = 0;

    g_sig = -1;

    sigfillset(&sa.sa_mask );
    sa.sa_flags = SA_NOCLDSTOP;

    res =
        setup_one_signal_handler(SIGTERM, &sa, sighnd_note)
        || setup_one_signal_handler(SIGINT, &sa, sighnd_note)
        || setup_one_signal_handler(SIGHUP, &sa, sighnd_note)
        || setup_one_signal_handler(SIGUSR1, &sa, sighnd_note)
        || setup_one_signal_handler(SIGCHLD, &sa, sighnd_note)
        || setup_one_signal_handler(SIGPIPE, &sa, SIG_IGN);

    if(!res) {
        sa.sa_flags |= SA_RESETHAND;

        res = res
            || setup_one_signal_handler(SIGSEGV, &sa, sighnd_crash)
            || setup_one_signal_handler(SIGQUIT, &sa, sighnd_crash)
            || setup_one_signal_handler(SIGABRT, &sa, sighnd_crash)
            || setup_one_signal_handler(SIGILL, &sa, sighnd_crash)
            || setup_one_signal_handler(SIGBUS, &sa, sighnd_crash)
            || setup_one_signal_handler(SIGFPE, &sa, sighnd_crash);
    }

    if(0 != res) {
        LOG(LL_FATAL, "Can't setup signal handlers: %s", strerror(errno));
        exit(EXIT_CANT_RECOVER);
    }
}

int
setup_one_signal_handler(int sig, struct sigaction* sa, void (*handler)(int))
{
    sa->sa_handler = handler;
    return (sigaction(sig, sa, NULL));
}

static void sighnd_note(int sig)
{ 
    g_sig = sig;
}

static void sighnd_crash(int sig)
{
    initiate_quit(EXIT_CRASH);
}

void
main_proc(void)
{
    if(SMRZR_TRUE == g_is_daemon) {

        check_existing_process();

        register_pid();

        handle_io_streams();
    }

    init_workers();

    setup_socket();

    quit(handle_accept_nb());
}

void
check_existing_process(void)
{
    if(0 > (g_pid_fd = open(g_pid_file, O_WRONLY|O_CREAT|O_TRUNC, 0644))) {
        fprintf(stderr, "Can't open pid file '%s' to write", g_pid_file);
        quit(EXIT_CANT_RECOVER);
    }

    if(0 > file_lock_ex(g_pid_fd)) {
        fprintf(stderr, "Can't lock pid file '%s' - another process running ", g_pid_file);
        close(g_pid_fd);
        quit(EXIT_OK);
    }

    LOG(LL_INFO, "No existing summarizerd process found");
}

void
register_pid(void)
{
    char pidstr[32] = { 0 };

    g_pid = getpid();

    sprintf(pidstr, "%d\n", g_pid);

    if((int)strlen(pidstr) != write(g_pid_fd, pidstr, strlen(pidstr))) {
        LOG(LL_CRIT, "Can't write into pid file '%s'", g_pid_file);
        quit(EXIT_CANT_RECOVER);
    }
}

int
file_lock_ex(int fd)
{
    struct flock  lk;

    lk.l_type = F_WRLCK;
    lk.l_whence = SEEK_SET;
    lk.l_start = lk.l_len = 0;

    return((0 <= fcntl(fd, F_SETLK, &lk)) ? 0 : -1);
}

void
handle_io_streams(void)
{
    /* dev/null the streams */
    if(0 > g_dev_null) {
        if(0 > (g_dev_null = open("/dev/null", O_RDWR))) {
            LOG(LL_CRIT, "Can't open /dev/null");
            quit(EXIT_CANT_RECOVER);
        }
    }

    /* close input output streams */
    close(0); close(1); close(2);

    /* dup dev/null -> streams */
    dup2(g_dev_null, 0);
    dup2(g_dev_null, 1);
    dup2(g_dev_null, 2);
}

void
init_workers(void)
{
    int              i;
    pthread_attr_t   attr;

    if(0 != pthread_attr_init(&attr)) {
        LOG(LL_FATAL, "Can't init pthread attr object");
        quit(EXIT_CANT_RECOVER);
    }

    if(0 != pthread_attr_setstacksize(&attr, MAX_STACK_SIZE)) {
        LOG(LL_FATAL, "Can't init pthread stack size tp '%u'", MAX_STACK_SIZE);
        quit(EXIT_CANT_RECOVER);
    }

    for(i = 0; i < g_num_workers; ++i) {

        if(0 != pthread_mutex_lock(&g_worker_contexts[i].mutex)) {
            LOG(LL_FATAL, "Can't lock worker context# %u", i);
            quit(EXIT_CANT_RECOVER);
        }

        if(NULL == g_worker_contexts[i].sock_contexts) {

            if(NULL == (g_worker_contexts[i].sock_contexts = array_new(SMRZR_TRUE,
                sizeof(sock_context_t), EXPECTED_CLI_PER_WORKER, NULL)))
            {
                LOG(LL_FATAL, "Can't create socket contexts for worker# %u", i);
		pthread_mutex_unlock(&g_worker_contexts[i].mutex);
                quit(EXIT_CANT_RECOVER);
            }

            if(0 != pthread_create(&g_workers[i], &attr, &worker,
                                   &g_worker_contexts[i]))
            {
                LOG(LL_FATAL, "Can't create worker# %u", i);
                pthread_mutex_unlock(&g_worker_contexts[i].mutex);
                quit(EXIT_CANT_RECOVER);
            }

            if(0 != pthread_detach(g_workers[i])) {
                LOG(LL_FATAL, "Can't detach worker# %u", i);
                pthread_mutex_unlock(&g_worker_contexts[i].mutex);
                quit(EXIT_CANT_RECOVER);
            }
        }

        if(0 != pthread_mutex_unlock(&g_worker_contexts[i].mutex)) {
            LOG(LL_FATAL, "Can't lock worker context# %u", i);
            quit(EXIT_CANT_RECOVER);
        }
    }

    pthread_attr_destroy(&attr);

    LOG(LL_DEBUG, "Completed setting up workers");
}

void
setup_socket(void)
{
    struct sockaddr_in  a;
    int                 on = 1, tries = 5, res;

    if(0 > (g_main_sock = socket(AF_INET, SOCK_STREAM, 0))) {
        LOG(LL_FATAL, "socket: failed to create - %s", strerror(errno));
        quit(EXIT_CANT_RECOVER);
    }

    if(0 != setsockopt(g_main_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&on,
                       sizeof(on)))
    {
        LOG(LL_FATAL, "socket: failed to set REUSE - %s", strerror(errno));
        quit(EXIT_CANT_RECOVER);
    }

    if(0 != fcntl(g_main_sock, F_SETFL, O_NONBLOCK)) {
        LOG(LL_FATAL, "socket: failed to NB - %s", strerror(errno));
        quit(EXIT_CANT_RECOVER);
    }

    memset(&a, 0, sizeof(a));

    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    a.sin_port = htons(g_port);

    while(tries-- > 0) {

        if(0 == (res = bind(g_main_sock,(struct sockaddr *)&a, sizeof(a))))
            break;

        LOG(LL_NOTICE, "bind: failed - %s, retrying...", strerror(errno));
        usleep(1000000);
    }

    if(res) {
        LOG(LL_FATAL, "bind: failed - %s", strerror(errno));
        quit(EXIT_CANT_RECOVER);
    }

    if(0 > listen(g_main_sock, g_num_cli)) {
        LOG(LL_FATAL, "listen: failed for %u clients - %s",
                      g_num_cli, strerror(errno));
        quit(EXIT_CANT_RECOVER);
    }
}

void
quit(int err)
{
    int i;
    array_t* a;
    sock_context_t* s;

    LOG(LL_DEBUG, "Exiting with err - %d", err);

    if(0 != g_err) err = g_err; /* somebody initiated quit */
    else g_err = err; /* don't initiate quit anymore */

    g_exiting = 1;

    LOG(LL_DEBUG, "Signaling all the workers");
    for(i = 0; i < g_num_workers; ++i) {
        pthread_cond_signal(&g_worker_contexts[i].cond);
        pthread_kill(g_workers[i], SIGUSR1);
    }

    LOG(LL_DEBUG, "Waiting for all the workers");
    for(i = 0; i < g_num_workers; ++i) {
        pthread_join(g_workers[i], NULL);
        pthread_cond_destroy(&g_worker_contexts[i].cond);
        pthread_mutex_destroy(&g_worker_contexts[i].mutex);
    }

    LOG(LL_DEBUG, "Closing all the sockets used by workers");
    for(i = 0; i < g_num_workers; ++i) {
        a = g_worker_contexts[i].sock_contexts;
        for(s = (sock_context_t*)ARR_FIRST(a); !ARR_END(a); s = (sock_context_t*)ARR_NEXT(a)) {
            close(s->sock);
        }
        array_free(a);
    }

    LOG(LL_DEBUG, "Closing main listening socket");
    close(g_main_sock);

    LOG(LL_DEBUG, "Cleaning up pid registration");
    file_lock_un(g_pid_fd);
    close(g_pid_fd);
    unlink(g_pid_file);

    LOG(LL_DEBUG, "Misc steps");
    close(g_dev_null);

    LOG(LL_DEBUG, "Exiting with status - %d", err);

    LOG(LL_DEBUG, "Closing log file");

    fclose(g_log);

    exit(err);
}

int
file_lock_un(int fd)
{
    struct flock  lk;

    lk.l_type = F_UNLCK;
    lk.l_whence = SEEK_SET;
    lk.l_start = lk.l_len = 0;

    return((0 <= fcntl(fd, F_SETLK, &lk)) ? 0 : -1);
}

int
handle_accept_nb(void)
{
    struct sockaddr_in  a;
    fd_set              fds;
    int                 res, sock;
    socklen_t           a_len;

    LOG(LL_INFO, "Listening on all interfaces...");

    while(1) {

        FD_ZERO(&fds);
        FD_SET(g_main_sock, &fds);

        if(0 == (res = select(g_main_sock+1, &fds, NULL, NULL, NULL))) {
            LOG(LL_FATAL, "select: woke up abruptly");
            return(EXIT_CANT_RECOVER);
        }

        if(res < 0) {
            return handle_select_error();
        }

        a_len = sizeof(a);

        if(0 > (sock = accept(g_main_sock, (struct sockaddr*)&a, &a_len))) {
            switch(errno) {
                BLOCKCASES: /* select misreported activity */
                    LOG(LL_NOTICE, "accept: select spurious, continuing");
                    continue;
                case ECONNABORTED: /* it's ok, continue accepting */
                    LOG(LL_NOTICE, "accept: connection aborted, continuing");
                    continue;
                case EINTR: /* interrpted by signal */
                    switch(g_sig) {
                        TERMSIGCASES:
                            LOG(LL_NOTICE, "accept: term signal %d", g_sig);
                            return(EXIT_OK);
                        CRASHSIGCASES:
                            LOG(LL_CRIT, "accept: crash signal %d", g_sig);
                            return(EXIT_CRASH);
                        default:
                            LOG(LL_CRIT, "accept: signal %d", g_sig);
                            return(EXIT_CANT_RECOVER);
                    }
                default: /* anything else is bad */
                    LOG(LL_FATAL, "accept: %s", strerror(errno));
                    return(EXIT_CANT_RECOVER);
            }
        }

        LOG(LL_DEBUG, "Socket accepted: %d", sock);

        if(0 != fcntl(sock, F_SETFL, O_NONBLOCK)) {
            LOG(LL_FATAL, "accepted socket: failed to NB %s", strerror(errno));
            return(EXIT_CANT_RECOVER);
        }

        if(0 != (res = assign_to_worker(sock))) {
            return(res);
        }
    }

    return(EXIT_OK); /* should not reach here */
}

int
assign_to_worker(int sock)
{
#define MASTER_LOCK_WORKER(ctxt) \
do { \
    if(0 != pthread_mutex_lock(&ctxt.mutex)) { \
        LOG(LL_FATAL, "Can't lock worker context mutex"); \
        return(EXIT_CANT_RECOVER); \
    } \
} while(0)

#define MASTER_UNLOCK_WORKER(ctxt) \
do { \
    if(0 != pthread_mutex_unlock(&ctxt.mutex)) { \
        LOG(LL_FATAL, "Can't unlock worker context mutex"); \
        return(EXIT_CANT_RECOVER); \
    } \
} while(0)

#define MASTER_SIGNAL_WORKER(ctxt) \
do { \
    if(0 != pthread_cond_signal(&ctxt.cond)) { \
        LOG(LL_FATAL, "Can't signal worker"); \
        return(EXIT_CANT_RECOVER); \
    } \
} while(0)

    static int       s_worker_no = 0;
    sock_context_t * sock_ctxt = NULL;
    bool_t           is_new = SMRZR_FALSE;

    /* enqueue the new socket to a worker in round-robin */

    MASTER_LOCK_WORKER(g_worker_contexts[s_worker_no]);

    if(NULL == (sock_ctxt = array_search_or_alloc(
                                &g_worker_contexts[s_worker_no].sock_contexts,
                                (elem_t)(intptr_t)sock, comp_sock_context, &is_new)))
    {
        LOG(LL_ERROR, "Failed to allocate context for worker %d for socket %d",
                      s_worker_no, sock);
        MASTER_UNLOCK_WORKER(g_worker_contexts[s_worker_no]);
        return(EXIT_CANT_RECOVER);
    }

    assert(SMRZR_TRUE == is_new);

    sock_ctxt->sock = sock;
    sock_ctxt->status = SOCK_READ;
    sock_ctxt->rep_type = REP_SUMMARY;
    sock_ctxt->req_offset = 0;
    memset(&sock_ctxt->reqhdr, 0, sizeof(request_header_t));
    memset(&sock_ctxt->filename, 0, MAX_FILENAME_LEN);

    if(g_worker_contexts[s_worker_no].max_fds <= sock)
        g_worker_contexts[s_worker_no].max_fds = sock + 1;

    MASTER_UNLOCK_WORKER(g_worker_contexts[s_worker_no]);

    LOG(LL_DEBUG, "Added client sock %d to worker %u", sock, s_worker_no);

    MASTER_SIGNAL_WORKER(g_worker_contexts[s_worker_no]);

    s_worker_no = (s_worker_no + 1) % g_num_workers; /* rotate worker */

    return(0);
}

void*
worker(void* arg)
{
#define THREAD_EXIT(status) \
    lang_destroy(&lang); \
    article_destroy(&article); \
    initiate_quit(status); \
    pthread_exit(NULL)

    worker_context_t* ctxt = (worker_context_t*)arg;
    status_t          status;
    lang_t            lang;
    article_t         article;
    int               res;

    status =
        lang_init(&lang)
     || parse_lang_xml(DICTIONARY_DIR"/en.xml", &lang)
     || article_init(&article);

    if(SMRZR_OK != status) {
        LOG(LL_ERROR, "Failed to parse EN language specific rules file\n");
        THREAD_EXIT(EXIT_CANT_RECOVER);
    }

    while(1) {

        /* wait until we have a sock available */

        if(0 != pthread_mutex_lock(&ctxt->mutex)) {
            LOG(LL_FATAL, "Can't lock thread context mutex");
            THREAD_EXIT(EXIT_CANT_RECOVER);
        }

        while(0 == ctxt->max_fds && 0 == g_exiting) {
            LOG(LL_DEBUG, "Waiting as there is no client sock available");
            if(0 != pthread_cond_wait(&ctxt->cond, &ctxt->mutex)) {
                LOG(LL_FATAL, "Can't wait in worker ");
                THREAD_EXIT(EXIT_CANT_RECOVER);
            }
        }

        if(0 != pthread_mutex_unlock(&ctxt->mutex)) {
            LOG(LL_FATAL, "Can't unlock thread context mutex");
            THREAD_EXIT(EXIT_CANT_RECOVER);
        }

        if(1 == g_exiting) {
            LOG(LL_NOTICE, "Graceful exit requested.. worker exiting");
            THREAD_EXIT(EXIT_OK);
        }

        /* handle a client on this sock */
        if(0 != (res = worker_loop(ctxt, &lang, &article))) {
            LOG(LL_CRIT, "Encountered errors in worker loop");
            THREAD_EXIT(res);
        }
    }
}

void
initiate_quit(int err)
{
    if(0 == g_err) { /* don't do more than once */
        LOG(LL_INFO, "Initiating quit with err %d", err);
        g_err = err;
        kill(g_pid, SIGUSR1); /*send signal to main thread */
    }
}

int
worker_loop(worker_context_t* ctxt, lang_t* lang, article_t* article)
{
    int              res, i;
    uint32_t         r;
    float            ratio;
    status_t         status;
    fd_set           read_fds, write_fds, except_fds;
    array_t        * a = ctxt->sock_contexts;
    struct timeval   tv;
    sock_context_t * s;

    while(1) {

        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);
        FD_ZERO(&except_fds);

        if(0 != pthread_mutex_lock(&ctxt->mutex)) {
            LOG(LL_FATAL, "Can't lock thread context mutex");
            return(EXIT_CANT_RECOVER);
        }

        for(s = (sock_context_t*)ARR_FIRST(a); !ARR_END(a); s = (sock_context_t*)ARR_NEXT(a)) {
            FD_SET(s->sock, &read_fds);
            FD_SET(s->sock, &except_fds);
            if(SOCK_WRITE == s->status)
                FD_SET(s->sock, &write_fds);
        }

        if(0 != pthread_mutex_unlock(&ctxt->mutex)) {
            LOG(LL_FATAL, "Can't unlock thread context mutex");
            return(EXIT_CANT_RECOVER);
        }

        tv.tv_sec = 0;
        tv.tv_usec = CLIENT_WAIT_TIME;

        if(0 == (res = select(ctxt->max_fds, &read_fds, &write_fds, &except_fds, &tv))) {
            LOG(LL_DEBUG, "select: woke up after timeout");
            continue;
        }

        if(res < 0) {
            return handle_select_error();
        }

        for(i = 0; i < ctxt->max_fds; ++i) {

            res = 0; s = NULL;

            if(FD_ISSET(i, &read_fds)) { /* read the request */

                LOG(LL_DEBUG, "Socket %d has read activity", i);

                s = array_search(ctxt->sock_contexts, (elem_t)(intptr_t)i, comp_sock_context);

                assert(NULL != s);

                if(0 > (res = read_summary_request(s))) {

                    if(0 >= (res = handle_read_error(res, ctxt, s)))
                        return(res);

                    if(s->status == SOCK_WRITE) res = 0;

                } else if (res > 0) {
                    LOG(LL_DEBUG, "Set reply type to summary for %d", i);
                    s->rep_type = REP_SUMMARY;
                    s->status = SOCK_WRITE;
                    res = 0;
                }
            }

            if(0 == res && FD_ISSET(i, &write_fds)) {

                LOG(LL_DEBUG, "Socket %d ready to write", i);

                /* read gave an EAGAIN or it was not in read-set */
                if(NULL == s)
                    s = array_search(ctxt->sock_contexts, (elem_t)(intptr_t)i, comp_sock_context);

                assert(NULL != s);
                assert(SOCK_WRITE == s->status);

                if(REP_SUMMARY == s->rep_type) {

                    r = s->reqhdr.ratio;
                    ratio = *((float*)(void*)&r);
                    ratio = ratio / 100;

                    LOG(LL_INFO, "Going to parse article %s for ratio %.2f",
                                  s->filename, ratio);

                    if(SMRZR_OK != (status =
                        (parse_article(s->filename, lang, article)
                         || grade_article(article, lang, ratio))))
                    {
                        LOG(LL_ERROR, "Failed to create summary of '%s' with ratio '%.2f'",
                                      s->filename, ratio);

                        article_reset(article);

                        if(0 > (res = write_error_response(i, PROTO_INTERNAL_ERROR))) {

                            LOG(LL_ERROR, "Failed to send error response");

                            if(0 >= (res = handle_write_error(res, ctxt, s))) 
                                return(res);

                        } else if(res > 0) { /* done writing */
                            s->status = SOCK_READ;
                        } /* else [ res = 0 => EAGAIN => try select again ] */

                    } else {

                        if(0 > (res = write_summary_response(i, article))) {

                            LOG(LL_ERROR, "Failed to send summary of '%s' with ratio '%.2f'",
                                      s->filename, ratio);

                            article_reset(article);

                            if(0 >= (res = handle_write_error(res, ctxt, s)))
                                return(res);

                        } else if(res > 0) { /* sent response properly */
                            s->status = SOCK_READ;
                        } /* else [ res = 0 => EAGAIN => try select again ] */
                    }

                    article_reset(article);

                } else {
                    if( 0 > (res = write_error_response(i, s->rep_type))) {

                        LOG(LL_ERROR, "Failed to send error response");

                        if(0 >= (res = handle_write_error(res, ctxt, s)))
                            return(res);

                    } else if(res > 0) { /* done writing */
                        s->status = SOCK_READ;
                    } /* else [ res = 0 => EAGAIN => try select again ] */
                }
            }
        }
    }

    return(0);
}

int
read_summary_request(sock_context_t* ctxt)
{
    /* request
     * proto[2] | ver[2] | ratio[4] | filename_len[4] | filename[filename_len] |
     */
    int              res;
    float            ratio;
    uint32_t         r;
    request_header_t reqhdr;

    if(0 == ctxt->req_offset) {

        if(sizeof(reqhdr) != (res = read_nb(ctxt->sock, &reqhdr, sizeof(reqhdr)))) {
            return(res);
        }

        ctxt->req_offset = sizeof(reqhdr);

        reqhdr.proto = ntohs(reqhdr.proto);
        reqhdr.ver = ntohs(reqhdr.ver);
        reqhdr.ratio = ntohl(reqhdr.ratio);
        reqhdr.filename_len = ntohl(reqhdr.filename_len);

        if(SUMMARIZERD_PROTO != reqhdr.proto) {
            LOG(LL_INFO, "Server protocol - %u, Client protocol - %u",
                SUMMARIZERD_PROTO, reqhdr.proto);
            return(PROTO_INVALID);
        }

        if(SUMMARIZERD_VERSION != reqhdr.ver) {
            LOG(LL_INFO, "Server version - %u, Client version - %u",
                SUMMARIZERD_VERSION, reqhdr.ver);
            return(PROTO_INVALID);
        }

        r = reqhdr.ratio;

        ratio = *((float*)(void*)&r);

        if(ratio > 100 || ratio < 0) {
            LOG(LL_INFO, "Invalid ratio - %.2f", ratio);
            return(PROTO_INVALID);
        }

        if(reqhdr.filename_len > MAX_FILENAME_LEN) {
            LOG(LL_INFO, "Too long filename - %u", reqhdr.filename_len);
            return(PROTO_INVALID);
        }

        memcpy(&ctxt->reqhdr, &reqhdr, sizeof(request_header_t));
    }

    if((int)ctxt->reqhdr.filename_len !=
       (res = read_nb(ctxt->sock, ctxt->filename, ctxt->reqhdr.filename_len)))
    {
        return(res);
    }

    ctxt->req_offset = 0;
    ctxt->status = SOCK_WRITE; /* req read, ready to write */

    LOG(LL_DEBUG, "Read request on socket %d", ctxt->sock);

    LOG(LL_DEBUG, "File to create summary for - %s", ctxt->filename);

    return(1); /* 0 = EAGAIN */
}

int
read_nb(int sock, void* buf, size_t len)
{
    int     read_len, total_len = 0;

    LOG(LL_DEBUG, "To read total of %lu bytes", len);

    while(len) {

        if(0 < (read_len = recv(sock, buf, len, 0))) {
            len -= read_len;
            buf += read_len;
            total_len += read_len;
            LOG(LL_DEBUG, "Read %d, remaining %lu", read_len, len);
        } else
        if(0 == read_len) {
            LOG(LL_INFO, "recv: socket closed, read %d, remaining %lu",
                          total_len, len);
            return(PROTO_PEER_LOST); /* peer closed */
        } else {
            switch(errno) {
            case EINTR:
                switch(g_sig) {
                    TERMSIGCASES:
                        LOG(LL_NOTICE, "recv: term sig %d", g_sig);
                        return(EXIT_OK); /* time to quit */
                    CRASHSIGCASES:
                        LOG(LL_CRIT, "recv: crash signal %d", g_sig);
                        return(EXIT_CRASH);
                    default:
                        LOG(LL_FATAL, "recv: signal %d", g_sig);
                        return(EXIT_CANT_RECOVER);
                }
            case ECONNREFUSED:
                LOG(LL_INFO, "recv: conn refused (client may have died)");
                return(PROTO_PEER_LOST);
            BLOCKCASES: /* no more to read */
                LOG(LL_DEBUG, "recv: nothing to read");
                return(0);
            default:
                LOG(LL_FATAL, "recv: %s", strerror(errno));
                return(EXIT_CANT_RECOVER);
            }
        }
    }

    LOG(LL_DEBUG, "Read total of %u bytes", total_len);
    return(total_len);
}

int
write_summary_response(int sock, article_t* article)
{
    /* response
     * proto[2] | ver[2] | status[4] | summary_len[4] | summary[filename_len] |
     */

    array_t     * a;
    sentence_t  * s;
    string_t      w;
    size_t        len = 0;
    int           res;

    response_header_t rephdr;

    a = article->sentences;

    LOG(LL_DEBUG, "Number of sentences in article - %lu", ARR_SZ(a));

    for(s=(sentence_t*)ARR_FIRST(a); !ARR_END(a); s=(sentence_t*)ARR_NEXT(a)) {

        if(s->is_selected) {

            LOG(LL_DEBUG, "Found a selected article");

            if(s->is_para_begin) ++len; /* fprintf(stdout, "\n"); */

            w = s->begin;

            while(w < s->end) {

                while(0 == *w && w < s->end) ++w;
    
                if(w >= s->end) break;
    
                len += (strlen(w) + 1); /* fprintf(stdout, "%s ", w); */
    
                w = w + strlen(w);
            }
        }
    }

    rephdr.proto  = htons(SUMMARIZERD_PROTO);
    rephdr.ver    = htons(SUMMARIZERD_VERSION);
    rephdr.status = htonl(REP_SUMMARY);
    rephdr.summary_len = htonl(len);

    LOG(LL_INFO, "Sending summary response (%lu bytes) on %d",
                 len + sizeof(rephdr), sock);

    if(sizeof(rephdr) != (res = write_nb(sock, &rephdr, sizeof(rephdr)))) {
        return(res);
    }

    for(s=(sentence_t*)ARR_FIRST(a); !ARR_END(a); s=(sentence_t*)ARR_NEXT(a)) {

        if(s->is_selected) {

            if(s->is_para_begin)
                if(1 != (res = write_nb(sock, "\n", 1))) return res;

            w = s->begin;

            while(w < s->end) {

                while(0 == *w && w < s->end) ++w;
    
                if(w >= s->end) break;
    
                if((int)strlen(w) != (res = write_nb(sock, w, strlen(w))))
                    return res;

                if(1 != (res = write_nb(sock, " ", 1))) return res;
    
                w = w + strlen(w);
            }
        }
    }

    return(1); /* 0 == EAGAIN */
}

int
write_error_response(int sock, int err)
{
    /* response
     * proto[2] | ver[2] | status[4] | -- error case
     */
    int               res;
    size_t            len;
    error_header_t    rephdr;

    rephdr.proto  = htons(SUMMARIZERD_PROTO);
    rephdr.ver    = htons(SUMMARIZERD_VERSION);
    rephdr.status = htonl((int)err);

    len = sizeof(rephdr);

    LOG(LL_INFO, "Sending error response (error %d, %lu bytes) on %d",
                 err, len, sock);

    if((int)len != (res = write_nb(sock, &rephdr, len))) {
        return(res);
    }

    return(1);
}

int
write_nb(int sock, const void* buf, size_t len)
{
    int     wrote_len, total_len = 0;

    LOG(LL_DEBUG, "To write total of %lu bytes", len);

    while(len) {

        if(0 < (wrote_len = send(sock, buf, len, 0))) {
            len -= wrote_len;
            buf += wrote_len;
            total_len += wrote_len;
            LOG(LL_DEBUG, "Wrote %d, remaining %lu", wrote_len, len);
        } else
        if(0 > wrote_len) {
            switch(errno) {
            case EINTR:
                switch(g_sig) {
                    TERMSIGCASES:
                        LOG(LL_NOTICE, "send: term sig %d", g_sig);
                        return(EXIT_OK);
                    CRASHSIGCASES:
                        LOG(LL_CRIT, "send: crash signal %d", g_sig);
                        return(EXIT_CRASH);
                    default:
                        LOG(LL_FATAL, "send: signal %d", g_sig);
                        return(EXIT_CANT_RECOVER);
                }
            case ECONNRESET: case EPIPE:
                LOG(LL_INFO, "send: conn reset/pipe (client may have died)");
                return(PROTO_PEER_LOST);
            BLOCKCASES:
                LOG(LL_DEBUG, "send: not ready to write");
                return(0);
            default:
                LOG(LL_FATAL, "send: %s", strerror(errno));
                return(EXIT_CANT_RECOVER);
            }
        } else {
            LOG(LL_FATAL, "send: returned 0");
            return(EXIT_CANT_RECOVER);
        }
    }

    LOG(LL_DEBUG, "Wrote total of %d bytes", total_len);
    return(total_len);
}

int
handle_select_error(void)
{
    if(EINTR == errno) { /* signal */
        LOG(LL_INFO, "select: interrupted by signal");
        switch(g_sig) {
            TERMSIGCASES:
                LOG(LL_NOTICE, "select: term signal %d", g_sig);
                return(EXIT_OK);
            CRASHSIGCASES:
                LOG(LL_CRIT, "select: crash signal %d", g_sig);
                return(EXIT_CRASH);
            default:
                LOG(LL_FATAL, "select: signal %d", g_sig);
                return(EXIT_CANT_RECOVER);
        }
    } else {
        /* anything else is bad */
        LOG(LL_FATAL, "select: %s", strerror(errno));
        return(EXIT_CANT_RECOVER);
    }
}

int
handle_read_error(int res, worker_context_t* ctxt, sock_context_t* s)
{
    switch(res) {
        case PROTO_PEER_LOST:
            LOG(LL_INFO, "Socket %d closed from peer, removing"
                         " from worker's set", s->sock);
            res = close_peer(ctxt, s->sock);
            break;

        case PROTO_INVALID:
            LOG(LL_DEBUG, "Set reply type to error for %d", s->sock);
            s->rep_type = REP_ERROR_INVALID_REQ;
            s->status = SOCK_WRITE;
            res = 1;
            break;

        case EXIT_OK: case EXIT_CRASH: case EXIT_CANT_RECOVER:
            break;

        default:
            LOG(LL_ERROR, "Unknown error %d", res);
            res = EXIT_CANT_RECOVER;
            break;
    }

    return(res); /* things alright */
}

int
handle_write_error(int res, worker_context_t* ctxt, sock_context_t* s)
{
    switch(res) {
        case PROTO_PEER_LOST:
            LOG(LL_INFO, "Socket %d closed from peer, removing"
                 " from worker's set", s->sock);
            res = close_peer(ctxt, s->sock);
            break;

        case EXIT_OK: case EXIT_CRASH: case EXIT_CANT_RECOVER:
            break;

        default:
            LOG(LL_ERROR, "Unknown error %d", res);
            res = EXIT_CANT_RECOVER;
            break;
    }

    return(res);
}

int
close_peer(worker_context_t *ctxt, int sock)
{
    int       res = 1;
    array_t * a = ctxt->sock_contexts;

    if(0 != pthread_mutex_lock(&ctxt->mutex)) {
        LOG(LL_FATAL, "Can't lock thread context mutex");
        return(EXIT_CANT_RECOVER);
    }

    /* remove from our set and see if we hit 0 socks */
    array_remove(a, (elem_t)(intptr_t)sock, comp_sock_context);

    if(ARR_EMPTY(a)) {
        LOG(LL_DEBUG, "No more sockets with worker.");
        ctxt->max_fds = 0;
        res = 0;
    } else {
        ctxt->max_fds = ((sock_context_t*)ARR_LAST(a))->sock + 1;
    }

    /* close sock */
    close(sock);

    if(0 != pthread_mutex_unlock(&ctxt->mutex)) {
        LOG(LL_FATAL, "Can't unlock thread context mutex");
        return(EXIT_CANT_RECOVER);
    }

    return(res);
}

relation_t
comp_sock_context(const elem_t ctxt_obj, const elem_t sock) /* word_t*, int */
{
    const sock_context_t* ctxt = (const sock_context_t*)ctxt_obj;
    const int s = (const int)(const intptr_t)sock;

    int res = ctxt->sock - s;

    if(res < 0) return(SMRZR_LT);
    else if(res > 0) return(SMRZR_GT);
    else return(SMRZR_EQ);
}
