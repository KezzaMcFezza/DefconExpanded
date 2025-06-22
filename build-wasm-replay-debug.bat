@echo off
echo Building Defcon for WebAssembly (DEBUG MODE - Replay Viewer)...

REM Check if EMSDK is installed
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

REM Activate Emscripten environment
echo Activating Emscripten environment...
call C:\emsdk\emsdk_env.bat

REM Store current directory
set ORIGINAL_DIR=%CD%

REM Create build directory
if not exist build\wasm-replay-debug mkdir build\wasm-replay-debug

REM Change to build directory
cd /d %ORIGINAL_DIR%\build\wasm-replay-debug

REM Configure with Emscripten in DEBUG mode
echo Configuring with Emscripten (DEBUG MODE - Replay Viewer)...
python %EMSDK%/upstream/emscripten/emcmake.py cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_EMSCRIPTEN_REPLAY_VIEWER=ON -G "Ninja" "%ORIGINAL_DIR%"

REM Build (showing all output for debugging)
echo Building (DEBUG mode - showing all output)...
python %EMSDK%/upstream/emscripten/emmake.py ninja defcon

if %ERRORLEVEL% neq 0 (
    echo Build failed with error code %ERRORLEVEL%
    pause
    exit /b %ERRORLEVEL%
)

echo Debug build complete!
echo Output files should be in: %ORIGINAL_DIR%\build\wasm-replay-debug\result\Debug\
echo Look for: defcon.html, defcon.js, defcon.wasm

echo.
echo Copying WebAssembly files to demo_recordings folder...
if not exist "%ORIGINAL_DIR%\website\demo_recordings" mkdir "%ORIGINAL_DIR%\website\demo_recordings"

copy /Y "%ORIGINAL_DIR%\build\wasm-replay-debug\result\Debug\defcon.html" "%ORIGINAL_DIR%\website\demo_recordings\"
copy /Y "%ORIGINAL_DIR%\build\wasm-replay-debug\result\Debug\defcon.js" "%ORIGINAL_DIR%\website\demo_recordings\"
copy /Y "%ORIGINAL_DIR%\build\wasm-replay-debug\result\Debug\defcon.wasm" "%ORIGINAL_DIR%\website\demo_recordings\"
copy /Y "%ORIGINAL_DIR%\build\wasm-replay-debug\result\Debug\defcon.data" "%ORIGINAL_DIR%\website\demo_recordings\" 2>nul

if %ERRORLEVEL% equ 0 (
    echo WebAssembly files copied successfully to website\demo_recordings\
) else (
    echo Warning: Some files may not have been copied. This is normal if defcon.data doesn't exist.
)

echo.
echo To run the debug version via Node.js server:
echo 1. cd website\node.js
echo 2. node server.js
echo 3. Open browser to http://localhost:3000/replay-viewer
echo.
echo Debug features enabled:
echo - Detailed assertions and error checking
echo - Stack overflow detection
echo - OpenGL debug output
echo - Exception handling enabled
echo - Source maps for debugging
echo - No optimization (easier debugging)
echo.
echo REPLAY VIEWER MODE: This build will skip the main menu and show only recording playback options.
echo Files are now available at: website\demo_recordings\defcon.html

REM Return to original directory
cd /d %ORIGINAL_DIR%
pause 