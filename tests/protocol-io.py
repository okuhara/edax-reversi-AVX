"""Check the standard input and the protocols of Edax (Edax, Cassio switch, XBoard, GGS).

- a last line without '\\n' is executed once, and the end of the input ends Edax;
- after "engine-protocol init", the Cassio engine alone reads the standard input;
- the save of the book at the end writes "@save ok=<0|1> file=<book>", and a failed save ends with the exit code 1;
- XBoard: "go" on a finished game sends the result (no move), and "analyze"/"exit" keep the state;
- GGS: the password is sent, but written neither to the standard output nor to the log, and Edax ends when the
  server closes the connection.

Run with Python 3, passing an Edax executable and optionally --cwd for data/eval.dat.
Each case runs Edax with 1 thread and a small hash table, for a few seconds at most.
"""

import argparse
from pathlib import Path
import shutil
import socket
import subprocess
import tempfile
import threading
import unittest

TIMEOUT = 30


class Protocol(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp(prefix="edax-protocol-io-"))
        self.book = self.tmp / "book.dat"

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def run_edax(self, args, data):
        result = subprocess.run(
            [str(EXECUTABLE), "-n", "1", "-h", "10", "-book-file", str(self.book)] + args,
            input=data,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            cwd=DIRECTORY,
            timeout=TIMEOUT,
        )
        out = result.stdout.decode(errors="replace")
        self.assertEqual(result.returncode, 0, out)
        return out

    # result of the save of the book at the end: "@save ok=<0|1> file=<book>" (also with -q), exit code 1 if not saved
    def run_save(self, book, commands):
        result = subprocess.run(
            [str(EXECUTABLE), "-q", "-n", "1", "-h", "10", "-book-file", str(book)],
            input=commands, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, cwd=DIRECTORY, timeout=TIMEOUT)
        return result.returncode, result.stdout.decode(errors="replace")

    def test_save_at_end(self):
        code, out = self.run_save(self.book, b"book new 1 1\n")
        self.assertEqual(code, 0, out)
        self.assertEqual(out.count("@save ok=1 file=%s" % self.book), 1, out)
        self.assertTrue(self.book.exists())

    def test_save_at_end_fails(self):
        book = self.tmp / "missing-folder" / "book.dat"
        code, out = self.run_save(book, b"book new 1 1\n")
        self.assertEqual(code, 1, out)
        self.assertEqual(out.count("@save ok=0 file=%s" % book), 1, out)
        self.assertNotIn("@save ok=1", out)

    def test_save_at_end_nothing_to_save(self):
        # (a missing book file gives a new book, saved at the end: the book is made first)
        code, out = self.run_save(self.book, b"book new 1 1\n")
        self.assertEqual(code, 0, out)
        code, out = self.run_save(self.book, b"init\n")
        self.assertEqual(code, 0, out)
        self.assertNotIn("@save", out)

    # result of "book load": "@load ok=<0|1> file=<book>" (also with -q); a book that is not loaded keeps the current one
    def test_book_load_result(self):
        b1, b3 = self.tmp / "b1.dat", self.tmp / "b3.dat"
        missing, bad = self.tmp / "missing.dat", self.tmp / "bad.dat"
        bad.write_bytes(b"not a book\n")
        code, out = self.run_save(self.tmp / "other.dat", ("book new 3 1\nbook save %s\n" % b1).encode())
        self.assertEqual(code, 0, out)
        code, out = self.run_save(self.book, ("book load %s\nbook load %s\nbook load %s\nbook save %s\n" % (b1, missing, bad, b3)).encode())
        self.assertEqual(code, 0, out)
        self.assertEqual(out.count("@load ok=1 file=%s" % b1), 1, out)
        self.assertEqual(out.count("@load ok=0 file=%s" % missing), 1, out)
        self.assertEqual(out.count("@load ok=0 file=%s" % bad), 1, out)
        self.assertEqual(out.count("@load "), 3, out)
        data = b3.read_bytes()
        self.assertEqual(data[:8], b"XADEKOOB", out)  # (form 1, as the book loaded)
        self.assertEqual(int.from_bytes(data[18:22], "little"), 3, out)  # the level of b1: the book loaded is kept

    # NBoard: the save of the book at the end writes "@save ok=<0|1> file=<book>", as the other protocols
    def test_nboard_save_at_end(self):
        result = subprocess.run(
            [str(EXECUTABLE), "-q", "-n", "1", "-h", "10", "-l", "1", "-book-file", str(self.book), "-nboard"],
            input=b"nboard 1\ndepth 1\nmove f5\nlearn\nquit\n", stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            cwd=DIRECTORY, timeout=TIMEOUT)
        out = result.stdout.decode(errors="replace")
        self.assertEqual(result.returncode, 0, out)
        self.assertEqual(out.count("@save ok=1 file=%s" % self.book), 1, out)
        self.assertTrue(self.book.exists())

    # standard input of the Edax protocol
    def test_last_line_without_newline(self):
        out = self.run_edax([], b"help")
        self.assertEqual(out.count("Options must be entered"), 1, out)

    def test_last_line_with_newline(self):
        out = self.run_edax([], b"help\n")
        self.assertEqual(out.count("Options must be entered"), 1, out)

    def test_cassio_switch(self):
        for end in (b"\n", b""):
            out = self.run_edax([], b"engine-protocol init\nENGINE-PROTOCOL get-version\nENGINE-PROTOCOL quit" + end)
            self.assertLessEqual(out.count("version: Edax"), 1, out)  # (quit clears the commands not done yet)
            self.assertIn("bye bye.", out)

    def test_cassio_switch_eof(self):
        for end in (b"\n", b""):
            out = self.run_edax([], b"engine-protocol init\nENGINE-PROTOCOL get-version" + end)
            self.assertEqual(out.count("version: Edax"), 1, out)
            self.assertIn("bye bye.", out)

    # XBoard (no "quit": read at once from a pipe, it clears the commands before it; the end of the input ends)
    def test_xboard_go_game_over(self):
        full = "/".join(["PPPPPPPP"] * 8)
        out = self.run_edax(["-xboard"], ("xboard\nprotover 2\nsetboard %s w - - 0 1\ngo\n" % full).encode())
        self.assertNotIn("move ", out)
        self.assertEqual(sum(out.count(r) for r in ("1-0 {", "0-1 {", "1/2-1/2 {")), 2, out)  # setboard, then go

    def test_xboard_analyze_exit_keeps_state(self):
        out = self.run_edax(["-xboard"], b"xboard\nprotover 2\nnew\nresult 1-0 {x}\nanalyze\nexit\nanalyze\nexit\n")
        self.assertIn("1 games played", out)

    # GGS
    def test_ggs_password_and_close(self):
        server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server.bind(("127.0.0.1", 0))
        server.listen(1)
        port = server.getsockname()[1]
        password = "Pw-1a2b3c4d"
        received = []

        def serve():
            conn, _ = server.accept()
            conn.settimeout(TIMEOUT)
            data = b""
            conn.sendall(b": Enter login (yours, or one you'd like to use).\n")
            while b"\n" not in data:
                data += conn.recv(4096)
            conn.sendall(b": Enter your password.\n")
            while (password + "\n").encode() not in data:
                chunk = conn.recv(4096)
                if not chunk:
                    break
                data += chunk
            received.append(data)
            conn.close()

        thread = threading.Thread(target=serve)
        thread.start()
        log = self.tmp / "ggs.log"
        args = ["-ggs", "-ggs-host", "127.0.0.1", "-ggs-port", str(port), "-ggs-login", "tester",
                "-ggs-password", password, "-ggs-log-file", str(log)]
        try:
            out = self.run_edax(args, b"")
        finally:
            thread.join(TIMEOUT)
            server.close()
        self.assertTrue(received and (password + "\n").encode() in received[0], received)
        self.assertNotIn(password, out)
        self.assertIn("(password)", out)
        self.assertIn("[GGS connection closed]", out)
        self.assertNotIn(password, log.read_text(errors="replace"))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--cwd", type=Path)
    args, rest = parser.parse_known_args()
    EXECUTABLE = args.executable.resolve()
    DIRECTORY = args.cwd.resolve() if args.cwd else EXECUTABLE.parent
    unittest.main(argv=[__file__] + rest, verbosity=2)
