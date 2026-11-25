"""Solve the positions of an OBF file with -solve and compare the results with the scores written in the file.

Each line of the OBF file gives the position and the scores of its moves ("G8:+18; H1:+12; ..."). For every
position the check needs: a line of result, the best score of the file as its score, and a move of that score as
its move. The number of results must be the number of positions of the file.

usage: python tests/check-solve.py <executable> <file.obf> [--eval <eval.dat>] [--cwd <folder>]
                                   [--threads N] [--level L] [--timeout S] [--prefix "<command>"]
--prefix: a command that runs the program (an emulator: --prefix "qemu-aarch64 -L /usr/aarch64-linux-gnu").
The exit code is 0 when every position is right, 1 when a result differs from the file, 2 when the program
cannot be run, fails, or prints fewer results than expected.
"""
import argparse
import re
import shlex
import subprocess
import sys
from pathlib import Path

MOVE = re.compile(r'^([A-Ha-h][1-8]|ps|pa|PS|PA)$')
SCORE = re.compile(r'^[<>]?([+-]\d+)$')


def read_obf(path):
    """the positions of an OBF file: (best score, set of the moves of that score, line)"""
    expected = []
    for number, line in enumerate(Path(path).read_text(encoding='utf-8').splitlines(), 1):
        line = line.strip()
        if not line or line.startswith('%'):
            continue
        fields = [f.strip() for f in line.split(';')]
        scores = {}
        for field in fields[1:]:
            if not field:
                continue
            move, _, value = field.partition(':')
            if not value:
                sys.exit(f'{path}:{number}: "{field}" is not a move and its score')
            scores[move.strip().lower()] = int(value)
        if not scores:
            sys.exit(f'{path}:{number}: no score of a move')
        best = max(scores.values())
        expected.append((best, {m for m, s in scores.items() if s == best}, number))
    return expected


def read_results(output):
    """the results printed by -solve: {problem number: (score, move)}"""
    results = {}
    for line in output.splitlines():
        head, bar, rest = line.partition('|')
        if not bar or not head.strip().isdigit():
            continue
        tokens = rest.split()
        # depth, score, time, nodes, [nodes per second], principal variation
        if len(tokens) < 4 or not SCORE.match(tokens[1]):
            continue
        k = 4
        if k < len(tokens) and tokens[k].isdigit():
            k += 1
        move = tokens[k].lower() if k < len(tokens) and MOVE.match(tokens[k]) else None
        results[int(head)] = (int(SCORE.match(tokens[1]).group(1)), move)
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('exe')
    parser.add_argument('obf')
    parser.add_argument('--eval', help='eval.dat (default: the one the program finds itself)')
    parser.add_argument('--cwd', help='folder to run the program in')
    parser.add_argument('--threads', type=int, default=1)
    parser.add_argument('--level', type=int, default=60)
    parser.add_argument('--timeout', type=float, default=600)
    parser.add_argument('--prefix', default='', help='command that runs the program (an emulator)')
    args = parser.parse_args()

    expected = read_obf(args.obf)
    if not expected:
        print(f'FAIL: {args.obf} has no position')
        return 2
    command = shlex.split(args.prefix) + [str(Path(args.exe).resolve()), '-n', str(args.threads), '-l', str(args.level),
               '-solve', str(Path(args.obf).resolve())]
    if args.eval:
        command += ['-eval-file', str(Path(args.eval).resolve())]
    try:
        run = subprocess.run(command, cwd=args.cwd, capture_output=True, text=True, errors='replace',
                             timeout=args.timeout)
    except (OSError, subprocess.TimeoutExpired) as e:
        print(f'FAIL: {" ".join(command)}: {e}')
        return 2
    print(run.stdout, end='' if run.stdout.endswith('\n') or not run.stdout else '\n')
    if run.returncode != 0:
        print(run.stderr, end='' if run.stderr.endswith('\n') or not run.stderr else '\n')
        print(f'FAIL: the program ended with the exit code {run.returncode}')
        return 2

    results = read_results(run.stdout)
    bad = 0
    for n, (best, moves, line) in enumerate(expected, 1):
        if n not in results:
            print(f'FAIL: no result for the position {n} (line {line})')
            bad = max(bad, 2)
            continue
        score, move = results[n]
        if score != best:
            print(f'FAIL: position {n}: score {score:+d}, {best:+d} expected')
            bad = max(bad, 1)
        if move not in moves:
            print(f'FAIL: position {n}: move {move}, {" or ".join(sorted(moves))} expected')
            bad = max(bad, 1)
    extra = sorted(set(results) - set(range(1, len(expected) + 1)))
    if extra:
        print(f'FAIL: results for positions not in the file: {extra}')
        bad = max(bad, 2)
    print(f'{len(results)} results, {len(expected)} positions in {args.obf}: {"ok" if bad == 0 else "FAILED"}')
    return bad


if __name__ == '__main__':
    sys.exit(main())
