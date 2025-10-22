#!/bin/bash

# for Linux

set -x

cd "$1"

mkdir -p lib

# We now use a pre built SDL2 library, not the package manager one for cross distro compiling
SDL2_LIB_DIR="../../../contrib/systemIV/contrib/SDL2-2.32.10/linux/lib"

if [ -d "$SDL2_LIB_DIR" ]; then
    echo "Bundling SDL2 libraries..."
    cp "${SDL2_LIB_DIR}/libSDL2-2.0.so" ./lib/
    cd ./lib/
    ln -sf libSDL2-2.0.so libSDL2-2.0.so.0
    ln -sf libSDL2-2.0.so libSDL2.so
    cd ..
    echo "SDL2 libraries bundled successfully"
else
    echo "Error: SDL2 library directory not found at $SDL2_LIB_DIR"
    exit 1
fi
