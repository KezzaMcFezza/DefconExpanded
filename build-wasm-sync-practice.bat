@echo off
echo Building Defcon WebAssembly Release (Silo Practice Client)...

set ORIGINAL_DIR=%CD%
call "%ORIGINAL_DIR%\contrib\systemIV\contrib\emsdk\emsdk_env.bat" > nul 2>&1
set NINJA_PATH=%ORIGINAL_DIR%\tools\ninja.exe
if not exist build\wasm-sync_practice-release mkdir build\wasm-sync_practice-release
cd /d %ORIGINAL_DIR%\build\wasm-sync_practice-release

set EMCC_OPTIMIZE_FLAGS="-O3 -flto -ffast-math"
set EMCC_CLOSURE=1
set EMCC_WASM_BACKEND=1

echo Configuring...
python "%ORIGINAL_DIR%\contrib\systemIV\contrib\emsdk\upstream\emscripten\emcmake.py" cmake ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_CXX_FLAGS_RELEASE="-O3 -flto -funroll-loops -fvectorize -DNDEBUG -DSYNC_PRACTICE=1 -DNOT_REPLAY_VIEWER=1" ^
    -DCMAKE_C_FLAGS_RELEASE="-O3 -flto -funroll-loops -fvectorize -DNDEBUG -DSYNC_PRACTICE=1 -DNOT_REPLAY_VIEWER=1" ^
    -DSYNC_PRACTICE_BUILD=ON ^
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
echo Output: %ORIGINAL_DIR%\build\wasm-sync_practice-release\result\Release\

echo.
echo Copying WebAssembly files to public folder...
if not exist "%ORIGINAL_DIR%\website\sync_practice" mkdir "%ORIGINAL_DIR%\website\sync_practice"

for %%f in ("%ORIGINAL_DIR%\website\sync_practice\sync_practice_*.js") do del "%%f" 2>nul
for %%f in ("%ORIGINAL_DIR%\website\sync_practice\sync_practice_*.wasm") do del "%%f" 2>nul
for %%f in ("%ORIGINAL_DIR%\website\sync_practice\sync_practice_*.data") do del "%%f" 2>nul

copy /Y "%ORIGINAL_DIR%\build\wasm-sync_practice-release\result\Release\sync_practice_*.js" "%ORIGINAL_DIR%\website\sync_practice\" 2>nul
copy /Y "%ORIGINAL_DIR%\build\wasm-sync_practice-release\result\Release\sync_practice_*.wasm" "%ORIGINAL_DIR%\website\sync_practice\" 2>nul
copy /Y "%ORIGINAL_DIR%\build\wasm-sync_practice-release\result\Release\sync_practice_*.data" "%ORIGINAL_DIR%\website\sync_practice\" 2>nul

if %ERRORLEVEL% equ 0 (
    echo WebAssembly files copied successfully
) else (
    echo Warning: Some files may not have been copied. This is normal if sync_practice_*.data doesn't exist.
)

echo.
echo Updating HTML file versions automatically...
python "%ORIGINAL_DIR%\tools\update-sync-practice-client.py"

if %ERRORLEVEL% equ 0 (
    echo HTML version update completed successfully!
) else (
    echo ERROR: HTML version update failed. You may need to update manually.
    echo Manual steps:
    echo 1. Rename sync_practice_*.html to match the new version
    echo 2. Update the script tag inside the HTML file
    pause
    exit /b 1
)

echo.
echo === BUILD COMPLETE ===
echo Files are in: %ORIGINAL_DIR%\website\sync_practice\
echo Double check it works first before you upload to the live server
pause

