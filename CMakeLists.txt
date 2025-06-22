cmake_minimum_required(VERSION 3.16)
project(Defcon VERSION 1.0)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find PkgConfig early since it's used for cross-platform library detection
find_package(PkgConfig QUIET)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/result/Debug")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/result/Release")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
SET(ROOT_PATH ".")
SET(SYSTEMIV_PATH
    "${CMAKE_SOURCE_DIR}/contrib/systemIV"
    CACHE PATH "Location of the SystemIV base"
)

set(CMAKE_CONFIGURATION_TYPES 
    "Release"
    "Debug"
    CACHE STRING "" FORCE
)

# new and improved system for including the recording parsing code without needing to uncomment it
option(ENABLE_RECORDING_PARSING "Enable recording parsing functionality" ON)

# Emscripten Replay Viewer option - shows only recording playback interface
option(ENABLE_EMSCRIPTEN_REPLAY_VIEWER "Enable Emscripten Replay Viewer mode (recording playback only)" ON)

# is the recording parsing option enabled?
if(ENABLE_RECORDING_PARSING)
    add_definitions(-DRECORDING_PARSING=1)
    message(STATUS "Recording parsing enabled")
else()
    add_definitions(-DRECORDING_PARSING=0)
    message(STATUS "Recording parsing disabled")
endif()

# is the replay viewer option enabled?
if(ENABLE_EMSCRIPTEN_REPLAY_VIEWER)
    # Replay viewer requires recording parsing, so force it on
    set(ENABLE_RECORDING_PARSING ON CACHE BOOL "Enable recording parsing functionality" FORCE)
    add_definitions(-DRECORDING_PARSING=1)
    add_definitions(-DEMSCRIPTEN_REPLAY_VIEWER=1)
    message(STATUS "Emscripten Replay Viewer mode enabled (recording parsing auto-enabled)")
else()
    add_definitions(-DEMSCRIPTEN_REPLAY_VIEWER=0)
    message(STATUS "Emscripten Replay Viewer mode disabled")
endif()

# GLEW Configuration
set(GLEW_PATH "${CMAKE_SOURCE_DIR}/contrib/glew")

# Platform-specific GLEW library paths
if(MSVC)
    set(GLEW_LIBRARY "${GLEW_PATH}/lib/Release/Win32/glew32s.lib")
elseif(APPLE)
    # On macOS, try to find system GLEW first, fallback to our version
    find_library(GLEW_LIBRARY NAMES GLEW glew libglew.a
        PATHS ${GLEW_PATH}/lib/Release/macOS /usr/local/lib /opt/homebrew/lib
        NO_DEFAULT_PATH)
    if(NOT GLEW_LIBRARY)
        set(GLEW_LIBRARY "${GLEW_PATH}/lib/Release/macOS/libglew.a")
    endif()
else()
    # On Linux, try multiple approaches to find GLEW
    # First try pkg-config if available
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(GLEW glew)
        if(GLEW_FOUND)
            set(GLEW_LIBRARY ${GLEW_LIBRARIES})
            message(STATUS "Found GLEW via pkg-config: ${GLEW_LIBRARY}")
        endif()
    endif()
    
    if(NOT GLEW_FOUND)
        # Try to find system GLEW manually
        find_library(GLEW_LIBRARY NAMES GLEW glew
            PATHS /usr/lib/x86_64-linux-gnu /usr/local/lib /usr/lib
            NO_DEFAULT_PATH)
        if(NOT GLEW_LIBRARY)
            # Fallback to our bundled version
            set(GLEW_LIBRARY "${GLEW_PATH}/lib/Release/Linux/libglew.a")
            message(STATUS "Using bundled GLEW library: ${GLEW_LIBRARY}")
        else()
            message(STATUS "Found system GLEW library: ${GLEW_LIBRARY}")
        endif()
    endif()
endif()

# Include directories
include_directories(
    ${ROOT_PATH}/source
    ${ROOT_PATH}/source/app
    ${SYSTEMIV_PATH}
    ${SYSTEMIV_PATH}/contrib
    ${SYSTEMIV_PATH}/contrib/unrar
    ${SYSTEMIV_PATH}/contrib/binreloc
    ${SYSTEMIV_PATH}/linux/include
    ${SYSTEMIV_PATH}/lib/gucci
    ${SYSTEMIV_PATH}/lib/netlib
    ${SYSTEMIV_PATH}/lib/eclipse
    ${GLEW_PATH}/include
)

set(RESOURCES_DIR "${ROOT_PATH}/resources")
set(RESOURCE_FILES
    "${RESOURCES_DIR}/appicon.ico"
    "${RESOURCES_DIR}/nuke.ico"
    "${RESOURCES_DIR}/sub.ico"
    "${RESOURCES_DIR}/timer.ico"
    "${RESOURCES_DIR}/trayicon.ico"
    "${RESOURCES_DIR}/victory.ico"
    "${RESOURCES_DIR}/Script1.rc"
)

# WebAssembly/Emscripten specific configuration
if(EMSCRIPTEN)
    message(STATUS "Configuring for WebAssembly/Emscripten build")
    
    # Force enable recording parsing for WebAssembly builds
    set(ENABLE_RECORDING_PARSING ON CACHE BOOL "Enable recording parsing functionality" FORCE)
    add_definitions(-DRECORDING_PARSING=1)
    
    # WebAssembly-specific definitions
    add_definitions(-DTARGET_EMSCRIPTEN -DWASM_BUILD)
    add_definitions(-DTARGET_OS_LINUX -DENABLE_BINRELOC)
    add_definitions(-DSDL_MAIN_HANDLED -DGUI -DSILENT -DGLEW_STATIC)
    
    # WebAssembly debug output control (only enabled for Debug builds)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        #add_definitions(-DEMSCRIPTEN_UNIT_TESTBED)
        #add_definitions(-DEMSCRIPTEN_NETWORK_TESTBED)
        #add_definitions(-DEMSCRIPTEN_SOUND_TESTBED)
        #add_definitions(-DEMSCRIPTEN_PLAYBACK_TESTBED)
        #add_definitions(-DEMSCRIPTEN_DEBUG)
    endif()

    if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_definitions(-DEMSCRIPTEN_NO_SOUND)
    #add_definitions(-DEMSCRIPTEN_UNIT_TESTBED)
    #add_definitions(-DEMSCRIPTEN_NETWORK_TESTBED)
    #add_definitions(-DEMSCRIPTEN_SOUND_TESTBED)
    #add_definitions(-DEMSCRIPTEN_PLAYBACK_TESTBED)
    #add_definitions(-DEMSCRIPTEN_DEBUG)
endif()
    
    # Core Emscripten settings
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s USE_SDL=2")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s USE_WEBGL2=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s FULL_ES3=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s FILESYSTEM=1 -s FORCE_FILESYSTEM=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w -fpermissive")
    
    # SIMD Vectorization (2-30x performance boost for math operations)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msimd128")
    
    # Advanced Memory Management (significant performance improvement)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        # Fixed memory size for maximum performance (no growth overhead)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s INITIAL_MEMORY=512MB")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s ALLOW_MEMORY_GROWTH=0")
        # Use mimalloc for better multithreaded performance
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s MALLOC=mimalloc")
    else()
        # Keep memory growth for debug builds
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s ALLOW_MEMORY_GROWTH=1 -s MAXIMUM_MEMORY=1024MB")
    endif()
    
    # Advanced Compiler Optimizations
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        # Graphics and math-specific optimizations (safe for recording parsing)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffast-math")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rounding-math")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-signaling-nans")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-trapping-math")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s ENVIRONMENT=web,worker")
        # Disable ALL aggressive optimizations that might affect parsing
        #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s AGGRESSIVE_VARIABLE_ELIMINATION=1")
        #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s ELIMINATE_DUPLICATE_FUNCTIONS=1")
        # Keep loop optimizations disabled to be safe
        #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -funroll-loops")
        #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvectorize")
        # Enable exception handling for debugging
        #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s DISABLE_EXCEPTION_CATCHING=1")
        # Keep debug output enabled
        #add_definitions(-DNO_DEBUG_OUTPUT)
    endif()
    
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s USE_PTHREADS=1")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s PTHREAD_POOL_SIZE=4")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s PTHREAD_POOL_SIZE_STRICT=0")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s SHARED_MEMORY=1")
    endif()
    
    # WebGL Performance Optimizations
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s GL_ENABLE_GET_PROC_ADDRESS=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s GL_SUPPORT_AUTOMATIC_ENABLE_EXTENSIONS=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s GL_SUPPORT_SIMPLE_ENABLE_EXTENSIONS=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s GL_POOL_TEMP_BUFFERS=1")
    
    # GPU-Specific Performance Optimizations (safe for recording parsing)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s GL_PREINITIALIZED_CONTEXT=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s GL_EXPLICIT_UNIFORM_LOCATION=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s GL_EXPLICIT_UNIFORM_BINDING=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s GL_TRACK_ERRORS=0")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s GL_UNSAFE_OPTS=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s LEGACY_GL_EMULATION=0")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s GL_FFP_ONLY=0")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s GL_EMULATE_GLES_VERSION_STRING_FORMAT=0")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s FULL_ES2=1")
    # Enable WebGL-specific optimizations for better GPU utilization
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s MIN_WEBGL_VERSION=2")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s MAX_WEBGL_VERSION=2")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s WEBGL2_BACKWARDS_COMPATIBILITY_EMULATION=0")
    # Graphics-specific compiler optimizations (don't affect I/O or parsing)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s PRECISE_F32=0")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s TEXTDECODER=0")
    
    # Audio Performance
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s USE_VORBIS=1 -s USE_OGG=1")
    
    # Stack size optimization
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s STACK_SIZE=8MB")
    
    # Debug-specific settings
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s ASSERTIONS=2 -s SAFE_HEAP=1")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s STACK_OVERFLOW_CHECK=2")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s GL_ASSERTIONS=1")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s GL_DEBUG=1")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g4 --source-map-base http://localhost:8000/")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s DISABLE_EXCEPTION_CATCHING=0")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s EXCEPTION_DEBUG=1")
        # Remove optimization for better debugging
        string(REPLACE "-O3" "-O0" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
        string(REPLACE "-DNDEBUG" "-D_DEBUG" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    endif()
    
    # Platform-specific files for WebAssembly
    set(PLATFORM_SPECIFIC_FILES
        ${ROOT_PATH}/source/lib/spawn.cpp
        ${SYSTEMIV_PATH}/lib/gucci/input_sdl.cpp
        ${SYSTEMIV_PATH}/lib/gucci/window_manager_sdl.cpp
        ${SYSTEMIV_PATH}/lib/sound/sound_library_2d_sdl.cpp
        ${SYSTEMIV_PATH}/lib/sound/sound_library_webassembly.cpp
        ${SYSTEMIV_PATH}/lib/debug_utils_wasm.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_lib_wasm.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_mutex_wasm.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_thread_wasm.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_socket_wasm.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_socket_listener_wasm.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_socket_session_wasm.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_udp_packet_wasm.cpp
        ${SYSTEMIV_PATH}/lib/netlib/condition_wasm.cpp
        ${SYSTEMIV_PATH}/contrib/binreloc/binreloc.c
        ${SYSTEMIV_PATH}/contrib/unrar/log.cpp
    )
    
    set(EXE_TYPE)
    
    # Files to exclude for WebAssembly (mainly networking and platform-specific)
    set(FILES_TO_EXCLUDE
        # Windows-specific files
        ${ROOT_PATH}/source/app/trayicon.cpp
        ${SYSTEMIV_PATH}/lib/gucci/input_win32.cpp
        ${SYSTEMIV_PATH}/lib/gucci/win32library.cpp
        ${SYSTEMIV_PATH}/lib/gucci/window_manager_win32.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_mutex_win32.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_thread_win32.cpp
        ${SYSTEMIV_PATH}/lib/netlib/condition_win32.cpp
        ${SYSTEMIV_PATH}/lib/netlib/condition_winxp.cpp
        ${SYSTEMIV_PATH}/lib/sound/sound_library_3d_dsound.cpp
        
        # macOS-specific files
        ${ROOT_PATH}/source/app/macutil.cpp
        ${ROOT_PATH}/source/app/dockicon.mm
        
        # Linux-specific files that don't apply to WebAssembly
        ${ROOT_PATH}/source/lib/debug_utils_linux.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_mutex_linux.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_thread_linux.cpp
        
        # Original networking files that don't work in WebAssembly
        ${SYSTEMIV_PATH}/lib/netlib/net_lib.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_socket.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_socket_listener.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_socket_session.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_udp_packet.cpp
        
        # Debug utils uses execinfo.h which doesn't exist in WebAssembly
        ${SYSTEMIV_PATH}/lib/debug_utils.cpp
        
        # Condition variables use pthread which don't work properly in WebAssembly
        ${SYSTEMIV_PATH}/lib/netlib/condition_osxlinux.cpp
        
        # Desktop sound systems not needed for WebAssembly (we have our own WebAssembly implementation)
        ${SYSTEMIV_PATH}/lib/sound/sound_library_3d_software.cpp
    )

elseif(MSVC)
    set(CMAKE_GENERATOR_PLATFORM Win32)
    add_definitions(-D_WINSOCK_DEPRECATED_NO_WARNINGS -D_CRT_SECURE_NO_WARNINGS)
    enable_language(RC)
    add_definitions(-D_WIN32 -DWIN32 -DTARGET_MSVC -DGLEW_STATIC)
    set(CMAKE_GENERATOR_TOOLSET "v143")  # Updated to match your VS version
    set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} /I\"${RESOURCES_DIR}\"")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP /EHsc /Zm2000 /Zf /D_SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS /NODEFAULTLIB:LIBCMT")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4530 /wd4018 /wd4267 /wd4244 /wd4305 /wd4101 /wd4099 /wd26495 /wd4005 /wd4006 /wd4996 /wd4800 /wd4251")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4244 /wd4305 /wd4005 /wd4996")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /IGNORE:4006")
    set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} /IGNORE:4006")
    
    set(PLATFORM_SPECIFIC_FILES
        ${ROOT_PATH}/source/app/trayicon.cpp
        ${SYSTEMIV_PATH}/lib/gucci/input_win32.cpp
        ${SYSTEMIV_PATH}/lib/gucci/win32library.cpp
        ${SYSTEMIV_PATH}/lib/gucci/window_manager_win32.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_mutex_win32.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_thread_win32.cpp
        ${SYSTEMIV_PATH}/lib/netlib/condition_win32.cpp
        ${SYSTEMIV_PATH}/lib/netlib/condition_winxp.cpp
        ${SYSTEMIV_PATH}/lib/sound/sound_library_3d_dsound.cpp
    )
    
    set(EXE_TYPE WIN32)
    
    set(FILES_TO_EXCLUDE
        ${ROOT_PATH}/source/lib/debug_utils_linux.cpp 
        ${ROOT_PATH}/source/lib/spawn.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_mutex_linux.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_thread_linux.cpp
        ${SYSTEMIV_PATH}/lib/netlib/condition_osxlinux.cpp
        ${SYSTEMIV_PATH}/lib/gucci/input_sdl.cpp
        ${SYSTEMIV_PATH}/lib/gucci/window_manager_sdl.cpp
        ${SYSTEMIV_PATH}/lib/sound/sound_library_2d_sdl.cpp
        ${ROOT_PATH}/source/app/macutil.cpp
        ${ROOT_PATH}/source/app/dockicon.mm
    )
    
elseif(APPLE) 
    # macOS configurations
    set(CMAKE_MACOSX_RPATH 1)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS deployment version")
    set(MACOS_FRAMEWORKS_DIR "${SYSTEMIV_PATH}/contrib/macosx/Frameworks")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -F${MACOS_FRAMEWORKS_DIR}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -F${MACOS_FRAMEWORKS_DIR}")

    include_directories(
        ${MACOS_FRAMEWORKS_DIR}
        ${MACOS_FRAMEWORKS_DIR}/SDL2.framework
        ${MACOS_FRAMEWORKS_DIR}/SDL2.framework/Headers
        ${MACOS_FRAMEWORKS_DIR}/Ogg.framework/Headers
        ${MACOS_FRAMEWORKS_DIR}/Vorbis.framework/Headers
    )

    add_definitions(-DSDL_MAIN_HANDLED)
    add_definitions(-D_DARWIN -DMACOS -DTARGET_OS_MACOSX -DGUI -DSILENT -DGLEW_STATIC)
    
    if(NOT CMAKE_OSX_ARCHITECTURES)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
            set(CMAKE_OSX_ARCHITECTURES "arm64")
            add_definitions(-DARCH_ARM64)
        else()
            set(CMAKE_OSX_ARCHITECTURES "x86_64")
            add_definitions(-DARCH_X86_64)
        endif()
    else()
        if(CMAKE_OSX_ARCHITECTURES MATCHES "arm64")
            add_definitions(-DARCH_ARM64)
        endif()
        if(CMAKE_OSX_ARCHITECTURES MATCHES "x86_64")
            add_definitions(-DARCH_X86_64)
        endif()
    endif()
    
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-deprecated-declarations -Wno-error=format-security -fpermissive")
    set(PLATFORM_SPECIFIC_FILES
        ${ROOT_PATH}/source/lib/spawn.cpp
        ${ROOT_PATH}/source/app/macutil.cpp
        ${ROOT_PATH}/source/app/dockicon.mm
        ${SYSTEMIV_PATH}/lib/gucci/input_sdl.cpp
        ${SYSTEMIV_PATH}/contrib/unrar/log.cpp
        ${SYSTEMIV_PATH}/contrib/binreloc/binreloc.c
        ${SYSTEMIV_PATH}/lib/netlib/net_mutex_linux.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_thread_linux.cpp
        ${SYSTEMIV_PATH}/lib/gucci/window_manager_sdl.cpp
        ${SYSTEMIV_PATH}/lib/sound/sound_library_2d_sdl.cpp
        ${SYSTEMIV_PATH}/lib/netlib/condition_osxlinux.cpp
        ${MACOS_SPECIFIC_SOURCES}
    )
    
    set_source_files_properties(${ROOT_PATH}/source/app/dockicon.mm PROPERTIES COMPILE_FLAGS "-x objective-c++")
    set_source_files_properties(${MACOS_SPECIFIC_SOURCES} PROPERTIES COMPILE_FLAGS "-x objective-c++")
    
    set(EXE_TYPE MACOSX_BUNDLE)
    set(FILES_TO_EXCLUDE
        ${ROOT_PATH}/source/lib/debug_utils_linux.cpp
        ${ROOT_PATH}/source/app/trayicon.cpp
        ${SYSTEMIV_PATH}/lib/gucci/input_win32.cpp
        ${SYSTEMIV_PATH}/lib/gucci/win32library.cpp
        ${SYSTEMIV_PATH}/lib/gucci/window_manager_win32.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_mutex_win32.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_thread_win32.cpp
        ${SYSTEMIV_PATH}/lib/netlib/condition_win32.cpp
        ${SYSTEMIV_PATH}/lib/netlib/condition_winxp.cpp
        ${SYSTEMIV_PATH}/lib/sound/sound_library_3d_dsound.cpp
    )

else()
    find_package(SDL2 REQUIRED) 
    find_package(OpenGL REQUIRED)
    
    # Use desktop OpenGL with GLEW instead of OpenGL ES
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(VORBIS REQUIRED vorbisfile vorbis ogg)
    else()
        message(FATAL_ERROR "pkg-config is required for Linux builds but was not found")
    endif()
    
    add_definitions(-DTARGET_OS_LINUX -DENABLE_BINRELOC -DGUI -DSILENT -DGLEW_STATIC)
    
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-deprecated-declarations -Wno-error=format-security -fpermissive")
    set(PLATFORM_SPECIFIC_FILES
        ${ROOT_PATH}/source/lib/debug_utils_linux.cpp
        ${ROOT_PATH}/source/lib/spawn.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_mutex_linux.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_thread_linux.cpp
        ${SYSTEMIV_PATH}/lib/netlib/condition_osxlinux.cpp
        ${SYSTEMIV_PATH}/lib/gucci/input_sdl.cpp
        ${SYSTEMIV_PATH}/lib/gucci/window_manager_sdl.cpp
        ${SYSTEMIV_PATH}/lib/sound/sound_library_2d_sdl.cpp
        ${SYSTEMIV_PATH}/contrib/binreloc/binreloc.c
        ${SYSTEMIV_PATH}/contrib/unrar/log.cpp
    )
    
    set(EXE_TYPE)
    set(FILES_TO_EXCLUDE
        ${ROOT_PATH}/source/app/trayicon.cpp
        ${SYSTEMIV_PATH}/lib/gucci/input_win32.cpp
        ${SYSTEMIV_PATH}/lib/gucci/win32library.cpp
        ${SYSTEMIV_PATH}/lib/gucci/window_manager_win32.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_mutex_win32.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_thread_win32.cpp
        ${SYSTEMIV_PATH}/lib/netlib/condition_win32.cpp
        ${SYSTEMIV_PATH}/lib/netlib/condition_winxp.cpp
        ${SYSTEMIV_PATH}/lib/sound/sound_library_3d_dsound.cpp
        ${ROOT_PATH}/source/app/macutil.cpp
        ${ROOT_PATH}/source/app/dockicon.mm
    )
    
    set(CMAKE_INSTALL_RPATH "$ORIGIN")
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--no-as-needed")
endif()

foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${CONFIG} CONFIG_UPPER)

    if(MSVC)
        set(CMAKE_CXX_FLAGS_${CONFIG_UPPER} "${CMAKE_CXX_FLAGS_${CONFIG_UPPER}} /MP")
    
        if(${CONFIG} MATCHES "Debug")
            string(APPEND CMAKE_CXX_FLAGS_${CONFIG_UPPER} " /Od /Zi /MDd /RTC1 /GS /EHsc /FC /analyze /Z7")
            set(CMAKE_C_FLAGS_${CONFIG_UPPER} "${CMAKE_C_FLAGS_${CONFIG_UPPER}} /MDd -D_DEBUG -DTARGET_MSVC")
            set(CMAKE_CXX_FLAGS_${CONFIG_UPPER} "${CMAKE_CXX_FLAGS_${CONFIG_UPPER}} /MDd -D_DEBUG -DTARGET_MSVC")
        else()
            string(APPEND CMAKE_CXX_FLAGS_${CONFIG_UPPER} " /MD /O2")
            set(CMAKE_C_FLAGS_${CONFIG_UPPER} "${CMAKE_C_FLAGS_${CONFIG_UPPER}} /MD -DNDEBUG -DTARGET_MSVC")
            set(CMAKE_CXX_FLAGS_${CONFIG_UPPER} "${CMAKE_CXX_FLAGS_${CONFIG_UPPER}} /MD -DNDEBUG -DTARGET_MSVC")
        endif()
    else()
       if(${CONFIG} MATCHES "Debug")
            set(CMAKE_C_FLAGS_${CONFIG_UPPER} "${CMAKE_C_FLAGS_${CONFIG_UPPER}} -g -O0 -D_DEBUG")
            set(CMAKE_CXX_FLAGS_${CONFIG_UPPER} "${CMAKE_CXX_FLAGS_${CONFIG_UPPER}} -g -O0 -D_DEBUG")
        else()
            set(CMAKE_C_FLAGS_${CONFIG_UPPER} "${CMAKE_C_FLAGS_${CONFIG_UPPER}} -O3 -DNDEBUG")
            set(CMAKE_CXX_FLAGS_${CONFIG_UPPER} "${CMAKE_CXX_FLAGS_${CONFIG_UPPER}} -O3 -DNDEBUG")
        endif()
    endif()
endforeach()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_TESTS "${CMAKE_BINARY_DIR}/tests")
set(TEST_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

if(MSVC)
    set(MAIN_EXE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
elseif(APPLE)
    set(MAIN_EXE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/result/$<CONFIG>")
else()
    set(MAIN_EXE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/result/$<CONFIG>")
endif()

if(MSVC)
    set(DATA_SOURCE_DIR "${CMAKE_SOURCE_DIR}/localisation")
elseif(APPLE)
    set(DATA_SOURCE_DIR "${CMAKE_SOURCE_DIR}/localisation")
else()
    set(DATA_SOURCE_DIR "${CMAKE_SOURCE_DIR}/localisation")
endif()

set(DEFCON_SOURCE
    ${ROOT_PATH}/source/defcon.cpp
    ${ROOT_PATH}/source/curves.cpp
    ${ROOT_PATH}/source/random_object.cpp
    ${ROOT_PATH}/source/app/tutorial.cpp
    ${ROOT_PATH}/source/app/tutorial_defcon.cpp
    ${ROOT_PATH}/source/app/modsystem.cpp
    ${ROOT_PATH}/source/app/defcon_soundinterface.cpp
    ${ROOT_PATH}/source/app/version_manager.cpp
    ${ROOT_PATH}/source/app/app.cpp
    ${ROOT_PATH}/source/app/game.cpp
    ${ROOT_PATH}/source/app/statusicon.cpp
    ${ROOT_PATH}/source/app/x11icon.cpp
    ${ROOT_PATH}/source/app/game_history.cpp
    ${ROOT_PATH}/source/lib/multiline_text.cpp
    ${ROOT_PATH}/source/lib/universal_include.cpp
    ${ROOT_PATH}/source/lib/domain_protection.cpp
    ${ROOT_PATH}/source/renderer/map_renderer.cpp
    ${ROOT_PATH}/source/renderer/animated_icon.cpp
    ${ROOT_PATH}/source/renderer/lobby_renderer.cpp
    ${ROOT_PATH}/source/world/radarstation.cpp
    ${ROOT_PATH}/source/world/movingobject.cpp
    ${ROOT_PATH}/source/world/team.cpp
    ${ROOT_PATH}/source/world/date.cpp
    ${ROOT_PATH}/source/world/explosion.cpp
    ${ROOT_PATH}/source/world/fighter.cpp
    ${ROOT_PATH}/source/world/battleship.cpp
    ${ROOT_PATH}/source/world/silo.cpp
    ${ROOT_PATH}/source/world/gunfire.cpp
    ${ROOT_PATH}/source/world/saucer.cpp
    ${ROOT_PATH}/source/world/whiteboard.cpp
    ${ROOT_PATH}/source/world/world.cpp
    ${ROOT_PATH}/source/world/worldobject.cpp
    ${ROOT_PATH}/source/world/blip.cpp
    ${ROOT_PATH}/source/world/sub.cpp
    ${ROOT_PATH}/source/world/earthdata.cpp
    ${ROOT_PATH}/source/world/radargrid.cpp
    ${ROOT_PATH}/source/world/airbase.cpp
    ${ROOT_PATH}/source/world/votingsystem.cpp
    ${ROOT_PATH}/source/world/city.cpp
    ${ROOT_PATH}/source/world/carrier.cpp
    ${ROOT_PATH}/source/world/fleet.cpp
    ${ROOT_PATH}/source/world/nuke.cpp
    ${ROOT_PATH}/source/world/node.cpp
    ${ROOT_PATH}/source/world/depthcharge.cpp
    ${ROOT_PATH}/source/world/bomber.cpp
    ${ROOT_PATH}/source/world/tornado.cpp
    ${ROOT_PATH}/source/network/ClientToServer.cpp
    ${ROOT_PATH}/source/network/Server.cpp
    ${ROOT_PATH}/source/network/ServerToClient.cpp
    ${ROOT_PATH}/source/interface/mod_window.cpp
    ${ROOT_PATH}/source/interface/badkey_window.cpp
    ${ROOT_PATH}/source/interface/network_window.cpp
    ${ROOT_PATH}/source/interface/connecting_window.cpp
    ${ROOT_PATH}/source/interface/tutorial_window.cpp
    ${ROOT_PATH}/source/interface/fading_window.cpp
    ${ROOT_PATH}/source/interface/profile_window.cpp
    ${ROOT_PATH}/source/interface/chat_window.cpp
    ${ROOT_PATH}/source/interface/lobby_window.cpp
    ${ROOT_PATH}/source/interface/interface.cpp
    ${ROOT_PATH}/source/interface/mainmenu.cpp
    ${ROOT_PATH}/source/interface/playback_control_window.cpp
    ${ROOT_PATH}/source/interface/side_panel.cpp
    ${ROOT_PATH}/source/interface/toolbar.cpp
    ${ROOT_PATH}/source/interface/authkey_window.cpp
    ${ROOT_PATH}/source/interface/alliances_window.cpp
    ${ROOT_PATH}/source/interface/resynchronise_window.cpp
    ${ROOT_PATH}/source/interface/fleet_placement_icon.cpp
    ${ROOT_PATH}/source/interface/casualties_window.cpp
    ${ROOT_PATH}/source/interface/serverbrowser_window.cpp
    ${ROOT_PATH}/source/interface/serverdetails_window.cpp
    ${ROOT_PATH}/source/interface/debug_window.cpp
    ${ROOT_PATH}/source/interface/demo_window.cpp
    ${ROOT_PATH}/source/interface/whiteboard_panel.cpp
    ${ROOT_PATH}/source/interface/worldstatus_window.cpp
    ${ROOT_PATH}/source/interface/worldobject_window.cpp
    ${ROOT_PATH}/source/interface/info_window.cpp
    ${ROOT_PATH}/source/interface/placement_icon.cpp
)

# include the recording parsing files if we are enabled
if(ENABLE_RECORDING_PARSING)
    list(APPEND DEFCON_SOURCE
        #${ROOT_PATH}/source/app/synctestrecordings.cpp
        ${ROOT_PATH}/source/network/recordingparser.cpp
        ${ROOT_PATH}/source/interface/recording_selection.cpp
    )
endif()

set(SYSTEMIV_SOURCE
    ${SYSTEMIV_PATH}/interface/components/checkbox.cpp
    ${SYSTEMIV_PATH}/interface/components/core.cpp
    ${SYSTEMIV_PATH}/interface/components/drop_down_menu.cpp
    ${SYSTEMIV_PATH}/interface/components/filedialog.cpp
    ${SYSTEMIV_PATH}/interface/components/inputfield.cpp
    ${SYSTEMIV_PATH}/interface/components/message_dialog.cpp
    ${SYSTEMIV_PATH}/interface/components/scrollbar.cpp
    ${SYSTEMIV_PATH}/interface/options/screenoptions_window.cpp
    ${SYSTEMIV_PATH}/interface/options/soundoptions_window.cpp
    ${SYSTEMIV_PATH}/interface/soundsystem/soundeditor_window.cpp
    ${SYSTEMIV_PATH}/interface/soundsystem/soundstats_window.cpp
    ${SYSTEMIV_PATH}/interface/soundsystem/soundparameter_window.cpp
    ${SYSTEMIV_PATH}/interface/styleeditor_window.cpp
    ${SYSTEMIV_PATH}/lib/eclipse/eclbutton.cpp
    ${SYSTEMIV_PATH}/lib/eclipse/eclwindow.cpp
    ${SYSTEMIV_PATH}/lib/eclipse/eclipse.cpp
    ${SYSTEMIV_PATH}/lib/filesys/binary_stream_readers.cpp
    ${SYSTEMIV_PATH}/lib/filesys/file_system.cpp
    ${SYSTEMIV_PATH}/lib/filesys/filesys_utils.cpp
    ${SYSTEMIV_PATH}/lib/filesys/text_file_writer.cpp
    ${SYSTEMIV_PATH}/lib/filesys/text_stream_readers.cpp
    ${SYSTEMIV_PATH}/lib/math/fixed_float.cpp
    ${SYSTEMIV_PATH}/lib/math/fixed_64.cpp
    ${SYSTEMIV_PATH}/lib/math/hash.cpp
    ${SYSTEMIV_PATH}/lib/math/math_utils.cpp
    ${SYSTEMIV_PATH}/lib/math/random_number.cpp
    ${SYSTEMIV_PATH}/lib/math/vector2.cpp
    ${SYSTEMIV_PATH}/lib/math/vector3.cpp
    ${SYSTEMIV_PATH}/lib/metaserver/authentication.cpp
    ${SYSTEMIV_PATH}/lib/metaserver/matchmaker.cpp
    ${SYSTEMIV_PATH}/lib/metaserver/metaserver.cpp
    ${SYSTEMIV_PATH}/lib/gucci/input.cpp
    ${SYSTEMIV_PATH}/lib/gucci/window_manager.cpp
    ${SYSTEMIV_PATH}/lib/netlib/net_lib.cpp
    ${SYSTEMIV_PATH}/lib/netlib/net_socket.cpp
    ${SYSTEMIV_PATH}/lib/netlib/net_socket_listener.cpp
    ${SYSTEMIV_PATH}/lib/netlib/net_socket_session.cpp
    ${SYSTEMIV_PATH}/lib/netlib/net_udp_packet.cpp
    ${SYSTEMIV_PATH}/lib/netlib/thread_controller.cpp
    ${SYSTEMIV_PATH}/lib/render/colour.cpp
    ${SYSTEMIV_PATH}/lib/render/renderer.cpp
    ${SYSTEMIV_PATH}/lib/render/renderer3d.cpp
    ${SYSTEMIV_PATH}/lib/render/styletable.cpp
    ${SYSTEMIV_PATH}/lib/resource/bitmap.cpp
    ${SYSTEMIV_PATH}/lib/resource/bitmapfont.cpp
    ${SYSTEMIV_PATH}/lib/resource/image.cpp
    ${SYSTEMIV_PATH}/lib/resource/resource.cpp
    ${SYSTEMIV_PATH}/lib/sound/sound_blueprint_manager.cpp
    ${SYSTEMIV_PATH}/lib/sound/sound_filter.cpp
    ${SYSTEMIV_PATH}/lib/sound/sound_instance.cpp
    ${SYSTEMIV_PATH}/lib/sound/sound_library_2d.cpp
    ${SYSTEMIV_PATH}/lib/sound/sound_library_3d.cpp
    ${SYSTEMIV_PATH}/lib/sound/sound_library_3d_software.cpp
    ${SYSTEMIV_PATH}/lib/sound/sound_parameter.cpp
    ${SYSTEMIV_PATH}/lib/sound/sound_sample_bank.cpp
    ${SYSTEMIV_PATH}/lib/sound/sound_sample_decoder.cpp
    ${SYSTEMIV_PATH}/lib/sound/soundsystem.cpp
    ${SYSTEMIV_PATH}/lib/sound/soundsystem_interface.cpp
    ${SYSTEMIV_PATH}/lib/tosser/directory.cpp
    ${SYSTEMIV_PATH}/lib/debug_utils.cpp
    ${SYSTEMIV_PATH}/lib/hi_res_time.cpp
    ${SYSTEMIV_PATH}/lib/language_table.cpp
    ${SYSTEMIV_PATH}/lib/preferences.cpp
    ${SYSTEMIV_PATH}/lib/profiler.cpp
    ${SYSTEMIV_PATH}/lib/string_utils.cpp
)

set(UNRAR_SOURCE 
    ${SYSTEMIV_PATH}/contrib/unrar/archive.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/arcread.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/cmddata.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/consio.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/crc.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/crypt.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/encname.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/errhnd.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/extinfo.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/extract.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/filcreat.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/file.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/filefn.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/filestr.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/find.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/getbits.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/global.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/int64.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/isnt.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/list.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/match.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/options.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/pathfn.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/rarresource.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/rarvm.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/rarvmtbl.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/rawread.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/rdwrfn.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/recvol.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/rijndael.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/rs.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/savepos.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/scantree.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/sha1.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/smallfn.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/strfn.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/strlist.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/system.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/timefn.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/ulinks.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/unicode.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/unpack.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/unrar.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/volume.cpp
)

foreach(FILE ${FILES_TO_EXCLUDE})
    list(REMOVE_ITEM DEFCON_SOURCE ${FILE})
    list(REMOVE_ITEM SYSTEMIV_SOURCE ${FILE})
    list(REMOVE_ITEM UNRAR_SOURCE ${FILE})
endforeach()

set(ALL_SOURCES ${DEFCON_SOURCE} ${SYSTEMIV_SOURCE} ${UNRAR_SOURCE} ${PLATFORM_SPECIFIC_FILES} ${RESOURCE_FILES})

add_executable(defcon ${EXE_TYPE} ${ALL_SOURCES})

if(EMSCRIPTEN)
    set_target_properties(defcon PROPERTIES
        WIN32_EXECUTABLE FALSE
        RUNTIME_OUTPUT_DIRECTORY "${MAIN_EXE_OUTPUT_DIR}"
        SUFFIX ".html"
    )
    
    # === LINKER OPTIMIZATION FLAGS ===
    
    # Core settings
    set(EMSCRIPTEN_LINK_FLAGS "-s USE_SDL=2")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s USE_WEBGL2=1")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s FULL_ES3=1")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s FILESYSTEM=1 -s FORCE_FILESYSTEM=1")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s USE_VORBIS=1 -s USE_OGG=1")
    
    # SIMD and performance optimizations
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -msimd128")
    
    # Memory optimization (release vs debug)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s INITIAL_MEMORY=512MB")
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s ALLOW_MEMORY_GROWTH=0")
        # Threading enabled for better performance
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s MALLOC=mimalloc")
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s USE_PTHREADS=1")
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s PTHREAD_POOL_SIZE=4")
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s PTHREAD_POOL_SIZE_STRICT=0")
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s SHARED_MEMORY=1")
    else()
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s ALLOW_MEMORY_GROWTH=1 -s MAXIMUM_MEMORY=1024MB")
    endif()
    
    # WebGL performance optimizations
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s GL_ENABLE_GET_PROC_ADDRESS=1")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s GL_SUPPORT_AUTOMATIC_ENABLE_EXTENSIONS=1")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s GL_POOL_TEMP_BUFFERS=1")
    
    # GPU-Specific Performance Optimizations (safe for recording parsing)
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s GL_PREINITIALIZED_CONTEXT=1")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s GL_EXPLICIT_UNIFORM_LOCATION=1")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s GL_EXPLICIT_UNIFORM_BINDING=1")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s GL_TRACK_ERRORS=0")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s GL_UNSAFE_OPTS=1")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s LEGACY_GL_EMULATION=0")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s GL_FFP_ONLY=0")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s GL_EMULATE_GLES_VERSION_STRING_FORMAT=0")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s FULL_ES2=1")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s MIN_WEBGL_VERSION=2")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s MAX_WEBGL_VERSION=2")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s WEBGL2_BACKWARDS_COMPATIBILITY_EMULATION=0")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s PRECISE_F32=0")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s TEXTDECODER=0")
    
    # Stack size optimization
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s STACK_SIZE=8MB")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s EXPORTED_FUNCTIONS=['_main','_malloc','_free','_jsCreateDspChain','_jsUpdateDspFilter','_jsRemoveDspChain','_jsConnectDspChain','_jsRequestAudioDataForProcessor','_jsFreeAudioData']")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s EXPORTED_RUNTIME_METHODS=['ccall','cwrap']")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} --preload-file ${CMAKE_SOURCE_DIR}/localisation/data@data")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} --preload-file ${CMAKE_SOURCE_DIR}/localisation/preferences.txt@preferences.txt")
    # Also include mods if they exist
    if(EXISTS "${CMAKE_SOURCE_DIR}/localisation/mods")
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} --preload-file ${CMAKE_SOURCE_DIR}/localisation/mods@mods")
    endif()
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -w")
    
    # Build-type specific optimizations
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        # Graphics-specific optimizations that don't affect recording parsing
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -O2")
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s ENVIRONMENT=web,worker")
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s MODULARIZE=0")
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s EXPORT_ES6=0")
        
        # Code obfuscation and protection (production only)
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s MINIFY_HTML=1")
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s SINGLE_FILE=0")
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s DEMANGLE_SUPPORT=0")
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -fno-rtti")
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -fvisibility=hidden")
        # Disable ALL aggressive optimizations that might affect parsing
        #set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s AGGRESSIVE_VARIABLE_ELIMINATION=1")
        #set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s ELIMINATE_DUPLICATE_FUNCTIONS=1")
        # Enable exception handling for debugging
        #set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s DISABLE_EXCEPTION_CATCHING=1")
    else()
        # Debug-specific flags
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s ASSERTIONS=2 -s SAFE_HEAP=1")
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s STACK_OVERFLOW_CHECK=2")
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s GL_ASSERTIONS=1")
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s GL_DEBUG=1")
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s DISABLE_EXCEPTION_CATCHING=0")
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s EXCEPTION_DEBUG=1")
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -g4 --source-map-base http://localhost:8000/")
        # Remove optimization for better debugging
        string(REPLACE "-O3" "-O0" EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS}")
    endif()
    
    set_target_properties(defcon PROPERTIES LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS}")
    
    # No additional libraries needed for Emscripten (SDL2, OpenGL, Vorbis provided by flags)
    
elseif(MSVC)
    set_target_properties(defcon PROPERTIES
        WIN32_EXECUTABLE TRUE
        LINK_FLAGS "/SUBSYSTEM:WINDOWS /IGNORE:4006"
        RUNTIME_OUTPUT_DIRECTORY "${MAIN_EXE_OUTPUT_DIR}"
    )
    
    target_link_libraries(defcon PRIVATE 
            "${CMAKE_SOURCE_DIR}/contrib/systemIV/contrib/oggvorbis/lib/vs2022/ogg.lib"
            "${CMAKE_SOURCE_DIR}/contrib/systemIV/contrib/oggvorbis/lib/vs2022/vorbis.lib"
            "${CMAKE_SOURCE_DIR}/contrib/systemIV/contrib/oggvorbis/lib/vs2022/vorbisfile.lib"
            "${GLEW_LIBRARY}"
        wsock32 opengl32 glu32 dsound dxguid dbghelp winmm Ws2_32
    )
    
    target_include_directories(defcon PRIVATE
        ${ROOT_PATH}/contrib/systemIV/contrib/oggvorbis/include/
        ${ROOT_PATH}/contrib/ambx1.1/inc
    )
    
elseif(APPLE)
    set_target_properties(defcon PROPERTIES
        WIN32_EXECUTABLE FALSE
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/targets/macosx/Info_cmake.plist"
        RUNTIME_OUTPUT_DIRECTORY "${MAIN_EXE_OUTPUT_DIR}"
        MACOSX_BUNDLE_ICON_FILE "Defcon.icns"
        MACOSX_BUNDLE_GUI_IDENTIFIER "uk.co.introversion.Defcon"
        MACOSX_BUNDLE_INFO_STRING "DEFCON"
        MACOSX_BUNDLE_BUNDLE_NAME "DEFCON"
        MACOSX_BUNDLE_COPYRIGHT "© Introversion Software"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        INSTALL_RPATH "@executable_path/../Frameworks"
        BUILD_WITH_INSTALL_RPATH TRUE
    )

    set_target_properties(defcon PROPERTIES
        LINK_FLAGS "-F${MACOS_FRAMEWORKS_DIR}"
    )

    target_link_libraries(defcon PRIVATE 
        "-framework SDL2" 
        "-framework Ogg" 
        "-framework Vorbis" 
        "-framework OpenGL" 
        "-framework Carbon" 
        "-framework IOKit" 
        "-framework AppKit"
        "${GLEW_LIBRARY}"
        "-liconv"
    )
    
else()
    set_target_properties(defcon PROPERTIES
        WIN32_EXECUTABLE FALSE
        RUNTIME_OUTPUT_DIRECTORY "${MAIN_EXE_OUTPUT_DIR}"
    )
    
    target_include_directories(defcon PUBLIC 
        ${SDL2_INCLUDE_DIRS}
        ${OPENGL_INCLUDE_DIRS}
    )
    target_link_libraries(defcon PRIVATE 
        SDL2main
        ${SDL2_LIBRARIES}
        ${OPENGL_LIBRARIES}
        "${GLEW_LIBRARY}"
        ${VORBIS_LIBRARIES}
        dl
        pthread
    )
endif()

target_include_directories(defcon PUBLIC
    ${CMAKE_SOURCE_DIR}/resources
    ${ROOT_PATH}/source
    ${SYSTEMIV_PATH}
    ${GLEW_PATH}/include
)

add_library(systemIV INTERFACE)
target_link_libraries(defcon PUBLIC systemIV)

if(NOT APPLE)
    target_precompile_headers(defcon PRIVATE
        ${ROOT_PATH}/source/lib/universal_include.h
    )
endif()

if(MSVC) # it is very much non-caanon to copy executables into the source directory...
  add_custom_command(TARGET defcon POST_BUILD 
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "$<TARGET_FILE:defcon>"
    "${MAIN_EXE_OUTPUT_DIR}/$<TARGET_FILE_NAME:defcon>"
)
elseif(LINUX)
    # copy data into build directory
    add_custom_command(TARGET defcon PRE_BUILD 
        COMMAND ${CMAKE_COMMAND} -E make_directory "${MAIN_EXE_OUTPUT_DIR}"
    )
    add_custom_command(TARGET defcon POST_BUILD 
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different "${DATA_SOURCE_DIR}/data" "${MAIN_EXE_OUTPUT_DIR}/data"
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${MAIN_EXE_OUTPUT_DIR}/data/sounds"
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different "${DATA_SOURCE_DIR}/mods" "${MAIN_EXE_OUTPUT_DIR}/mods"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DATA_SOURCE_DIR}/*.dat" "${MAIN_EXE_OUTPUT_DIR}/"
        COMMAND ${CMAKE_SOURCE_DIR}/targets/linux/bundle_sdl.sh ${MAIN_EXE_OUTPUT_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/targets/linux/launch_cmake.sh ${MAIN_EXE_OUTPUT_DIR}/launch.sh
    )
elseif(APPLE)
    add_custom_command(TARGET defcon PRE_BUILD 
        COMMAND ${CMAKE_COMMAND} -E make_directory "${MAIN_EXE_OUTPUT_DIR}"
    )
    set(BUNDLE_DIR ${MAIN_EXE_OUTPUT_DIR}/defcon.app/Contents)
    set(DATA_TARGET_DIR ${BUNDLE_DIR}/Resources/)
    add_custom_command(TARGET defcon POST_BUILD 
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different "${DATA_SOURCE_DIR}/data" "${DATA_TARGET_DIR}/data"
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${DATA_TARGET_DIR}/data/sounds"
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different "${DATA_SOURCE_DIR}/mods" "${DATA_TARGET_DIR}/mods"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/resources/macosx/*" "${DATA_TARGET_DIR}/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DATA_SOURCE_DIR}/*.dat" "${DATA_TARGET_DIR}/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/targets/macosx/Info_cmake.plist" "${BUNDLE_DIR}/Info.plist"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/targets/macosx/Base.lproj/SDLMain.nib" "${DATA_TARGET_DIR}/SDLMain.nib"
        COMMAND ${CMAKE_SOURCE_DIR}/targets/macosx/bundle_sdl.sh ${BUNDLE_DIR}
    )
endif()