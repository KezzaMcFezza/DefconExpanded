#!/usr/bin/env bash

# for Linux

set -x

cd "$1"

mkdir -p lib

# find SDL depencency, copy it here
SDL=`ldd defcon | grep libSDL | sed -e 's/.*=> //' -e 's/ .*//'`
cp ${SDL} ./lib/

# also find potential transitive SLD2 dependency
SDL2=`ldd ${SDL} | grep libSDL | sed -e 's/.*=> //' -e 's/ .*//'`
if test -z ${SDL2}; then
    # if that fails, locate libSDL2 via pkgconfig, or take a guess
    SRC="`pkg-config --variable=libdir sdl2`" || for f in /usr/lib /usr/lib/*-linux-*; do SRC=$f; done
    for f in "${SRC}"/libSDL2-*.so.* "${SRC}"/libSDL2-*.so "${SRC}"/libSDL2.so*; do
        # and copy everything, preserving links (we can't predict what SDL12-compat will load)
        test -r $f && cp -av $f ./lib/ || true
    done
else
    # just copy
    cp ${SDL2} ./lib/
fi

# find and bundle GLEW
GLEW=`ldd defcon | grep libGLEW | sed -e 's/.*=> //' -e 's/ .*//'`
if [ -n "${GLEW}" ]; then
    echo "Found GLEW: ${GLEW}"
    cp -av ${GLEW} ./lib/
else
    # try to locate GLEW via pkg-config or common paths
    echo "GLEW not found in ldd output, searching common locations..."
    if pkg-config --exists glew; then
        GLEW_LIBDIR="`pkg-config --variable=libdir glew`"
        for f in "${GLEW_LIBDIR}"/libGLEW.so* "${GLEW_LIBDIR}"/x86_64-linux-gnu/libGLEW.so*; do
            test -r $f && cp -av $f ./lib/ && echo "Bundled: $f" || true
        done
    else
        for libdir in /usr/lib /usr/lib/x86_64-linux-gnu /usr/local/lib; do
            for f in "${libdir}"/libGLEW.so*; do
                test -r $f && cp -av $f ./lib/ && echo "Bundled: $f" || true
            done
        done
    fi
fi