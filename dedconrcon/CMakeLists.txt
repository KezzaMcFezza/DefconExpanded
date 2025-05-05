if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.15)
    project(DedconRCON VERSION 1.0)
    
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(OPENSSL_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../OpenSSL")
    set(OPENSSL_INCLUDE_DIR "${OPENSSL_ROOT_DIR}/include")
    set(OPENSSL_LIBRARIES_DIR "${OPENSSL_ROOT_DIR}/lib/VC/x86")
    
    include_directories(${OPENSSL_INCLUDE_DIR})
    
    add_definitions(-DOPENSSL_STATIC -DWIN32_LEAN_AND_MEAN)
    
    if(APPLE)
        set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/../../bin")
    else()
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/../../Dedcon EXE's")
    endif()
    
    set(CMAKE_CONFIGURATION_TYPES "Release;Debug" CACHE STRING "" FORCE)
    
    if(MSVC)
        add_definitions(-D_WIN32 -DWIN32 -D_WINSOCK_DEPRECATED_NO_WARNINGS -D_CRT_SECURE_NO_WARNINGS)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:CONSOLE")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP /EHsc")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4530 /wd4018 /wd4267 /wd4244 /wd4305 /wd4101 /wd4099 /wd26495 /wd4146")
    elseif(APPLE)
        add_definitions(-D_DARWIN -DMACOS)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-deprecated-declarations")
    endif()
    
    if(MSVC)
        set(OPENSSL_LIBRARIES_DEBUG 
            "${OPENSSL_LIBRARIES_DIR}/MTd/libssl_static.lib" 
            "${OPENSSL_LIBRARIES_DIR}/MTd/libcrypto_static.lib"
            crypt32.lib
            ws2_32.lib
            advapi32.lib
            user32.lib)
        
        set(OPENSSL_LIBRARIES_RELEASE 
            "${OPENSSL_LIBRARIES_DIR}/MT/libssl_static.lib" 
            "${OPENSSL_LIBRARIES_DIR}/MT/libcrypto_static.lib"
            crypt32.lib
            ws2_32.lib
            advapi32.lib
            user32.lib)
    endif()
    
    foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER ${CONFIG} CONFIG_UPPER)
        
        if(MSVC)
            set(CMAKE_CXX_FLAGS_${CONFIG_UPPER} "${CMAKE_CXX_FLAGS_${CONFIG_UPPER}} /MP")
        endif()
        
        if(${CONFIG} MATCHES "Debug")
            if(MSVC)
                string(APPEND CMAKE_CXX_FLAGS_${CONFIG_UPPER} " /Od /Zi /MTd /RTC1 /GS /EHsc /FC /analyze")
                set(CMAKE_CXX_FLAGS_${CONFIG_UPPER} "${CMAKE_CXX_FLAGS_${CONFIG_UPPER}} -DDEBUG")
            else()
                set(CMAKE_CXX_FLAGS_${CONFIG_UPPER} "${CMAKE_CXX_FLAGS_${CONFIG_UPPER}} -O0 -g -DDEBUG")
            endif()
        else()
            if(MSVC)
                string(APPEND CMAKE_CXX_FLAGS_${CONFIG_UPPER} " /MT /O2")
            else()
                set(CMAKE_CXX_FLAGS_${CONFIG_UPPER} "${CMAKE_CXX_FLAGS_${CONFIG_UPPER}} -O3")
            endif()
        endif()
    endforeach()
endif()

add_executable(DedconRCON 
    RCONClient.cpp
    RCONClient.h 
    ClientEncryption.cpp
    ClientEncryption.h
)

target_include_directories(DedconRCON PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

if(MSVC)
    target_link_libraries(DedconRCON debug ${OPENSSL_LIBRARIES_DEBUG})
    target_link_libraries(DedconRCON optimized ${OPENSSL_LIBRARIES_RELEASE})
    set_property(TARGET DedconRCON PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_compile_options(DedconRCON PRIVATE /utf-8)
else()
    find_package(OpenSSL REQUIRED)
    target_link_libraries(DedconRCON OpenSSL::SSL OpenSSL::Crypto)
endif() 