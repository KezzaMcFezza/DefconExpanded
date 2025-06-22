@echo off
REM Windows bundling script for Defcon
REM Usage: bundle.bat [build_dir] [output_dir]

setlocal enabledelayedexpansion

set BUILD_DIR=%~1
if "%BUILD_DIR%"=="" set BUILD_DIR=build\bin\Release
set OUTPUT_DIR=%~2
if "%OUTPUT_DIR%"=="" set OUTPUT_DIR=build\result\Release

echo Creating Windows bundle from "%BUILD_DIR%" to "%OUTPUT_DIR%"

REM Create output directory if it doesn't exist
if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

REM Copy the executable
echo Copying executable...
if exist "%BUILD_DIR%\defcon.exe" (
    copy /Y "%BUILD_DIR%\defcon.exe" "%OUTPUT_DIR%\"
) else (
    echo ERROR: defcon.exe not found in %BUILD_DIR%
    echo Available files:
    dir /b "%BUILD_DIR%"
    exit /b 1
)

REM Copy data files from localisation directory
echo Copying data files...
if exist "localisation\sounds.dat" (
    copy /Y "localisation\*.dat" "%OUTPUT_DIR%\"
) else (
    echo WARNING: sounds.dat not found in localisation directory
)

REM Create data directory structure
echo Creating data directory structure...
if not exist "%OUTPUT_DIR%\data" mkdir "%OUTPUT_DIR%\data"
if not exist "%OUTPUT_DIR%\data\earth" mkdir "%OUTPUT_DIR%\data\earth"
if not exist "%OUTPUT_DIR%\data\fonts" mkdir "%OUTPUT_DIR%\data\fonts"
if not exist "%OUTPUT_DIR%\data\language" mkdir "%OUTPUT_DIR%\data\language"

REM Copy data files
if exist "localisation\data" (
    xcopy /E /Y /I "localisation\data\earth\*" "%OUTPUT_DIR%\data\earth\"
    xcopy /E /Y /I "localisation\data\fonts\*" "%OUTPUT_DIR%\data\fonts\"
    xcopy /E /Y /I "localisation\data\language\*" "%OUTPUT_DIR%\data\language\"
) else (
    echo WARNING: data directory not found in localisation directory
)

REM Create mods directory
echo Creating mods directory...
if not exist "%OUTPUT_DIR%\mods" mkdir "%OUTPUT_DIR%\mods"

REM Copy mods if they exist
if exist "localisation\mods" (
    xcopy /E /Y /I "localisation\mods\*" "%OUTPUT_DIR%\mods\"
) else (
    echo INFO: No mods found in localisation directory
)

echo Bundle created successfully in %OUTPUT_DIR%
echo Directory listing:
dir /b "%OUTPUT_DIR%"
echo.
echo Subdirectories:
dir /b /AD "%OUTPUT_DIR%"

exit /b 0