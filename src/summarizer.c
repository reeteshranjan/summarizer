/*
 * summarizer.c
 */

#include "header.h"

/* FUNCTIONS */

void usage(const char* prog)
{
    fprintf(stderr, "Usage: %s -i <input-file> -r <ratio>\n", prog);
    fprintf(stderr, "Usage: %s -h\n\n", prog);
    fprintf(stderr, "input-file : the file to summarize\n");
    fprintf(stderr, "     ratio : indicated using a percentage (without %%) sign\n");
    fprintf(stderr, "        -h : print this help\n");
}

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
