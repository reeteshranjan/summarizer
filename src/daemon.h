/*
 * daemon.h
 *
 * Summarizer daemon entities also used by the test client
 */

#ifndef SUMMARIZER_DAEMON_H
#define SUMMARIZER_DAEMON_H


/* MACROS */

#define SUMMARIZERD_PORT      9872
#define SUMMARIZERD_PROTO     0x1421
#define SUMMARIZERD_VERSION   0x1

#define MAX_FILENAME_LEN      256

/* TYPES */

typedef enum {
    REP_SUMMARY = 0,
    REP_ERROR_INVALID_REQ,
    REP_ERROR_INTERNAL_ERROR
} response_type_t;

typedef struct {
    uint16_t           proto;
    uint16_t           ver;
    uint32_t           ratio;
    uint32_t           filename_len;
} request_header_t;

typedef struct {
    uint16_t           proto;
    uint16_t           ver;
    uint32_t           status;
    uint32_t           summary_len;
} response_header_t;

typedef struct {
    uint16_t           proto;
    uint16_t           ver;
    uint32_t           status;
} error_header_t;

#endif /* SUMMARIZER_DAEMON_H */
