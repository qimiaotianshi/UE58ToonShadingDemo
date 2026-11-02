@echo off
set "SunFlareEditor=F:\ToonUE58\Engine\Binaries\Win64\UnrealEditor.exe"
if not exist "%SunFlareEditor%" (
  echo Cannot find UnrealEditor: %SunFlareEditor%
  pause
  exit /b 1
)
start "" "%SunFlareEditor%" "%~dp0..\..\Toon58ShadingDemo.uproject" "/Game/Render/SunLensFlare/L_SunLensFlareDemo"
