bin_PROGRAMS = summarizer summarizerd

summarizer_SOURCES = summarizer.c lib.c
summarizerd_SOURCES = summarizerd.c lib.c

DEFS = @DEFS@ -DDICTIONARY_DIR=\"$(pkgdatadir)/\"

CFLAGS = -Wall -Werror -Wextra

summarizer.o: summarizer.c header.h
summarizerd.o: summarizerd.c header.h
lib.o : lib.c header.h
