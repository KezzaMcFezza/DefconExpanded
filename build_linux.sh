#!/bin/bash

# Kezza and Bert's Linux Build Script
# Mimics the GitHub workflow for local building

set -e  # Exit on any error

echo "========================================"
echo "Kezza and Bert's Linux Build Script"
echo "========================================"

# Get the script directory to ensure we're in the right place
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "Building from: $SCRIPT_DIR"

# Check if we're in a git repository
if [ ! -d ".git" ]; then
    echo "Warning: Not in a git repository. Make sure you're in the project root."
fi

echo ""
echo "Step 1: Installing Dependencies..."
echo "====================================="

# Update package list
sudo apt-get update

sudo apt-get install -y \
    build-essential \
    cmake \
    libgl1-mesa-dev \
    libglu1-mesa-dev \
    libasound2-dev \
    libpulse-dev \
    pkg-config \
    libpkgconf-dev \
    libvorbis-dev \
    libogg-dev \
    libomp-dev \
    mesa-common-dev \
    git \
    wget \
    gpg \
    software-properties-common

echo "Dependencies installed successfully!"

echo ""
echo "Step 2: Installing Modern CMake..."
echo "=================================="

# Check if CMake is already installed and recent enough
if command -v cmake &> /dev/null; then
    CMAKE_VERSION=$(cmake --version | head -n1 | cut -d' ' -f3)
    echo "Found CMake version: $CMAKE_VERSION"
    
    # Check if version is >= 3.16
    if [ "$(printf '%s\n' "3.16" "$CMAKE_VERSION" | sort -V | head -n1)" = "3.16" ]; then
        echo "CMake version is sufficient (>= 3.16)"
    else
        echo "CMake version is too old, installing newer version..."
        INSTALL_CMAKE=true
    fi
else
    echo "CMake not found, installing..."
    INSTALL_CMAKE=true
fi

if [ "$INSTALL_CMAKE" = true ]; then
    # Try to install modern CMake from Kitware repository
    if wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | sudo tee /etc/apt/trusted.gpg.d/kitware.gpg >/dev/null; then
        sudo apt-add-repository "deb https://apt.kitware.com/ubuntu/ $(lsb_release -cs) main" -y
        sudo apt-get update
        sudo apt-get install -y cmake
    else
        echo "Failed to add Kitware repository, using default CMake from apt..."
        sudo apt-get install -y cmake
    fi
fi

echo "Final CMake version:"
cmake --version

echo ""
echo "Step 3: Setting Execute Permissions..."
echo "======================================"

# Set execute permissions on the bundle script
if [ -f "targets/linux/bundle_sdl.sh" ]; then
    chmod +x targets/linux/bundle_sdl.sh
    echo "Execute permissions set for targets/linux/bundle_sdl.sh"
else
    echo "Warning: targets/linux/bundle_sdl.sh not found, skipping..."
fi

echo ""
echo "Step 4: Configuring CMake for Release..."
echo "========================================"

# Create build directory
mkdir -p build
cd build

# Configure CMake
cmake .. \
    -DCMAKE_BUILD_TYPE="Release" \
    -DCMAKE_CXX_STANDARD="17" \
    -DCMAKE_CXX_FLAGS="-w" \
    -DCMAKE_C_FLAGS="-w"

echo "CMake configuration completed!"

echo ""
echo "Step 5: Building Release..."
echo "==========================="

# Build the project using all available CPU cores
cmake --build . --parallel $(nproc) --config "Release"

echo ""
echo "Step 6: Packaging Build..."
echo "=========================="

# Show the results
if [ -d "result/Release" ]; then
    echo "Build artifacts found in: build/result/Release/"
    echo "Contents:"
    ls -la result/Release/
    
    # Create tarball with git revision count (mimicking GitHub workflow)
    cd "$SCRIPT_DIR"
    if [ -d ".git" ]; then
        GIT_COUNT=$(git rev-list --count HEAD 2>/dev/null || echo "unknown")
        TARBALL_NAME="defcon-git${GIT_COUNT}-linux-Release.tar.bz2"
    else
        TARBALL_NAME="defcon-linux-Release.tar.bz2"
    fi
    
    echo ""
    echo "Creating package: $TARBALL_NAME"
    cd build/result
    tar -cjf "../$TARBALL_NAME" "Release"
    cd "$SCRIPT_DIR"
    
    # Move tarball to project root for easy access
    if [ -f "build/$TARBALL_NAME" ]; then
        mv "build/$TARBALL_NAME" "./"
        echo "Package created successfully: $TARBALL_NAME"
        echo "Package size: $(du -h "$TARBALL_NAME" | cut -f1)"
    else
        echo "Warning: Failed to create package"
    fi
else
    echo "Build completed, but result/Release directory not found."
    echo "Build output may be in a different location."
fi

echo ""
echo "========================================"
echo "Build completed successfully!"
echo "========================================"

# Final summary
echo ""
echo "BUILD SUMMARY:"
echo "=================="

if [ -d "build/result/Release" ]; then
    echo "Binary location: build/result/Release/"
    echo "Executable: build/result/Release/defcon"
    
    if [ -d "build/result/Release/lib" ]; then
        echo "Bundled libraries in: build/result/Release/lib/"
        echo "   Libraries bundled:"
        ls -1 build/result/Release/lib/ 2>/dev/null | sed 's/^/   - /' || echo "   (library list unavailable)"
    fi
    
    if [ -f "$TARBALL_NAME" ]; then
        echo "Package: $TARBALL_NAME"
        echo ""
        echo "Ready to distribute!"
        echo "   - Run directly: ./build/result/Release/defcon"
        echo "   - Or extract package: tar -xjf $TARBALL_NAME"
    fi
else
    echo "Build artifacts not found in expected location"
fi

echo ""
echo "To rebuild:"
echo "   - Clean rebuild: rm -rf build && ./build_linux.sh"
echo "   - Quick rebuild: ./build_linux.sh"