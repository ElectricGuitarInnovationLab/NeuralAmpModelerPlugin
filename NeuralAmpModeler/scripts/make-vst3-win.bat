@echo off
setlocal

for %%I in ("%~dp0..") do set "PROJECT_DIR=%%~fI"
set "PLUGIN_NAME=Puke Amp.vst3"
set "BUNDLE_PATH=%PROJECT_DIR%\build-win\%PLUGIN_NAME%"
set "OUTPUT_DIR=%PROJECT_DIR%\build-vst3\windows"
set "OUTPUT_PATH=%OUTPUT_DIR%\%PLUGIN_NAME%"
set "ARCHIVE_PATH=%OUTPUT_DIR%\Puke Amp-VST3-Windows.zip"

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

echo Built "%OUTPUT_PATH%"
echo Packaged "%ARCHIVE_PATH%"
exit /b 0
