#ifndef INCLUDED_RENDER2D_VERTEX_SHADER_HLSL_H
#define INCLUDED_RENDER2D_VERTEX_SHADER_HLSL_H

//
// 2D vertex shader (HLSL)
// for both color and texture rendering

#ifdef RENDERER_DIRECTX11

static const char* VERTEX_2D_SHADER_SOURCE_HLSL = R"(
struct VSInput {
    float2 aPos : POSITION;
    float4 aColor : COLOR;
    float2 aTexCoord : TEXCOORD0;
};

struct VSOutput {
    float4 position : SV_Position;
    float4 vertexColor : COLOR;
    float2 texCoord : TEXCOORD0;
};

cbuffer TransformBuffer : register(b0) {
    float4x4 uProjection;
    float4x4 uModelView;
};

VSOutput main(VSInput input) {
    VSOutput output;
    float4 pos = float4(input.aPos, 0.0, 1.0);
    pos = mul(uModelView, pos);
    pos = mul(uProjection, pos);
    output.position = pos;
    output.vertexColor = input.aColor;
    output.texCoord = input.aTexCoord;
    return output;
}
)";

#endif // RENDERER_DIRECTX11
#endif

