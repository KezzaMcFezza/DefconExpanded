#!/bin/bash

# Kezza and Bert's Linux Build Script
# Mimics the GitHub workflow for local building

set -e  # Exit on any error

# Default options
BUILD_TYPE="Release"
INSTALL_DEPS=false
REBUILD_DATA=false
BUILD_PACKAGE=false
TARBALL_NAME=""

# Parse command line arguments
show_help() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -d, --debug           Build in Debug mode (default: Release)"
    echo "  -i, --install-deps    Install dependencies via apt-get (default: skip)"
    echo "  -r, --rebuild-data    Rebuild main.dat from localisation/data (default: skip)"
    echo "  -p, --package         Create release tar.bz2 package (default: skip)"
    echo "  -h, --help            Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0                    # Build Release (no package)"
    echo "  $0 --debug            # Build Debug without installing deps"
    echo "  $0 --install-deps     # Build Release and install deps"
    echo "  $0 -d -i              # Build Debug and install deps"
    echo "  $0 --rebuild-data     # Rebuild main.dat and build Release"
    echo "  $0 --package          # Build Release and create tar.bz2"
    exit 0
}

while [[ $# -gt 0 ]]; do
    case $1 in
        -d|--debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        -i|--install-deps)
            INSTALL_DEPS=true
            shift
            ;;
        -r|--rebuild-data)
            REBUILD_DATA=true
            shift
            ;;
        -p|--package)
            BUILD_PACKAGE=true
            shift
            ;;
        -h|--help)
            show_help
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use -h or --help for usage information"
            exit 1
            ;;
    esac
done

echo "========================================"
echo "Kezza and Bert's Linux Build Script"
echo "========================================"
echo "Build Type: $BUILD_TYPE"
echo "Install Dependencies: $INSTALL_DEPS"
echo "Rebuild Data Archive: $REBUILD_DATA"
echo "Create Package:       $BUILD_PACKAGE"
echo "========================================"

# Get the script directory to ensure we're in the right place
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "Building from: $SCRIPT_DIR"

# Check if we're in a git repository
if [ ! -d ".git" ]; then
    echo "Warning: Not in a git repository. Make sure you're in the project root."
fi

if [ "$REBUILD_DATA" = true ]; then
    echo ""
    echo "Step 1: Rebuilding main.dat..."
    echo "=============================="
    
    # Check if the Python script exists
    if [ ! -f "tools/python/datatoarchive.py" ]; then
        echo "Error: tools/python/datatoarchive.py not found!"
        exit 1
    fi
    
    # Check if Python 3 is available
    if ! command -v python3 &> /dev/null; then
        echo "Error: python3 is required to rebuild main.dat"
        exit 1
    fi
    
    # Make rar.bin executable
    if [ -f "tools/bin/rar.bin" ]; then
        chmod +x tools/bin/rar.bin
        echo "Set execute permissions for tools/bin/rar.bin"
    else
        echo "Error: tools/bin/rar.bin not found!"
        exit 1
    fi
    
    # Run the data archive script
    echo "Running datatoarchive.py..."
    python3 tools/python/datatoarchive.py
    
    if [ $? -eq 0 ]; then
        echo "main.dat created in localisation/ directory"
        
        # Copy main.dat to build output directory if it exists
        if [ -d "build/result/$BUILD_TYPE" ]; then
            echo "Copying main.dat to build/result/$BUILD_TYPE/..."
            cp localisation/main.dat "build/result/$BUILD_TYPE/"
            echo "main.dat copied successfully!"
        else
            echo "Note: Build directory doesn't exist yet. main.dat will be copied during build."
        fi
    else
        echo "Error: Failed to rebuild main.dat"
        exit 1
    fi
fi

if [ "$INSTALL_DEPS" = true ]; then
    echo ""
    echo "Step 2: Installing Dependencies..."
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
    echo "Step 3: Installing Modern CMake..."
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
else
    echo ""
    echo "Skipping dependency installation..."
    echo "===================================="
    echo "If you need to install dependencies, run with --install-deps flag"
    echo ""
fi

echo ""
echo "Step 4: Setting Execute Permissions..."
echo "======================================"

# Set execute permissions on the bundle script
if [ -f "targets/linux/bundle_sdl.sh" ]; then
    chmod +x targets/linux/bundle_sdl.sh
    echo "Execute permissions set for targets/linux/bundle_sdl.sh"
else
    echo "Warning: targets/linux/bundle_sdl.sh not found, skipping..."
fi

echo ""
echo "Step 5: Configuring CMake for $BUILD_TYPE..."
echo "========================================"

# Create build directory
mkdir -p build
cd build

# Configure CMake
cmake .. \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_CXX_STANDARD="17" \
    -DCMAKE_CXX_FLAGS="-w" \
    -DCMAKE_C_FLAGS="-w"

echo "CMake configuration completed!"

echo ""
echo "Step 6: Building $BUILD_TYPE..."
echo "==========================="

# Build the project using all available CPU cores
cmake --build . --parallel $(nproc) --config "$BUILD_TYPE"

echo ""
echo "Step 7: Build Results"
echo "======================"

# Show the results
if [ -d "result/$BUILD_TYPE" ]; then
    echo "Build artifacts found in: build/result/$BUILD_TYPE/"
    echo "Contents:"
    ls -la result/$BUILD_TYPE/

    # Set execute permissions for launch.sh if it exists
    if [ -f "result/$BUILD_TYPE/launch.sh" ]; then
        chmod +x result/$BUILD_TYPE/launch.sh
        echo "Execute permissions set for launch.sh"
    fi

    if [ "$BUILD_PACKAGE" = true ]; then
        echo ""
        echo "Packaging enabled: creating tar.bz2 archive..."
        # Create tarball with git revision count (mimicking GitHub workflow)
        cd "$SCRIPT_DIR"
        if [ -d ".git" ]; then
            GIT_COUNT=$(git rev-list --count HEAD 2>/dev/null || echo "unknown")
            TARBALL_NAME="defcon-git${GIT_COUNT}-linux-$BUILD_TYPE.tar.bz2"
        else
            TARBALL_NAME="defcon-linux-$BUILD_TYPE.tar.bz2"
        fi

        echo "Creating package: $TARBALL_NAME"
        cd build/result
        tar -cjf "../$TARBALL_NAME" "$BUILD_TYPE"
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
        echo ""
        echo "Packaging is disabled by default."
        echo "Run with --package to create a tar.bz2 archive."
    fi
else
    echo "Build completed, but result/$BUILD_TYPE directory not found."
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
echo "Build Type: $BUILD_TYPE"

if [ -d "build/result/$BUILD_TYPE" ]; then
    echo "Binary location: build/result/$BUILD_TYPE/"
    echo "Executable: build/result/$BUILD_TYPE/defcon"
    
    if [ -d "build/result/$BUILD_TYPE/lib" ]; then
        echo "Bundled libraries in: build/result/$BUILD_TYPE/lib/"
        echo "   Libraries bundled:"
        ls -1 build/result/$BUILD_TYPE/lib/ 2>/dev/null | sed 's/^/   - /' || echo "   (library list unavailable)"
    fi
    
    if [ -n "$TARBALL_NAME" ] && [ -f "$TARBALL_NAME" ]; then
        echo "Package: $TARBALL_NAME"
        echo ""
        echo "Ready to distribute!"
        echo "   - Run directly: ./build/result/$BUILD_TYPE/defcon"
        echo "   - Or extract package: tar -xjf $TARBALL_NAME"
    fi
else
    echo "Build artifacts not found in expected location"
fi

echo ""
echo "To rebuild:"
echo "   - Clean rebuild: rm -rf build && ./build_linux.sh"
echo "   - Quick rebuild: ./build_linux.sh"
echo "   - Debug build: ./build_linux.sh --debug"
echo "   - With dependencies: ./build_linux.sh --install-deps"
echo "   - Rebuild main.dat: ./build_linux.sh --rebuild-data"
echo "   - Create package: ./build_linux.sh --package"
