@echo off

:menu
echo Which build type?
echo.
echo 1. Release
echo 2. Debug
echo.
set /p choice="Enter your choice (1 or 2): "

if "%choice%"=="1" set BUILD_TYPE=release& goto build_start
if "%choice%"=="2" set BUILD_TYPE=debug& goto build_start
echo Invalid choice. Please enter 1 or 2.
echo.
goto menu

:build_start
echo.
echo Building Defcon WebAssembly %BUILD_TYPE%...

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
set BUILD_DIR=wasm-defcon-%BUILD_TYPE%
set CMAKE_BUILD_FLAG=-DDEFCON_BUILD=ON
set TARGET_DIR=defcon

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
    -DCMAKE_CXX_FLAGS="%CMAKE_FLAGS%" ^
    -DCMAKE_C_FLAGS="%CMAKE_FLAGS%" ^
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
python "%ORIGINAL_DIR%\targets\emscripten\copy_files.py" %BUILD_TYPE% "%ORIGINAL_DIR%\build\%BUILD_DIR%\result\%BUILD_OUTPUT_DIR%" "%ORIGINAL_DIR%\website\%TARGET_DIR%"

if %ERRORLEVEL% neq 0 (
    echo File copying failed!
    pause
    exit /b 1
)

echo.
echo Updating HTML file versions automatically...
python "%ORIGINAL_DIR%\targets\emscripten\update_defcon_version.py"
if errorlevel 1 (
    echo ERROR: HTML version update failed. You may need to update manually.
    echo Manual steps:
    echo 1. Rename defcon_*.html to match the new version
    echo 2. Update the script tag inside the HTML file
    pause
    exit /b 1
)
echo HTML version update completed successfully!

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
