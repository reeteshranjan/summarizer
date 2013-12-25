summarizer
==========

Version

    0.1

Overview

    This is an adaptation of Open-Text-Summarizer (aka ots) with
    mean optimizations for a faster output.

Build

    $ ./configure [--prefix=/usr/local/summarizer/]
    $ make
    $ [sudo] make install

Usage

    $ [prefix]/bin/summarizer -i <file-to-summarize> -r <summary-ratio>

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
