@echo off
echo Building Defcon WebAssembly Release...

call C:\emsdk\emsdk_env.bat > nul 2>&1

set ORIGINAL_DIR=%CD%
if not exist build\wasm-release mkdir build\wasm-release
cd /d %ORIGINAL_DIR%\build\wasm-release

set EMCC_OPTIMIZE_FLAGS="-O3 -flto -ffast-math"
set EMCC_CLOSURE=1
set EMCC_WASM_BACKEND=1

echo Configuring...
python %EMSDK%/upstream/emscripten/emcmake.py cmake ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_CXX_FLAGS_RELEASE="-O3 -flto -ffast-math -funroll-loops -fvectorize -DNDEBUG" ^
    -DCMAKE_C_FLAGS_RELEASE="-O3 -flto -ffast-math -funroll-loops -fvectorize -DNDEBUG" ^
    -G "Ninja" ^
    "%ORIGINAL_DIR%" > nul

if %ERRORLEVEL% neq 0 (
    echo Configuration failed.
    cd /d %ORIGINAL_DIR%
    pause
    exit /b 1
)

echo Building...
python %EMSDK%/upstream/emscripten/emmake.py ninja defcon 2>&1 | findstr /V "warning:"
set BUILD_RESULT=%ERRORLEVEL%

cd /d %ORIGINAL_DIR%

if %BUILD_RESULT% neq 0 (
    echo Build failed.
    pause
    exit /b 1
)

echo Build completed successfully.
echo Output: %ORIGINAL_DIR%\build\wasm-release\result\Release\
pause 