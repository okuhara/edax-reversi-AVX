@echo off
rem Appends one line for a program linked with /LTCG:PGO to pgo-build.txt in the folder of the program:
rem   /LTCG:PGO <name> sha256=<sha-256 of the program> commit=<HEAD> source=<clean|modified> slots=<slots> eval=<sha-256 of the eval.dat of the profile runs>
rem source is "modified" when src, include or tests differ from HEAD (files not in git are not seen).
rem package-release.py reads the last line of each program and refuses a program without one.
rem usage: pgo-record.cmd <program> <eval.dat of the profile runs> [<slots>]
setlocal
if "%~2"=="" (
    echo usage: pgo-record.cmd ^<program^> ^<eval.dat^> [^<slots^>]
    exit /b 2
)
set "SLOTS=%~3"
if "%SLOTS%"=="" set "SLOTS=-"
call :hash "%~f1" || exit /b 1
set "HASH_PROGRAM=%HASH%"
call :hash "%~f2" || exit /b 1
set "HASH_EVAL=%HASH%"
set "COMMIT="
for /f %%c in ('git -C "%~dp0.." rev-parse HEAD') do set "COMMIT=%%c"
if not defined COMMIT (
    echo pgo-record: git rev-parse HEAD failed
    exit /b 1
)
set "SOURCE=clean"
git -C "%~dp0.." diff --quiet HEAD -- src include tests
if errorlevel 1 set "SOURCE=modified"
>>"%~dp1pgo-build.txt" echo /LTCG:PGO %~nx1 sha256=%HASH_PROGRAM% commit=%COMMIT% source=%SOURCE% slots=%SLOTS% eval=%HASH_EVAL%
if errorlevel 1 exit /b 1
echo pgo-record: %~nx1 sha256=%HASH_PROGRAM% source=%SOURCE%
exit /b 0

rem the SHA-256 of a file (the second line of certutil, without spaces) in HASH
:hash
set "HASH="
if not exist "%~1" (
    echo pgo-record: %~1 is not found
    exit /b 1
)
for /f "skip=1 delims=" %%h in ('certutil -hashfile "%~1" SHA256') do if not defined HASH set "HASH=%%h"
if not defined HASH (
    echo pgo-record: certutil failed on %~1
    exit /b 1
)
set "HASH=%HASH: =%"
if "%HASH:~63,1%"=="" goto bad_hash
if not "%HASH:~64,1%"=="" goto bad_hash
exit /b 0
:bad_hash
echo pgo-record: certutil gave no SHA-256 for %~1
exit /b 1
