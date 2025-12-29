#ifndef INCLUDED_RENDER3D_TEXTURED_VERTEX_SHADER_HLSL_H
#define INCLUDED_RENDER3D_TEXTURED_VERTEX_SHADER_HLSL_H

//
// 3D textured vertex shader (HLSL)
// for unit sprites, text, and other textured 3D elements

#ifdef RENDERER_DIRECTX11

static const char* TEXTURED_VERTEX_3D_SHADER_SOURCE_HLSL = R"(
struct VSInput {
    float3 aPos : POSITION;
    float3 aNormal : NORMAL;
    float4 aColor : COLOR;
    float2 aTexCoord : TEXCOORD0;
};

struct VSOutput {
    float4 position : SV_Position;
    float3 Normal : NORMAL;
    float4 vertexColor : COLOR;
    float2 texCoord : TEXCOORD0;
    float fogFactor : FOGFACTOR;
    float fogDistance : FOGDISTANCE;
};

cbuffer TransformBuffer : register(b0) {
    float4x4 uProjection;
    float4x4 uModelView;
    float3 uCameraPos;
    bool uFogOrientationBased;
};

VSOutput main(VSInput input) {
    VSOutput output;
    
    float4 viewPos = mul(uModelView, float4(input.aPos, 1.0));
    output.position = mul(uProjection, viewPos);
    
    // Calculate normal matrix (transpose of inverse of modelview)
    // For proper normal transformation, we need inverse transpose
    // Simplified: assume uniform scaling, so we can use transpose
    float3x3 normalMatrix = (float3x3)uModelView;
    normalMatrix = transpose(normalMatrix);
    output.Normal = normalize(mul(normalMatrix, input.aNormal));
    
    output.vertexColor = input.aColor;
    output.texCoord = input.aTexCoord;
    
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

