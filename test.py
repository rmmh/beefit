#!/usr/bin/env python

import hashlib
import subprocess
import sys
import time

def get_output(program, stdin):
    p = subprocess.Popen(['./beefit', program] + sys.argv[1:], stdout=subprocess.PIPE, stdin=subprocess.PIPE)
    start = time.time()
    output = p.communicate(input=stdin + '\x00')[0]
    return output, time.time() - start

expected_output_hashes = {
    'bench/mandelbrot.bf': 'b77a017f811831f0b74e0d69c08b78e620dbda2b',
    'test/hanoi.bf': '32cdfe329039ce63531dcd4b340df269d4fd8f7f',
    'test/fizzbuzz.bf': 'b0a6d742a6e9aa22b410e28e071609c44d3a985e',
    'test/quine.bf': '526e4d3d73ab7df064e8ffd9bc502b261f28075a',
    'test/hello.bf': '2ef7bde608ce5404e97d5f042f95f89f1c232871',
    'test/dce.bf': 'ae4f281df5a5d0ff3cad6371f76d5c29b6d953ec',
    'test/issue03.bf': 'da39a3ee5e6b4b0d3255bfef95601890afd80709',
    'bench/long.bf': '7ef8aa6a336b4a7122031d713f383ffbbe5fac93',
    ('bench/factor.bf', '%s\n' % (31337**2)): '18d066471aa0f6deff502bb9e7042a524f32503b',
    ('test/bfcl.bf', open('test/fizzbuzz.bf').read()): 'c3ea975b719157ac70336d68d0e86ded69f500c6',
    ('test/awib.bf', open('test/awib.bf').read()): '3b4f9a78ec3ee32e05969e108916a4affa0c2bba',
    ('bench/dbfi.bf', open('bench/factor.bf').read() + '!10\n'): '5b806d37efa89f95782f526d9943ef7452d64934',
}

for filename, expected_hash in expected_output_hashes.iteritems():
    stdin = ''
    if isinstance(filename, tuple):
        filename, stdin = filename
    output, elapsed = get_output(filename, stdin)
    actual_hash = hashlib.sha1(output).hexdigest()
    print filename.ljust(30),
    if actual_hash == expected_hash:
        print '\tGOOD\t%.1fms' % (elapsed * 1000)
    else:
        print "bad output: expected %s got %s" % (
            expected_hash, actual_hash)
        print output.decode('ascii', 'replace')
