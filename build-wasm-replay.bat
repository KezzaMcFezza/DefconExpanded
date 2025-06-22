@echo off
echo Building Defcon WebAssembly Release (Replay Viewer Mode)...

call C:\emsdk\emsdk_env.bat > nul 2>&1

set ORIGINAL_DIR=%CD%
if not exist build\wasm-replay-release mkdir build\wasm-replay-release
cd /d %ORIGINAL_DIR%\build\wasm-replay-release

set EMCC_OPTIMIZE_FLAGS="-O3 -flto -ffast-math"
set EMCC_CLOSURE=1
set EMCC_WASM_BACKEND=1

echo Configuring...
python %EMSDK%/upstream/emscripten/emcmake.py cmake ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_CXX_FLAGS_RELEASE="-O3 -flto -ffast-math -funroll-loops -fvectorize -DNDEBUG" ^
    -DCMAKE_C_FLAGS_RELEASE="-O3 -flto -ffast-math -funroll-loops -fvectorize -DNDEBUG" ^
    -DENABLE_EMSCRIPTEN_REPLAY_VIEWER=ON ^
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
echo Output: %ORIGINAL_DIR%\build\wasm-replay-release\result\Release\

echo.
echo Copying WebAssembly files to demo_recordings folder...
if not exist "%ORIGINAL_DIR%\website\demo_recordings" mkdir "%ORIGINAL_DIR%\website\demo_recordings"

copy /Y "%ORIGINAL_DIR%\build\wasm-replay-release\result\Release\defcon.html" "%ORIGINAL_DIR%\website\demo_recordings\"
copy /Y "%ORIGINAL_DIR%\build\wasm-replay-release\result\Release\defcon.js" "%ORIGINAL_DIR%\website\demo_recordings\"
copy /Y "%ORIGINAL_DIR%\build\wasm-replay-release\result\Release\defcon.wasm" "%ORIGINAL_DIR%\website\demo_recordings\"
copy /Y "%ORIGINAL_DIR%\build\wasm-replay-release\result\Release\defcon.data" "%ORIGINAL_DIR%\website\demo_recordings\" 2>nul

if %ERRORLEVEL% equ 0 (
    echo WebAssembly files copied successfully to website\demo_recordings\
) else (
    echo Warning: Some files may not have been copied. This is normal if defcon.data doesn't exist.
)

echo.
echo REPLAY VIEWER MODE: This build will skip the main menu and show only recording playback options.
echo Files are now available at: website\demo_recordings\defcon.html
pause 