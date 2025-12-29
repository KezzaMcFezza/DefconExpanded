#ifndef INCLUDED_RENDER3D_FRAGMENT_SHADER_HLSL_H
#define INCLUDED_RENDER3D_FRAGMENT_SHADER_HLSL_H

//
// 3D fragment shader (HLSL) - non textured
// for coastlines, borders, trails, and other non textured 3D geometry

#ifdef RENDERER_DIRECTX11

static const char* FRAGMENT_3D_SHADER_SOURCE_HLSL = R"(
struct PSInput {
    float4 position : SV_Position;
    float4 vertexColor : COLOR;
    float fogFactor : FOGFACTOR;
    float fogDistance : FOGDISTANCE;
};

cbuffer FogBuffer : register(b1) {
    bool uFogEnabled;
    bool uFogOrientationBased;
    float uFogStart;
    float uFogEnd;
    float4 uFogColor;
};

float4 main(PSInput input) : SV_Target {
    float4 finalColor = input.vertexColor;
    
    if (uFogEnabled) {
        if (uFogOrientationBased) {
            finalColor.rgb = lerp(input.vertexColor.rgb, uFogColor.rgb, input.fogFactor);
        } else {
            float distanceFogFactor = saturate((uFogEnd - input.fogDistance) / (uFogEnd - uFogStart));
            finalColor.rgb = lerp(uFogColor.rgb, input.vertexColor.rgb, distanceFogFactor);
        }
    }
    
    return finalColor;
}
)";

#endif // RENDERER_DIRECTX11
#endif

