/*
 * common.h
 */

#ifndef SMRZR_COMMON_H
#define SMRZR_COMMON_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <ctype.h>
#include <assert.h>
#include "config.h"

/* MACROS */

#define PTR_DIFF(p1, p2)         ((ptr_t)p1 - (ptr_t)p2)
#define PTR_ADD(type, p1, sz)   ((type)((ptr_t)p1 + sz))

/* profiling */
#if defined SMRZRPROF

#define PROF_START     \
    struct timeval t1, t2; gettimeofday(&t1, NULL);

#define PROF_END(desc) \
    gettimeofday(&t2, NULL); \
    fprintf(stdout, desc ": time taken - %lu\n", \
        (t2.tv_sec - t1.tv_sec)*1000000 + (t2.tv_usec - t1.tv_usec))
#else
#define PROF_START
#define PROF_END(desc)
#endif

/* debugging */

#define ERROR_RET    \
do { \
    fprintf(stderr, "Error at #%d\n", __LINE__); \
    return(SMRZR_ERROR); \
} while(0)

#if defined SMRZRDEBUG
#else
#endif

/* stream*/

#define SPACE   " \t\n\r\v\f"

#define STREAM_FIND(s, c)  \
    (s)->curr = strchr((s)->curr, c)

#define STREAM_TOKEN(s, c) \
    (strsep(&(s)->curr, c))

#define STREAM_FIND_WORD(s) \
    (s)->curr += strspn((s)->curr, SPACE)

#define STREAM_GET_WORD(s, w, f) \
do { \
    charpos_t end; \
    w = (s)->curr; \
    (s)->curr += strcspn((s)->curr, SPACE); \
    end = (s)->curr; \
    STREAM_FIND_WORD(s); \
    while(end != (s)->curr) { \
        if(*end == '\n' || *end == '\r') f = SMRZR_TRUE; \
        *end = 0; ++end; \
    } \
} while(0)

#define STREAM_END(s) \
    ((size_t)((ptr_t)(s)->curr - (ptr_t)(s)->begin) >= (s)->len)

/* efficient list */

#define ARRAY_DEFAULT_SZ     16384

#define ARR_FULL(a, sz)    \
  ((SMRZR_TRUE == (a)->is_array && \
      (((ptr_t)((a)->curr) + (a)->elem_sz - ((ptr_t)a)) > (a)->array_sz)) || \
   (SMRZR_FALSE == (a)->is_array && \
      ((((ptr_t)(a)->curr) + sz + 1 - ((ptr_t)a)) > (a)->array_sz)))

#define ARR_FIRST(a) \
  (a->iter = (elem_t)((ptr_t)a + sizeof(array_t)))

#define ARR_END(a) \
  ((a->iter != a->curr) ? SMRZR_FALSE : SMRZR_TRUE)

#define ARR_NEXT(a) \
  (a->iter = (elem_t)((ptr_t)a->iter + a->elem_sz))

#define ARR_SZ(a) \
    (PTR_DIFF(a->curr, PTR_ADD(elem_t, a, sizeof(array_t))) / a->elem_sz)

/* lang info parsing */

#define XML_TAG_BEGIN_CHAR   '<'
#define XML_TAG_END_CHAR     '>'
#define RULE_SEPARATOR_CHAR  '|'

#define XML_TAG_BEGIN_STR    "<"
#define XML_TAG_END_STR      ">"
#define RULE_SEPARATOR_STR   "|"

/* article parsing */

#define MATCH_AT_END(w, e, wl, el) (el < wl && !strcasecmp(w + wl - el, e))
#define MATCH_AT_BEG(w, b, wl, bl) (bl < wl && !strncasecmp(w, b, bl))


/* TYPEDEFS */

typedef char       * charpos_t;
typedef char       * string_t;
typedef void       * elem_t;
typedef const char * literal_t;
typedef intptr_t     ptr_t;

typedef enum status_e   status_t;
typedef enum bool_e     bool_t;
typedef enum relation_e relation_t;

typedef struct stream_s stream_t;

typedef struct array_s array_t;

typedef struct sentence_s sentence_t;
typedef struct word_s     word_t;
typedef struct lang_s     lang_t;
typedef struct article_s  article_t;

typedef struct context_s context_t;

typedef relation_t (*compfunc_t)(const elem_t obj, const elem_t key);

/* ENUMS */

enum status_e {
    SMRZR_OK = 0,
    SMRZR_ERROR
};

enum bool_e {
    SMRZR_FALSE = 0,
    SMRZR_TRUE
};

enum relation_e {
    SMRZR_LT = -1,
    SMRZR_EQ = 0,
    SMRZR_GT = 1
};

/* STRUCTS */

/* In memory region */

struct stream_s {
    charpos_t           begin;
    charpos_t           curr;
    size_t              len;
    size_t              map_len;
    int                 fd;
};

/* Efficient list and lookup */

struct array_s {
    uint32_t            is_array;
    size_t              elem_sz;
    size_t              num_elems;
    size_t              array_sz;
    elem_t              curr;
    elem_t              iter;
};

/* Document processing */

struct sentence_s {
    charpos_t           begin;
    charpos_t           end;
    size_t              num_words;
    uint32_t            score;
    uint32_t            is_para_begin;
    uint32_t            is_selected;
    elem_t              cookie;
};

struct word_s {
    string_t            stem;
    size_t              num_occ;
};

struct lang_s {
    stream_t            stream;
    array_t           * pre1;
    array_t           * post1;
    array_t           * manual;
    array_t           * synonyms;
    array_t           * pre;
    array_t           * post;
    array_t           * line_break;
    array_t           * line_dont_break;
    array_t           * exclude;
};

struct article_s {
    stream_t            stream;
    size_t              num_words;
    array_t           * sentences;
    array_t           * words;
    array_t           * stack;
};


/* PROTOTYPES */

/* file stream */

status_t stream_create(const char* file_name, stream_t* stream);

void     stream_destroy(stream_t* stream);

/* efficient list and lookup */

array_t* array_new(uint32_t is_array, size_t elem_sz, size_t num_elems, array_t*
orig);

void     array_free(array_t* array);

status_t array_add_elemptr(array_t** array, elem_t elem);

elem_t   array_alloc(array_t** array);

elem_t   array_push_alloc(array_t** array, size_t sz);

void     array_pop_free(array_t* array, elem_t);

elem_t   array_sorted_alloc(array_t** array, const elem_t key, compfunc_t cf);

elem_t   array_search(const array_t* array, const elem_t key, compfunc_t cf);

elem_t   array_search_or_alloc(array_t** array, const elem_t key, compfunc_t cf, bool_t* is_new);

/* lang info parsing */

status_t lang_init(lang_t* lang);

void     lang_destroy(lang_t* lang);

status_t parse_lang_xml(const char* file_name, lang_t* lang);

status_t parse_stemmer_xml(lang_t* lang);

status_t parse_parser_xml(lang_t* lang);

status_t parse_exclude_xml(lang_t* lang);

status_t parse_children_for_array(stream_t* stream, array_t** array, literal_t
child_name);

string_t get_child_value(stream_t* stream, literal_t child_name);

string_t get_xml_tag(stream_t* stream);

/* article parsing */

status_t article_init(article_t* article);

void     article_destroy(article_t* article);

status_t parse_article(const char* file_name, lang_t* lang, article_t* article);

sentence_t* sentence_new(array_t** array, charpos_t begin);

string_t get_word_core(array_t** stack, lang_t* lang, const string_t word);

string_t get_word_stem(array_t** stack, lang_t* lang, const string_t word, bool_t is_core);

relation_t comp_strings(const elem_t s1, const elem_t s2); /* char**, char* */

relation_t comp_string_with_rule(const elem_t s1, const elem_t s2); /* char**, char* */

relation_t comp_word_by_stem(const elem_t word_obj, const elem_t stem); /* word_t*, char* */

bool_t   replace_word_head(string_t word, string_t rule);

bool_t   replace_word_tail(string_t word, string_t rule);

void     replace_word(array_t** stack, string_t word, string_t rule);

bool_t   end_of_line(lang_t* lang, string_t word);

/* article grading */

status_t grade_article(article_t* article, lang_t* lang, float ratio);

relation_t comp_sentence_by_score(const elem_t sen_obj, const elem_t num_occ);
/* sentence_t*, size_t */

/* output */

void     print_summary(article_t*);

/* others */

status_t init_globals(void);


/* GLOBALS */

#endif /* SMRZR_COMMON_H */
