@echo off
echo Building Defcon for WebAssembly (DEBUG MODE)...

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
if not exist build\wasm-debug mkdir build\wasm-debug

REM Change to build directory
cd /d %ORIGINAL_DIR%\build\wasm-debug

REM Configure with Emscripten in DEBUG mode
echo Configuring with Emscripten (DEBUG MODE)...
python %EMSDK%/upstream/emscripten/emcmake.py cmake -DCMAKE_BUILD_TYPE=Debug -G "Ninja" "%ORIGINAL_DIR%"

REM Build (showing all output for debugging)
echo Building (DEBUG mode - showing all output)...
python %EMSDK%/upstream/emscripten/emmake.py ninja defcon

if %ERRORLEVEL% neq 0 (
    echo Build failed with error code %ERRORLEVEL%
    pause
    exit /b %ERRORLEVEL%
)

echo Debug build complete!
echo Output files should be in: %ORIGINAL_DIR%\build\wasm-debug\result\Debug\
echo Look for: defcon.html, defcon.js, defcon.wasm

echo.
echo To run the debug version:
echo 1. cd build\wasm-debug\result\Debug
echo 2. python -m http.server 8000
echo 3. Open browser to http://localhost:8000/defcon.html
echo.
echo Debug features enabled:
echo - Detailed assertions and error checking
echo - Stack overflow detection
echo - OpenGL debug output
echo - Exception handling enabled
echo - Source maps for debugging
echo - No optimization (easier debugging)

REM Return to original directory
cd /d %ORIGINAL_DIR%
pause 