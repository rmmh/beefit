#!/usr/bin/env python

import hashlib
import subprocess
import sys
import time


def get_output(program, stdin):
    start = time.time()
    p = subprocess.Popen(['./beefit', program] + sys.argv[1:],
                         stdout=subprocess.PIPE, stdin=subprocess.PIPE)
    output = p.communicate(input=stdin + '\x00')[0]
    return output, time.time() - start


def read(fname):
    return open(fname).read()


tests = [
    ('bench/awib.bf',       'f94bc1ef8f59', '@386_linux\n' + read('bench/awib.bf')),
    ('bench/dbfi.bf',       '8aeb36d17807', read('bench/dbfi.bf') + '!' + read('test/echo9.bf') + '!hello123\n'),
    ('bench/factor.bf',     '7e9559968c3e', '133333333333337\n'),
    ('bench/long.bf',       '7ef8aa6a336b'),
    ('bench/mandelbrot.bf', 'b77a017f8118'),
    ('bench/awib.bf',       'a289bc5208b7', read('bench/awib.bf')),
    ('test/bfcl.bf',        'c3ea975b7191', read('test/fizzbuzz.bf')),
    ('test/dce.bf',         'ae4f281df5a5'),
    ('test/echo9.bf',       '8aeb36d17807', 'hello123\n'),
    ('test/fizzbuzz.bf',    'b0a6d742a6e9'),
    ('test/hanoi.bf',       '32cdfe329039'),
    ('test/hello.bf',       '2ef7bde608ce'),
    ('test/issue03.bf',     'da39a3ee5e6b'),
    ('test/quine.bf',       '526e4d3d73ab'),
]

def run_tests():
    for test in tests:
        filename = test[0]
        expected_hash = test[1]
        stdin = test[2] if len(test) >= 3 else ''
        print filename.ljust(30),
        sys.stdout.flush()
        output, elapsed = get_output(filename, stdin)
        actual_hash = hashlib.sha1(output).hexdigest()[:12]
        if actual_hash == expected_hash:
            print '\tGOOD\t%.1fms' % (elapsed * 1000)
        else:
            print "bad output: expected %s got %s" % (
                expected_hash, actual_hash)
            print output.decode('ascii', 'replace')

if __name__ == '__main__':
    run_tests()
