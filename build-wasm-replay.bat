@echo off
echo Building Defcon WebAssembly Release (Replay Viewer Mode)...

set ORIGINAL_DIR=%CD%
call "%ORIGINAL_DIR%\contrib\systemIV\contrib\emsdk\emsdk_env.bat" > nul 2>&1
set NINJA_PATH=%ORIGINAL_DIR%\tools\ninja.exe
if not exist build\wasm-replay-release mkdir build\wasm-replay-release
cd /d %ORIGINAL_DIR%\build\wasm-replay-release

set EMCC_OPTIMIZE_FLAGS="-O3 -flto -ffast-math"
set EMCC_CLOSURE=1
set EMCC_WASM_BACKEND=1

echo Configuring...
python "%ORIGINAL_DIR%\contrib\systemIV\contrib\emsdk\upstream\emscripten\emcmake.py" cmake ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_CXX_FLAGS_RELEASE="-O3 -flto -ffast-math -funroll-loops -fvectorize -DNDEBUG" ^
    -DCMAKE_C_FLAGS_RELEASE="-O3 -flto -ffast-math -funroll-loops -fvectorize -DNDEBUG" ^
    -DENABLE_REPLAY_VIEWER=ON ^
    -DCMAKE_MAKE_PROGRAM="%NINJA_PATH%" ^
    -G "Ninja" ^
    "%ORIGINAL_DIR%" > nul

if %ERRORLEVEL% neq 0 (
    echo Configuration failed.
    cd /d %ORIGINAL_DIR%
    pause
    exit /b 1
)

echo Building...
python "%ORIGINAL_DIR%\contrib\systemIV\contrib\emsdk\upstream\emscripten\emmake.py" "%NINJA_PATH%" 2>&1 | findstr /V "warning:"
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

for %%f in ("%ORIGINAL_DIR%\website\demo_recordings\replay_viewer_*.js") do del "%%f" 2>nul
for %%f in ("%ORIGINAL_DIR%\website\demo_recordings\replay_viewer_*.wasm") do del "%%f" 2>nul
for %%f in ("%ORIGINAL_DIR%\website\demo_recordings\replay_viewer_*.data") do del "%%f" 2>nul

copy /Y "%ORIGINAL_DIR%\build\wasm-replay-release\result\Release\replay_viewer_*.js" "%ORIGINAL_DIR%\website\demo_recordings\" 2>nul
copy /Y "%ORIGINAL_DIR%\build\wasm-replay-release\result\Release\replay_viewer_*.wasm" "%ORIGINAL_DIR%\website\demo_recordings\" 2>nul
copy /Y "%ORIGINAL_DIR%\build\wasm-replay-release\result\Release\replay_viewer_*.data" "%ORIGINAL_DIR%\website\demo_recordings\" 2>nul

if %ERRORLEVEL% equ 0 (
    echo WebAssembly .js, .wasm, and .data files copied successfully
) else (
    echo Warning: Some files may not have been copied. This is normal if replay_viewer_*.data doesn't exist.
)

echo.
echo Updating HTML file versions automatically...
python "%ORIGINAL_DIR%\tools\update-replay-viewer-version.py"

if %ERRORLEVEL% equ 0 (
    echo HTML version update completed successfully!
) else (
    echo ERROR: HTML version update failed. You may need to update manually.
    echo Manual steps:
    echo 1. Rename replay_viewer_*.html to match the new version
    echo 2. Update the script tag inside the HTML file
    pause
    exit /b 1
)

echo.
echo === BUILD COMPLETE ===
echo Double check it works first before you upload to the live server
pause 