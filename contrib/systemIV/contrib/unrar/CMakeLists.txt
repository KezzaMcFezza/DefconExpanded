cmake_minimum_required(VERSION 3.16)
project(Unrar LANGUAGES CXX)

set(UNRAR_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(UNRAR_SOURCES
    ${UNRAR_DIR}/rarpch.cpp
    ${UNRAR_DIR}/archive.cpp
    ${UNRAR_DIR}/arcread.cpp
    ${UNRAR_DIR}/blake2s.cpp
    ${UNRAR_DIR}/cmddata.cpp
    ${UNRAR_DIR}/consio.cpp
    ${UNRAR_DIR}/crc.cpp
    ${UNRAR_DIR}/crypt.cpp
    ${UNRAR_DIR}/encname.cpp
    ${UNRAR_DIR}/errhnd.cpp
    ${UNRAR_DIR}/extinfo.cpp
    ${UNRAR_DIR}/extract.cpp
    ${UNRAR_DIR}/filcreat.cpp
    ${UNRAR_DIR}/file.cpp
    ${UNRAR_DIR}/filefn.cpp
    ${UNRAR_DIR}/filestr.cpp
    ${UNRAR_DIR}/find.cpp
    ${UNRAR_DIR}/getbits.cpp
    ${UNRAR_DIR}/global.cpp
    ${UNRAR_DIR}/hash.cpp
    ${UNRAR_DIR}/headers.cpp
    ${UNRAR_DIR}/largepage.cpp
    ${UNRAR_DIR}/list.cpp
    ${UNRAR_DIR}/log.cpp
    ${UNRAR_DIR}/match.cpp
    ${UNRAR_DIR}/options.cpp
    ${UNRAR_DIR}/pathfn.cpp
    ${UNRAR_DIR}/qopen.cpp
    ${UNRAR_DIR}/rar.cpp
    ${UNRAR_DIR}/rarvm.cpp
    ${UNRAR_DIR}/rawread.cpp
    ${UNRAR_DIR}/rdwrfn.cpp
    ${UNRAR_DIR}/recvol.cpp
    ${UNRAR_DIR}/resourcerar.cpp
    ${UNRAR_DIR}/rijndael.cpp
    ${UNRAR_DIR}/rs.cpp
    ${UNRAR_DIR}/rs16.cpp
    ${UNRAR_DIR}/scantree.cpp
    ${UNRAR_DIR}/secpassword.cpp
    ${UNRAR_DIR}/sha1.cpp
    ${UNRAR_DIR}/sha256.cpp
    ${UNRAR_DIR}/smallfn.cpp
    ${UNRAR_DIR}/strfn.cpp
    ${UNRAR_DIR}/strlist.cpp
    ${UNRAR_DIR}/system.cpp
    ${UNRAR_DIR}/threadpool.cpp
    ${UNRAR_DIR}/timefn.cpp
    ${UNRAR_DIR}/ui.cpp
    ${UNRAR_DIR}/unicode.cpp
    ${UNRAR_DIR}/unpack.cpp
    ${UNRAR_DIR}/unrar.cpp
    ${UNRAR_DIR}/volume.cpp
)

if(MSVC)
    list(APPEND UNRAR_SOURCES ${UNRAR_DIR}/isnt.cpp ${UNRAR_DIR}/motw.cpp)
endif()

add_library(Unrar STATIC ${UNRAR_SOURCES})

target_include_directories(Unrar PUBLIC
    ${UNRAR_DIR}
    ${CMAKE_SOURCE_DIR}/contrib/systemIV
    ${CMAKE_SOURCE_DIR}/contrib/systemIV/contrib
)

target_compile_definitions(Unrar PRIVATE
    TARGET_MSVC
    _CRT_SECURE_NO_DEPRECATE
    _CRT_NONSTDC_NO_DEPRECATE
    _WINSOCK_DEPRECATED_NO_WARNINGS
)

if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(Unrar PRIVATE USE_CRASHREPORTING)
endif()

set_target_properties(Unrar PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)

target_precompile_headers(Unrar PRIVATE rar.hpp)
set_source_files_properties(${UNRAR_DIR}/global.cpp PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
