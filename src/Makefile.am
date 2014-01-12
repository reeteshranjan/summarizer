bin_PROGRAMS = summarizer summarizerd daemontest

summarizer_SOURCES = summarizer.c lib.c
summarizerd_SOURCES = summarizerd.c lib.c
daemontest_SOURCES = daemontest.c

summarizerd_LDADD = -lpthread

DEFS = @DEFS@ -DDICTIONARY_DIR=\"$(pkgdatadir)/\"

#CFLAGS = -Wall -Werror -Wextra -Wno-unused-parameter -DSMRZRLOG -DSMRZRPROF
CFLAGS = -g -ggdb3 -O0 -Wall -Werror -Wextra -Wno-strict-aliasing -Wno-unused-parameter -DSMRZRLOG -DSMRZRPROF

summarizer.o: summarizer.c header.h
summarizerd.o: summarizerd.c header.h daemon.h
daemontest.o: daemontest.c daemon.h
lib.o : lib.c header.h
