@echo off
setlocal
cd /d "%~dp0"
where cl >nul 2>&1
if errorlevel 1 (
    echo Run this script from a Visual Studio 2022 x64 Developer Command Prompt.
    exit /b 1
)
if not exist bin mkdir bin
cl /nologo /utf-8 /O2 /MT /DNDEBUG /D_CRT_SECURE_NO_DEPRECATE /D HAS_CPU_64 /arch:AVX512 /vlen=256 /D POPCOUNT "/d1trimfile:%~dp0src\\" /Iinclude src\all.c ws2_32.lib /Fo:bin\edax-v4.obj /Fe:bin\wEdax-x86-64-v4.exe
if errorlevel 1 exit /b 1
copy /Y config.ini bin\config.ini >nul
exit /b 0
