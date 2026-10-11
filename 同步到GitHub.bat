@echo off
title Sync UE Project to GitHub
cd /d "%~dp0"

echo ============================================
echo   UE58ToonShadingDemo - Sync to GitHub
echo ============================================
echo.

set /p msg="Commit message (Enter for default): "
if "%msg%"=="" set msg=Update project

echo.
echo [1/4] Changed files:
git status -s
echo.

echo [2/4] Staging all changes (including new files)...
git add -A
echo.

echo [3/4] Committing...
> "%TEMP%\ue_sync_msg.txt" echo %msg%
git commit -F "%TEMP%\ue_sync_msg.txt"
if errorlevel 1 (
    echo.
    echo [!] Nothing to commit, trying push directly...
)
echo.

echo [4/4] Pushing to GitHub (large files, please wait)...
git push
echo.

if errorlevel 1 (
    echo ============================================
    echo   [FAILED] Push error
    echo   Reason : network issue or file too large
    echo   Fix    : just run this script again to resume
    echo ============================================
) else (
    echo ============================================
    echo   [OK] Synced to GitHub
    echo   https://github.com/qimiaotianshi/UE58ToonShadingDemo
    echo ============================================
)

echo.
pause
