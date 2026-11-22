@echo off
setlocal
title Pack Assets for GitHub Release
cd /d "%~dp0"

rem ============================================================
rem  UE58ToonShadingDemo - Asset Packager
rem  Packs ALL data (Content + Plugins, INCLUDING the AI runtime)
rem  into split 7z volumes for a GitHub Release.
rem  The git repository only keeps text source files.
rem ============================================================

set "RELROOT=..\Releases"

echo ============================================
echo   UE58ToonShadingDemo - Asset Packager
echo ============================================
echo.
echo   Packs: Content\ + Plugins\
echo          (AI runtime INCLUDED - no separate download)
echo.
echo   Split into 1900 MB volumes, each under the
echo   GitHub Release 2 GB per-file limit.
echo   Full data is ~30 GB, about 16 volumes.
echo.
echo   NOTE: Close Unreal Editor before packing.
echo.

rem ---- locate a command line archiver ----
set "MODE="
set "EXE="
if exist "%ProgramFiles%\Bandizip\bz.exe" (
    set "MODE=bz"
    set "EXE=%ProgramFiles%\Bandizip\bz.exe"
) else if exist "%ProgramFiles%\7-Zip\7z.exe" (
    set "MODE=7z"
    set "EXE=%ProgramFiles%\7-Zip\7z.exe"
)
if not defined MODE (
    echo [ERROR] No command line archiver found.
    echo         Install 7-Zip from https://www.7-zip.org/
    echo         or Bandizip, then run this script again.
    echo.
    pause
    exit /b 1
)
echo   Archiver : %MODE%
echo   Location : %EXE%
echo.

rem ---- release tag (letters, digits, dot, dash, underscore) ----
set "TAG=v1"
set /p INPUT="Release tag (Enter for v1): "
if not "%INPUT%"=="" set "TAG=%INPUT%"

if not exist "%RELROOT%" mkdir "%RELROOT%"
set "DEST=%RELROOT%\%TAG%"
if not exist "%DEST%" mkdir "%DEST%"

set "ARC=%DEST%\UE58Assets_%TAG%.7z"
set "LOG=%DEST%\pack_log.txt"

echo.
echo   Tag    : %TAG%
echo   Output : %DEST%
echo.

rem ---- remove old volumes for the same tag ----
del /q "%DEST%\*.7z.*" >nul 2>&1
del /q "%DEST%\*.7z" >nul 2>&1

echo [1/2] Packing Content + Plugins ...
echo        First run may take 30-90 minutes.
echo        Live progress is written to:
echo          %LOG%
echo        (open that file in another window to watch)
echo.

if "%MODE%"=="bz" (
    "%EXE%" c -fmt:7z -v:1900MB -l:1 -r -y "%ARC%" Content Plugins > "%LOG%" 2>&1
) else (
    "%EXE%" a -t7z -v1900m -mx=1 -y "%ARC%" Content Plugins > "%LOG%" 2>&1
)

if errorlevel 1 (
    echo.
    echo [FAILED] Archiver returned an error.
    echo          Check the log: %LOG%
    echo.
    pause
    exit /b 1
)

echo [2/2] Done. Generated volumes:
echo --------------------------------------------
dir /b "%DEST%\*.7z.*" 2>nul
dir /b "%DEST%\*.7z" 2>nul
echo --------------------------------------------
for /f %%A in ('dir /b "%DEST%\*.7z.*" 2^>nul ^| find /c /v ""') do set "COUNT=%%A"
echo   Volume count : %COUNT%
echo   Log file     : %LOG%
echo.
echo NEXT STEPS
echo   1. Open  https://github.com/qimiaotianshi/UE58ToonShadingDemo/releases/new
echo   2. Set the tag to %TAG%, then upload ALL .7z.00* volumes above
echo   3. Publish the release
echo.
echo   Code changes still use SyncToGitHub.bat (no LFS involved).
echo.
pause
endlocal
