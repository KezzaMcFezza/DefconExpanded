cmake_minimum_required(VERSION 3.16)
project(Highway LANGUAGES CXX)

set(HIGHWAY_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(HIGHWAY_SOURCES
    ${HIGHWAY_DIR}/hwy/abort.cc
    ${HIGHWAY_DIR}/hwy/nanobenchmark.cc
    ${HIGHWAY_DIR}/hwy/per_target.cc
    ${HIGHWAY_DIR}/hwy/print.cc
    ${HIGHWAY_DIR}/hwy/targets.cc
    ${HIGHWAY_DIR}/hwy/timer.cc
)

if(NOT EMSCRIPTEN)
    list(APPEND HIGHWAY_SOURCES ${HIGHWAY_DIR}/hwy/aligned_allocator.cc)
endif()

add_library(Highway STATIC ${HIGHWAY_SOURCES})

target_include_directories(Highway PUBLIC ${HIGHWAY_DIR})

if(MSVC)
    target_compile_definitions(Highway PRIVATE
        TARGET_MSVC
        _CRT_SECURE_NO_DEPRECATE
        _CRT_NONSTDC_NO_DEPRECATE
    )
endif()

set_target_properties(Highway PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)

if(HIGHWAY_NEEDS_ATOMIC_LIB)
    target_link_libraries(Highway PUBLIC atomic)
endif()
