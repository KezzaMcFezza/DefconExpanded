#ifndef INCLUDED_RENDER2D_TEXTURE_FRAGMENT_SHADER_H
#define INCLUDED_RENDER2D_TEXTURE_FRAGMENT_SHADER_H

//
// 2D texture fragment shader
// for textured quads (sprites, text, UI elements)

#if defined(TARGET_EMSCRIPTEN)

//
// WebGL 2.0 (OpenGL ES 3.0)

static const char* TEXTURE_FRAGMENT_2D_SHADER_SOURCE = R"(#version 300 es
precision mediump float;
in vec4 vertexColor;
in vec2 texCoord;
uniform sampler2D ourTexture;
out vec4 FragColor;
void main() {
    FragColor = texture(ourTexture, texCoord) * vertexColor;
}
)";

#elif defined(TARGET_OS_MACOSX)

//
// macOS desktop OpenGL 3.2 Core (GLSL 1.50)

static const char* TEXTURE_FRAGMENT_2D_SHADER_SOURCE = R"(#version 150 core
in vec4 vertexColor;
in vec2 texCoord;
uniform sampler2D ourTexture;
out vec4 FragColor;
void main() {
    FragColor = texture(ourTexture, texCoord) * vertexColor;
}
)";

#else

//
// windows/linux desktop OpenGL 3.3 Core/Compatibility

static const char* TEXTURE_FRAGMENT_2D_SHADER_SOURCE = R"(#version 330 core
in vec4 vertexColor;
in vec2 texCoord;
uniform sampler2D ourTexture;
out vec4 FragColor;
void main() {
    FragColor = texture(ourTexture, texCoord) * vertexColor;
}
)";

#endif
#endif