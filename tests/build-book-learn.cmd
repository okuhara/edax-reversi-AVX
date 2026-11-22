@echo off
rem Build the tests of book play, book play <n>, book extend, book exhaust, book extract all and the base commands.
rem Run tests\book-learn.exe from a folder for temporary files, with EDAX_EVAL_FILE set.
setlocal
cd /d "%~dp0\.."
where cl >nul 2>&1
if errorlevel 1 (
    echo Run this script from a Visual Studio 2022 x64 Developer Command Prompt.
    exit /b 1
)
cl /nologo /utf-8 /O2 /MT /DNDEBUG /D_CRT_SECURE_NO_DEPRECATE /D HAS_CPU_64 /arch:AVX2 /D POPCOUNT /Iinclude tests\book-learn.c ws2_32.lib /Fo:tests\book-learn.obj /Fe:tests\book-learn.exe
exit /b %errorlevel%
