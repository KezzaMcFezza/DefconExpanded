cmake_minimum_required(VERSION 3.16)
project(Defcon VERSION 1.0)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(ROOT_PATH ".")
set(SYSTEMIV_PATH "${CMAKE_SOURCE_DIR}/contrib/systemIV" CACHE PATH "Location of the SystemIV base")
set(GLAD_PATH "${CMAKE_SOURCE_DIR}/contrib/systemIV/contrib/glad")
set(HIGHWAY_PATH "${CMAKE_SOURCE_DIR}/contrib/systemIV/contrib/highway-1.3.0")
set(TINYGLTF_PATH "${CMAKE_SOURCE_DIR}/contrib/systemIV/contrib/tinygltf-2.9.7")
set(IMGUI_PATH "${CMAKE_SOURCE_DIR}/contrib/systemIV/contrib/imgui-1.92.5")
set(RESOURCES_DIR "${ROOT_PATH}/resources")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/result/Debug")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/result/Release")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_TESTS "${CMAKE_BINARY_DIR}/tests")
set(TEST_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
set(CMAKE_CONFIGURATION_TYPES "Release" "Debug" CACHE STRING "" FORCE)

find_package(PkgConfig QUIET)
find_package(OpenMP)

if(OpenMP_CXX_FOUND)
    message(STATUS "OpenMP found")
    add_definitions(-DOPENMP)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
else()
    message(STATUS "OpenMP not found")
endif()

include(CheckCXXSourceCompiles)

set(atomic_test_code "
    #include <atomic>
    #include <cstdint>
    std::atomic<uint8_t> n8(0);
    std::atomic<uint64_t> n64(0);
    int main() {
        ++n8;
        ++n64;
        return 0;
    }")

set(CMAKE_REQUIRED_QUIET_SAVE ${CMAKE_REQUIRED_QUIET})
set(CMAKE_REQUIRED_QUIET TRUE)

check_cxx_source_compiles("${atomic_test_code}" ATOMICS_LOCK_FREE)

if(NOT ATOMICS_LOCK_FREE)
    set(CMAKE_REQUIRED_LIBRARIES "-latomic")
    check_cxx_source_compiles("${atomic_test_code}" ATOMICS_IN_LIBRARY)
    set(CMAKE_REQUIRED_LIBRARIES)
    if(ATOMICS_IN_LIBRARY)
        set(HIGHWAY_NEEDS_ATOMIC_LIB TRUE)
    endif()
else()
    set(HIGHWAY_NEEDS_ATOMIC_LIB FALSE)
endif()

set(CMAKE_REQUIRED_QUIET ${CMAKE_REQUIRED_QUIET_SAVE})
unset(atomic_test_code)

message(STATUS "Google Highway SIMD library Enabled")

if(EMSCRIPTEN)
    set(GLAD_INCLUDE_DIR "${GLAD_PATH}/emscripten/include")
    set(GLAD_SOURCE_FILE "${GLAD_PATH}/emscripten/src/glad.c")
else()
    set(GLAD_INCLUDE_DIR "${GLAD_PATH}/desktop/include")
    set(GLAD_SOURCE_FILE "${GLAD_PATH}/desktop/src/glad.c")
endif()

set(REPLAY_VIEWER_VERSION "R1.30" CACHE STRING "Version prefix for replay viewer (e.g., A1.04, B1.6, R1.0)")
set(SYNC_PRACTICE_VERSION "B1.16.1" CACHE STRING "Version prefix for silo practice client (e.g., B1.0, B1.1)")

# build mode is now determined by preprocessor definitions passed via CMAKE_CXX_FLAGS
if(NOT REPLAY_VIEWER_BUILD AND NOT SYNC_PRACTICE_BUILD)
    message(STATUS "Standard build mode")
endif()

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
    ${SYSTEMIV_PATH}/contrib/crashpad/linux/include
    ${SYSTEMIV_PATH}/contrib/crashpad/linux/include/third_party/mini_chromium/mini_chromium
    ${GLAD_INCLUDE_DIR}
    ${HIGHWAY_PATH}
    ${TINYGLTF_PATH}
    ${IMGUI_PATH}
    ${IMGUI_PATH}/backends
)

set(RESOURCE_FILES
    "${RESOURCES_DIR}/appicon.ico"
    "${RESOURCES_DIR}/nuke.ico"
    "${RESOURCES_DIR}/sub.ico"
    "${RESOURCES_DIR}/timer.ico"
    "${RESOURCES_DIR}/trayicon.ico"
    "${RESOURCES_DIR}/victory.ico"
    "${RESOURCES_DIR}/Script1.rc"
)

# EMSCRIPTEN (WebAssembly)
if(EMSCRIPTEN)
    message(STATUS "Configuring for WebAssembly/Emscripten build")
    
    add_definitions(-DTARGET_EMSCRIPTEN -DWASM_BUILD -DTARGET_OS_LINUX -DENABLE_BINRELOC)
    add_definitions(-DSDL_MAIN_HANDLED -DGUI -DSILENT)
    add_definitions(-D__EMSCRIPTEN_PTHREADS__)
    add_definitions(-D_GNU_SOURCE)
    add_definitions(-DRENDERER_OPENGL)
    
    # WebAssembly debug output control (only enabled for Debug builds)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        #add_definitions(-DTOGGLE_SOUND)
        #add_definitions(-DEMSCRIPTEN_UNIT_TESTBED)
        add_definitions(-DEMSCRIPTEN_DEBUG_OUTPUT)
        add_definitions(-DEMSCRIPTEN_IMGUI)
        #add_definitions(-DEMSCRIPTEN_NETWORK_TESTBED)
        add_definitions(-DTOGGLE_SOUND_TESTBED)
        #add_definitions(-DEMSCRIPTEN_PLAYBACK_TESTBED)
        add_definitions(-DEMSCRIPTEN_DEBUG)
    endif()

    if(CMAKE_BUILD_TYPE STREQUAL "Release")
    #add_definitions(-DTOGGLE_SOUND)
    #add_definitions(-DEMSCRIPTEN_IMGUI)
    #add_definitions(-DEMSCRIPTEN_DEBUG_OUTPUT)
    #add_definitions(-DEMSCRIPTEN_UNIT_TESTBED)
    #add_definitions(-DEMSCRIPTEN_NETWORK_TESTBED)
    #add_definitions(-DTOGGLE_SOUND_TESTBED)
    #add_definitions(-DEMSCRIPTEN_PLAYBACK_TESTBED)
    #add_definitions(-DEMSCRIPTEN_DEBUG)
    endif()

    
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s USE_SDL=2 -s USE_WEBGL2=1 -s FULL_ES3=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s FILESYSTEM=1 -s FORCE_FILESYSTEM=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s USE_VORBIS=1 -s USE_OGG=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s STACK_SIZE=8MB -msimd128")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w -fpermissive")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread")
    
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s INITIAL_MEMORY=128MB -s ALLOW_MEMORY_GROWTH=1 -s MAXIMUM_MEMORY=1024MB")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s MALLOC=mimalloc")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s USE_PTHREADS=1 -s PTHREAD_POOL_SIZE=4")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s PTHREAD_POOL_SIZE_STRICT=0 -s SHARED_MEMORY=1")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s ALLOW_MEMORY_GROWTH=1 -s MAXIMUM_MEMORY=1024MB")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s USE_PTHREADS=1 -s PTHREAD_POOL_SIZE=4")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s PTHREAD_POOL_SIZE_STRICT=0 -s SHARED_MEMORY=1")
    endif()
    
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -fno-rounding-math")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-signaling-nans -fno-trapping-math")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s ENVIRONMENT=web,worker")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s AGGRESSIVE_VARIABLE_ELIMINATION=1")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s ELIMINATE_DUPLICATE_FUNCTIONS=1")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -funroll-loops -fvectorize")
    endif()
    
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s GL_ENABLE_GET_PROC_ADDRESS=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s GL_SUPPORT_AUTOMATIC_ENABLE_EXTENSIONS=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s GL_POOL_TEMP_BUFFERS=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s GL_PREINITIALIZED_CONTEXT=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s GL_EXPLICIT_UNIFORM_LOCATION=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s GL_TRACK_ERRORS=0 -s GL_UNSAFE_OPTS=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s LEGACY_GL_EMULATION=0 -s GL_FFP_ONLY=0")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s MIN_WEBGL_VERSION=2 -s MAX_WEBGL_VERSION=2")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s WEBGL2_BACKWARDS_COMPATIBILITY_EMULATION=0")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s PRECISE_F32=0 -s TEXTDECODER=1")
    
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s ASSERTIONS=2 -s SAFE_HEAP=1")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s STACK_OVERFLOW_CHECK=2")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s GL_ASSERTIONS=1")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g4 --source-map-base https://defconexpandedhost.ddns.net/replay-viewer/")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s DISABLE_EXCEPTION_CATCHING=0")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s EXCEPTION_DEBUG=1")
        string(REPLACE "-O3" "-O0" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
        string(REPLACE "-DNDEBUG" "-D_DEBUG" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    endif()
    
    set(PLATFORM_SPECIFIC_FILES
        ${SYSTEMIV_PATH}/lib/spawn.cpp
        ${SYSTEMIV_PATH}/lib/debug/debug_utils_wasm.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_lib_wasm.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_mutex_linux.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_thread_linux.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_socket_wasm.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_socket_listener_wasm.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_socket_session_wasm.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_udp_packet_wasm.cpp
        ${SYSTEMIV_PATH}/lib/netlib/condition_osxlinux.cpp
        ${SYSTEMIV_PATH}/contrib/binreloc/binreloc.c
        ${IMGUI_PATH}/backends/imgui_impl_opengl3.cpp
    )
    
    set(EXE_TYPE)
    
    set(FILES_TO_EXCLUDE
        ${ROOT_PATH}/source/app/trayicon.cpp
        ${SYSTEMIV_PATH}/lib/gucci/win32library.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_mutex_win32.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_thread_win32.cpp
        ${SYSTEMIV_PATH}/lib/netlib/condition_win32.cpp
        ${SYSTEMIV_PATH}/lib/netlib/condition_winxp.cpp
        ${ROOT_PATH}/source/app/macutil.cpp
        ${ROOT_PATH}/source/app/dockicon.mm
        ${SYSTEMIV_PATH}/lib/netlib/net_lib.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_socket.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_socket_listener.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_socket_session.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_udp_packet.cpp
        ${SYSTEMIV_PATH}/lib/debug/debug_utils.cpp
        ${HIGHWAY_PATH}/hwy/aligned_allocator.cc # aligned_allocator.cc causes clang crash on wasm32 target with complex pointer arithmetic
    )

elseif(MSVC)
    set(CMAKE_GENERATOR_PLATFORM Win32)
    set(CMAKE_GENERATOR_TOOLSET "v143")
    enable_language(RC)

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(ARCH_TYPE "lib64")
        set(VORBIS_ARCH_LIB "oggvorbis64/lib")
        set(VORBIS_ARCH_INCLUDE "oggvorbis64/include")
    else()
        set(ARCH_TYPE "lib32")
        set(VORBIS_ARCH_LIB "oggvorbis/lib/vs2022")
        set(VORBIS_ARCH_INCLUDE "oggvorbis/include")
    endif()

    set(SDL2_PATH "${CMAKE_SOURCE_DIR}/contrib/systemIV/contrib/SDL2-2.32.10/windows")
    set(SDL2_INCLUDE_DIRS ${SDL2_PATH}/include)
    set(SDL2_LIBRARIES 
        ${SDL2_PATH}/${ARCH_TYPE}/SDL2main.lib 
        ${SDL2_PATH}/${ARCH_TYPE}/SDL2-static.lib
    )
    set(SDL2_FOUND TRUE)

    if(NOT EXISTS ${SDL2_PATH}/${ARCH_TYPE})
        message(FATAL_ERROR "SDL2 library not found at: ${SDL2_PATH}/${ARCH_TYPE}")
    endif()
    
    add_definitions(-D_WIN32 -DWIN32 -DTARGET_MSVC)
    add_definitions(-D_WINSOCK_DEPRECATED_NO_WARNINGS -D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-DRENDERER_OPENGL)
    add_definitions(-DRENDERER_DIRECTX11)

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_definitions(-DTOGGLE_SOUND)
        #add_definitions(-DSYNC_PRACTICE)
        #add_definitions(-DREPLAY_VIEWER) 
        #add_definitions(-DREPLAY_VIEWER_DESKTOP)
    endif()

    if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_definitions(-DTOGGLE_SOUND)
    #add_definitions(-DSYNC_PRACTICE)
    #add_definitions(-DREPLAY_VIEWER) 
    #add_definitions(-DREPLAY_VIEWER_DESKTOP)
    endif()
    
    set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} /I\"${RESOURCES_DIR}\"")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP /EHsc /Zm2000 /Zf")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D_SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS /NODEFAULTLIB:LIBCMT")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4530 /wd4018 /wd4267 /wd4244 /wd4305 /wd4101")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4099 /wd26495 /wd4005 /wd4006 /wd4996 /wd4800 /wd4251")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4244 /wd4305 /wd4005 /wd4996")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /IGNORE:4006")
    set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} /IGNORE:4006")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:AVX2")
    
    set(PLATFORM_SPECIFIC_FILES
        ${ROOT_PATH}/source/app/trayicon.cpp
        ${SYSTEMIV_PATH}/lib/gucci/win32library.cpp
        ${SYSTEMIV_PATH}/lib/gucci/window_manager_d3d11.cpp
        ${SYSTEMIV_PATH}/lib/render/renderer_d3d11.cpp
        ${SYSTEMIV_PATH}/lib/render2d/renderer_2d_d3d11.cpp
        ${SYSTEMIV_PATH}/lib/render3d/renderer_3d_d3d11.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_mutex_win32.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_thread_win32.cpp
        ${SYSTEMIV_PATH}/lib/netlib/condition_win32.cpp
        ${SYSTEMIV_PATH}/lib/netlib/condition_winxp.cpp
        ${SYSTEMIV_PATH}/contrib/unrar/isnt.cpp
        ${SYSTEMIV_PATH}/contrib/unrar/motw.cpp
        ${HIGHWAY_PATH}/hwy/aligned_allocator.cc
        ${IMGUI_PATH}/backends/imgui_impl_opengl3.cpp
        ${IMGUI_PATH}/backends/imgui_impl_dx11.cpp
    )
    
    set(EXE_TYPE WIN32)
    
    set(FILES_TO_EXCLUDE
        ${SYSTEMIV_PATH}/lib/netlib/net_mutex_linux.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_thread_linux.cpp
        ${SYSTEMIV_PATH}/lib/netlib/condition_osxlinux.cpp
        ${ROOT_PATH}/source/app/macutil.cpp
        ${ROOT_PATH}/source/app/dockicon.mm
    )
    
elseif(APPLE) 
    set(CMAKE_MACOSX_RPATH 1)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS deployment version")
    set(MACOS_FRAMEWORKS_DIR "${SYSTEMIV_PATH}/contrib/macosx/Frameworks")
    
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
    
    add_definitions(-DSDL_MAIN_HANDLED)
    add_definitions(-D_DARWIN -DMACOS -DTARGET_OS_MACOSX -DGUI -DSILENT)
    add_definitions(-DRENDERER_OPENGL)

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_definitions(-DTOGGLE_SOUND)
        #add_definitions(-DSYNC_PRACTICE)
        #add_definitions(-DREPLAY_VIEWER) 
        #add_definitions(-DREPLAY_VIEWER_DESKTOP)
    endif()

    if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_definitions(-DTOGGLE_SOUND)
    #add_definitions(-DSYNC_PRACTICE)
    #add_definitions(-DREPLAY_VIEWER) 
    #add_definitions(-DREPLAY_VIEWER_DESKTOP)
    endif()
    
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -F${MACOS_FRAMEWORKS_DIR}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -F${MACOS_FRAMEWORKS_DIR}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-deprecated-declarations -Wno-error=format-security -fpermissive")
    
    # architecture specific SIMD for macos
    if(CMAKE_OSX_ARCHITECTURES MATCHES "x86_64")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2 -mfma -mbmi2")
    elseif(CMAKE_OSX_ARCHITECTURES MATCHES "arm64")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    endif()
    
    include_directories(
        ${MACOS_FRAMEWORKS_DIR}
        ${MACOS_FRAMEWORKS_DIR}/SDL2.framework
        ${MACOS_FRAMEWORKS_DIR}/SDL2.framework/Headers
        ${MACOS_FRAMEWORKS_DIR}/Ogg.framework/Headers
        ${MACOS_FRAMEWORKS_DIR}/Vorbis.framework/Headers
    )
    
    set(PLATFORM_SPECIFIC_FILES
        ${ROOT_PATH}/source/app/macutil.cpp
        ${ROOT_PATH}/source/app/dockicon.mm
        ${SYSTEMIV_PATH}/lib/spawn.cpp
        ${SYSTEMIV_PATH}/contrib/binreloc/binreloc.c
        ${SYSTEMIV_PATH}/lib/netlib/net_mutex_linux.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_thread_linux.cpp
        ${SYSTEMIV_PATH}/lib/netlib/condition_osxlinux.cpp
        ${SYSTEMIV_PATH}/lib/gucci/macosxlibrary.cpp
        ${HIGHWAY_PATH}/hwy/aligned_allocator.cc
        ${MACOS_SPECIFIC_SOURCES}
        ${IMGUI_PATH}/backends/imgui_impl_opengl3.cpp
    )
    
    set_source_files_properties(${ROOT_PATH}/source/app/dockicon.mm PROPERTIES COMPILE_FLAGS "-x objective-c++")
    set_source_files_properties(${MACOS_SPECIFIC_SOURCES} PROPERTIES COMPILE_FLAGS "-x objective-c++")
    
    set(EXE_TYPE MACOSX_BUNDLE)
    
    set(FILES_TO_EXCLUDE
        ${ROOT_PATH}/source/app/trayicon.cpp
        ${SYSTEMIV_PATH}/lib/gucci/win32library.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_mutex_win32.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_thread_win32.cpp
        ${SYSTEMIV_PATH}/lib/netlib/condition_win32.cpp
        ${SYSTEMIV_PATH}/lib/netlib/condition_winxp.cpp
        ${SYSTEMIV_PATH}/contrib/unrar/isnt.cpp
        ${SYSTEMIV_PATH}/contrib/unrar/motw.cpp
    )

elseif(UNIX AND NOT APPLE AND NOT EMSCRIPTEN)

    set(SDL2_PATH "${CMAKE_SOURCE_DIR}/contrib/systemIV/contrib/SDL2-2.32.10/linux")
    set(SDL2_INCLUDE_DIRS ${SDL2_PATH}/include)
    set(SDL2_LIBRARIES ${SDL2_PATH}/lib/libSDL2-2.0.so)
    set(SDL2_FOUND TRUE)

    if(NOT EXISTS ${SDL2_LIBRARIES})
        message(FATAL_ERROR "SDL2 library not found at: ${SDL2_LIBRARIES}")
    endif()
    
    find_package(OpenGL REQUIRED)
    
    set(VORBIS_PATH "${CMAKE_SOURCE_DIR}/contrib/systemIV/contrib/oggvorbis64/linux")
    set(VORBIS_INCLUDE_DIRS ${VORBIS_PATH}/include)
    set(CMAKE_PREFIX_PATH "${VORBIS_PATH}" ${CMAKE_PREFIX_PATH})
    set(CMAKE_LIBRARY_PATH "${VORBIS_PATH}/lib64" ${CMAKE_LIBRARY_PATH})
    
    find_library(OGG_LIBRARY NAMES ogg PATHS ${VORBIS_PATH}/lib64 NO_DEFAULT_PATH)
    find_library(VORBIS_LIBRARY NAMES vorbis PATHS ${VORBIS_PATH}/lib64 NO_DEFAULT_PATH)
    find_library(VORBISFILE_LIBRARY NAMES vorbisfile PATHS ${VORBIS_PATH}/lib64 NO_DEFAULT_PATH)
    
    if(NOT OGG_LIBRARY AND NOT VORBIS_LIBRARY AND NOT VORBISFILE_LIBRARY)
        message(FATAL_ERROR "Prebuilt Vorbis/OGG libraries not found in ${VORBIS_PATH}/lib64/")
    endif()
    
    set(VORBIS_LIBRARIES ${VORBISFILE_LIBRARY} ${VORBIS_LIBRARY} ${OGG_LIBRARY})
    set(CRASHPAD_PATH "${CMAKE_SOURCE_DIR}/contrib/systemIV/contrib/crashpad/linux")
    
    add_definitions(-DTARGET_OS_LINUX -DENABLE_BINRELOC -DGUI -DSILENT)
    add_definitions(-DRENDERER_OPENGL)
    
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_definitions(-DTOGGLE_SOUND)
        #add_definitions(-DSYNC_PRACTICE)
        #add_definitions(-DREPLAY_VIEWER) 
        #add_definitions(-DREPLAY_VIEWER_DESKTOP)
    endif()

    if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_definitions(-DTOGGLE_SOUND)
    #add_definitions(-DSYNC_PRACTICE)
    #add_definitions(-DREPLAY_VIEWER) 
    #add_definitions(-DREPLAY_VIEWER_DESKTOP)
    endif()
    
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-deprecated-declarations -Wno-error=format-security -fpermissive")
    set(CMAKE_INSTALL_RPATH "$ORIGIN")
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--no-as-needed")
    set(CMAKE_POSITION_INDEPENDENT_CODE OFF)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-pie")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-pie")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -no-pie")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2 -mfma -mbmi2")
    
    set(PLATFORM_SPECIFIC_FILES
        ${SYSTEMIV_PATH}/lib/spawn.cpp
        ${SYSTEMIV_PATH}/lib/debug/debug_utils_linux.cpp
        ${SYSTEMIV_PATH}/lib/gucci/crashpad.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_mutex_linux.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_thread_linux.cpp
        ${SYSTEMIV_PATH}/lib/netlib/condition_osxlinux.cpp
        ${SYSTEMIV_PATH}/contrib/binreloc/binreloc.c
        ${HIGHWAY_PATH}/hwy/aligned_allocator.cc
        ${IMGUI_PATH}/backends/imgui_impl_opengl3.cpp
    )
    
    set(EXE_TYPE)
    
    set(FILES_TO_EXCLUDE
        ${ROOT_PATH}/source/app/trayicon.cpp
        ${SYSTEMIV_PATH}/lib/gucci/win32library.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_mutex_win32.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_thread_win32.cpp
        ${SYSTEMIV_PATH}/lib/netlib/condition_win32.cpp
        ${SYSTEMIV_PATH}/lib/netlib/condition_winxp.cpp
        ${SYSTEMIV_PATH}/contrib/unrar/isnt.cpp
        ${SYSTEMIV_PATH}/contrib/unrar/motw.cpp
        ${ROOT_PATH}/source/app/macutil.cpp
        ${ROOT_PATH}/source/app/dockicon.mm
    )
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

if(MSVC)
    set(MAIN_EXE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
else()
    set(MAIN_EXE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/result/$<CONFIG>")
endif()

set(DATA_SOURCE_DIR "${CMAKE_SOURCE_DIR}/localisation")

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
    ${ROOT_PATH}/source/lib/universal_include.cpp
    ${ROOT_PATH}/source/renderer/world_renderer.cpp
    ${ROOT_PATH}/source/renderer/map_renderer.cpp
    ${ROOT_PATH}/source/renderer/globe_renderer.cpp
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
    ${ROOT_PATH}/source/interface/screenoptions_window.cpp
    ${ROOT_PATH}/source/interface/soundoptions_window.cpp
    ${ROOT_PATH}/source/interface/soundeditor_window.cpp
    ${ROOT_PATH}/source/interface/soundparameter_window.cpp
    ${ROOT_PATH}/source/interface/soundstats_window.cpp
    ${ROOT_PATH}/source/interface/styleeditor_window.cpp
    ${ROOT_PATH}/source/network/recordingparser.cpp
    ${ROOT_PATH}/source/interface/recording_selection.cpp
    ${ROOT_PATH}/source/interface/recording_file_dialog.cpp
)

set(SYSTEMIV_SOURCE
    ${SYSTEMIV_PATH}/lib/eclipse/components/checkbox.cpp
    ${SYSTEMIV_PATH}/lib/eclipse/components/core.cpp
    ${SYSTEMIV_PATH}/lib/eclipse/components/drop_down_menu.cpp
    ${SYSTEMIV_PATH}/lib/eclipse/components/filedialog.cpp
    ${SYSTEMIV_PATH}/lib/eclipse/components/inputfield.cpp
    ${SYSTEMIV_PATH}/lib/eclipse/components/message_dialog.cpp
    ${SYSTEMIV_PATH}/lib/eclipse/components/scrollbar.cpp
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
    ${SYSTEMIV_PATH}/lib/math/matrix33.cpp
    ${SYSTEMIV_PATH}/lib/math/matrix34.cpp
    ${SYSTEMIV_PATH}/lib/math/invert_matrix.cpp
    ${SYSTEMIV_PATH}/lib/math/hash_data.cpp
    ${SYSTEMIV_PATH}/lib/math/Matrix4f.cpp
    ${SYSTEMIV_PATH}/lib/math/math_utils.cpp
    ${SYSTEMIV_PATH}/lib/math/random_number.cpp
    ${SYSTEMIV_PATH}/lib/math/vector2.cpp
    ${SYSTEMIV_PATH}/lib/math/vector3.cpp
    ${SYSTEMIV_PATH}/lib/metaserver/authentication.cpp
    ${SYSTEMIV_PATH}/lib/metaserver/matchmaker.cpp
    ${SYSTEMIV_PATH}/lib/metaserver/metaserver.cpp
    ${SYSTEMIV_PATH}/lib/gucci/input.cpp
    ${SYSTEMIV_PATH}/lib/gucci/input_sdl.cpp
    ${SYSTEMIV_PATH}/lib/gucci/window_manager.cpp
    ${SYSTEMIV_PATH}/lib/gucci/window_manager_opengl.cpp
    ${SYSTEMIV_PATH}/lib/netlib/net_lib.cpp
    ${SYSTEMIV_PATH}/lib/netlib/net_socket.cpp
    ${SYSTEMIV_PATH}/lib/netlib/net_socket_listener.cpp
    ${SYSTEMIV_PATH}/lib/netlib/net_socket_session.cpp
    ${SYSTEMIV_PATH}/lib/netlib/net_udp_packet.cpp
    ${SYSTEMIV_PATH}/lib/netlib/thread_controller.cpp
    ${SYSTEMIV_PATH}/lib/render/colour.cpp
    ${SYSTEMIV_PATH}/lib/render/renderer.cpp
    ${SYSTEMIV_PATH}/lib/render/renderer_opengl.cpp
    ${SYSTEMIV_PATH}/lib/render2d/renderer_2d.cpp
    ${SYSTEMIV_PATH}/lib/render2d/renderer_2d_opengl.cpp
    ${SYSTEMIV_PATH}/lib/render2d/megavbo/megavbo_2d.cpp
    ${SYSTEMIV_PATH}/lib/render2d/megavbo/linestrips_2d.cpp
    ${SYSTEMIV_PATH}/lib/render2d/megavbo/quads_2d.cpp
    ${SYSTEMIV_PATH}/lib/render2d/megavbo/triangles_2d.cpp
    ${SYSTEMIV_PATH}/lib/render2d/operations_2d.cpp
    ${SYSTEMIV_PATH}/lib/render2d/primitives_2d.cpp
    ${SYSTEMIV_PATH}/lib/render2d/sprites_2d.cpp
    ${SYSTEMIV_PATH}/lib/render2d/text_2d.cpp
    ${SYSTEMIV_PATH}/lib/render3d/renderer_3d.cpp
    ${SYSTEMIV_PATH}/lib/render3d/renderer_3d_opengl.cpp
    ${SYSTEMIV_PATH}/lib/render3d/operations_3d.cpp
    ${SYSTEMIV_PATH}/lib/render3d/primitives_3d.cpp
    ${SYSTEMIV_PATH}/lib/render3d/sprites_3d.cpp
    ${SYSTEMIV_PATH}/lib/render3d/text_3d.cpp
    ${SYSTEMIV_PATH}/lib/render3d/megavbo/megavbo_3d.cpp
    ${SYSTEMIV_PATH}/lib/render3d/megavbo/quads_3d.cpp
    ${SYSTEMIV_PATH}/lib/render3d/megavbo/linestrips_3d.cpp
    ${SYSTEMIV_PATH}/lib/render3d/megavbo/triangles_3d.cpp
    ${SYSTEMIV_PATH}/lib/render3d/megavbo/instancing_3d.cpp
    ${SYSTEMIV_PATH}/lib/render/renderer_overlay.cpp
    ${SYSTEMIV_PATH}/lib/render/styletable.cpp
    ${SYSTEMIV_PATH}/lib/resource/bitmap.cpp
    ${SYSTEMIV_PATH}/lib/resource/bitmapfont.cpp
    ${SYSTEMIV_PATH}/lib/resource/image.cpp
    ${SYSTEMIV_PATH}/lib/resource/tinygltf/model.cpp
    ${SYSTEMIV_PATH}/lib/resource/tinygltf/model_builder.cpp
    ${SYSTEMIV_PATH}/lib/resource/tinygltf/model_loader.cpp
    ${SYSTEMIV_PATH}/lib/resource/tinygltf/model_material.cpp
    ${SYSTEMIV_PATH}/lib/resource/resource.cpp
    ${SYSTEMIV_PATH}/lib/resource/sprite_atlas.cpp
    ${SYSTEMIV_PATH}/lib/sound/sound_blueprint_manager.cpp
    ${SYSTEMIV_PATH}/lib/sound/sound_filter.cpp
    ${SYSTEMIV_PATH}/lib/sound/sound_instance.cpp
    ${SYSTEMIV_PATH}/lib/sound/sound_library_2d.cpp
    ${SYSTEMIV_PATH}/lib/sound/sound_library_3d.cpp
    ${SYSTEMIV_PATH}/lib/sound/sound_library_2d_sdl.cpp
    ${SYSTEMIV_PATH}/lib/sound/sound_library_3d_software.cpp
    ${SYSTEMIV_PATH}/lib/sound/sound_debug_overlay.cpp
    ${SYSTEMIV_PATH}/lib/sound/sound_parameter.cpp
    ${SYSTEMIV_PATH}/lib/sound/sound_sample_bank.cpp
    ${SYSTEMIV_PATH}/lib/sound/resampler_polyphase.cpp
    ${SYSTEMIV_PATH}/lib/sound/sound_sample_decoder.cpp
    ${SYSTEMIV_PATH}/lib/sound/soundsystem.cpp
    ${SYSTEMIV_PATH}/lib/sound/soundsystem_interface.cpp
    ${SYSTEMIV_PATH}/lib/tosser/directory.cpp
    ${SYSTEMIV_PATH}/lib/unicode/unicode_string.cpp
    ${SYSTEMIV_PATH}/lib/unicode/unicode_text_stream_reader.cpp
    ${SYSTEMIV_PATH}/lib/debug/profiler.cpp
    ${SYSTEMIV_PATH}/lib/debug/debug_utils.cpp
    ${SYSTEMIV_PATH}/lib/imgui/imgui.cpp
    ${SYSTEMIV_PATH}/lib/imgui/debug_console.cpp
    ${SYSTEMIV_PATH}/lib/imgui/preferences_editor.cpp
    ${SYSTEMIV_PATH}/lib/imgui/components/menu_bar.cpp
    ${SYSTEMIV_PATH}/lib/imgui/components/text_editor.cpp
    ${SYSTEMIV_PATH}/lib/hi_res_time.cpp
    ${SYSTEMIV_PATH}/lib/multiline_text.cpp
    ${SYSTEMIV_PATH}/lib/language_table.cpp
    ${SYSTEMIV_PATH}/lib/preferences.cpp
    ${SYSTEMIV_PATH}/lib/string_utils.cpp
    ${SYSTEMIV_PATH}/systemiv.cpp
)

set(UNRAR_SOURCE 
    ${SYSTEMIV_PATH}/contrib/unrar/unrar.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/archive.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/arcread.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/blake2s.cpp
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
    ${SYSTEMIV_PATH}/contrib/unrar/hash.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/headers.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/largepage.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/log.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/list.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/match.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/options.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/pathfn.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/qopen.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/rar.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/rarpch.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/rarvm.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/rawread.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/rdwrfn.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/recvol.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/resourcerar.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/rijndael.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/rs.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/rs16.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/scantree.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/secpassword.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/sha1.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/sha256.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/smallfn.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/strfn.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/strlist.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/system.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/threadpool.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/timefn.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/ui.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/unicode.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/unpack.cpp
    ${SYSTEMIV_PATH}/contrib/unrar/volume.cpp
)

set(HIGHWAY_SOURCE
    ${HIGHWAY_PATH}/hwy/abort.cc
    ${HIGHWAY_PATH}/hwy/nanobenchmark.cc
    ${HIGHWAY_PATH}/hwy/per_target.cc
    ${HIGHWAY_PATH}/hwy/print.cc
    ${HIGHWAY_PATH}/hwy/targets.cc
    ${HIGHWAY_PATH}/hwy/timer.cc
)

set(TINYGLTF_SOURCE
    ${TINYGLTF_PATH}/tiny_gltf.cc
)

set(IMGUI_SOURCE
    ${IMGUI_PATH}/imgui.cpp
    ${IMGUI_PATH}/imgui_draw.cpp
    ${IMGUI_PATH}/imgui_tables.cpp
    ${IMGUI_PATH}/imgui_widgets.cpp
    ${IMGUI_PATH}/backends/imgui_impl_sdl2.cpp
)

foreach(FILE ${FILES_TO_EXCLUDE})
    list(REMOVE_ITEM DEFCON_SOURCE ${FILE})
    list(REMOVE_ITEM SYSTEMIV_SOURCE ${FILE})
    list(REMOVE_ITEM UNRAR_SOURCE ${FILE})
    list(REMOVE_ITEM HIGHWAY_SOURCE ${FILE})
    list(REMOVE_ITEM TINYGLTF_SOURCE ${FILE})
    list(REMOVE_ITEM IMGUI_SOURCE ${FILE})
endforeach()

set(ALL_SOURCES ${DEFCON_SOURCE} ${SYSTEMIV_SOURCE} ${UNRAR_SOURCE} ${HIGHWAY_SOURCE} ${TINYGLTF_SOURCE} ${IMGUI_SOURCE} ${PLATFORM_SPECIFIC_FILES} ${RESOURCE_FILES} ${GLAD_SOURCE_FILE})

if(REPLAY_VIEWER_BUILD)
    set(EXECUTABLE_NAME "replay_viewer_${REPLAY_VIEWER_VERSION}")
    message(STATUS "Building WebAssembly replay viewer with version: ${REPLAY_VIEWER_VERSION}")
elseif(SYNC_PRACTICE_BUILD)
    set(EXECUTABLE_NAME "sync_practice_${SYNC_PRACTICE_VERSION}")
    message(STATUS "Building WebAssembly silo practice client with version: ${SYNC_PRACTICE_VERSION}")
else()
    set(EXECUTABLE_NAME "defcon")
endif()

add_executable(${EXECUTABLE_NAME} ${EXE_TYPE} ${ALL_SOURCES})

if(EMSCRIPTEN)
    set_target_properties(${EXECUTABLE_NAME} PROPERTIES
        WIN32_EXECUTABLE FALSE
        RUNTIME_OUTPUT_DIRECTORY "${MAIN_EXE_OUTPUT_DIR}"
        SUFFIX ".html"
    )
    
    set(EMSCRIPTEN_LINK_FLAGS "-s USE_SDL=2 -s USE_WEBGL2=1 -s FULL_ES3=1")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s FILESYSTEM=1 -s FORCE_FILESYSTEM=1")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s USE_VORBIS=1 -s USE_OGG=1")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -msimd128 -s STACK_SIZE=8MB")

    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s INITIAL_MEMORY=128MB -s ALLOW_MEMORY_GROWTH=1 -s MAXIMUM_MEMORY=1024MB")
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s MALLOC=mimalloc")
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s USE_PTHREADS=1 -s PTHREAD_POOL_SIZE=4")
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s PTHREAD_POOL_SIZE_STRICT=0 -s SHARED_MEMORY=1")
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s ALLOW_BLOCKING_ON_MAIN_THREAD=0")
    else()
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s ALLOW_MEMORY_GROWTH=1 -s MAXIMUM_MEMORY=1024MB")
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s USE_PTHREADS=1 -s PTHREAD_POOL_SIZE=4")
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s PTHREAD_POOL_SIZE_STRICT=0 -s SHARED_MEMORY=1")
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s ALLOW_BLOCKING_ON_MAIN_THREAD=0")
    endif()

    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s GL_ENABLE_GET_PROC_ADDRESS=1")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s GL_SUPPORT_AUTOMATIC_ENABLE_EXTENSIONS=1")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s GL_POOL_TEMP_BUFFERS=1")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s GL_PREINITIALIZED_CONTEXT=1")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s GL_TRACK_ERRORS=0 -s GL_UNSAFE_OPTS=1")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s LEGACY_GL_EMULATION=0")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s MIN_WEBGL_VERSION=2 -s MAX_WEBGL_VERSION=2")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s PRECISE_F32=0 -s TEXTDECODER=1")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s EXPORTED_FUNCTIONS=['_main','_malloc','_free']")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s EXPORTED_RUNTIME_METHODS=['ccall','cwrap','print','printErr']")
    
    set(EMSCRIPTEN_DATA_DIR "${CMAKE_SOURCE_DIR}/localisation")  
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} --preload-file ${EMSCRIPTEN_DATA_DIR}/main.dat@main.dat")

    if(REPLAY_VIEWER_BUILD AND EXISTS "${EMSCRIPTEN_DATA_DIR}/mods")
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} --preload-file ${EMSCRIPTEN_DATA_DIR}/mods@mods")
    endif()

    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -w")
    
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -O3 -s ENVIRONMENT=web,worker")
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s MODULARIZE=0 -s EXPORT_ES6=0")
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s MINIFY_HTML=1 -s SINGLE_FILE=0")
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s DEMANGLE_SUPPORT=0 -fno-rtti -fvisibility=hidden")
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s AGGRESSIVE_VARIABLE_ELIMINATION=1")
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s ELIMINATE_DUPLICATE_FUNCTIONS=1")
    else()
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s ASSERTIONS=2 -s SAFE_HEAP=1")
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s STACK_OVERFLOW_CHECK=2")
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s GL_ASSERTIONS=1")
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s DISABLE_EXCEPTION_CATCHING=0")
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s EXCEPTION_DEBUG=1")
        set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -g4 --source-map-base https://defconexpandedhost.ddns.net/replay-viewer/")
        string(REPLACE "-O3" "-O0" EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS}")
    endif()
    
    set_target_properties(${EXECUTABLE_NAME} PROPERTIES LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS}")
 
elseif(MSVC)
    set_target_properties(${EXECUTABLE_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
        LINK_FLAGS "/SUBSYSTEM:WINDOWS /IGNORE:4006"
        RUNTIME_OUTPUT_DIRECTORY "${MAIN_EXE_OUTPUT_DIR}"
    )
    
    target_include_directories(${EXECUTABLE_NAME} PRIVATE
        ${ROOT_PATH}/contrib/systemIV/contrib/${VORBIS_ARCH_INCLUDE}
        ${SDL2_INCLUDE_DIRS}
        ${SDL2_INCLUDE_DIRS}/SDL2
        ${ROOT_PATH}/contrib/ambx1.1/inc
        ${TINYGLTF_PATH}
        ${IMGUI_PATH}
        ${IMGUI_PATH}/backends
    )
    
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE 
        ${SDL2_LIBRARIES}
        "${CMAKE_SOURCE_DIR}/contrib/systemIV/contrib/${VORBIS_ARCH_LIB}/ogg.lib"
        "${CMAKE_SOURCE_DIR}/contrib/systemIV/contrib/${VORBIS_ARCH_LIB}/vorbis.lib"
        "${CMAKE_SOURCE_DIR}/contrib/systemIV/contrib/${VORBIS_ARCH_LIB}/vorbisfile.lib"
        wsock32 dbghelp winmm Ws2_32 setupapi version imm32 cfgmgr32
    )
    
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE d3d11 dxgi d3dcompiler opengl32 glu32)
    
    if(OpenMP_CXX_FOUND)
        target_link_libraries(${EXECUTABLE_NAME} PRIVATE OpenMP::OpenMP_CXX)
    endif()
    
elseif(APPLE)
    set_target_properties(${EXECUTABLE_NAME} PROPERTIES
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
        LINK_FLAGS "-F${MACOS_FRAMEWORKS_DIR}"
    )

    target_link_libraries(${EXECUTABLE_NAME} PRIVATE 
        "-framework SDL2" 
        "-framework Ogg" 
        "-framework Vorbis" 
        "-framework OpenGL" 
        "-framework Carbon" 
        "-framework IOKit" 
        "-framework AppKit"
        "-liconv"
    )
    
    if(OpenMP_CXX_FOUND)
        target_link_libraries(${EXECUTABLE_NAME} PRIVATE OpenMP::OpenMP_CXX)
    endif()

else()
    set_target_properties(${EXECUTABLE_NAME} PROPERTIES
        WIN32_EXECUTABLE FALSE
        RUNTIME_OUTPUT_DIRECTORY "${MAIN_EXE_OUTPUT_DIR}"
    )
    
    set(CRASHPAD_INCLUDE_DIRS 
        ${CRASHPAD_PATH}/include
        ${CRASHPAD_PATH}/include/third_party/mini_chromium/mini_chromium
    )
    
    target_include_directories(${EXECUTABLE_NAME} PUBLIC 
        ${SDL2_INCLUDE_DIRS}
        ${SDL2_INCLUDE_DIRS}/SDL2
        ${OPENGL_INCLUDE_DIRS}
        ${VORBIS_INCLUDE_DIRS}
        ${CRASHPAD_INCLUDE_DIRS}
        ${IMGUI_PATH}
        ${IMGUI_PATH}/backends
    )
    
    set(CRASHPAD_LIBRARIES
        ${CRASHPAD_PATH}/lib/libclient.a
        ${CRASHPAD_PATH}/lib/libcommon.a
        ${CRASHPAD_PATH}/lib/libutil.a
        ${CRASHPAD_PATH}/lib/libbase.a
    )
    
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE 
        ${SDL2_LIBRARIES}
        ${OPENGL_LIBRARIES}
        ${VORBIS_LIBRARIES}
        ${CRASHPAD_LIBRARIES}
        dl
        pthread
    )
    
    if(HIGHWAY_NEEDS_ATOMIC_LIB)
        target_link_libraries(${EXECUTABLE_NAME} PRIVATE atomic)
    endif()
    
    if(OpenMP_CXX_FOUND)
        target_link_libraries(${EXECUTABLE_NAME} PRIVATE OpenMP::OpenMP_CXX)
    endif()

endif()

target_include_directories(${EXECUTABLE_NAME} PUBLIC
    ${CMAKE_SOURCE_DIR}/resources
    ${ROOT_PATH}/source
    ${SYSTEMIV_PATH}
    ${TINYGLTF_PATH}
    ${IMGUI_PATH}
    ${IMGUI_PATH}/backends
)


add_library(systemIV INTERFACE)
target_link_libraries(${EXECUTABLE_NAME} PUBLIC systemIV)



if(MSVC)
    add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD 
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:${EXECUTABLE_NAME}>"
        "${MAIN_EXE_OUTPUT_DIR}/$<TARGET_FILE_NAME:${EXECUTABLE_NAME}>"
    )

elseif(UNIX AND NOT APPLE AND NOT EMSCRIPTEN)
    add_custom_command(TARGET ${EXECUTABLE_NAME} PRE_BUILD 
        COMMAND ${CMAKE_COMMAND} -E make_directory "${MAIN_EXE_OUTPUT_DIR}"
    )
    add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD 
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different "${DATA_SOURCE_DIR}/mods" "${MAIN_EXE_OUTPUT_DIR}/mods"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DATA_SOURCE_DIR}/main.dat" "${MAIN_EXE_OUTPUT_DIR}/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DATA_SOURCE_DIR}/sounds.dat" "${MAIN_EXE_OUTPUT_DIR}/"
        COMMAND ${CMAKE_SOURCE_DIR}/targets/linux/bundle_sdl.sh ${MAIN_EXE_OUTPUT_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/targets/linux/launch_cmake.sh ${MAIN_EXE_OUTPUT_DIR}/launch.sh
    )
    
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD 
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CRASHPAD_PATH}/crashpad_handler" "${MAIN_EXE_OUTPUT_DIR}/crashpad_handler"
        )
    endif()

elseif(APPLE)
    add_custom_command(TARGET ${EXECUTABLE_NAME} PRE_BUILD 
        COMMAND ${CMAKE_COMMAND} -E make_directory "${MAIN_EXE_OUTPUT_DIR}"
    )
    set(BUNDLE_DIR ${MAIN_EXE_OUTPUT_DIR}/${EXECUTABLE_NAME}.app/Contents)
    set(DATA_TARGET_DIR ${BUNDLE_DIR}/Resources/)
    add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD 
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different "${DATA_SOURCE_DIR}/mods" "${DATA_TARGET_DIR}/mods"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DATA_SOURCE_DIR}/main.dat" "${DATA_TARGET_DIR}/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DATA_SOURCE_DIR}/sounds.dat" "${DATA_TARGET_DIR}/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/resources/macosx/*" "${DATA_TARGET_DIR}/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/targets/macosx/Info_cmake.plist" "${BUNDLE_DIR}/Info.plist"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/targets/macosx/Base.lproj/SDLMain.nib" "${DATA_TARGET_DIR}/SDLMain.nib"
        COMMAND ${CMAKE_SOURCE_DIR}/targets/macosx/bundle_sdl.sh ${BUNDLE_DIR}
    )
endif()
