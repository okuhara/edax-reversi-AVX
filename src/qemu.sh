make build OS=linux ARCH=ARM COMP=gcc CC=aarch64-linux-gnu-gcc
# make build OS=linux ARCH=ARMv7 COMP=gcc CC=arm-linux-gnueabi-gcc
cd ../bin
qemu-aarch64 -L /usr/aarch64-linux-gnu ./lEdax-ARM -n 1 -l 60 -solve ../problem/fforum-20-39.obf
# qemu-arm -L /usr/arm-linux-gnueabi ./lEdax-ARMv7 -n 1 -l 60 -solve ../problem/fforum-20-39.obf
cd ../src
# C:\Android\android-ndk-r21\toolchains\llvm\prebuilt\windows-x86_64\bin\clang.exe --target=aarch64-linux-gnu -O2 -S flip_neon_bitscan.c
