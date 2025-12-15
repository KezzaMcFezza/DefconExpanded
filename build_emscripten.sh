#!/bin/bash

show_menu() {
    echo "Which build do you want to make?"
    echo "Type --Release or --Debug after entering your number"
    echo
    echo "1. Replay Viewer"
    echo "2. Sync Practice"
    echo
    echo "Examples: \"1 --Release\", \"2 --Debug\", \"1\", \"2\""
    read -p "Enter your choice: " choice
    
    read -r project_choice build_type <<< "$choice"
    
    if [[ -z "$build_type" ]]; then
        build_type=""
    fi
    
    case "$project_choice" in
        1) handle_replay_choice "$build_type" ;;
        2) handle_sync_choice "$build_type" ;;
        *) echo "Invalid choice. Please enter 1 or 2."
           echo
           show_menu ;;
    esac
}

handle_replay_choice() {
    local build_type="$1"
    echo
    echo "Replay Viewer selected."
    
    if [[ -z "$build_type" ]]; then
        show_replay_menu
    else
        case "${build_type,,}" in
            --Release|1) start_build "replay" "release" "Replay Viewer" ;;
            --Debug|2) start_build "replay" "debug" "Replay Viewer" ;;
            *) echo "Invalid build type. Please enter --Release or --Debug."
               echo
               show_replay_menu ;;
        esac
    fi
}

handle_sync_choice() {
    local build_type="$1"
    echo
    echo "Sync Practice selected."
    
    if [[ -z "$build_type" ]]; then
        show_sync_menu
    else
        case "${build_type,,}" in
            --Release|1) start_build "sync" "release" "Sync Practice" ;;
            --Debug|2) start_build "sync" "debug" "Sync Practice" ;;
            *) echo "Invalid build type. Please enter --Release or --Debug."
               echo
               show_sync_menu ;;
        esac
    fi
}

show_replay_menu() {
    echo "Choose build type:"
    echo "1. Release"
    echo "2. Debug"
    echo
    read -p "Enter build type (1 or 2): " build_type
    
    case "$build_type" in
        1) start_build "replay" "release" "Replay Viewer" ;;
        2) start_build "replay" "debug" "Replay Viewer" ;;
        *) echo "Invalid choice. Please enter 1 or 2."
           echo
           show_replay_menu ;;
    esac
}

show_sync_menu() {
    echo "Choose build type:"
    echo "1. Release"
    echo "2. Debug"
    echo
    read -p "Enter build type (1 or 2): " build_type
    
    case "$build_type" in
        1) start_build "sync" "release" "Sync Practice" ;;
        2) start_build "sync" "debug" "Sync Practice" ;;
        *) echo "Invalid choice. Please enter 1 or 2."
           echo
           show_sync_menu ;;
    esac
}

start_build() {
    local project_type="$1"
    local build_type="$2"
    local project_name="$3"
    
    echo
    echo "Building Defcon WebAssembly $build_type ($project_name)..."
    
    ORIGINAL_DIR="$(pwd)"
    NINJA_PATH="$ORIGINAL_DIR/tools/bin/ninja.bin"
    EMSDK_DIR="$ORIGINAL_DIR/contrib/systemIV/contrib/emsdk"
    
    if [[ ! -f "$NINJA_PATH" ]]; then
        echo "ERROR: Ninja binary not found at $NINJA_PATH"
        exit 1
    fi
    
    if [[ ! -x "$NINJA_PATH" ]]; then
        chmod +x "$NINJA_PATH"
    fi
    
    if [[ ! -d "$EMSDK_DIR/upstream/emscripten" ]]; then
        echo "Emscripten not installed. Installing and activating..."
        cd "$EMSDK_DIR"
        
        if [[ ! -f "./emsdk" ]]; then
            echo "ERROR: emsdk script not found at $EMSDK_DIR/emsdk"
            exit 1
        fi
        
        chmod +x ./emsdk
        
        echo "Installing Emscripten..."
        python3 ./emsdk.py install latest
        
        echo "Activating Emscripten..."
        python3 ./emsdk.py activate latest
    fi
    
    source "$EMSDK_DIR/emsdk_env.sh" > /dev/null 2>&1

    if [[ "$project_type" == "replay" ]]; then
        BUILD_DIR="wasm-replay-$build_type"
        CMAKE_BUILD_FLAG="-DREPLAY_VIEWER_BUILD=ON"
        CMAKE_DEFINES="-DREPLAY_VIEWER=1"
        TARGET_DIR="replay_viewer"
        UPDATE_SCRIPT="update_replay_viewer_version.py"
    else
        BUILD_DIR="wasm-sync_practice-$build_type"
        CMAKE_BUILD_FLAG="-DSYNC_PRACTICE_BUILD=ON"
        CMAKE_DEFINES="-DSYNC_PRACTICE=1 -DNOT_REPLAY_VIEWER=1"
        TARGET_DIR="sync_practice"
        UPDATE_SCRIPT="update_sync_practice_client.py"
    fi
    
    mkdir -p "build/$BUILD_DIR"
    cd "build/$BUILD_DIR"
    
    if [[ "$build_type" == "release" ]]; then
        export EMCC_OPTIMIZE_FLAGS="-O3 -flto -ffast-math"
        export EMCC_CLOSURE=1
        export EMCC_WASM_BACKEND=1
        CMAKE_BUILD_TYPE="Release"
        CMAKE_FLAGS="-O3 -flto -funroll-loops -fvectorize -DNDEBUG"
        BUILD_OUTPUT_DIR="Release"
        BUILD_QUIET="> /dev/null 2>&1"
    else
        CMAKE_BUILD_TYPE="Debug"
        CMAKE_FLAGS="-g -O0 -D_DEBUG"
        BUILD_OUTPUT_DIR="Debug"
        BUILD_QUIET=""
    fi
    
    echo "Configuring..."
    echo "Using ninja at: $NINJA_PATH"
    echo "Build directory: $(pwd)"
    echo "Original directory: $ORIGINAL_DIR"
    
    python3 "$ORIGINAL_DIR/contrib/systemIV/contrib/emsdk/upstream/emscripten/emcmake.py" cmake \
        -DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE \
        -DCMAKE_CXX_FLAGS="$CMAKE_FLAGS $CMAKE_DEFINES" \
        -DCMAKE_C_FLAGS="$CMAKE_FLAGS $CMAKE_DEFINES" \
        $CMAKE_BUILD_FLAG \
        -DCMAKE_MAKE_PROGRAM="$NINJA_PATH" \
        -G "Ninja" \
        "$ORIGINAL_DIR"
    
    if [[ $? -ne 0 ]]; then
        echo "Configuration failed."
        cd "$ORIGINAL_DIR"
        exit 1
    fi
    
    echo "Building..."
    if [[ "$build_type" == "release" ]]; then
        python3 "$ORIGINAL_DIR/contrib/systemIV/contrib/emsdk/upstream/emscripten/emmake.py" "$NINJA_PATH" 2>&1 | grep -v "warning:"
    else
        python3 "$ORIGINAL_DIR/contrib/systemIV/contrib/emsdk/upstream/emscripten/emmake.py" "$NINJA_PATH"
    fi
    BUILD_RESULT=$?
    
    cd "$ORIGINAL_DIR"
    
    if [[ $BUILD_RESULT -ne 0 ]]; then
        echo "Build failed."
        exit 1
    fi
    
    echo "Build completed successfully."
    echo "Output: $ORIGINAL_DIR/build/$BUILD_DIR/result/$BUILD_OUTPUT_DIR/"
    
    echo
    echo "Copying WebAssembly files..."
    python3 "$ORIGINAL_DIR/targets/emscripten/copy_files.py" "$project_type" "$build_type" "$ORIGINAL_DIR/build/$BUILD_DIR/result/$BUILD_OUTPUT_DIR" "$ORIGINAL_DIR/website/$TARGET_DIR"
    
    if [[ $? -ne 0 ]]; then
        echo "File copying failed!"
        exit 1
    fi
    
    echo
    echo "Updating HTML file versions automatically..."
    python3 "$ORIGINAL_DIR/targets/emscripten/$UPDATE_SCRIPT"
    
    if [[ $? -eq 0 ]]; then
        echo "HTML version update completed successfully!"
    else
        echo "ERROR: HTML version update failed. You may need to update manually."
        echo "Manual steps:"
        echo "1. Rename ${TARGET_DIR}_*.html to match the new version"
        echo "2. Update the script tag inside the HTML file"
        exit 1
    fi
    
    echo
    echo "Build complete"
    if [[ "$build_type" == "debug" ]]; then
        echo "Enjoy hell on earth"
    else
        echo "Double check it works first before you upload to the live server"
    fi
    echo "Files are in: $ORIGINAL_DIR/website/$TARGET_DIR/"
}

show_menu
