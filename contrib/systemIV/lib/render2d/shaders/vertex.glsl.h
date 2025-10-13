#ifndef INCLUDED_RENDER2D_VERTEX_SHADER_H
#define INCLUDED_RENDER2D_VERTEX_SHADER_H

//
// 2D vertex shader
// for both color and texture rendering

#if defined(TARGET_EMSCRIPTEN)

//
// WebGL 2.0 (OpenGL ES 3.0)

static const char* VERTEX_2D_SHADER_SOURCE = R"(#version 300 es
precision highp float;
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec4 aColor;
layout (location = 2) in vec2 aTexCoord;
uniform mat4 uProjection;
uniform mat4 uModelView;
out vec4 vertexColor;
out vec2 texCoord;
void main() {
    gl_Position = uProjection * uModelView * vec4(aPos, 0.0, 1.0);
    vertexColor = aColor;
    texCoord = aTexCoord;
}
)";

#elif defined(TARGET_OS_MACOSX)

//
// macOS desktop OpenGL 3.2 Core (GLSL 1.50)
// GLSL 1.50 doesnt support layout qualifiers, bind before linking

static const char* VERTEX_2D_SHADER_SOURCE = R"(#version 150 core
in vec2 aPos;
in vec4 aColor;
in vec2 aTexCoord;
uniform mat4 uProjection;
uniform mat4 uModelView;
out vec4 vertexColor;
out vec2 texCoord;
void main() {
    gl_Position = uProjection * uModelView * vec4(aPos, 0.0, 1.0);
    vertexColor = aColor;
    texCoord = aTexCoord;
}
)";

#else

//
// windows/linux desktop OpenGL 3.3 Core/Compatibility

static const char* VERTEX_2D_SHADER_SOURCE = R"(#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec4 aColor;
layout (location = 2) in vec2 aTexCoord;
uniform mat4 uProjection;
uniform mat4 uModelView;
out vec4 vertexColor;
out vec2 texCoord;
void main() {
    gl_Position = uProjection * uModelView * vec4(aPos, 0.0, 1.0);
    vertexColor = aColor;
    texCoord = aTexCoord;
}
)";

#endif
#endif