@echo off
setlocal enabledelayedexpansion

echo ===============================================
echo DEFCON Windows Release Build Script
echo ===============================================

REM Set build directory
set BUILD_DIR=build
set CMAKE_BUILD_TYPE=Release

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
echo Configuring CMake for Windows Release Build
echo ===============================================

REM Configure with CMake - using Visual Studio generator
cmake -G "Visual Studio 17 2022" ^
      -A Win32 ^
      -T v143 ^
      -DCMAKE_BUILD_TYPE=Release ^
      -DCMAKE_CONFIGURATION_TYPES="Release" ^
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
echo Building DEFCON in Release mode
echo ===============================================

REM Build the project
cmake --build . --config Release --parallel

if errorlevel 1 (
    echo.
    echo ERROR: Build failed!
    cd ..
    pause
    exit /b 1
)

echo.
echo ===============================================
echo Build completed successfully!
echo ===============================================

REM Show where the executable was built
echo.
echo Executable should be located at:
echo %cd%\result\Release\defcon.exe
echo.

if exist "result\Release\defcon.exe" (
    echo ✓ Build successful! Executable found.
) else (
    echo ⚠ Warning: Executable not found in expected location.
    echo Check the build output above for any issues.
)

REM Return to original directory
cd ..

echo.
echo Press any key to exit...
pause >nul