@echo off
chcp 65001 >nul
title 同步 UE 工程到 GitHub
cd /d "%~dp0"

echo ============================================
echo   UE58ToonShadingDemo 一键同步到 GitHub
echo ============================================
echo.

set /p msg="请输入本次修改说明（直接回车用默认）: "
if "%msg%"=="" set msg=Update project

echo.
echo [1/4] 检查改动...
git status -s
echo.

echo [2/4] 添加所有改动（含新增文件）...
git add -A
echo.

echo [3/4] 提交...
rem 用临时文件传递消息，避免 & | ^ 等特殊字符导致命令解析出错
> "%TEMP%\ue_sync_msg.txt" echo %msg%
git commit -F "%TEMP%\ue_sync_msg.txt"
if errorlevel 1 (
    echo.
    echo [!] 没有需要提交的改动，尝试直接推送...
)
echo.

echo [4/4] 推送到 GitHub（大文件较多，请耐心等待）...
git push
echo.

if errorlevel 1 (
    echo ============================================
    echo   [失败] 推送出错
    echo   常见原因：网络中断 / 大文件超限
    echo   解决办法：重新运行本脚本即可续传
    echo ============================================
) else (
    echo ============================================
    echo   [成功] 已同步到 GitHub
    echo   https://github.com/qimiaotianshi/UE58ToonShadingDemo
    echo ============================================
)

echo.
pause
