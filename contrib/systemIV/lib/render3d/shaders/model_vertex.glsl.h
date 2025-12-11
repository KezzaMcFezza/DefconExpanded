#ifndef INCLUDED_RENDER3D_MODEL_VERTEX_SHADER_H
#define INCLUDED_RENDER3D_MODEL_VERTEX_SHADER_H

//
// 3D model vertex shader with per-instance model matrix
// for rendering 3D models with GPU-side transforms

#if defined(TARGET_EMSCRIPTEN)

//
// WebGL 2.0 (OpenGL ES 3.0)

static const char* MODEL_VERTEX_3D_SHADER_SOURCE = R"(#version 300 es
precision highp float;

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec4 aColor;

uniform mat4 uProjection;
uniform mat4 uModelView;
uniform mat4 uModelMatrix;
uniform vec4 uModelColor;
uniform mat4 uModelMatrices[64];
uniform vec4 uModelColors[64];
uniform int uInstanceCount;
uniform bool uUseInstancing;
uniform vec3 uCameraPos;
uniform bool uFogOrientationBased;

out vec4 vertexColor;
out float fogFactor;
out float fogDistance;

void main() {
    mat4 modelMatrix;
    vec4 modelColor;
    
    if (uUseInstancing && gl_InstanceID < uInstanceCount) {
        modelMatrix = uModelMatrices[gl_InstanceID];
        modelColor = uModelColors[gl_InstanceID];
    } else {
        modelMatrix = uModelMatrix;
        modelColor = uModelColor;
    }
    
    vec4 worldPos = modelMatrix * vec4(aPos, 1.0);
    vec4 viewPos = uModelView * worldPos;
    gl_Position = uProjection * viewPos;
    vertexColor = aColor * modelColor;
    
    if (uFogOrientationBased) {
        vec3 surfaceNormal = normalize(worldPos.xyz);
        vec3 cameraDir = normalize(uCameraPos - worldPos.xyz);
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

static const char* MODEL_VERTEX_3D_SHADER_SOURCE = R"(#version 150 core

in vec3 aPos;
in vec4 aColor;

uniform mat4 uProjection;
uniform mat4 uModelView;
uniform mat4 uModelMatrix;
uniform vec4 uModelColor;
uniform mat4 uModelMatrices[64];
uniform vec4 uModelColors[64];
uniform int uInstanceCount;
uniform bool uUseInstancing;
uniform vec3 uCameraPos;
uniform bool uFogOrientationBased;

out vec4 vertexColor;
out float fogFactor;
out float fogDistance;

void main() {
    mat4 modelMatrix;
    vec4 modelColor;
    
    if (uUseInstancing && gl_InstanceID < uInstanceCount) {
        modelMatrix = uModelMatrices[gl_InstanceID];
        modelColor = uModelColors[gl_InstanceID];
    } else {
        modelMatrix = uModelMatrix;
        modelColor = uModelColor;
    }
    
    vec4 worldPos = modelMatrix * vec4(aPos, 1.0);
    vec4 viewPos = uModelView * worldPos;
    gl_Position = uProjection * viewPos;
    vertexColor = aColor * modelColor;
    
    if (uFogOrientationBased) {
        vec3 surfaceNormal = normalize(worldPos.xyz);
        vec3 cameraDir = normalize(uCameraPos - worldPos.xyz);
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

static const char* MODEL_VERTEX_3D_SHADER_SOURCE = R"(#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec4 aColor;

uniform mat4 uProjection;
uniform mat4 uModelView;
uniform mat4 uModelMatrix;
uniform vec4 uModelColor;
uniform mat4 uModelMatrices[64];
uniform vec4 uModelColors[64];
uniform int uInstanceCount;
uniform bool uUseInstancing;
uniform vec3 uCameraPos;
uniform bool uFogOrientationBased;

out vec4 vertexColor;
out float fogFactor;
out float fogDistance;

void main() {
    mat4 modelMatrix;
    vec4 modelColor;
    
    if (uUseInstancing && gl_InstanceID < uInstanceCount) {
        modelMatrix = uModelMatrices[gl_InstanceID];
        modelColor = uModelColors[gl_InstanceID];
    } else {
        modelMatrix = uModelMatrix;
        modelColor = uModelColor;
    }
    
    vec4 worldPos = modelMatrix * vec4(aPos, 1.0);
    vec4 viewPos = uModelView * worldPos;
    gl_Position = uProjection * viewPos;
    vertexColor = aColor * modelColor;
    
    if (uFogOrientationBased) {
        vec3 surfaceNormal = normalize(worldPos.xyz);
        vec3 cameraDir = normalize(uCameraPos - worldPos.xyz);
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

