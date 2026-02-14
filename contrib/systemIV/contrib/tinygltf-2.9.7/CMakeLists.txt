cmake_minimum_required(VERSION 3.16)
project(TinyGLTF LANGUAGES CXX)

set(TINYGLTF_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(TINYGLTF_SOURCES
    ${TINYGLTF_DIR}/tiny_gltf.cc
)

add_library(TinyGLTF STATIC ${TINYGLTF_SOURCES})

target_include_directories(TinyGLTF PUBLIC ${TINYGLTF_DIR})

if(MSVC)
    target_compile_definitions(TinyGLTF PRIVATE
        TARGET_MSVC
        _CRT_SECURE_NO_DEPRECATE
        _CRT_NONSTDC_NO_DEPRECATE
    )
endif()

set_target_properties(TinyGLTF PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
