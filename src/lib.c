/*
 * lib.c
 */

#include "header.h"

/* GLOBALS */

static size_t PAGESIZE;

/* FUNCTIONS */

status_t
init_globals(void)
{
    PAGESIZE = sysconf(_SC_PAGE_SIZE);

    return(SMRZR_OK);
}

status_t
lang_init(lang_t* lang)
{
#define PRE1_ELEM_ESTIMATE    20
#define POST1_ELEM_ESTIMATE   40
#define MANUAL_ELEM_ESTIMATE  200
#define SYNO_ELEM_ESTIMATE    200
#define PRE_ELEM_ESTIMATE     20
#define POST_ELEM_ESTIMATE    100
#define LBRK_ELEM_ESTIMATE    20
#define LNOBRK_ELEM_ESTIMATE  40
#define EXCLUDE_ELEM_ESTIMATE 400

    if(NULL == (lang->pre1 = array_new(SMRZR_TRUE, sizeof(elem_t),
                                       PRE1_ELEM_ESTIMATE, NULL)))
        ERROR_RET;

    if(NULL == (lang->post1 = array_new(SMRZR_TRUE, sizeof(elem_t),
                                       POST1_ELEM_ESTIMATE, NULL)))
        ERROR_RET;

    if(NULL == (lang->manual = array_new(SMRZR_TRUE, sizeof(elem_t),
                                       MANUAL_ELEM_ESTIMATE, NULL)))
        ERROR_RET;

    if(NULL == (lang->synonyms = array_new(SMRZR_TRUE, sizeof(elem_t),
                                       SYNO_ELEM_ESTIMATE, NULL)))
        ERROR_RET;

    if(NULL == (lang->pre = array_new(SMRZR_TRUE, sizeof(elem_t),
                                       PRE_ELEM_ESTIMATE, NULL)))
        ERROR_RET;

    if(NULL == (lang->post = array_new(SMRZR_TRUE, sizeof(elem_t),
                                       POST_ELEM_ESTIMATE, NULL)))
        ERROR_RET;

    if(NULL == (lang->line_break = array_new(SMRZR_TRUE, sizeof(elem_t),
                                       LBRK_ELEM_ESTIMATE, NULL)))
        ERROR_RET;

    if(NULL == (lang->line_dont_break = array_new(SMRZR_TRUE, sizeof(elem_t),
                                       LNOBRK_ELEM_ESTIMATE, NULL)))
        ERROR_RET;

    if(NULL == (lang->exclude = array_new(SMRZR_TRUE, sizeof(elem_t),
                                       EXCLUDE_ELEM_ESTIMATE, NULL)))
        ERROR_RET;

    return(SMRZR_OK);
}

status_t
parse_lang_xml(const char* file_name, lang_t* lang)
{
    string_t tag;
    PROF_START;

    if(SMRZR_OK != stream_create(file_name, &lang->stream))
        ERROR_RET;

    if(NULL == get_xml_tag(&lang->stream)) /* ignore the first line */
        ERROR_RET;

    if(NULL == get_xml_tag(&lang->stream)) /* next tag is the dictionary */
        ERROR_RET;

    /* next tag is a child - stemmer/parser/exclude */
    while(NULL != (tag = get_xml_tag(&lang->stream))) {
        if(!strcmp("stemmer", tag)) {
            if(SMRZR_OK != parse_stemmer_xml(lang)) return SMRZR_ERROR;
        } else
        if(!strcmp("parser", tag)) {
            if(SMRZR_OK != parse_parser_xml(lang)) return SMRZR_ERROR;
        } else
        if(!strcmp("exclude", tag)) {
            if(SMRZR_OK != parse_exclude_xml(lang)) return SMRZR_ERROR;
        } else
        if(!strcmp("/dictionary", tag)) { /* done with xml doc */
            PROF_END("lang info xml parsing");
            return(SMRZR_OK);
        } else {
            fprintf(stderr, "Invalid child '%s' of 'dictionary' node\n", tag);
            ERROR_RET;
        }
    }

    PROF_END("lang info xml parsing");
    return(SMRZR_OK);
}

status_t
parse_stemmer_xml(lang_t* lang)
{
    string_t tag;

    /* next tag is a child - pre1/post1/manual/pre/post/synonyms */
    while(NULL != (tag = get_xml_tag(&lang->stream))) {

        if(!strcmp("pre1", tag)) {
            if(SMRZR_OK != parse_children_for_array(&lang->stream,
                                                     &lang->pre1, "rule"))
                ERROR_RET;
        } else
        if(!strcmp("post1", tag)) {
            if(SMRZR_OK != parse_children_for_array(&lang->stream,
                                                     &lang->post1, "rule"))
                ERROR_RET;
        } else
        if(!strcmp("manual", tag)) {
            if(SMRZR_OK != parse_children_for_array(&lang->stream,
                                                     &lang->manual, "rule"))
                ERROR_RET;
        } else
        if(!strcmp("pre", tag)) {
            if(SMRZR_OK != parse_children_for_array(&lang->stream,
                                                     &lang->pre, "rule"))
                ERROR_RET;
        } else
        if(!strcmp("post", tag)) {
            if(SMRZR_OK != parse_children_for_array(&lang->stream,
                                                     &lang->post, "rule"))
                ERROR_RET;
        } else
        if(!strcmp("synonyms", tag)) {
            if(SMRZR_OK != parse_children_for_array(&lang->stream,
                                                     &lang->synonyms, "rule"))
                ERROR_RET;
        } else
        if(!strcmp("/stemmer", tag)) { /* done with all children */
            return(SMRZR_OK);
        } else {
            fprintf(stderr, "Invalid child '%s' of 'stemmer' node\n", tag);
            ERROR_RET;
        }
    }

    return(SMRZR_OK);
}

status_t
parse_parser_xml(lang_t* lang)
{
    string_t tag;

    /* next tag is a child - linebreak/linedontbreak */
    while(NULL != (tag = get_xml_tag(&lang->stream))) {

        if(!strcmp("linebreak", tag)) {
            if(SMRZR_OK != parse_children_for_array(&lang->stream,
                                             &lang->line_break, "rule"))
                ERROR_RET;
        } else
        if(!strcmp("linedontbreak", tag)) {
            if(SMRZR_OK != parse_children_for_array(&lang->stream,
                                             &lang->line_dont_break, "rule"))
                ERROR_RET;
        } else
        if(!strcmp("/parser", tag)) { /* done with all children */
            return(SMRZR_OK);
        } else {
            fprintf(stderr, "Invalid child '%s' of 'parser' node\n", tag);
            ERROR_RET;
        }
    }

    return(SMRZR_OK);
}

status_t
parse_exclude_xml(lang_t* lang)
{
    /* it has all direct children with name 'word' */
    return(parse_children_for_array(&lang->stream, &lang->exclude, "word"));
}

status_t
parse_children_for_array(stream_t* stream, array_t** array, literal_t
child_name)
{
    string_t tag;

    while(NULL != (tag = get_child_value(stream, child_name))) {

        if(SMRZR_OK != array_add_elemptr(array, tag))
            ERROR_RET;
    }

    return(SMRZR_OK);
}

string_t
get_child_value(stream_t* stream, literal_t child_name)
{
    string_t child = get_xml_tag(stream);

    /* next tag should match given child name */
    if(NULL == child || strcmp(child, child_name))
        return(NULL);

    /* take the value till beginning of next token */
    if(NULL == (child = STREAM_TOKEN(stream, XML_TAG_BEGIN_STR)))
        return(NULL);

    /* next tag end is end of the child tag */
    STREAM_FIND(stream, XML_TAG_END_CHAR);
    if(0 == *stream->curr) return(NULL);

    /* all done well */
    return(child);
}

string_t
get_xml_tag(stream_t* stream)
{
    STREAM_FIND(stream, XML_TAG_BEGIN_CHAR);
    if(0 == *stream->curr) return(NULL);

    ++(stream->curr); /* go to char next to '<' */

    return(STREAM_TOKEN(stream, XML_TAG_END_STR));
}

status_t
array_add_elemptr(array_t** array, elem_t elem)
{
    array_t* a = *array;
    assert(SMRZR_TRUE == a->is_array && sizeof(elem_t) == a->elem_sz);

    if(ARR_FULL(a, sizeof(elem_t))) {
        if(NULL == (*array = array_new(a->is_array, a->elem_sz,
                                       2 * a->num_elems, a)))
            ERROR_RET;
    }

    ptr_t ptr_val = (ptr_t)elem;

    *((ptr_t*)a->curr) = ptr_val;
    a->curr = PTR_ADD(elem_t, a->curr, a->elem_sz);

    return(SMRZR_OK);
}

status_t
article_init(article_t* article)
{
#define SENTENCE_ESTIMATE    100
#define WORDS_ESTIMATE       400

    article->num_words = 0;

    if(NULL == (article->sentences = array_new(SMRZR_TRUE, sizeof(sentence_t),
                                      SENTENCE_ESTIMATE, NULL)))
        ERROR_RET;

    if(NULL == (article->words = array_new(SMRZR_TRUE, sizeof(word_t),
                                      WORDS_ESTIMATE, NULL)))
        ERROR_RET;

    if(NULL == (article->stack = array_new(SMRZR_FALSE, 0, 0, NULL)))
        ERROR_RET;

    return(SMRZR_OK);
}

status_t
parse_article(const char* file_name, lang_t* lang, article_t* article)
{
    string_t    word, word_core, word_stem;
    sentence_t* sentence;
    word_t*     word_entry;
    stream_t*   stream = &article->stream;
    bool_t      is_new, is_para_end = SMRZR_FALSE;

    PROF_START;

    if(SMRZR_OK != stream_create(file_name, stream))
        ERROR_RET;

    while(!STREAM_END(stream)) {

        STREAM_FIND_WORD(stream);

        if(STREAM_END(stream)) break;

        sentence = sentence_new(&article->sentences, article->stream.curr);
        assert(NULL != sentence);

        if(SMRZR_TRUE == is_para_end) {
            sentence->is_para_begin = SMRZR_TRUE;
            is_para_end = SMRZR_FALSE;
        }

        while(!STREAM_END(stream)) {

            STREAM_GET_WORD(stream, word, is_para_end);
            
            sentence->num_words++;

            if(NULL == (word_core = get_word_core(&article->stack, lang, word)))
                ERROR_RET;

            if(NULL == array_search(lang->exclude, word_core, comp_strings)) {

                if(NULL == (word_stem = get_word_stem(&article->stack, lang,
                                                      word_core, SMRZR_TRUE)))
                    ERROR_RET;

                if(NULL == (word_entry = array_search_or_alloc(&article->words,
                                        word_stem, comp_word_by_stem, &is_new)))
                    ERROR_RET;

                if(SMRZR_TRUE == is_new) {
                    word_entry->num_occ = 1;
                    word_entry->stem = word_stem;
                } else {
                    ++(word_entry->num_occ);
                    array_pop_free(article->stack, word_stem);
                }
            } else {
                array_pop_free(article->stack, word_core);
            }

            if(end_of_line(lang, word)) {
                sentence->end = word + strlen(word);
                article->num_words += sentence->num_words;
                break;
            }
        }
    }

    PROF_END("article parsing");

    /*fprintf(stdout, "Number of sentences - %lu\n", ARR_SZ(article->sentences));
    fprintf(stdout, "Number of words - %lu\n", ARR_SZ(article->words));*/

    return(SMRZR_OK);
}

#define TOP_OCCS_MAX 4
static uint32_t occ2score[] = { 3, 2, 2, 2, 1 };

status_t
grade_article(article_t* article, lang_t* lang, float ratio)
{
    array_t     * a, * temp;
    word_t      * w;
    sentence_t  * s, * s_score;
    size_t        top_occs[] = { 0, 0, 0, 0}, occs, i, max_words;
    word_t      * top_words[] = { 0, 0, 0, 0};
    string_t      ws, ws_stem;
    bool_t        is_first = SMRZR_TRUE;

    PROF_START;

    /* find top occs and corresponding words */
    a = article->words;

    for(w = (word_t*)ARR_FIRST(a); !ARR_END(a); w = (word_t*)ARR_NEXT(a)) {
        for(occs = 0; occs < TOP_OCCS_MAX; ++occs) {
            if(top_occs[occs] < w->num_occ) {
                for(i = TOP_OCCS_MAX-1; i > occs; --i) {
                    top_occs[i] = top_occs[i-1];
                    top_words[i] = top_words[i-1];
                }
                top_occs[occs] = w->num_occ;
                top_words[occs] = w;
                break;
            }
        }
    }

    /*for(occs = 0; occs < TOP_OCCS_MAX; ++occs) {
        fprintf(stdout, "top occ %lu - %lu [%s]\n", occs, top_occs[occs],
               top_words[occs]->stem);
    }*/

    /* score all sentences */
    a = article->sentences;

    for(s=(sentence_t*)ARR_FIRST(a); !ARR_END(a); s=(sentence_t*)ARR_NEXT(a)) {

        ws = s->begin;

        while(ws < s->end) {

            while(0 == *ws && ws < s->end) ++ws;

            if(ws >= s->end) break;

            if(NULL == (ws_stem = get_word_stem(&article->stack, lang, ws,
                                                SMRZR_FALSE)))
                ERROR_RET;

            if(NULL == (w = (word_t*)array_search(article->words, ws_stem,
                                                  comp_word_by_stem)))
            { /* possibly a word excluded */
                ws = ws + strlen(ws);
                continue;
            }

            occs = 0;
            while(top_occs[occs] != w->num_occ && occs < TOP_OCCS_MAX) ++occs;

            switch(occ2score[occs]) {
                case 3: /* score += occ * 3 */
                    s->score += ((w->num_occ << 1) + w->num_occ); break;
                case 2: /* score += occ * 2 */
                    s->score += (w->num_occ << 1); break;
                case 1: /* score += occ */
                    s->score += w->num_occ; break;
                default: 
                    ERROR_RET;
            }

            ws = ws + strlen(ws);
        }

        if(SMRZR_TRUE == s->is_para_begin) {
            s->score *= 1.6;
        } else if(SMRZR_TRUE == is_first) {
            s->score = (s->score << 1); /* super-boost 1st line */
            is_first = SMRZR_FALSE;
        }

        /*fprintf(stdout, "%u ", s->score);*/
    }

    /*fprintf(stdout, "\n");*/

    /* sort on sentence score */
    if(NULL == (temp = array_new(SMRZR_TRUE, sizeof(sentence_t),
                                 ARR_SZ(article->sentences), NULL)))
        ERROR_RET;

    for(s=(sentence_t*)ARR_FIRST(a); !ARR_END(a); s=(sentence_t*)ARR_NEXT(a)) {
        if(NULL == (s_score = array_sorted_alloc(&temp, (elem_t)(size_t)(s->score),
                                                 comp_sentence_by_score)))
            ERROR_RET;

        memcpy(s_score, s, sizeof(sentence_t));
        s_score->cookie = (elem_t)s;
    }

    /* pick sentences with highest scores until we get required ratio of words*/
    max_words = article->num_words * ratio;

    a = temp;
    for(s=(sentence_t*)ARR_FIRST(a); !ARR_END(a) && (ssize_t)max_words > 0;
                                     s=(sentence_t*)ARR_NEXT(a))
    {
        ((sentence_t*)s->cookie)->is_selected = SMRZR_TRUE;
        max_words -= s->num_words;
        /*fprintf(stdout, "Selected sentence: score %u, %lu words, %ld
          remaining\n", s->score, s->num_words, (ssize_t)max_words);*/
    }

    array_free(temp);

    PROF_END("article grading");

    return(SMRZR_OK);
}

string_t
get_word_stem(array_t** stack, lang_t* lang, const string_t word, bool_t is_core)
{
    string_t  changed, copy, *r;
    array_t*  a;

    if(SMRZR_TRUE == is_core) {
        changed = word;
    } else {
        if(NULL == (changed = get_word_core(stack, lang, word)))
            return(NULL);
    }

    if(isupper(changed[0]) && strlen(changed) > 1) return(changed);

    copy = array_push_alloc(stack, strlen(changed)+1);
    strcpy(copy, changed);

    a = lang->manual;

    if(NULL!=(r=(string_t*)array_search(a, changed, comp_string_with_rule))){
        replace_word(stack, changed, *r);
    }

    a = lang->pre;

    for(r = (string_t*)ARR_FIRST(a); !ARR_END(a); r = (string_t*)ARR_NEXT(a)) {
        if(SMRZR_TRUE == replace_word_head(changed, *r)) break;
    }

    a = lang->post;

    for(r = (string_t*)ARR_FIRST(a); !ARR_END(a); r = (string_t*)ARR_NEXT(a)) {
        if(SMRZR_TRUE == replace_word_tail(changed, *r)) break;
    }

    a = lang->synonyms;

    if(NULL!=(r=(string_t*)array_search(a, changed, comp_string_with_rule))){
        replace_word(stack, changed, *r);
    }

    /* quality check */
    if(strlen(changed) < 3) {
        memmove(changed, copy, strlen(copy)+1);
        (*stack)->curr = PTR_ADD(elem_t, changed, strlen(changed)+1);
    } else {
        array_pop_free(*stack, copy);
    }

    return(changed);
}

string_t
get_word_core(array_t** stack, lang_t* lang, const string_t word)
{
    size_t    sz = strlen(word)+1, i;
    string_t  changed = array_push_alloc(stack, sz), *r;
    array_t*  a;

    if(NULL == changed) return(NULL);

    strcpy(changed, word);

    if(isupper(changed[0]) && strlen(changed) > 1) return(changed);

    for(i = 0; i < sz; ++i) changed[i] = tolower(changed[i]);

    a = lang->pre1;

    for(r = (string_t*)ARR_FIRST(a); !ARR_END(a); r = (string_t*)ARR_NEXT(a)) {
        if(SMRZR_TRUE == replace_word_head(changed, *r)) break;
    }

    a = lang->post1;

    for(r = (string_t*)ARR_FIRST(a); !ARR_END(a); r = (string_t*)ARR_NEXT(a)) {
        if(SMRZR_TRUE == replace_word_tail(changed, *r)) break;
    }

    return(changed);
}

bool_t
replace_word_head(string_t word, string_t rule)
{
    bool_t   res = SMRZR_FALSE;
    string_t from;
    size_t   word_len = strlen(word), from_len, to_len;

    from = strsep(&rule, RULE_SEPARATOR_STR);

    from_len = strlen(from);

    assert(from_len > strlen(rule));

    if(MATCH_AT_BEG(word, from, word_len, from_len)) {

        to_len = strlen(rule);

        memmove(word + to_len, word + from_len, word_len - from_len + 1);

        if(0 != *rule) {
            memcpy(word, rule, to_len);
        }

        res = SMRZR_TRUE;
    }

    *(rule-1) = RULE_SEPARATOR_CHAR; /* undo rule tokenizing */

    return(res);
}

bool_t
replace_word_tail(string_t word, string_t rule)
{
    bool_t   res = SMRZR_FALSE;
    string_t from;
    size_t   word_len = strlen(word), from_len;

    from = strsep(&rule, RULE_SEPARATOR_STR);

    from_len = strlen(from);

    assert(from_len > strlen(rule));

    if(MATCH_AT_END(word, from, word_len, from_len)) {

        if(0 == *rule) /* nothing to replace with */
            *(word + word_len - from_len) = 0;
        else
            strcpy(word + word_len - from_len, rule);

        res = SMRZR_TRUE;
    }

    *(rule-1) = RULE_SEPARATOR_CHAR; /* undo rule tokenizing */

    return(res);
}

void
replace_word(array_t** stack, string_t word, string_t rule)
{
    string_t from;
    size_t   word_len = strlen(word), to_len, offset;

    from = strsep(&rule, RULE_SEPARATOR_STR);

    assert(0 != *rule); /* something to replace must be there */

    /*fprintf(stdout, "%s %s\n", word, from);*/

    if(0 != strcasecmp(word, from)) return;

    to_len = strlen(rule);

    if(to_len > word_len) { /* need to extend 'word' memory */

        if(ARR_FULL(*stack, to_len - word_len)) { /* re-alloc, adjust word */

            offset = PTR_DIFF(word, *stack);

            assert(NULL != (*stack = array_new(SMRZR_FALSE, 0, 0, *stack)));

            word = PTR_ADD(string_t, *stack, offset);
        }
    }

    strcpy(word, rule); /* replace */

    (*stack)->curr = PTR_ADD(elem_t, word, to_len + 1); /* adjust curr */
}

elem_t
array_search(const array_t* a, const elem_t key, compfunc_t cf)
{
    size_t  num_elems;
    elem_t  elem0, center;
    int     lo, hi, mid;

    assert(SMRZR_TRUE == a->is_array && 0 != a->elem_sz);

    elem0 = PTR_ADD(elem_t, a, sizeof(array_t));

    num_elems = PTR_DIFF(a->curr, elem0) / a->elem_sz;

    lo = 0;
    hi = num_elems-1;

    while(lo <= hi) {

        mid = (lo + hi)/2;

        center = PTR_ADD(elem_t, elem0, mid * a->elem_sz);

        switch(cf(center, key)) {
            case SMRZR_EQ: return(center);
            case SMRZR_GT: hi = mid - 1; break;
            case SMRZR_LT: lo = mid + 1; break;
            default: assert(SMRZR_FALSE);
        }
    }

    return(NULL);
}

elem_t
array_search_or_alloc(array_t** array, const elem_t key, compfunc_t cf, bool_t* is_new)
{
    size_t      num_elems;
    elem_t      elem0, center, elem;
    int         lo, hi, mid;
    array_t   * a = *array;

    assert(SMRZR_TRUE == a->is_array && 0 != a->elem_sz);

    elem0 = PTR_ADD(elem_t, a, sizeof(array_t));

    num_elems = PTR_DIFF(a->curr, elem0) / a->elem_sz;

    if(!num_elems) {
        a->curr = PTR_ADD(elem_t, elem0, a->elem_sz);
        *is_new = SMRZR_TRUE;
        return(elem0);
    }

    lo = 0;
    hi = num_elems-1;

    while(lo <= hi) {

        mid = (lo + hi)/2;

        center = PTR_ADD(elem_t, elem0, mid * a->elem_sz);

        switch(cf(center, key)) {
            case SMRZR_EQ:
                *is_new = SMRZR_FALSE;
                return(center);
            case SMRZR_GT: hi = mid - 1; break;
            case SMRZR_LT: lo = mid + 1; break;
            default: assert(SMRZR_FALSE);
        }
    }

    /* insert just before lo */
    if(SMRZR_TRUE == ARR_FULL(a, a->elem_sz)) {
        if(NULL == (*array = array_new(a->is_array, a->elem_sz,
                                       2 * a->num_elems, a)))
            return(NULL);

        elem0 = PTR_ADD(elem_t, a, sizeof(array_t));
    }

    elem = PTR_ADD(elem_t, elem0, lo * a->elem_sz);

    memmove(PTR_ADD(elem_t, elem, a->elem_sz), elem, PTR_DIFF(a->curr, elem));

    a->curr = PTR_ADD(elem_t, a->curr, a->elem_sz);

    *is_new = SMRZR_TRUE;

    return(elem);
}

elem_t
array_push_alloc(array_t** array, size_t sz)
{
    elem_t elem;
    array_t* a = *array;

    assert(SMRZR_FALSE == a->is_array);

    if(ARR_FULL(a, sz)) {
        if(NULL == (*array = array_new(SMRZR_FALSE, 0, 0, a)))
            return(NULL);
    }

    elem = a->curr;
    a->curr = PTR_ADD(elem_t, a->curr, sz);

    return(elem);
}

void
array_pop_free(array_t* a, elem_t elem)
{
    assert(SMRZR_FALSE == a->is_array);

    assert(PTR_DIFF(elem, a) > 0 && PTR_DIFF(a->curr, elem) > 0);

    a->curr = elem;
}

relation_t
comp_strings(const elem_t s1, const elem_t s2) /* char**, char* */
{
    const char* a = *((const char**)s1);
    const char* b = (const char*)s2;
    int res;

    if(0 > (res = strcasecmp(a, b))) return(SMRZR_LT);
    else if(0 < res) return(SMRZR_GT);
    else return(SMRZR_EQ);
}

relation_t
comp_string_with_rule(const elem_t s1, const elem_t s2) /* char**, char* */
{
    const char* a = *((const char**)s1);
    const char* b = (const char*)s2;
    int res;

    size_t a_len = strlen(a), b_len = strcspn(b, RULE_SEPARATOR_STR);

    if(a_len < b_len) return(SMRZR_LT);
    else if(a_len > b_len) return(SMRZR_GT);

    if(0 > (res = strcasecmp(a, b))) return(SMRZR_LT);
    else if(0 < res) return(SMRZR_GT);
    else return(SMRZR_EQ);
}

relation_t
comp_word_by_stem(const elem_t word_obj, const elem_t stem) /* word_t*, char* */
{
    const word_t* w = (const word_t*)word_obj;
    const char* s = (const char*)stem;
    int res;

    if(0 > (res = strcasecmp(w->stem, s))) return(SMRZR_LT);
    else if(0 < res) return(SMRZR_GT);
    else return(SMRZR_EQ);
}

sentence_t*
sentence_new(array_t** array, charpos_t begin)
{
    sentence_t* s = (sentence_t*)array_alloc(array);

    if(NULL == s) return(NULL);

    s->begin = begin;
    s->num_words = s->score = 0;
    s->is_para_begin = SMRZR_FALSE;
    s->is_selected = SMRZR_FALSE;

    return(s);
}

elem_t
array_alloc(array_t** array)
{
    elem_t elem;
    array_t* a = *array;

    assert(SMRZR_TRUE == a->is_array);

    if(ARR_FULL(a, a->elem_sz)) {
        if(NULL == (*array = array_new(a->is_array, a->elem_sz,
                                       2 * a->num_elems, a)))
            return(NULL);
    }

    elem = a->curr;
    a->curr = PTR_ADD(elem_t, a->curr, a->elem_sz);

    return(elem);
}

bool_t
end_of_line(lang_t* lang, string_t word)
{
    bool_t       res = SMRZR_FALSE;
    string_t   * r;
    size_t       word_len = strlen(word), rule_len;
    array_t    * a;

    a = lang->line_break;

    for(r = (string_t*)ARR_FIRST(a); !ARR_END(a); r = (string_t*)ARR_NEXT(a)) {

        rule_len = strlen(*r);

        if(MATCH_AT_END(word, *r, word_len, rule_len)) {
            res = SMRZR_TRUE;
            break;
        }
    }

    if(SMRZR_FALSE == res) return(res);

    a = lang->line_dont_break;

    for(r = (string_t*)ARR_FIRST(a); !ARR_END(a); r = (string_t*)ARR_NEXT(a)) {

        rule_len = strlen(*r);

        if(MATCH_AT_END(word, *r, word_len, rule_len)) {
            res = SMRZR_FALSE;
            break;
        }
    }

    return(res);
}

elem_t
array_sorted_alloc(array_t** array, const elem_t key, compfunc_t cf)
{
    size_t      num_elems;
    elem_t      elem0, center, elem;
    int         lo, hi, mid;
    array_t   * a = *array;

    assert(SMRZR_TRUE == a->is_array && 0 != a->elem_sz);

    elem0 = PTR_ADD(elem_t, a, sizeof(array_t));

    num_elems = PTR_DIFF(a->curr, elem0) / a->elem_sz;

    if(!num_elems) {
        a->curr = PTR_ADD(elem_t, elem0, a->elem_sz);
        return(elem0);
    }

    lo = 0;
    hi = num_elems-1;

    while(lo <= hi) {

        mid = (lo + hi)/2;

        center = PTR_ADD(elem_t, elem0, mid * a->elem_sz);

        switch(cf(center, key)) {
            case SMRZR_EQ:
                lo = mid; hi = lo -1; break;
            case SMRZR_GT:
                hi = mid - 1; break;
            case SMRZR_LT:
                lo = mid + 1; break;
            default:
                assert(SMRZR_FALSE);
        }
    }

    /* insert just before lo */
    if(SMRZR_TRUE == ARR_FULL(a, a->elem_sz)) {
        if(NULL == (*array = array_new(a->is_array, a->elem_sz,
                                       2 * a->num_elems, a)))
            return(NULL);

        elem0 = PTR_ADD(elem_t, a, sizeof(array_t));
    }

    elem = PTR_ADD(elem_t, elem0, lo * a->elem_sz);

    memmove(PTR_ADD(elem_t, elem, a->elem_sz), elem, PTR_DIFF(a->curr, elem));

    a->curr = PTR_ADD(elem_t, a->curr, a->elem_sz);

    return(elem);
}

relation_t
comp_sentence_by_score(const elem_t sen_obj, const elem_t score) /* sentence_t*, size_t */
{
    const sentence_t* s = (const sentence_t*)sen_obj;
    int res = s->score - (size_t)score;

    if(0 > res) return(SMRZR_GT);
    else if(0 < res) return(SMRZR_LT);
    else return(SMRZR_EQ);
}

status_t
stream_create(const char* file_name, stream_t* stream)
{
    int         fd;
    struct stat st;
    size_t      map_len;

    /* open */
    if(0 > (fd = open(file_name, O_RDONLY))) {
        perror("Error in opening file: ");
        ERROR_RET;
    }

    /* get stat */
    if(0 != fstat(fd, &st)) {
        perror("Error in taking stat of file: ");
        ERROR_RET;
    }

    /* mmap */
    map_len = (1 + (st.st_size / PAGESIZE)) * PAGESIZE;

    if(MAP_FAILED == (stream->begin =
        mmap(NULL, map_len, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0)))
    {
        perror("Error in mmap'ing file: ");
        ERROR_RET;
    }

    stream->len = st.st_size;
    stream->map_len = map_len;
    stream->begin[stream->len] = 0; /* null-terminated for token processing */
    stream->fd = fd;
    stream->curr = stream->begin;

    return(SMRZR_OK);
}

array_t*
array_new(uint32_t is_array, size_t elem_sz, size_t num_elems, array_t* orig)
{
    array_t* array;
    size_t   curr_offset;
    size_t   array_sz;

    assert(0 != elem_sz || SMRZR_FALSE == is_array);

    if(SMRZR_TRUE == is_array) {

        array_sz = elem_sz * num_elems + sizeof(array_t);

        array_sz = (1 + (array_sz / PAGESIZE)) * PAGESIZE;

        num_elems = (array_sz - sizeof(array_t)) / elem_sz;

    } else {
        if(NULL == orig)
            array_sz = ARRAY_DEFAULT_SZ;
        else
            array_sz = 2 * orig->array_sz;
    }

    if(NULL == orig) curr_offset = sizeof(array_t);
    else curr_offset = PTR_DIFF(orig->curr, orig);

    if(NULL == orig) {
        if(NULL == (array = (array_t*)malloc(array_sz))) {
            return(NULL);
        }
    } else {
        if(NULL == (array = (array_t*)realloc(orig, array_sz))) {
            return(NULL);
        }
    }

    array->is_array = is_array;
    array->elem_sz = ((SMRZR_TRUE == is_array) ? elem_sz : 0);
    array->num_elems = num_elems;
    array->array_sz = array_sz;
    array->curr = PTR_ADD(elem_t, array, curr_offset);

    return(array);
}

void
print_summary(article_t* article)
{
    array_t     * a;
    sentence_t  * s;
    string_t      w;

    PROF_START;

    a = article->sentences;

    for(s=(sentence_t*)ARR_FIRST(a); !ARR_END(a); s=(sentence_t*)ARR_NEXT(a)) {

        if(s->is_selected) {

            if(s->is_para_begin) fprintf(stdout, "\n");

            w = s->begin;

            while(w < s->end) {

                while(0 == *w && w < s->end) ++w;

                if(w >= s->end) break;

                fprintf(stdout, "%s ", w);

                w = w + strlen(w);
            }
        }
    }

    PROF_END("summary output");
}

void
lang_destroy(lang_t* lang)
{
    stream_destroy(&lang->stream);

    array_free(lang->pre1);
    array_free(lang->post1);
    array_free(lang->manual);
    array_free(lang->synonyms);
    array_free(lang->pre);
    array_free(lang->post);
    array_free(lang->line_break);
    array_free(lang->line_dont_break);
    array_free(lang->exclude);
}

void
article_destroy(article_t* article)
{
    stream_destroy(&article->stream);

    array_free(article->stack);
    array_free(article->words);
    array_free(article->sentences);
}

void
stream_destroy(stream_t* stream)
{
    close(stream->fd);

    munmap(stream->begin, stream->map_len);

    memset(stream, 0, sizeof(stream_t));
}

void
array_free(array_t* array)
{
    free(array);
}
