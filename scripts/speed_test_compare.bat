@echo off
setlocal enableextensions

rem Usage:
rem   speed_test_compare.bat [image1] [image2] [path_to_exe] [blend_mode]

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..") do set "REPO_DIR=%%~fI"

set "IMG1=%~1"
set "IMG2=%~2"
set "EXE_PATH=%~3"
set "BLEND_MODE=%~4"
set "SAMPLE_DIR=%REPO_DIR%\sample_images"

if "%IMG1%"=="" goto :usage
if "%IMG2%"=="" goto :usage

if /I "%EXE_PATH%"=="auto" (
  set "BLEND_MODE=%EXE_PATH%"
  set "EXE_PATH="
)
if /I "%EXE_PATH%"=="quality" (
  set "BLEND_MODE=%EXE_PATH%"
  set "EXE_PATH="
)
if /I "%EXE_PATH%"=="speed" (
  set "BLEND_MODE=%EXE_PATH%"
  set "EXE_PATH="
)

if "%EXE_PATH%"=="" set "EXE_PATH=%REPO_DIR%\x64\Debug\Image_Stitching.exe"
if "%BLEND_MODE%"=="" set "BLEND_MODE=auto"

if /I not "%BLEND_MODE%"=="auto" if /I not "%BLEND_MODE%"=="quality" if /I not "%BLEND_MODE%"=="speed" (
  echo Invalid blend mode: %BLEND_MODE%
  echo Valid values: auto, quality, speed
  exit /b 5
)

call :resolve_image_path "%IMG1%" RESOLVED_IMG1
if not defined RESOLVED_IMG1 (
  echo Image not found: %IMG1%
  exit /b 2
)
set "IMG1=%RESOLVED_IMG1%"

call :resolve_image_path "%IMG2%" RESOLVED_IMG2
if not defined RESOLVED_IMG2 (
  echo Image not found: %IMG2%
  exit /b 3
)
set "IMG2=%RESOLVED_IMG2%"

if not exist "%EXE_PATH%" (
  echo EXE not found: %EXE_PATH%
  exit /b 4
)

echo Image 1: %IMG1%
echo Image 2: %IMG2%
echo Blend mode: %BLEND_MODE%

set "LOG_DIR=%REPO_DIR%\speed_logs"
if not exist "%LOG_DIR%" mkdir "%LOG_DIR%"

for /f %%I in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd_HHmmss"') do set "TS=%%I"

set "CPU_LOG=%LOG_DIR%\cpu_%TS%.log"
set "GPU_LOG=%LOG_DIR%\gpu_%TS%.log"
set "SUMMARY_CSV=%LOG_DIR%\speed_summary.csv"

echo Running CPU speed test...
start "" /wait "%EXE_PATH%" --input "%IMG1%" --input "%IMG2%" --cpu --blend "%BLEND_MODE%" --autoclosepreview --exit --log "%CPU_LOG%"

if errorlevel 1 (
  echo CPU run failed with exit code %errorlevel%
  exit /b %errorlevel%
)

echo Running GPU speed test...
start "" /wait "%EXE_PATH%" --input "%IMG1%" --input "%IMG2%" --gpu --blend "%BLEND_MODE%" --autoclosepreview --exit --log "%GPU_LOG%"

if errorlevel 1 (
  echo GPU run failed with exit code %errorlevel%
  exit /b %errorlevel%
)

echo.
echo Completed.
echo CPU log: %CPU_LOG%
echo GPU log: %GPU_LOG%
echo CSV summary: %SUMMARY_CSV%
echo.
echo Compare these lines in both logs:
echo   Phase backend:
echo   Pair timing summary:
echo   Fallback breakdown:

powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%append_speed_summary.ps1" -CpuLog "%CPU_LOG%" -GpuLog "%GPU_LOG%" -Image1 "%IMG1%" -Image2 "%IMG2%" -CsvPath "%SUMMARY_CSV%"
if errorlevel 1 (
  echo CSV summary export failed with exit code %errorlevel%
  exit /b %errorlevel%
)

endlocal
exit /b 0

:usage
echo Usage: %~nx0 [image1] [image2] [path_to_exe]
echo Example:
echo   %~nx0 "C:\tests\pair1_a.bmp" "C:\tests\pair1_b.bmp"
echo Optional blend mode argument: auto ^| quality ^| speed
exit /b 1

:resolve_image_path
set "%~2="
set "_candidate=%~1"
if exist "%_candidate%" (
  set "%~2=%_candidate%"
  goto :eof
)

set "_sampleCandidate=%SAMPLE_DIR%\%~nx1"
if exist "%_sampleCandidate%" (
  set "%~2=%_sampleCandidate%"
)
goto :eof
