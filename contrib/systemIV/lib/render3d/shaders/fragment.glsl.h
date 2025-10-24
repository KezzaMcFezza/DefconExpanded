#ifndef INCLUDED_RENDER3D_FRAGMENT_SHADER_H
#define INCLUDED_RENDER3D_FRAGMENT_SHADER_H

//
// 3D fragment shader (non textured)
// for coastlines, borders, trails, and other non textured 3D geometry

#if defined(TARGET_EMSCRIPTEN)

//
// WebGL 2.0 (OpenGL ES 3.0)

static const char* FRAGMENT_3D_SHADER_SOURCE = R"(#version 300 es
precision mediump float;

in vec4 vertexColor;
in float fogFactor;
in float fogDistance;

uniform bool uFogEnabled;
uniform bool uFogOrientationBased;
uniform float uFogStart;
uniform float uFogEnd;
uniform vec4 uFogColor;

out vec4 FragColor;

void main() {
    vec4 finalColor = vertexColor;
    
    if (uFogEnabled) {
        if (uFogOrientationBased) {
            finalColor.rgb = mix(vertexColor.rgb, uFogColor.rgb, fogFactor);
        } else {
            float distanceFogFactor = clamp((uFogEnd - fogDistance) / (uFogEnd - uFogStart), 0.0, 1.0);
            finalColor.rgb = mix(uFogColor.rgb, vertexColor.rgb, distanceFogFactor);
        }
    }
    
    FragColor = finalColor;
}
)";

#elif defined(TARGET_OS_MACOSX)

//
// macOS desktop OpenGL 3.2 Core (GLSL 1.50)

static const char* FRAGMENT_3D_SHADER_SOURCE = R"(#version 150 core

in vec4 vertexColor;
in float fogFactor;
in float fogDistance;

uniform bool uFogEnabled;
uniform bool uFogOrientationBased;
uniform float uFogStart;
uniform float uFogEnd;
uniform vec4 uFogColor;

out vec4 FragColor;

void main() {
    vec4 finalColor = vertexColor;
    
    if (uFogEnabled) {
        if (uFogOrientationBased) {
            finalColor.rgb = mix(vertexColor.rgb, uFogColor.rgb, fogFactor);
        } else {
            float distanceFogFactor = clamp((uFogEnd - fogDistance) / (uFogEnd - uFogStart), 0.0, 1.0);
            finalColor.rgb = mix(uFogColor.rgb, vertexColor.rgb, distanceFogFactor);
        }
    }
    
    FragColor = finalColor;
}
)";

#else

//
// windows/linux desktop OpenGL 3.3 Core/Compatibility

static const char* FRAGMENT_3D_SHADER_SOURCE = R"(#version 330 core

in vec4 vertexColor;
in float fogFactor;
in float fogDistance;

uniform bool uFogEnabled;
uniform bool uFogOrientationBased;
uniform float uFogStart;
uniform float uFogEnd;
uniform vec4 uFogColor;

out vec4 FragColor;

void main() {
    vec4 finalColor = vertexColor;
    
    if (uFogEnabled) {
        if (uFogOrientationBased) {
            finalColor.rgb = mix(vertexColor.rgb, uFogColor.rgb, fogFactor);
        } else {
            float distanceFogFactor = clamp((uFogEnd - fogDistance) / (uFogEnd - uFogStart), 0.0, 1.0);
            finalColor.rgb = mix(uFogColor.rgb, vertexColor.rgb, distanceFogFactor);
        }
    }
    
    FragColor = finalColor;
}
)";

#endif
#endif