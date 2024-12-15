if [ "$1" = arm32 ]
then
make build OS=linux ARCH=arm-neon COMP=gcc CC=arm-linux-gnueabi-gcc
cd ../bin
qemu-arm -L /usr/arm-linux-gnueabi ./lEdax-arm-neon -n 1 -l 60 -solve ../problem/fforum-20-39.obf
elif [ "$1" = sve ]
then
make build OS=linux ARCH=arm-sve COMP=gcc CC=aarch64-linux-gnu-gcc
# clang --target=aarch64-linux-gnu -std=c99 -pedantic -W -Wall -Wextra -pipe -D_GNU_SOURCE=1 -DUNICODE -Ofast -D NDEBUG -march=armv8.2-a+sve -flto all.c -s -o ../bin/lEdax-armv8+sve -lm -lrt -lpthread
cd ../bin
qemu-aarch64 -L /usr/aarch64-linux-gnu -cpu max,sve128=on ./lEdax-arm-sve -n 1 -l 60 -solve ../problem/fforum-20-39.obf
else
make build OS=linux ARCH=arm COMP=gcc CC=aarch64-linux-gnu-gcc
# clang --target=aarch64-linux-gnu -std=c99 -pedantic -W -Wall -Wextra -pipe -D_GNU_SOURCE=1 -DUNICODE -Ofast -D NDEBUG -march=armv8-a -flto all.c -s -o ../bin/lEdax-armv8 -lm -lrt -lpthread
cd ../bin
qemu-aarch64 -L /usr/aarch64-linux-gnu ./lEdax-arm -n 1 -l 60 -solve ../problem/fforum-20-39.obf
fi
cd ../src
# C:\Android\android-ndk-r21\toolchains\llvm\prebuilt\windows-x86_64\bin\clang.exe --target=aarch64-linux-gnu -O2 -S flip_neon_bitscan.c
