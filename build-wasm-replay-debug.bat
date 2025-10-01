@echo off
echo Building Defcon for WebAssembly (DEBUG MODE - Replay Viewer)...

set ORIGINAL_DIR=%CD%
call "%ORIGINAL_DIR%\contrib\systemIV\contrib\emsdk\emsdk_env.bat" > nul 2>&1

set NINJA_PATH=%ORIGINAL_DIR%\tools\ninja.exe

if not exist build\wasm-replay-debug mkdir build\wasm-replay-debug

cd /d %ORIGINAL_DIR%\build\wasm-replay-debug

echo Configuring with Emscripten (DEBUG MODE - Replay Viewer)...
python "%ORIGINAL_DIR%\contrib\systemIV\contrib\emsdk\upstream\emscripten\emcmake.py" cmake ^
    -DCMAKE_BUILD_TYPE=Debug ^
    -DCMAKE_CXX_FLAGS_DEBUG="-g -O0 -D_DEBUG -DRECORDING_PARSING=1 -DREPLAY_VIEWER=1" ^
    -DCMAKE_C_FLAGS_DEBUG="-g -O0 -D_DEBUG -DRECORDING_PARSING=1 -DREPLAY_VIEWER=1" ^
    -DREPLAY_VIEWER_BUILD=ON ^
    -DCMAKE_MAKE_PROGRAM="%NINJA_PATH%" ^
    -G "Ninja" ^
    "%ORIGINAL_DIR%"

echo Building (DEBUG mode - showing all output)...
python "%ORIGINAL_DIR%\contrib\systemIV\contrib\emsdk\upstream\emscripten\emmake.py" "%NINJA_PATH%"

if %ERRORLEVEL% neq 0 (
    echo Build failed with error code %ERRORLEVEL%
    pause
    exit /b %ERRORLEVEL%
)

echo Debug build complete!
echo Output files should be in: %ORIGINAL_DIR%\build\wasm-replay-debug\result\Debug\
echo Look for: replay_viewer_*.html, replay_viewer_*.js, replay_viewer_*.wasm, replay_viewer_*.wasm.map

echo.
echo Copying WebAssembly files to demo_recordings folder...
if not exist "%ORIGINAL_DIR%\website\replay_viewer" mkdir "%ORIGINAL_DIR%\website\replay_viewer"

for %%f in ("%ORIGINAL_DIR%\website\replay_viewer\replay_viewer_*.js") do del "%%f" 2>nul
for %%f in ("%ORIGINAL_DIR%\website\replay_viewer\replay_viewer_*.wasm") do del "%%f" 2>nul
for %%f in ("%ORIGINAL_DIR%\website\replay_viewer\replay_viewer_*.wasm.map") do del "%%f" 2>nul
for %%f in ("%ORIGINAL_DIR%\website\replay_viewer\replay_viewer_*.data") do del "%%f" 2>nul

copy /Y "%ORIGINAL_DIR%\build\wasm-replay-debug\result\Debug\replay_viewer_*.js" "%ORIGINAL_DIR%\website\replay_viewer\" 2>nul
copy /Y "%ORIGINAL_DIR%\build\wasm-replay-debug\result\Debug\replay_viewer_*.wasm" "%ORIGINAL_DIR%\website\replay_viewer\" 2>nul
copy /Y "%ORIGINAL_DIR%\build\wasm-replay-debug\result\Debug\replay_viewer_*.wasm.map" "%ORIGINAL_DIR%\website\replay_viewer\" 2>nul
copy /Y "%ORIGINAL_DIR%\build\wasm-replay-debug\result\Debug\replay_viewer_*.data" "%ORIGINAL_DIR%\website\replay_viewer\" 2>nul

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
echo === DEBUG BUILD COMPLETE ===
echo Enjoy hell on earth

cd /d %ORIGINAL_DIR%
pause 