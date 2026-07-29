@echo off
setlocal

for %%I in ("%~dp0..") do set "PROJECT_DIR=%%~fI"
set "PLUGIN_NAME=Puke Amp.vst3"
set "BUNDLE_PATH=%PROJECT_DIR%\build-win\%PLUGIN_NAME%"
set "OUTPUT_DIR=%PROJECT_DIR%\build-vst3\windows"
set "OUTPUT_PATH=%OUTPUT_DIR%\%PLUGIN_NAME%"
set "ARCHIVE_PATH=%OUTPUT_DIR%\Puke Amp-VST3-Windows.zip"
set "INSTALLER_PATH=%OUTPUT_DIR%\Puke Amp-VST3-Windows-Setup.exe"
set "INSTALLER_SCRIPT=%PROJECT_DIR%\installer\PukeAmp-VST3.iss"

where msbuild >nul 2>nul
if errorlevel 1 (
  echo error: msbuild was not found. Run this from a Visual Studio Developer Command Prompt.
  exit /b 1
)

if not exist "%PROJECT_DIR%\..\iPlug2\Dependencies\IPlug\VST3_SDK" (
  echo error: the VST3 SDK is missing.
  echo Run iPlug2\Dependencies\IPlug\download-iplug-sdks.sh first.
  exit /b 1
)

msbuild "%PROJECT_DIR%\NeuralAmpModeler.sln" /t:NeuralAmpModeler-vst3 /p:Configuration=Release /p:Platform=x64 /m /nologo
if errorlevel 1 exit /b 1

if not exist "%BUNDLE_PATH%" (
  echo error: build succeeded but "%BUNDLE_PATH%" was not created.
  exit /b 1
)

if exist "%OUTPUT_PATH%" rmdir /s /q "%OUTPUT_PATH%"
if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"
xcopy /E /I /Y "%BUNDLE_PATH%" "%OUTPUT_PATH%\" >nul
if errorlevel 1 exit /b 1

powershell -NoProfile -Command "Compress-Archive -LiteralPath '%OUTPUT_PATH%' -DestinationPath '%ARCHIVE_PATH%' -Force"
if errorlevel 1 exit /b 1

for /f "tokens=3" %%V in ('findstr /b /c:"#define PLUG_VERSION_STR " "%PROJECT_DIR%\config.h"') do set "PLUGIN_VERSION=%%~V"
if not defined PLUGIN_VERSION (
  echo error: PLUG_VERSION_STR was not found in "%PROJECT_DIR%\config.h".
  exit /b 1
)

set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" set "ISCC=%ProgramFiles%\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" (
  for /f "delims=" %%I in ('where iscc 2^>nul') do if not defined ISCC_FOUND set "ISCC_FOUND=%%I"
  if defined ISCC_FOUND set "ISCC=%ISCC_FOUND%"
)
if not exist "%ISCC%" (
  echo error: Inno Setup 6 was not found. Install it from https://jrsoftware.org/isinfo.php
  exit /b 1
)

if exist "%INSTALLER_PATH%" del /q "%INSTALLER_PATH%"
"%ISCC%" /Qp "/DAppVersion=%PLUGIN_VERSION%" "%INSTALLER_SCRIPT%"
if errorlevel 1 exit /b 1
if not exist "%INSTALLER_PATH%" (
  echo error: installer compilation succeeded but "%INSTALLER_PATH%" was not created.
  exit /b 1
)

echo Built "%OUTPUT_PATH%"
echo Packaged "%ARCHIVE_PATH%"
echo Packaged "%INSTALLER_PATH%"
exit /b 0
