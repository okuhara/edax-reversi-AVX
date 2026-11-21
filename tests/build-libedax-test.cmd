@echo off
rem Build the library (libedax-x64.dll) and its test into tests\libedax, and run the test.
rem Run from a Visual Studio 2022 x64 Developer Command Prompt; data\eval.dat must exist
rem in the repository root (or set EDAX_EVAL_FILE to its path).
setlocal
cd /d "%~dp0\.."
where cl >nul 2>&1
if errorlevel 1 (
    echo Run this script from a Visual Studio 2022 x64 Developer Command Prompt.
    exit /b 1
)
if "%EDAX_EVAL_FILE%"=="" set EDAX_EVAL_FILE=data\eval.dat
if not exist "%EDAX_EVAL_FILE%" (
    echo %EDAX_EVAL_FILE% is not found.
    exit /b 1
)
if not exist tests\libedax\data mkdir tests\libedax\data
copy /Y "%EDAX_EVAL_FILE%" tests\libedax\data\eval.dat >nul
cl /nologo /utf-8 /O2 /fp:fast /GS- /MT /GL /DNDEBUG /D UNICODE /D_CRT_SECURE_NO_DEPRECATE /D HAS_CPU_64 /D LIB_BUILD /LD /Iinclude src\all.c ws2_32.lib /Fo:tests\libedax\libedax.obj /Fe:tests\libedax\libedax-x64.dll /link /IMPLIB:tests\libedax\libedax-x64.lib
if errorlevel 1 exit /b 1
cl /nologo /utf-8 /O1 /MT /D_CRT_SECURE_NO_DEPRECATE tests\libedax_test.c tests\libedax\libedax-x64.lib /Fo:tests\libedax\libedax_test.obj /Fe:tests\libedax\libedax_test.exe
if errorlevel 1 exit /b 1
cd tests\libedax
.\libedax_test.exe > libedax_test.log 2>&1
if errorlevel 1 (
    type libedax_test.log
    echo FAILED: see tests\libedax\libedax_test.log
    exit /b 1
)
findstr /c:" checks, " libedax_test.log
exit /b 0
