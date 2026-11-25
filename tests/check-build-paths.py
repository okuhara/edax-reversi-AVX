"""Fails when an executable or a library holds the path of a source file on the build machine
(a drive path, or a /home/, /Users/, /mnt/, /tmp/ or /root/ path, ending in .c, .h, .cc, .cpp or .hpp),
as __FILE__ gives without /d1trimfile.
usage: python tests/check-build-paths.py <file or glob> ...
An argument that names no file is a failure too (a program missing from the build).
"""
import glob, os, re, sys

PATH = re.compile(rb'(?:[A-Za-z]:\\|/home/|/Users/|/mnt/|/tmp/|/root/)[\x20-\x7e]{0,400}?\.(?:c|h|cc|cpp|hpp)(?=\x00)')
if len(sys.argv) < 2:
    sys.exit(__doc__)
bad = 0
files = set()
for a in sys.argv[1:]:	# each argument (a file or a glob) must name at least one file
    found = [f for f in glob.glob(a) if os.path.isfile(f)]
    if not found:
        print(f'{a}: no such file')
        bad += 1
    files.update(found)
files = sorted(files)
for f in files:
    found = sorted(set(m.group().decode('latin1') for m in PATH.finditer(open(f, 'rb').read())))
    print(f'{f}: {len(found)} source paths' + (f' (e.g. {found[0]})' if found else ''))
    bad += bool(found)
sys.exit(1 if bad else 0)
