# Builds Edax for an ARM target, then solves problem/fforum-20-39.obf with QEMU and compares the results with
# the scores of the file (tests/check-solve.py). Run it from src.
# usage: sh qemu.sh [arm32|neon|sve|armv8]   (armv8 by default)
# The exit code is not 0 when the build fails, when the program fails under QEMU, or when a result differs.
set -e
case "$1" in
arm32)
	arch=arm; cc=arm-linux-gnueabi-gcc
	qemu="qemu-arm -L /usr/arm-linux-gnueabi -cpu max,neon=off";;
neon)
	arch=arm-neon; cc=arm-linux-gnueabi-gcc
	qemu="qemu-arm -L /usr/arm-linux-gnueabi";;
sve)
	arch=arm-sve256; cc=aarch64-linux-gnu-gcc
	qemu="qemu-aarch64 -L /usr/aarch64-linux-gnu -cpu max,sve256=on";;
""|armv8)
	arch=armv8; cc=aarch64-linux-gnu-gcc
	qemu="qemu-aarch64 -L /usr/aarch64-linux-gnu";;
*)
	echo "usage: sh qemu.sh [arm32|neon|sve|armv8]" >&2
	exit 2;;
esac
# clang --target=aarch64-linux-gnu -std=c99 -pedantic -W -Wall -Wextra -pipe -D_GNU_SOURCE=1 -DUNICODE -Ofast -D NDEBUG -march=armv8-a -flto all.c -s -o ../bin/lEdax-armv8 -lm -lrt -lpthread
make build OS=linux ARCH=$arch COMP=gcc CC=$cc
cd ../bin
python3 ../tests/check-solve.py ./lEdax-$arch ../problem/fforum-20-39.obf --prefix "$qemu"
# C:\Android\android-ndk-r21\toolchains\llvm\prebuilt\windows-x86_64\bin\clang.exe --target=aarch64-linux-gnu -O2 -S flip_neon_bitscan.c
