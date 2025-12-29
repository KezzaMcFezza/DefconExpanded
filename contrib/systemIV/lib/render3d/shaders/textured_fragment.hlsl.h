#ifndef INCLUDED_RENDER3D_TEXTURED_FRAGMENT_SHADER_HLSL_H
#define INCLUDED_RENDER3D_TEXTURED_FRAGMENT_SHADER_HLSL_H

//
// 3D textured fragment shader (HLSL)
// for unit sprites, text, and other textured 3D elements

#ifdef RENDERER_DIRECTX11

static const char* TEXTURED_FRAGMENT_3D_SHADER_SOURCE_HLSL = R"(
Texture2D ourTexture : register(t0);
SamplerState ourSampler : register(s0);

struct PSInput {
    float4 position : SV_Position;
    float3 Normal : NORMAL;
    float4 vertexColor : COLOR;
    float2 texCoord : TEXCOORD0;
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
    float4 texColor = ourTexture.Sample(ourSampler, input.texCoord);
    
    if (texColor.a < 0.01) {
        discard;
    }
    
    float4 finalColor = texColor * input.vertexColor;
    
    if (uFogEnabled) {
        float pixelLuminance = (finalColor.r + finalColor.g + finalColor.b) / 3.0;
        if (pixelLuminance > 0.05) {
            if (uFogOrientationBased) {
                finalColor.rgb = lerp(finalColor.rgb, uFogColor.rgb, input.fogFactor);
            } else {
                float distanceFogFactor = saturate((uFogEnd - input.fogDistance) / (uFogEnd - uFogStart));
                finalColor.rgb = lerp(uFogColor.rgb, finalColor.rgb, distanceFogFactor);
            }
        }
    }
    
    return finalColor;
}
)";

#endif // RENDERER_DIRECTX11
#endif

