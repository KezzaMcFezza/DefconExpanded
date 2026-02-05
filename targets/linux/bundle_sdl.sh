#!/bin/bash

# for Linux

set -x

cd "$1"

mkdir -p lib

# We use a pre built SDL3 library, not the package manager one for cross distro compiling
SDL3_LIB_DIR="../../../contrib/systemIV/contrib/SDL3-3.4.0/linux/lib"

if [ -d "$SDL3_LIB_DIR" ]; then
    echo "Bundling SDL2 libraries..."
    cp "${SDL3_LIB_DIR}/libSDL3.so" ./lib/
    cd ./lib/
    ln -sf libSDL3.so libSDL3.so.0
    cd ..
    echo "SDL3 libraries bundled successfully"
else
    echo "Error: SDL3 library directory not found at $SDL3_LIB_DIR"
    exit 1
fi
