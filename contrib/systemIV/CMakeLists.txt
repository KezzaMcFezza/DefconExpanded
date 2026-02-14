cmake_minimum_required(VERSION 3.16)
project(SystemIV LANGUAGES C CXX)

set(SYSTEMIV_PATH ${CMAKE_CURRENT_SOURCE_DIR})
set(GLAD_PATH ${SYSTEMIV_PATH}/contrib/glad)
set(HIGHWAY_PATH ${SYSTEMIV_PATH}/contrib/highway-1.3.0)
set(TINYGLTF_PATH ${SYSTEMIV_PATH}/contrib/tinygltf-2.9.7)
set(IMGUI_PATH ${SYSTEMIV_PATH}/contrib/imgui-1.92.5)

if(EMSCRIPTEN)
    set(GLAD_INCLUDE_DIR "${GLAD_PATH}/emscripten/include")
    set(GLAD_SOURCE_FILE "${GLAD_PATH}/emscripten/src/glad.c")
else()
    set(GLAD_INCLUDE_DIR "${GLAD_PATH}/desktop/include")
    set(GLAD_SOURCE_FILE "${GLAD_PATH}/desktop/src/glad.c")
endif()

add_subdirectory(contrib/unrar)
add_subdirectory(contrib/imgui-1.92.5)
add_subdirectory(contrib/highway-1.3.0)
add_subdirectory(contrib/tinygltf-2.9.7)

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
    ${SYSTEMIV_PATH}/lib/filesys/native_dialog.cpp
    ${SYSTEMIV_PATH}/lib/math/fixed_float.cpp
    ${SYSTEMIV_PATH}/lib/math/fixed_64.cpp
    ${SYSTEMIV_PATH}/lib/math/matrix33.cpp
    ${SYSTEMIV_PATH}/lib/math/matrix34.cpp
    ${SYSTEMIV_PATH}/lib/math/invert_matrix.cpp
    ${SYSTEMIV_PATH}/lib/math/hash_data.cpp
    ${SYSTEMIV_PATH}/lib/math/matrix4f.cpp
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
    ${SYSTEMIV_PATH}/lib/render/antialiasing/anti_aliasing.cpp
    ${SYSTEMIV_PATH}/lib/render/antialiasing/msaa_opengl.cpp
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
    ${SYSTEMIV_PATH}/lib/imgui/frame_debugger.cpp
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

if(EMSCRIPTEN)
    set(SYSTEMIV_PLATFORM_SOURCES
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
    )
    set(SYSTEMIV_EXCLUDE
        ${SYSTEMIV_PATH}/lib/gucci/win32library.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_mutex_win32.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_thread_win32.cpp
        ${SYSTEMIV_PATH}/lib/netlib/condition_win32.cpp
        ${SYSTEMIV_PATH}/lib/netlib/condition_winxp.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_lib.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_socket.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_socket_listener.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_socket_session.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_udp_packet.cpp
        ${SYSTEMIV_PATH}/lib/debug/debug_utils.cpp
    )
elseif(MSVC)
    set(SYSTEMIV_PLATFORM_SOURCES
        ${SYSTEMIV_PATH}/lib/gucci/win32library.cpp
        ${SYSTEMIV_PATH}/lib/gucci/window_manager_d3d11.cpp
        ${SYSTEMIV_PATH}/lib/render/renderer_d3d11.cpp
        ${SYSTEMIV_PATH}/lib/render/antialiasing/msaa_d3d11.cpp
        ${SYSTEMIV_PATH}/lib/render2d/renderer_2d_d3d11.cpp
        ${SYSTEMIV_PATH}/lib/render3d/renderer_3d_d3d11.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_mutex_win32.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_thread_win32.cpp
        ${SYSTEMIV_PATH}/lib/netlib/condition_win32.cpp
        ${SYSTEMIV_PATH}/lib/netlib/condition_winxp.cpp
    )
    set(SYSTEMIV_EXCLUDE
        ${SYSTEMIV_PATH}/lib/netlib/net_mutex_linux.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_thread_linux.cpp
        ${SYSTEMIV_PATH}/lib/netlib/condition_osxlinux.cpp
    )
elseif(APPLE)
    set(SYSTEMIV_PLATFORM_SOURCES
        ${SYSTEMIV_PATH}/lib/spawn.cpp
        ${SYSTEMIV_PATH}/contrib/binreloc/binreloc.c
        ${SYSTEMIV_PATH}/lib/netlib/net_mutex_linux.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_thread_linux.cpp
        ${SYSTEMIV_PATH}/lib/netlib/condition_osxlinux.cpp
        ${SYSTEMIV_PATH}/lib/gucci/macosxlibrary.cpp
    )
    set(SYSTEMIV_EXCLUDE
        ${SYSTEMIV_PATH}/lib/gucci/win32library.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_mutex_win32.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_thread_win32.cpp
        ${SYSTEMIV_PATH}/lib/netlib/condition_win32.cpp
        ${SYSTEMIV_PATH}/lib/netlib/condition_winxp.cpp
    )
else()
    set(SYSTEMIV_PLATFORM_SOURCES
        ${SYSTEMIV_PATH}/lib/spawn.cpp
        ${SYSTEMIV_PATH}/lib/debug/debug_utils_linux.cpp
        ${SYSTEMIV_PATH}/lib/gucci/crashpad.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_mutex_linux.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_thread_linux.cpp
        ${SYSTEMIV_PATH}/lib/netlib/condition_osxlinux.cpp
        ${SYSTEMIV_PATH}/contrib/binreloc/binreloc.c
    )
    set(SYSTEMIV_EXCLUDE
        ${SYSTEMIV_PATH}/lib/gucci/win32library.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_mutex_win32.cpp
        ${SYSTEMIV_PATH}/lib/netlib/net_thread_win32.cpp
        ${SYSTEMIV_PATH}/lib/netlib/condition_win32.cpp
        ${SYSTEMIV_PATH}/lib/netlib/condition_winxp.cpp
    )
endif()

foreach(F ${SYSTEMIV_EXCLUDE})
    list(REMOVE_ITEM SYSTEMIV_SOURCE ${F})
endforeach()

add_library(SystemIV STATIC
    ${SYSTEMIV_SOURCE}
    ${SYSTEMIV_PLATFORM_SOURCES}
    ${GLAD_SOURCE_FILE}
)

target_include_directories(SystemIV PUBLIC
    ${SYSTEMIV_PATH}
    ${SYSTEMIV_PATH}/contrib
    ${SYSTEMIV_PATH}/contrib/unrar
    ${SYSTEMIV_PATH}/linux/include
    ${SYSTEMIV_PATH}/lib/gucci
    ${SYSTEMIV_PATH}/lib/netlib
    ${SYSTEMIV_PATH}/lib/eclipse
    ${GLAD_INCLUDE_DIR}
    ${HIGHWAY_PATH}
    ${TINYGLTF_PATH}
    ${IMGUI_PATH}
    ${IMGUI_PATH}/backends
)

if(DEFINED SDL3_INCLUDE_DIRS)
    target_include_directories(SystemIV PUBLIC ${SDL3_INCLUDE_DIRS} ${SDL3_INCLUDE_DIRS}/SDL3)
endif()

if(DEFINED VORBIS_ARCH_INCLUDE)
    target_include_directories(SystemIV PUBLIC ${SYSTEMIV_PATH}/contrib/${VORBIS_ARCH_INCLUDE})
endif()

if(DEFINED VORBIS_INCLUDE_DIRS)
    target_include_directories(SystemIV PUBLIC ${VORBIS_INCLUDE_DIRS})
endif()

if(DEFINED SYSTEMIV_PATH_PARENT_CRASHPAD)
    target_include_directories(SystemIV PUBLIC
        ${SYSTEMIV_PATH}/contrib/crashpad/linux/include
        ${SYSTEMIV_PATH}/contrib/crashpad/linux/include/third_party/mini_chromium/mini_chromium
    )
endif()

target_compile_definitions(SystemIV PRIVATE RENDERER_OPENGL)

if(MSVC)
    target_compile_definitions(SystemIV PRIVATE
        TARGET_MSVC
        _CRT_SECURE_NO_DEPRECATE
        _CRT_NONSTDC_NO_DEPRECATE
        TOGGLE_SOUND
        OPENMP
        RENDERER_DIRECTX11
        _WINSOCK_DEPRECATED_NO_WARNINGS
    )
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_definitions(SystemIV PRIVATE USE_CRASHREPORTING)
    endif()
endif()

set_target_properties(SystemIV PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)


target_precompile_headers(SystemIV PRIVATE ${SYSTEMIV_PATH}/systemiv.h)

set_source_files_properties(${GLAD_SOURCE_FILE} PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
set_source_files_properties(${SYSTEMIV_PATH}/contrib/binreloc/binreloc.c PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
add_dependencies(SystemIV Unrar ImGui Highway TinyGLTF)
