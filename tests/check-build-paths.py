"""Fails when an executable or a library holds the path of a source file on the build machine
(a drive path, or a /home/, /Users/, /mnt/, /tmp/ or /root/ path, ending in .c, .h, .cc, .cpp or .hpp),
as __FILE__ gives without /d1trimfile.
usage: python tests/check-build-paths.py <file or glob> ...
"""
import glob, re, sys

PATH = re.compile(rb'(?:[A-Za-z]:\\|/home/|/Users/|/mnt/|/tmp/|/root/)[\x20-\x7e]{0,400}?\.(?:c|h|cc|cpp|hpp)(?=\x00)')
files = sorted(set(f for a in sys.argv[1:] for f in glob.glob(a)))
if not files:
    sys.exit('no file to check')
bad = 0
for f in files:
    found = sorted(set(m.group().decode('latin1') for m in PATH.finditer(open(f, 'rb').read())))
    print(f'{f}: {len(found)} source paths' + (f' (e.g. {found[0]})' if found else ''))
    bad += bool(found)
sys.exit(1 if bad else 0)
