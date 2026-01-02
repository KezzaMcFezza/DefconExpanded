#ifndef INCLUDED_RENDER3D_MODEL_VERTEX_SHADER_HLSL_H
#define INCLUDED_RENDER3D_MODEL_VERTEX_SHADER_HLSL_H

//
// 3D model vertex shader (HLSL) with per-instance model matrix
// for rendering 3D models with GPU-side transforms

#ifdef RENDERER_DIRECTX11

static const char* MODEL_VERTEX_3D_SHADER_SOURCE_HLSL = R"(
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
    int uFogEnabled;
    int uFogOrientationBased;
    float uFogStart;
    float uFogEnd;
    float4 uFogColor;
    float3 uCameraPos;
    float padding;
};

cbuffer ModelBuffer : register(b2) {
    float4x4 uModelMatrix;
    float4 uModelColor;
    int uUseInstancing;
    int uInstanceCount;
    float padding1;
    float padding2;
    float4x4 uModelMatrices[64];
    float4 uModelColors[64];
};

VSOutput main(VSInput input, uint instanceID : SV_InstanceID) {
    VSOutput output;
    
    float4x4 modelMatrix;
    float4 modelColor;
    
    if (uUseInstancing && instanceID < uInstanceCount) {
        modelMatrix = uModelMatrices[instanceID];
        modelColor = uModelColors[instanceID];
    } else {
        modelMatrix = uModelMatrix;
        modelColor = uModelColor;
    }
    
    float4 worldPos = mul(modelMatrix, float4(input.aPos, 1.0));
    float4 viewPos = mul(uModelView, worldPos);
    output.position = mul(uProjection, viewPos);
    output.vertexColor = input.aColor * modelColor;
    
    if (uFogOrientationBased != 0) {
        float3 surfaceNormal = normalize(worldPos.xyz);
        float3 cameraDir = normalize(uCameraPos - worldPos.xyz);
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

