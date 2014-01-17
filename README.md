summarizer
==========

Version

    1.0

Overview

    This is an adaptation of Open-Text-Summarizer (aka ots) with
    mean optimizations for a faster output.

Build

    $ ./configure [--prefix=/usr/local/summarizer/]
    $ make
    $ [sudo] make install

Usage

    Summarizer command line application

    $ [prefix]/bin/summarizer -i <file-to-summarize> -r <summary-ratio>

    Start/stop summarizer daemon

    $ sudo service summarizerd start
    $ sudo service summarizerd stop
    $ [prefix]/bin/summarizerd -h (for command line options)

Performance Comparison

    System: 1 VCPU, 512MB RAM, 20GB SSD

    Article: washingtonpost1.txt
    ots: between 40ms and 50ms
    summarizer: between 10ms and 12ms

    Article: washingtonpost2.txt
    ots: between 40ms and 50ms
    summarizer: between 8ms and 10ms

Limitations

    Languages: English only (as of now)

Daemon protocol

    NOTE: Refer src/daemontest.c for sample client application

    Request

    [2 bytes] Summarizerd protocol [Accepted: 0x1421]
    [2 bytes] Summarizerd version  [Accepted: 1]
    [4 bytes] Ratio ("Read" as float by daemon: refer daemontest.c)
    [4 bytes] Document name length [Max: 256]
    [N bytes] Document name (as long as above field's value)

    Response

    [2 bytes] Summarizerd protocol [Accepted: 0x1421]
    [2 bytes] Summarizerd version  [Accepted: 1]
    [4 bytes] Status code [0: summary, 1: bad request, 2: internal error]
    [4 bytes] Length of summary (if status == summary)
    [N bytes] Summary (as long as above field's value)

Tweaks

    Summarizerd supports multiple command line options to tweak its config. Here
    are the config options:

    *  Number of worker threads to use
    *  Number of clients to keep in listening queue
    *  Socket port to listen on
    *  Log/PID files, logging level
    *  For debugging, foreground mode can be used

    Enter '$ [prefix]/bin/summarizerd -h' for all the options

Bugs

    *  The etc/summarizerd init script is hardcoded to use /usr/local/summarizer
       as the default prefix
