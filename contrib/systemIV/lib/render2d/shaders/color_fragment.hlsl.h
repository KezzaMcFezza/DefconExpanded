#ifndef INCLUDED_RENDER2D_COLOR_FRAGMENT_SHADER_HLSL_H
#define INCLUDED_RENDER2D_COLOR_FRAGMENT_SHADER_HLSL_H

//
// 2D color fragment shader (HLSL)
// for non-textured primitives (lines, rectangles, circles)

#ifdef RENDERER_DIRECTX11

static const char* COLOR_FRAGMENT_2D_SHADER_SOURCE_HLSL = R"(
struct PSInput {
    float4 position : SV_Position;
    float4 vertexColor : COLOR;
    float2 texCoord : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target {
    return input.vertexColor;
}
)";

#endif // RENDERER_DIRECTX11
#endif

