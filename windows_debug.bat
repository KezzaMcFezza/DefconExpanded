@echo off
setlocal enabledelayedexpansion

echo ===============================================
echo DEFCON Windows Debug Build Script
echo ===============================================

REM Set build directory
set BUILD_DIR=build_debug
set CMAKE_BUILD_TYPE=Debug

REM Check if CMake is available
cmake --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: CMake is not installed or not in PATH
    echo Please install CMake and add it to your PATH
    pause
    exit /b 1
)

REM Create build directory
echo Creating build directory: %BUILD_DIR%
if not exist %BUILD_DIR% (
    mkdir %BUILD_DIR%
)

REM Change to build directory
cd %BUILD_DIR%

echo.
echo ===============================================
echo Configuring CMake for Windows Debug Build
echo ===============================================

REM Configure with CMake - using Visual Studio generator
cmake -G "Visual Studio 17 2022" ^
      -A Win32 ^
      -T v143 ^
      -DCMAKE_BUILD_TYPE=Debug ^
      -DCMAKE_CONFIGURATION_TYPES="Debug" ^
      ..

if errorlevel 1 (
    echo.
    echo ERROR: CMake configuration failed!
    echo Make sure you have Visual Studio 2022 installed with C++ support
    cd ..
    pause
    exit /b 1
)

echo.
echo ===============================================
echo Building DEFCON in Debug mode
echo ===============================================

REM Build the project
cmake --build . --config Debug --parallel

if errorlevel 1 (
    echo.
    echo ERROR: Build failed!
    cd ..
    pause
    exit /b 1
)

echo.
echo ===============================================
echo Debug build completed successfully!
echo ===============================================

REM Show where the executable was built
echo.
echo Debug executable should be located at:
echo %cd%\result\Debug\defcon.exe
echo.

if exist "result\Debug\defcon.exe" (
    echo ✓ Debug build successful! Executable found.
    echo.
    echo Debug build includes:
    echo   - Debug symbols for debugging
    echo   - Runtime checks enabled
    echo   - No optimizations (better for debugging)
    echo   - Additional error checking
) else (
    echo ⚠ Warning: Executable not found in expected location.
    echo Check the build output above for any issues.
)

REM Return to original directory
cd ..

echo.
echo Press any key to exit...
pause >nul