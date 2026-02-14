cmake_minimum_required(VERSION 3.16)
project(ImGui LANGUAGES CXX)

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
)

if(MSVC)
    list(APPEND IMGUI_SOURCES
        ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
        ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
    )
elseif(EMSCRIPTEN)
    list(APPEND IMGUI_SOURCES ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp)
else()
    list(APPEND IMGUI_SOURCES ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp)
endif()

add_library(ImGui STATIC ${IMGUI_SOURCES})

target_include_directories(ImGui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

if(DEFINED SDL3_INCLUDE_DIRS)
    target_include_directories(ImGui PUBLIC ${SDL3_INCLUDE_DIRS} ${SDL3_INCLUDE_DIRS}/SDL3)
endif()

if(DEFINED GLAD_INCLUDE_DIR)
    target_include_directories(ImGui PUBLIC ${GLAD_INCLUDE_DIR})
endif()

target_compile_definitions(ImGui PRIVATE RENDERER_OPENGL)

if(MSVC)
    target_compile_definitions(ImGui PRIVATE
        TARGET_MSVC
        _CRT_SECURE_NO_DEPRECATE
        _CRT_NONSTDC_NO_DEPRECATE
        RENDERER_DIRECTX11
    )
endif()

if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(ImGui PRIVATE USE_CRASHREPORTING)
endif()

set_target_properties(ImGui PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
