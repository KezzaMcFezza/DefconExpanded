#ifndef INCLUDED_RENDER2D_TEXTURE_FRAGMENT_SHADER_HLSL_H
#define INCLUDED_RENDER2D_TEXTURE_FRAGMENT_SHADER_HLSL_H

//
// 2D texture fragment shader (HLSL)
// for textured quads (sprites, text, UI elements)

#ifdef RENDERER_DIRECTX11

static const char* TEXTURE_FRAGMENT_2D_SHADER_SOURCE_HLSL = R"(
Texture2D ourTexture : register(t0);
SamplerState ourSampler : register(s0);

struct PSInput {
    float4 position : SV_Position;
    float4 vertexColor : COLOR;
    float2 texCoord : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target {
    float4 texColor = ourTexture.Sample(ourSampler, input.texCoord);
    return texColor * input.vertexColor;
}
)";

#endif // RENDERER_DIRECTX11
#endif

