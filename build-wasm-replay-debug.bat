@echo off
echo Building Defcon for WebAssembly (DEBUG MODE - Replay Viewer)...

if not exist "C:\emsdk\emsdk_env.bat" (
    echo Error: Emscripten SDK not found at C:\emsdk\
    echo Please install EMSDK first. Run:
    echo   cd C:\
    echo   git clone https://github.com/emscripten-core/emsdk.git
    echo   cd emsdk
    echo   emsdk install latest
    echo   emsdk activate latest
    pause
    exit /b 1
)

echo Activating Emscripten environment...
call C:\emsdk\emsdk_env.bat

set ORIGINAL_DIR=%CD%
set NINJA_PATH=%ORIGINAL_DIR%\tools\ninja.exe

if not exist build\wasm-replay-debug mkdir build\wasm-replay-debug

cd /d %ORIGINAL_DIR%\build\wasm-replay-debug

echo Configuring with Emscripten (DEBUG MODE - Replay Viewer)...
python %EMSDK%/upstream/emscripten/emcmake.py cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_REPLAY_VIEWER=ON -DCMAKE_MAKE_PROGRAM="%NINJA_PATH%" -G "Ninja" "%ORIGINAL_DIR%"

echo Building (DEBUG mode - showing all output)...
python %EMSDK%/upstream/emscripten/emmake.py "%NINJA_PATH%"

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
if not exist "%ORIGINAL_DIR%\website\demo_recordings" mkdir "%ORIGINAL_DIR%\website\demo_recordings"

for %%f in ("%ORIGINAL_DIR%\website\demo_recordings\replay_viewer_*.js") do del "%%f" 2>nul
for %%f in ("%ORIGINAL_DIR%\website\demo_recordings\replay_viewer_*.wasm") do del "%%f" 2>nul
for %%f in ("%ORIGINAL_DIR%\website\demo_recordings\replay_viewer_*.wasm.map") do del "%%f" 2>nul
for %%f in ("%ORIGINAL_DIR%\website\demo_recordings\replay_viewer_*.data") do del "%%f" 2>nul

copy /Y "%ORIGINAL_DIR%\build\wasm-replay-debug\result\Debug\replay_viewer_*.js" "%ORIGINAL_DIR%\website\demo_recordings\" 2>nul
copy /Y "%ORIGINAL_DIR%\build\wasm-replay-debug\result\Debug\replay_viewer_*.wasm" "%ORIGINAL_DIR%\website\demo_recordings\" 2>nul
copy /Y "%ORIGINAL_DIR%\build\wasm-replay-debug\result\Debug\replay_viewer_*.wasm.map" "%ORIGINAL_DIR%\website\demo_recordings\" 2>nul
copy /Y "%ORIGINAL_DIR%\build\wasm-replay-debug\result\Debug\replay_viewer_*.data" "%ORIGINAL_DIR%\website\demo_recordings\" 2>nul

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