#ifndef INCLUDED_RENDER3D_VERTEX_SHADER_H
#define INCLUDED_RENDER3D_VERTEX_SHADER_H

//
// 3D vertex shader (non textured)
// for coastlines, borders, trails, and other non-textured 3D geometry

#if defined(TARGET_EMSCRIPTEN)

//
// WebGL 2.0 (OpenGL ES 3.0)

static const char* VERTEX_3D_SHADER_SOURCE = R"(#version 300 es
precision highp float;

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec4 aColor;

uniform mat4 uProjection;
uniform mat4 uModelView;
uniform vec3 uCameraPos;
uniform bool uFogOrientationBased;

out vec4 vertexColor;
out float fogFactor;
out float fogDistance;

void main() {
    vec4 viewPos = uModelView * vec4(aPos, 1.0);
    gl_Position = uProjection * viewPos;
    vertexColor = aColor;
    
    if (uFogOrientationBased) {
        vec3 surfaceNormal = normalize(aPos);
        vec3 cameraDir = normalize(uCameraPos - aPos);
        float dotProduct = dot(surfaceNormal, cameraDir);
        fogFactor = 1.0 - clamp(dotProduct, 0.0, 1.0);
        fogDistance = 0.0; 
    } else {
        fogDistance = length(viewPos.xyz);
        fogFactor = 0.0; 
    }
}
)";

#elif defined(TARGET_OS_MACOSX)

//
// macOS desktop OpenGL 3.2 Core (GLSL 1.50)

static const char* VERTEX_3D_SHADER_SOURCE = R"(#version 150 core

in vec3 aPos;
in vec4 aColor;

uniform mat4 uProjection;
uniform mat4 uModelView;
uniform vec3 uCameraPos;
uniform bool uFogOrientationBased;

out vec4 vertexColor;
out float fogFactor;
out float fogDistance;

void main() {
    vec4 viewPos = uModelView * vec4(aPos, 1.0);
    gl_Position = uProjection * viewPos;
    vertexColor = aColor;
    
    if (uFogOrientationBased) {
        vec3 surfaceNormal = normalize(aPos);
        vec3 cameraDir = normalize(uCameraPos - aPos);
        float dotProduct = dot(surfaceNormal, cameraDir);
        fogFactor = 1.0 - clamp(dotProduct, 0.0, 1.0);
        fogDistance = 0.0; 
    } else {
        fogDistance = length(viewPos.xyz);
        fogFactor = 0.0; 
    }
}
)";

#else

//
// windows/linux desktop OpenGL 3.3 Core/Compatibility

static const char* VERTEX_3D_SHADER_SOURCE = R"(#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec4 aColor;

uniform mat4 uProjection;
uniform mat4 uModelView;
uniform vec3 uCameraPos;
uniform bool uFogOrientationBased;

out vec4 vertexColor;
out float fogFactor;
out float fogDistance;

void main() {
    vec4 viewPos = uModelView * vec4(aPos, 1.0);
    gl_Position = uProjection * viewPos;
    vertexColor = aColor;
    
    if (uFogOrientationBased) {
        vec3 surfaceNormal = normalize(aPos);
        vec3 cameraDir = normalize(uCameraPos - aPos);
        float dotProduct = dot(surfaceNormal, cameraDir);
        fogFactor = 1.0 - clamp(dotProduct, 0.0, 1.0);
        fogDistance = 0.0; 
    } else {
        fogDistance = length(viewPos.xyz);
        fogFactor = 0.0; 
    }
}
)";

#endif
#endif