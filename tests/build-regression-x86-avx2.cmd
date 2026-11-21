@echo off
setlocal
if /i not "%VSCMD_ARG_TGT_ARCH%"=="x86" (
    echo Run this script from a Visual Studio 2022 x86 Developer Command Prompt.
    exit /b 1
)
cd /d "%~dp0\.."
rem Keep /O2 /GL and AVX2: the x86 CRC/move-generation failure depends on them.
cl /nologo /utf-8 /O2 /fp:fast /GS- /MT /GL /DNDEBUG /D_CRT_SECURE_NO_DEPRECATE /D hasSSE2 /arch:AVX2 /D POPCOUNT /Iinclude tests\regression.c ws2_32.lib /Fo:tests\regression-x86-avx2.obj /Fe:tests\regression-x86-avx2.exe
exit /b %errorlevel%
