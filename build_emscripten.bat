@echo off

:menu
echo Which build do you want to make?
echo Type --Release or --Debug after entering your number
echo.
echo 1. Replay Viewer
echo 2. Sync Practice
echo.
echo Examples: "1 --Release", "2 --Debug", "1", "2"
set /p choice="Enter your choice: "

for /f "tokens=1,2" %%a in ("%choice%") do (
    set "project_choice=%%a"
    set "build_type=%%b"
)

if "%build_type%"=="" set "build_type="
if "%project_choice%"=="1" goto replay_parse
if "%project_choice%"=="2" goto sync_parse
echo Invalid choice. Please enter 1 or 2.
echo.
goto menu

:replay_parse
echo.
echo Replay Viewer selected.
if "%build_type%"=="" goto replay_menu
if /i "%build_type%"=="--Release" goto replay_release
if /i "%build_type%"=="--Debug" goto replay_debug
if "%build_type%"=="1" goto replay_release
if "%build_type%"=="2" goto replay_debug
echo Invalid build type. Please enter --Release or --Debug.
echo.
goto replay_menu

:replay_menu
echo Choose build type:
echo 1. Release
echo 2. Debug
echo.
set /p build_type="Enter build type (1 or 2): "
if "%build_type%"=="1" goto replay_release
if "%build_type%"=="2" goto replay_debug
echo Invalid choice. Please enter 1 or 2.
echo.
goto replay_menu

:sync_parse
echo.
echo Sync Practice selected.
if "%build_type%"=="" goto sync_menu
if /i "%build_type%"=="--Release" goto sync_release
if /i "%build_type%"=="--Debug" goto sync_debug
if "%build_type%"=="1" goto sync_release
if "%build_type%"=="2" goto sync_debug
echo Invalid build type. Please enter --Release or --Debug.
echo.
goto sync_menu

:sync_menu
echo Choose build type:
echo 1. Release
echo 2. Debug
echo.
set /p build_type="Enter build type (1 or 2): "
if "%build_type%"=="1" goto sync_release
if "%build_type%"=="2" goto sync_debug
echo Invalid choice. Please enter 1 or 2.
echo.
goto sync_menu

:replay_release
set PROJECT_TYPE=replay
set BUILD_TYPE=release
set PROJECT_NAME=Replay Viewer
goto build_start

:replay_debug
set PROJECT_TYPE=replay
set BUILD_TYPE=debug
set PROJECT_NAME=Replay Viewer
goto build_start

:sync_release
set PROJECT_TYPE=sync
set BUILD_TYPE=release
set PROJECT_NAME=Sync Practice
goto build_start

:sync_debug
set PROJECT_TYPE=sync
set BUILD_TYPE=debug
set PROJECT_NAME=Sync Practice
goto build_start

:build_start
echo.
echo Building Defcon WebAssembly %BUILD_TYPE% (%PROJECT_NAME%)...

set ORIGINAL_DIR=%CD%
set EMSDK_DIR=%ORIGINAL_DIR%\contrib\systemIV\contrib\emsdk

if not exist "%EMSDK_DIR%\upstream\emscripten" (
    echo Emscripten not installed. Installing and activating...
    cd /d "%EMSDK_DIR%"
    call emsdk.bat install latest
    call emsdk.bat activate latest
)

call "%EMSDK_DIR%\emsdk_env.bat" > nul 2>&1

set NINJA_PATH=%ORIGINAL_DIR%\tools\bin\ninja.exe

if "%PROJECT_TYPE%"=="replay" (
    set BUILD_DIR=wasm-replay-%BUILD_TYPE%
    set CMAKE_BUILD_FLAG=-DREPLAY_VIEWER_BUILD=ON
    set CMAKE_DEFINES=-DREPLAY_VIEWER=1
    set TARGET_DIR=replay_viewer
    set UPDATE_SCRIPT=update_replay_viewer_version.py
) else (
    set BUILD_DIR=wasm-sync_practice-%BUILD_TYPE%
    set CMAKE_BUILD_FLAG=-DSYNC_PRACTICE_BUILD=ON
    set CMAKE_DEFINES=-DSYNC_PRACTICE=1 -DNOT_REPLAY_VIEWER=1
    set TARGET_DIR=sync_practice
    set UPDATE_SCRIPT=update_sync_practice_client.py
)

if not exist build\%BUILD_DIR% mkdir build\%BUILD_DIR%
cd /d %ORIGINAL_DIR%\build\%BUILD_DIR%

if "%BUILD_TYPE%"=="release" (
    set EMCC_OPTIMIZE_FLAGS="-O3 -flto -ffast-math"
    set EMCC_CLOSURE=1
    set EMCC_WASM_BACKEND=1
    set CMAKE_BUILD_TYPE=Release
    set CMAKE_FLAGS=-O3 -flto -funroll-loops -fvectorize -DNDEBUG
    set BUILD_OUTPUT_DIR=Release
    set BUILD_QUIET=> nul
) else (
    set CMAKE_BUILD_TYPE=Debug
    set CMAKE_FLAGS=-g -O0 -D_DEBUG
    set BUILD_OUTPUT_DIR=Debug
    set BUILD_QUIET=
)

echo Configuring...
python "%ORIGINAL_DIR%\contrib\systemIV\contrib\emsdk\upstream\emscripten\emcmake.py" cmake ^
    -DCMAKE_BUILD_TYPE=%CMAKE_BUILD_TYPE% ^
    -DCMAKE_CXX_FLAGS="%CMAKE_FLAGS% %CMAKE_DEFINES%" ^
    -DCMAKE_C_FLAGS="%CMAKE_FLAGS% %CMAKE_DEFINES%" ^
    %CMAKE_BUILD_FLAG% ^
    -DCMAKE_MAKE_PROGRAM="%NINJA_PATH%" ^
    -G "Ninja" ^
    "%ORIGINAL_DIR%" %BUILD_QUIET%

if %ERRORLEVEL% neq 0 (
    echo Configuration failed.
    cd /d %ORIGINAL_DIR%
    pause
    exit /b 1
)

echo Building...
if "%BUILD_TYPE%"=="release" (
    python "%ORIGINAL_DIR%\contrib\systemIV\contrib\emsdk\upstream\emscripten\emmake.py" "%NINJA_PATH%" 2>&1 | findstr /V "warning:"
) else (
    python "%ORIGINAL_DIR%\contrib\systemIV\contrib\emsdk\upstream\emscripten\emmake.py" "%NINJA_PATH%"
)
set BUILD_RESULT=%ERRORLEVEL%

cd /d %ORIGINAL_DIR%

if %BUILD_RESULT% neq 0 (
    echo Build failed.
    pause
    exit /b 1
)

echo Build completed successfully.
echo Output: %ORIGINAL_DIR%\build\%BUILD_DIR%\result\%BUILD_OUTPUT_DIR%\

echo.
echo Copying WebAssembly files...
python "%ORIGINAL_DIR%\targets\emscripten\copy_files.py" %PROJECT_TYPE% %BUILD_TYPE% "%ORIGINAL_DIR%\build\%BUILD_DIR%\result\%BUILD_OUTPUT_DIR%" "%ORIGINAL_DIR%\website\%TARGET_DIR%"

if %ERRORLEVEL% neq 0 (
    echo File copying failed!
    pause
    exit /b 1
)

echo.
echo Updating HTML file versions automatically...
python "%ORIGINAL_DIR%\targets\emscripten\%UPDATE_SCRIPT%"

if %ERRORLEVEL% equ 0 (
    echo HTML version update completed successfully!
) else (
    echo ERROR: HTML version update failed. You may need to update manually.
    echo Manual steps:
    echo 1. Rename %TARGET_DIR%_*.html to match the new version
    echo 2. Update the script tag inside the HTML file
    pause
    exit /b 1
)

echo.
echo Build complete
if "%BUILD_TYPE%"=="debug" (
    echo Enjoy hell on earth
) else (
    echo Double check it works first before you upload to the live server
)
echo Files are in: %ORIGINAL_DIR%\website\%TARGET_DIR%\
pause
goto end

:end
