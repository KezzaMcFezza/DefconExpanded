#ifndef INCLUDED_RENDER2D_COLOR_FRAGMENT_SHADER_H
#define INCLUDED_RENDER2D_COLOR_FRAGMENT_SHADER_H

//
// 2D color fragment shader
// for non-textured primitives (lines, rectangles, circles)

#if defined(TARGET_EMSCRIPTEN)

//
// WebGL 2.0 (OpenGL ES 3.0)

static const char* COLOR_FRAGMENT_2D_SHADER_SOURCE = R"(#version 300 es
precision mediump float;
in vec4 vertexColor;
out vec4 FragColor;
void main() {
    FragColor = vertexColor;
}
)";

#elif defined(TARGET_OS_MACOSX)

//
// macOS desktop OpenGL 3.2 Core (GLSL 1.50)

static const char* COLOR_FRAGMENT_2D_SHADER_SOURCE = R"(#version 150 core
in vec4 vertexColor;
out vec4 FragColor;
void main() {
    FragColor = vertexColor;
}
)";

#else

//
// windows/linux desktop OpenGL 3.3 Core/Compatibility

static const char* COLOR_FRAGMENT_2D_SHADER_SOURCE = R"(#version 330 core
in vec4 vertexColor;
out vec4 FragColor;
void main() {
    FragColor = vertexColor;
}
)";

#endif
#endif