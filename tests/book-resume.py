"""Check that book correct, book leaf recalculate, book deepen and book learn go on from a book saved before they ended.

Run with Python 3: book-resume.py <Edax executable> <eval.dat> [--work <folder>] [--form2] [--old <executable>]

book correct and book leaf recalculate mark the positions that they have searched again (the two upper bits
of the byte of the level of a book file: 0x40 book correct, 0x80 book leaf recalculate). A run that did not
end leaves its marks in the books it saved; a new run does not search the marked positions again, and a run
that ends removes all the marks. book deepen gives the positions that it searches the level of the book: a
new run does not search them again.

The script stands for a run that did not end by setting the marks itself in a saved book (and by giving the
marked positions a wrong score, which the new run must keep: it does not search them):
- correct: 24 solved positions (14 empties, level 60); half of them marked with a wrong score; the next
  run searches the 12 others only, keeps the 12 wrong scores, and removes the marks; a third run finds the
  12 errors. With 1 thread (one position after the other) and 2 threads (two at the same time).
- deepen: positions of level 1 in a book of level 60: they get level 60; a second run searches none.
- leaf recalculate3 (every leaf of the walk): a small book (level 2, depth 8); every position marked: no leaf is searched, the marks go;
  half of them marked with a wrong leaf score: these keep it, the marks go.
- learn: a file of 6 games; a progress file of its first 3 lines (written here): the next run learns the 3 others
  and removes it; the progress of another book or another file is not used; a run stopped as soon as its
  progress file is written, then a new run on the book it saved to <book-file>.store.
- --form2 (C++23 version): the marks are kept by a book saved in form 2 and read again.
- --old: an executable of a version without marks reads a book with marks as a damaged book.
Small searches only (one thread, a few seconds in all).
"""

import argparse
import os
import random
import re
import shutil
import struct
import subprocess
import sys
import tempfile

HEADER = 42          # EDAX, BOOK, version, release, date (8), options (20), count
FIXED = 40           # board (16), 4 counts (16), value, lower, upper (2 each), n_link, level
MARK_CORRECTED = 0x40
MARK_RECALCULATED = 0x80

failures = []


def check(cond, what):
    print(("ok   " if cond else "FAIL ") + what)
    if not cond:
        failures.append(what)


# --- boards ---------------------------------------------------------------

DIRS = [(1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (1, -1), (-1, 1), (-1, -1)]


def flips(p, o, sq):
    if (p | o) >> sq & 1:
        return 0
    x, y = sq % 8, sq // 8
    f = 0
    for dx, dy in DIRS:
        t, cx, cy = 0, x + dx, y + dy
        while 0 <= cx < 8 and 0 <= cy < 8 and o >> (cy * 8 + cx) & 1:
            t |= 1 << (cy * 8 + cx)
            cx += dx
            cy += dy
        if t and 0 <= cx < 8 and 0 <= cy < 8 and p >> (cy * 8 + cx) & 1:
            f |= t
    return f


def moves(p, o):
    return [sq for sq in range(64) if flips(p, o, sq)]


def random_board(rng, n_empties):
    """A board of n_empties empty squares (player to move has a move), reached by random moves."""
    while True:
        p, o = 0x0000000810000000, 0x0000001008000000  # black to move: d5 e4 black, d4 e5 white
        ok = True
        while 64 - bin(p | o).count("1") > n_empties:
            m = moves(p, o)
            if not m:
                p, o = o, p
                if not moves(p, o):
                    ok = False
                    break
                continue
            sq = rng.choice(m)
            f = flips(p, o, sq)
            p, o = o & ~f, p | f | (1 << sq)
        if ok and moves(p, o):
            return p, o


def board_text(p, o):
    return "".join("X" if p >> i & 1 else "O" if o >> i & 1 else "-" for i in range(64)) + " X"


# --- book files (form 1) ----------------------------------------------------

def read_book(path):
    data = bytearray(open(path, "rb").read())
    if data[:8] != b"XADEKOOB":
        raise SystemExit(f"{path}: not a book of form 1")
    n = struct.unpack_from("<I", data, HEADER - 4)[0]
    records, k = [], HEADER
    for _ in range(n):
        n_link = data[k + 38]
        leaf = k + FIXED + 2 * n_link
        records.append({"key": bytes(data[k:k + 16]), "level_at": k + 39, "leaf_at": leaf})
        k = leaf + 2
    if k != len(data):
        raise SystemExit(f"{path}: {len(data) - k} bytes after the positions")
    return data, records


def summary(path):
    """board -> (level, marks, leaf score, leaf move)"""
    data, records = read_book(path)
    out = {}
    for r in records:
        b = data[r["level_at"]]
        out[r["key"]] = (b & 0x3f, b & 0xc0, struct.unpack_from("b", data, r["leaf_at"])[0], data[r["leaf_at"] + 1])
    return out


def n_marked(path):
    return sum(1 for v in summary(path).values() if v[1])


# --- running Edax ------------------------------------------------------------

def run(args, work, commands, threads=1, extra=()):
    cmd_file = os.path.join(work, "cmds.txt")
    with open(cmd_file, "w", newline="\n") as f:
        f.write("\n".join(commands + ["quit"]) + "\n")
    line = [args.exe, "-eval-file", args.eval, "-book-file", os.path.join(work, "book.dat"), "-n", str(threads),
            "-book-save-interval", "0", "-book-usage", "off", "-auto-store", "off", *extra]
    with open(cmd_file, "rb") as stdin:
        r = subprocess.run(line, stdin=stdin, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, cwd=work, timeout=600)
    text = r.stdout.decode("utf-8", "replace").replace("\r", "\n")
    with open(os.path.join(work, "log.txt"), "a", encoding="utf-8") as log:
        log.write(f"### {' '.join(line)}\n### {commands}\n{text}\n")
    return text


def last_int(pattern, text):
    m = re.findall(pattern, text)
    return [int(x) if not isinstance(x, tuple) else tuple(int(y) for y in x) for x in m][-1] if m else None


# --- tests -------------------------------------------------------------------

def test_correct(args, work, threads):
    print(f"-- book correct, {threads} thread(s)")
    rng = random.Random(1)
    with open(os.path.join(work, "solved.txt"), "w", newline="\n") as f:
        for _ in range(24):
            f.write(board_text(*random_board(rng, 14)) + ",60\n")
    b = lambda name: os.path.join(work, name)
    text = run(args, work, ["book import solved.txt", "book correct", "book save c1.dat"], threads)
    check(last_int(r"Correcting solved positions\.\.\.(\d+) done", text) == 24, "a first run searches the 24 solved positions")
    check(n_marked(b("c1.dat")) == 0, "a run that ends leaves no mark")
    first = summary(b("c1.dat"))

    # a run that did not end: half of the positions marked, with a wrong score
    data, records = read_book(b("c1.dat"))
    wrong = {}
    for r in records[::2]:
        data[r["level_at"]] |= MARK_CORRECTED
        s = struct.unpack_from("b", data, r["leaf_at"])[0]
        s = s + 2 if s <= 62 else s - 2
        struct.pack_into("b", data, r["leaf_at"], s)
        wrong[r["key"]] = s
    open(b("c2.dat"), "wb").write(data)
    text = run(args, work, ["book load c2.dat", "book correct", "book save c3.dat"], threads)
    check(re.search(r"12 already done by a run that did not end, 12 left", text) is not None, "the new run tells the 12 positions done and the 12 left")
    check(last_int(r"Correcting solved positions\.\.\.(\d+) done \((\d+) error", text) == (12, 0), "the new run searches the 12 positions left, without error")
    third = summary(b("c3.dat"))
    check(n_marked(b("c3.dat")) == 0, "the marks are removed when the run ends")
    check(all(third[k][2] == wrong[k] for k in wrong), "the marked positions are not searched again (their wrong scores are kept)")
    check(all(third[k][2] == first[k][2] for k in first if k not in wrong), "the other positions keep their scores")
    text = run(args, work, ["book load c3.dat", "book correct", "book save c4.dat"], threads)
    check(last_int(r"Correcting solved positions\.\.\.(\d+) done \((\d+) error", text) == (24, 12), "a run on the finished book searches all again and finds the 12 wrong scores")
    fourth = summary(b("c4.dat"))
    check(all(fourth[k][2] == first[k][2] for k in first), "the scores are corrected")
    return b("c2.dat")


def test_deepen(args, work):
    print("-- book deepen")
    rng = random.Random(2)
    with open(os.path.join(work, "deepen.txt"), "w", newline="\n") as f:
        for i in range(12):
            f.write(board_text(*random_board(rng, 14)) + (",60\n" if i == 0 else ",1\n"))
    text = run(args, work, ["book import deepen.txt", "book deepen", "book save d1.dat"])
    check(last_int(r"Deepening book\.\.\.(\d+) done", text) == 11, "the 11 positions of level 1 are searched with the level of the book (60)")
    levels = [v[0] for v in summary(os.path.join(work, "d1.dat")).values()]
    check(levels == [60] * 12, "they get the level of the book")
    text = run(args, work, ["book load d1.dat", "book deepen", "book save d2.dat"])
    check(last_int(r"Deepening book\.\.\.(\d+) done", text) == 0, "a new run searches no position again")


def test_leaf_recalculate(args, work):
    print("-- book leaf recalculate")
    b = lambda name: os.path.join(work, name)
    run(args, work, ["book new 2 8", "book deviate 2 4", "book save l0.dat"])
    n = len(summary(b("l0.dat")))
    text = run(args, work, ["book load l0.dat", "book leaf recalculate3 2 4", "book save l1.dat"])
    n_leaves = last_int(r"finished: (\d+) leaves", text)
    check(n > 20 and n_leaves is not None and n_leaves > 5, f"a first run searches leaves ({n_leaves} of a book of {n} positions)")
    check(n_marked(b("l1.dat")) == 0, "a run that ends leaves no mark")

    data, records = read_book(b("l0.dat"))
    for r in records:
        data[r["level_at"]] |= MARK_RECALCULATED
    open(b("l2.dat"), "wb").write(data)
    text = run(args, work, ["book load l2.dat", "book leaf recalculate3 2 4", "book save l3.dat"])
    check(last_int(r"(\d+) leaves already searched again", text) == n, "the new run tells the marked leaves")
    check(last_int(r"finished: (\d+) leaves", text) == 0, "every position marked: no leaf is searched")
    check(n_marked(b("l3.dat")) == 0, "the marks are removed when the run ends")

    data, records = read_book(b("l0.dat"))
    wrong = {}
    for r in records[::2]:
        data[r["level_at"]] |= MARK_RECALCULATED
        if data[r["leaf_at"] + 1] < 64:  # a leaf move: a wrong score that a search would correct
            s = struct.unpack_from("b", data, r["leaf_at"])[0]
            s = s - 1 if s > -64 else s + 1
            struct.pack_into("b", data, r["leaf_at"], s)
            wrong[r["key"]] = s
    open(b("l4.dat"), "wb").write(data)
    text = run(args, work, ["book load l4.dat", "book leaf recalculate3 2 4", "book save l5.dat"])
    after = summary(b("l5.dat"))
    check(all(after[k][2] == wrong[k] for k in wrong if k in after), "the marked leaves are not searched again (their wrong scores are kept)")
    check(n_marked(b("l5.dat")) == 0, "the marks are removed when the run ends")


GAMES = ["f5d6", "f5f6", "f5f4", "f5d6c3", "f5d6c5", "f5f6e6"]


def learn_hash(lines):
    h = 0xcbf29ce484222325
    for line in lines:
        for c in line.encode() + b"\n":
            h = ((h ^ c) * 0x100000001b3) & 0xffffffffffffffff
    return h


def book_header(path):
    data = open(path, "rb").read(HEADER)
    level, empties = struct.unpack_from("<ii", data, 18)
    return struct.unpack_from("<I", data, HEADER - 4)[0], level, empties


def write_progress(path, lines, book, positions=None):
    n, level, empties = book_header(book)
    with open(path, "w", newline="\n") as f:
        f.write(f"lines={len(lines)}\nhash={learn_hash(lines):016x}\ngames={len(lines)}\nlearned={len(lines)}\n"
                f"positions={n if positions is None else positions}\nlevel={level}\nempties={empties}\n")


def test_learn(args, work):
    print("-- book learn <file>")
    b = lambda name: os.path.join(work, name)
    opts = ("-l", "2", "-book-store-tasks", "1")
    with open(b("games.txt"), "w", newline="\n") as f:
        f.write("\n".join(GAMES) + "\n")
    with open(b("first3.txt"), "w", newline="\n") as f:
        f.write("\n".join(GAMES[:3]) + "\n")
    run(args, work, ["book new 2 50", "book save g0.dat"], extra=opts)
    progress = b("book.dat.learn")

    shutil.copy(b("g0.dat"), b("book.dat"))
    text = run(args, work, ["book learn games.txt"], extra=opts)
    check(re.search(r"6/6 games learned", text) is not None, "a whole file: 6 games learned")
    check(not os.path.exists(progress), "the progress file is removed when the whole file is learned")

    # a run that did not end after the first 3 lines: the book of these 3 games, and its progress file
    shutil.copy(b("g0.dat"), b("book.dat"))
    run(args, work, ["book learn first3.txt", "book save g3.dat"], extra=opts)
    write_progress(progress, GAMES[:3], b("g3.dat"))
    text = run(args, work, ["book load g3.dat", "book learn games.txt"], extra=opts)
    check(re.search(r"the first 3 lines of games\.txt \(3 games\) were learned by a run that did not end", text) is not None, "the new run tells the 3 lines learned")
    check(re.search(r"3/3 games learned", text) is not None, "the new run learns the 3 other games only")
    check(not os.path.exists(progress), "the progress file is removed at the end")

    write_progress(progress, GAMES[:3], b("g3.dat"), positions=1)
    text = run(args, work, ["book load g3.dat", "book learn games.txt"], extra=opts)
    check(re.search(r"progress of book learn of another book", text) is not None and re.search(r"6/6 games learned", text) is not None,
          "the progress of another book is not used: 6 games learned")
    write_progress(progress, ["f5d6", "f5f6", "f5f6"], b("g3.dat"))
    text = run(args, work, ["book load g3.dat", "book learn games.txt"], extra=opts)
    check(re.search(r"progress of book learn of another file", text) is not None and re.search(r"6/6 games learned", text) is not None,
          "the progress of another file is not used: 6 games learned")
    if os.path.exists(progress):
        os.remove(progress)

    # a run stopped while it learns: its progress file, and a new run on the book it saved to .store
    many = GAMES + ["f5d6c3d3", "f5d6c3d3c4", "f5d6c5f4", "f5f6e6f4", "f5f4e3", "f5f4e3f6"] * 4
    with open(b("many.txt"), "w", newline="\n") as f:
        f.write("\n".join(many) + "\n")
    shutil.copy(b("g0.dat"), b("book.dat"))
    stopped = learn_and_stop(args, work, "many.txt", opts, progress)
    if stopped is None or re.search(r"lines=(\d+)", stopped) is None:
        print("skip the run stopped while it learns (it ended before its progress file could be read)")
        return
    lines = int(re.search(r"lines=(\d+)", stopped).group(1))
    check(0 < lines < len(many), f"a run stopped while it learns leaves its progress ({lines} lines of {len(many)})")
    text = run(args, work, ["book load book.dat.store", "book learn many.txt"], extra=opts)
    check(re.search(rf"the first {lines} lines of many\.txt", text) is not None, "a new run on the book saved to .store skips these lines")
    check(re.search(rf"{len(many) - lines}/{len(many) - lines} games learned", text) is not None, "and learns the others")
    check(not os.path.exists(progress), "the progress file is removed at the end")


def learn_and_stop(args, work, games, opts, progress):
    """Start book learn, stop the process as soon as its progress file is written; return the file (None if the run ended first)."""
    import time
    cmd_file = os.path.join(work, "cmds.txt")
    with open(cmd_file, "w", newline="\n") as f:
        f.write(f"book learn {games}\nquit\n")
    line = [args.exe, "-eval-file", args.eval, "-book-file", os.path.join(work, "book.dat"), "-n", "1",
            "-book-save-interval", "0", "-book-usage", "off", "-auto-store", "off", *opts]
    with open(cmd_file, "rb") as stdin:
        p = subprocess.Popen(line, stdin=stdin, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, cwd=work)
        text = None
        while p.poll() is None:
            if os.path.exists(progress):
                p.kill()
                p.wait()
                time.sleep(0.2)
                text = open(progress).read()
                break
            time.sleep(0.005)
    return text


def test_form2(args, work, marked):
    print("-- form 2")
    run(args, work, [f"book load {os.path.basename(marked)}", "book save f2.dat"], extra=("-book-format", "2"))
    run(args, work, ["book load f2.dat", "book save f1.dat"], extra=("-book-format", "1"))
    check(summary(marked) == summary(os.path.join(work, "f1.dat")), "a book saved in form 2 and read again keeps the marks")


def test_old(args, work, marked):
    print("-- a version without marks")
    exe = args.exe
    args.exe = args.old
    text = run(args, work, [f"book load {os.path.basename(marked)}", "book info"])
    args.exe = exe
    check(re.search(r"damaged|Truncated", text, re.I) is not None, "it reads a book with marks as a damaged book")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("exe")
    ap.add_argument("eval")
    ap.add_argument("--work")
    ap.add_argument("--form2", action="store_true")
    ap.add_argument("--old")
    args = ap.parse_args()
    args.exe, args.eval = os.path.abspath(args.exe), os.path.abspath(args.eval)
    work = args.work or tempfile.mkdtemp(prefix="book-resume-")
    os.makedirs(work, exist_ok=True)
    print(f"work folder: {work}")
    marked = None
    for threads in (1, 2):
        sub = os.path.join(work, f"correct{threads}")
        os.makedirs(sub, exist_ok=True)
        marked = test_correct(args, sub, threads)
    sub = os.path.join(work, "deepen"); os.makedirs(sub, exist_ok=True); test_deepen(args, sub)
    sub = os.path.join(work, "leaf"); os.makedirs(sub, exist_ok=True); test_leaf_recalculate(args, sub)
    sub = os.path.join(work, "learn"); os.makedirs(sub, exist_ok=True); test_learn(args, sub)
    if args.form2:
        test_form2(args, os.path.dirname(marked), marked)
    if args.old:
        test_old(args, os.path.dirname(marked), marked)
    print(f"{len(failures)} failure(s)")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
