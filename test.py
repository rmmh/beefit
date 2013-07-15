import hashlib
import subprocess

def get_output(program, stdin):
	p = subprocess.Popen(['./beefit', program], stdout=subprocess.PIPE, stdin=subprocess.PIPE)
	output = p.communicate(input=stdin + '\x00')[0]
	return output

expected_output_hashes = {
	'bench/mandelbrot.bf': 'b77a017f811831f0b74e0d69c08b78e620dbda2b',
	'test/hanoi.bf': '32cdfe329039ce63531dcd4b340df269d4fd8f7f',
	'test/fizzbuzz.bf': 'b0a6d742a6e9aa22b410e28e071609c44d3a985e',
    'test/quine.bf': '526e4d3d73ab7df064e8ffd9bc502b261f28075a',
	('bench/factor.bf', '%s\n' % (31337**2)): '18d066471aa0f6deff502bb9e7042a524f32503b',
	('test/bfcl.bf', open('test/fizzbuzz.bf').read()): 'c3ea975b719157ac70336d68d0e86ded69f500c6',
    ('test/awib.bf', open('test/awib.bf').read()): '3b4f9a78ec3ee32e05969e108916a4affa0c2bba'
}

for filename, expected_hash in expected_output_hashes.iteritems():
	stdin = ''
	if isinstance(filename, tuple):
		filename, stdin = filename
	output = get_output(filename, stdin)
	actual_hash = hashlib.sha1(output).hexdigest()
	if actual_hash != expected_hash:
		print "bad output for %s expected %s got %s" % (
			filename, expected_hash, actual_hash)
		print output.decode('ascii', 'replace')
