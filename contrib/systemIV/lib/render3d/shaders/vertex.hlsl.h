#ifndef INCLUDED_RENDER3D_VERTEX_SHADER_HLSL_H
#define INCLUDED_RENDER3D_VERTEX_SHADER_HLSL_H

//
// 3D vertex shader (HLSL) - non textured
// for coastlines, borders, trails, and other non-textured 3D geometry

#ifdef RENDERER_DIRECTX11

static const char* VERTEX_3D_SHADER_SOURCE_HLSL = R"(
struct VSInput {
    float3 aPos : POSITION;
    float4 aColor : COLOR;
};

struct VSOutput {
    float4 position : SV_Position;
    float4 vertexColor : COLOR;
    float fogFactor : FOGFACTOR;
    float fogDistance : FOGDISTANCE;
};

cbuffer TransformBuffer : register(b0) {
    float4x4 uProjection;
    float4x4 uModelView;
};

cbuffer FogBuffer : register(b1) {
    bool uFogEnabled;
    bool uFogOrientationBased;
    float uFogStart;
    float uFogEnd;
    float4 uFogColor;
    float3 uCameraPos;
    float padding;
};

VSOutput main(VSInput input) {
    VSOutput output;
    
    float4 viewPos = mul(uModelView, float4(input.aPos, 1.0));
    output.position = mul(uProjection, viewPos);
    output.vertexColor = input.aColor;
    
    if (uFogOrientationBased) {
        float3 surfaceNormal = normalize(input.aPos);
        float3 cameraDir = normalize(uCameraPos - input.aPos);
        float dotProduct = dot(surfaceNormal, cameraDir);
        output.fogFactor = 1.0 - saturate(dotProduct);
        output.fogDistance = 0.0;
    } else {
        output.fogDistance = length(viewPos.xyz);
        output.fogFactor = 0.0;
    }
    
    return output;
}
)";

#endif // RENDERER_DIRECTX11
#endif

