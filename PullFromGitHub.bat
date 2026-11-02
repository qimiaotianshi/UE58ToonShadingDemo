@echo off
title Pull Latest From GitHub (Overwrite Local)
cd /d "%~dp0"

echo ============================================================
echo   Pull Latest From GitHub - OVERWRITE LOCAL CHANGES
echo   Repo : UE58ToonShadingDemo
echo ============================================================
echo.
echo   WARNING / Please read before continuing:
echo.
echo   1. Every uncommitted local change will be DISCARDED.
echo   2. New untracked files will be DELETED.
echo   3. Ignored folders are KEPT (Binaries/ Saved/ Intermediate/).
echo   4. Close Unreal Editor first, or files may be locked.
echo.
echo   A backup of your local changes is saved in git stash,
echo   so it can still be recovered (see notes at the end).
echo.
echo ============================================================
echo.

set confirm=
set /p confirm="Type YES (uppercase) to continue, anything else to cancel: "
if /i not "%confirm%"=="YES" goto :cancelled

echo.
echo ------------------------------------------------------------
echo [1/6] Checking for Unreal Editor process...
tasklist /FI "IMAGENAME eq UnrealEditor.exe" 2>nul | find /I "UnrealEditor.exe" >nul
if not errorlevel 1 (
    echo.
    echo   [!] UnrealEditor.exe is still running.
    echo       Please close Unreal Editor, then run this script again.
    echo.
    goto :failed
)
echo   OK - editor not running.
echo.

echo [2/6] Backing up local changes to git stash...
git status --porcelain > "%TEMP%\ue_pull_status.txt" 2>nul
set local_size=0
for %%A in ("%TEMP%\ue_pull_status.txt") do set local_size=%%~zA
if "%local_size%"=="0" (
    echo   Working tree already clean, nothing to back up.
) else (
    git stash push -u -m "auto-backup-before-pull" >nul 2>&1
    if errorlevel 1 (
        echo   [!] Stash failed, but continuing anyway.
    ) else (
        echo   Backup saved. Use "git stash list" to see it.
    )
)
echo.

echo [3/6] Fetching from GitHub...
git fetch --all --prune --tags
if errorlevel 1 (
    echo.
    echo   [X] Fetch FAILED - check your network / SSH key.
    echo.
    goto :failed
)
echo   Fetch OK.
echo.

echo [4/6] Switching to branch main and resetting to origin/main...
git checkout -f main
if errorlevel 1 (
    echo.
    echo   [X] Switch to main FAILED.
    echo.
    goto :failed
)
git reset --hard origin/main
if errorlevel 1 (
    echo.
    echo   [X] Reset FAILED.
    echo.
    goto :failed
)
echo   Reset OK.
echo.

echo [5/6] Downloading LFS assets (may take a while)...
git lfs install --local >nul 2>&1
git lfs pull
if errorlevel 1 (
    echo.
    echo   [!] LFS pull reported an error.
    echo       Assets may be pointer files only. Run this script again.
    echo.
)
echo.

echo [6/6] Removing untracked leftover files...
rem  -e : keep this script and the sync script even if they are untracked
git clean -fd -e PullFromGitHub.bat -e SyncToGitHub.bat
echo.

echo ============================================================
echo   DONE - Local copy now matches origin/main
echo ============================================================
echo.
echo   Latest commit:
git log -1 --pretty=format:"   %%h  %%ad  %%s" --date=format:"%%Y-%%m-%%d %%H:%%M"
echo.
echo.
echo   Working tree status:
git status -sb
echo.
echo ------------------------------------------------------------
echo   NOTES
echo ------------------------------------------------------------
echo   To see your backed-up changes : git stash list
echo   To restore them               : git stash pop
echo   To delete the backup for good : git stash clear
echo.
goto :end

:cancelled
echo.
echo   Cancelled. Nothing was changed.
echo.
goto :end

:failed
echo.
echo   Script stopped. Your local files were not fully updated.
echo.

:end
echo Press any key to exit...
pause >nul
