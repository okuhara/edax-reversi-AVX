# edax-reversi-AVX

Edax is a very strong othello program. Its main features are:
- fast bitboard based & multithreaded engine.
- accurate midgame-evaluation function.
- opening book learning capability.
- text based rich interface.
- multi-protocol support to connect to graphical interfaces or play on Internet (GGS).
- multi-OS support to run under MS-Windows, Linux and Mac OS X.

This is SSE/AVX optimized version of Edax 4.4.0. Functionally equivalent to the parent project, provided no bugs are introduced.

Thanks to AVX2, x64-modern build solves fforum-40-59.obf 60% faster than official edax-4.4 on Haswell, and runs level 30 autoplay 80% faster.

See http://www.amy.hi-ho.ne.jp/okuhara/bitboard.htm and http://www.amy.hi-ho.ne.jp/okuhara/edaxopt.htm for optimization details in Japanese.

## Comparison with [Edax 4.6]( https://github.com/abulmo/edax-reversi/)

Richard Delorme's own Edax 4.6 incorporated most of my changes and almost caught up Edax AVX 4.5.3.  However, I decided to maintain this forked project for further improvements.
- 4.6 dropped some optimizations toward Microsoft compilers on Windows, such as heavy use of vectorcall and keeping the bitboard (vboard) in non-volatile SSE registers.  On Windows(x64), the 4.5.3 MSVC build compiles much faster, and runs a little faster than clang.
- 4.6's algorithm choice is based on his particular environment, which may not be good for others.  For example I avoid PEXT in x86-64-v3 build since it is very [slow](https://boobyreversi.flop.jp/edaxopt.htm#boardscorebench) on some old AMD CPUs, and there are fairly good alternatives.

Now my 4.5.5 runs 13-17% faster than Edax 4.6 on Windows (Core i7-9700), by introducing thread local hash, simul lastflip tables and other refinements.

| fforum-20-39 (1 thread) | 4.4 | 4.6 | 4.5.3 | 4.5.4 | 4.5.5 |
|-----------------|-------|-------|-------|-------|-------|
| wEdax-x86-64    | 3.503 | 2.629 | 2.406 | 2.330 | 2.203 |
| wEdax-x86-64-v3 | 2.956 | 2.141 | 2.035 | 1.998 | 1.828 |

| fforum-40-59 (8 threads) | 4.4 | 4.6 | 4.5.3 | 4.5.4 | 4.5.5 |
|-----------------|--------|--------|--------|--------|--------|
| wEdax-x86-64    | 2:32.5 | 1:41.7 | 1:30.9 | 1:33.0 | 1:26.4 |
| wEdax-x86-64-v3 | 2:21.3 | 1:22.0 | 1:20.2 | 1:20.1 | 1:11.5 |

