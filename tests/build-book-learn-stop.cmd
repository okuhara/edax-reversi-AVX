@echo off
rem Build the Edax that stops at a given write of book learn (book-resume.py --stop-exe).
setlocal
cd /d "%~dp0\.."
where cl >nul 2>&1
if errorlevel 1 (
    echo Run this script from a Visual Studio 2022 x64 Developer Command Prompt.
    exit /b 1
)
cl /nologo /utf-8 /O2 /MT /DNDEBUG /D_CRT_SECURE_NO_DEPRECATE /D HAS_CPU_64 /arch:AVX2 /D POPCOUNT /Iinclude tests\book-learn-stop.c ws2_32.lib /Fo:tests\book-learn-stop.obj /Fe:tests\book-learn-stop.exe
exit /b %errorlevel%
