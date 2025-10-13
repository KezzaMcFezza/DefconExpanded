#ifndef INCLUDED_RENDER3D_TEXTURED_FRAGMENT_SHADER_H
#define INCLUDED_RENDER3D_TEXTURED_FRAGMENT_SHADER_H

//
// 3D textured fragment shader
// for unit sprites, text, and other textured 3D elements

#if defined(TARGET_EMSCRIPTEN)

//
// WebGL 2.0 (OpenGL ES 3.0)

static const char* TEXTURED_FRAGMENT_3D_SHADER_SOURCE = R"(#version 300 es
precision mediump float;

in vec4 vertexColor;
in vec2 texCoord;
in float fogFactor;
in float fogDistance;

uniform sampler2D ourTexture;
uniform bool uFogEnabled;
uniform bool uFogOrientationBased;
uniform float uFogStart;
uniform float uFogEnd;
uniform vec4 uFogColor;

out vec4 FragColor;

void main() {
    vec4 texColor = texture(ourTexture, texCoord);

    if (texColor.a < 0.01) {
        discard;
    }
    
    vec4 finalColor = texColor * vertexColor;
    
    if (uFogEnabled) {
        float pixelLuminance = (finalColor.r + finalColor.g + finalColor.b) / 3.0;
        if (pixelLuminance > 0.05) {
            if (uFogOrientationBased) {
                finalColor.rgb = mix(finalColor.rgb, uFogColor.rgb, fogFactor);
            } else {
                float distanceFogFactor = clamp((uFogEnd - fogDistance) / (uFogEnd - uFogStart), 0.0, 1.0);
                finalColor.rgb = mix(uFogColor.rgb, finalColor.rgb, distanceFogFactor);
            }
        }
    }
    
    FragColor = finalColor;
}
)";

#elif defined(TARGET_OS_MACOSX)

//
// macOS desktop OpenGL 3.2 Core (GLSL 1.50)

static const char* TEXTURED_FRAGMENT_3D_SHADER_SOURCE = R"(#version 150 core

in vec4 vertexColor;
in vec2 texCoord;
in float fogFactor;
in float fogDistance;

uniform sampler2D ourTexture;
uniform bool uFogEnabled;
uniform bool uFogOrientationBased;
uniform float uFogStart;
uniform float uFogEnd;
uniform vec4 uFogColor;

out vec4 FragColor;

void main() {
    vec4 texColor = texture(ourTexture, texCoord);
    
    if (texColor.a < 0.01) {
        discard;
    }

    vec4 finalColor = texColor * vertexColor;
    
    if (uFogEnabled) {
        float pixelLuminance = (finalColor.r + finalColor.g + finalColor.b) / 3.0;
        if (pixelLuminance > 0.05) {
            if (uFogOrientationBased) {
                finalColor.rgb = mix(finalColor.rgb, uFogColor.rgb, fogFactor);
            } else {
                float distanceFogFactor = clamp((uFogEnd - fogDistance) / (uFogEnd - uFogStart), 0.0, 1.0);
                finalColor.rgb = mix(uFogColor.rgb, finalColor.rgb, distanceFogFactor);
            }
        }
    }
    
    FragColor = finalColor;
}
)";

#else

//
// windows/linux desktop OpenGL 3.3 Core/Compatibility

static const char* TEXTURED_FRAGMENT_3D_SHADER_SOURCE = R"(#version 330 core

in vec4 vertexColor;
in vec2 texCoord;
in float fogFactor;
in float fogDistance;

uniform sampler2D ourTexture;
uniform bool uFogEnabled;
uniform bool uFogOrientationBased;
uniform float uFogStart;
uniform float uFogEnd;
uniform vec4 uFogColor;

out vec4 FragColor;

void main() {
    vec4 texColor = texture(ourTexture, texCoord);
    
    if (texColor.a < 0.01) {
        discard;
    }

    vec4 finalColor = texColor * vertexColor;
    
    if (uFogEnabled) {
        float pixelLuminance = (finalColor.r + finalColor.g + finalColor.b) / 3.0;
        if (pixelLuminance > 0.05) {
            if (uFogOrientationBased) {
                finalColor.rgb = mix(finalColor.rgb, uFogColor.rgb, fogFactor);
            } else {
                float distanceFogFactor = clamp((uFogEnd - fogDistance) / (uFogEnd - uFogStart), 0.0, 1.0);
                finalColor.rgb = mix(uFogColor.rgb, finalColor.rgb, distanceFogFactor);
            }
        }
    }
    
    FragColor = finalColor;
}
)";

#endif
#endif