/*
 * summarizer.c
 */

#include "header.h"

/* FUNCTIONS */

static void print_summary(article_t* article);
static void usage(const char* prog);

int
main(int argc, char** argv)
{
    lang_t     lang;
    article_t  article;
    status_t   status;
    int        opt;
    literal_t  file_name = NULL;
    float      ratio = 0.0;

    while(-1 != (opt = getopt(argc, argv, "i:r:h"))) {
        switch(opt) {
            case 'i': file_name = optarg; break;
            case 'r': ratio = atof(optarg)/100; break;
            case 'h': usage(argv[0]); return(0);
            default: usage(argv[0]); return(1);
        }
    }

    if(NULL == file_name) {
        fprintf(stderr, "No input file specified\n");
        usage(argv[0]);
        return(1);
    }

    if(0.0 == ratio) {
        fprintf(stderr, "Ratio cannot be 0.0\n");
        usage(argv[0]);
        return(1);
    }

    status =
        init_globals() ||

        lang_init(&lang) ||

        parse_lang_xml(DICTIONARY_DIR"/en.xml", &lang) ||

        article_init(&article) ||

        parse_article(file_name, &lang, &article) ||

        grade_article(&article, &lang, ratio);

    print_summary(&article);

    article_destroy(&article);

    lang_destroy(&lang);

    return(SMRZR_OK == status ? 0 : 1);
}

void usage(const char* prog)
{
    fprintf(stderr, "Usage: %s -i <input-file> -r <ratio>\n", prog);
    fprintf(stderr, "Usage: %s -h\n\n", prog);
    fprintf(stderr, "input-file : the file to summarize\n");
    fprintf(stderr, "     ratio : indicated using a percentage (without %%) sign\n");
    fprintf(stderr, "        -h : print this help\n");
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
