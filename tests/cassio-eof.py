"""Check that Cassio exits after EOF, including an unterminated final line.

Run with Python 3, passing an Edax executable and optionally --cwd for eval.dat.
"""

import argparse
from pathlib import Path
import subprocess
import unittest


class CassioEOF(unittest.TestCase):
    def check_input(self, data):
        result = subprocess.run(
            [str(EXECUTABLE), "-n", "1", "-h", "10", "-cassio"],
            input=data,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            cwd=DIRECTORY,
            timeout=5,
        )
        self.assertEqual(result.returncode, 0, result.stdout.decode(errors="replace"))
        self.assertNotIn(b"WARNING: ThreadSanitizer:", result.stdout)
        self.assertNotIn(b"ERROR: AddressSanitizer:", result.stdout)
        self.assertNotIn(b"runtime error:", result.stdout)
        self.assertIn(b"bye bye.", result.stdout)
        return result.stdout

    def test_empty_input(self):
        self.check_input(b"")

    def test_init_newline(self):
        self.assertIn(b"ready.", self.check_input(b"ENGINE-PROTOCOL init\n"))

    def test_init_without_newline(self):
        self.assertIn(b"ready.", self.check_input(b"ENGINE-PROTOCOL init"))

    def test_unqueued_command_without_newline(self):
        self.check_input(b"ENGINE-PROTOCOL init\nENGINE-PROTOCOL get-search-infos")

    def test_unknown_protocol_without_newline(self):
        self.assertIn(b"ERROR: Unknown protocol", self.check_input(b"OTHER init"))

    def test_quit_without_newline(self):
        self.check_input(b"ENGINE-PROTOCOL init\nENGINE-PROTOCOL quit")

    def test_quit_newline(self):
        self.check_input(b"ENGINE-PROTOCOL init\nENGINE-PROTOCOL quit\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--cwd", type=Path)
    args = parser.parse_args()
    EXECUTABLE = args.executable.resolve()
    DIRECTORY = args.cwd.resolve() if args.cwd else EXECUTABLE.parent
    unittest.main(argv=[__file__], verbosity=2)
