/*
 * summarizer.c
 */

#include "header.h"

/* FUNCTIONS */

int
main(int argc, char** argv)
{
    lang_t     lang;
    article_t  article;
    status_t   status;

    if(argc < 3) {
        fprintf(stderr, "Invalid number of arguments\n");
        return(-1);
    }

    status =
        init_globals() ||

        lang_init(&lang) ||

        parse_lang_xml(DICTIONARY_DIR"/en.xml", &lang) ||

        article_init(&article) ||

        parse_article(argv[1], &lang, &article) ||

        grade_article(&article, &lang, atof(argv[2])/100);

    print_summary(&article);

    article_destroy(&article);

    lang_destroy(&lang);

    return(SMRZR_OK == status ? 0 : 1);
}
