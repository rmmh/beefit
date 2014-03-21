Beefit - A Brainfuck JIT
======

Building
----
Beefit currently only supports x86-64 CPUs. Compilation requires Lua to be installed.

To compile:

    $ make

Usage
----
Run a program by providing it on stdin or specifying a file

    $ ./beefit bench/mandelbrot.bf

Use the -d (dump), -t (trace), or -s (stats) flags for more information.
