#ifndef INCLUDED_RENDER2D_LINE_GEOMETRY_SHADER_HLSL_H
#define INCLUDED_RENDER2D_LINE_GEOMETRY_SHADER_HLSL_H

// CREDIT GOES TO:
// https://stackoverflow.com/questions/42510542/render-thick-lines-with-instanced-rendering-in-directx-11
//
// His shader helped with the basic concept, the rounded caps have been
// converted to miter caps and vertex counts have been reduced to prevent
// a huge performance impact.

//
// 2D Line Geometry Shaders (HLSL)
// Two variants: Thin lines (no caps) and Thick lines (mitered caps)
//
// This method of line expansion is extremely slow and with line widths
// exceeding 1.8 the performance is worse than glLineWidth(), down the
// line i will implement a permanant solution that actuall gives us
// respectable performance, but for now this will suffice for feature
// parity with OpenGL.

#ifdef RENDERER_DIRECTX11

// ================
// THIN LINE SHADER
// Great for performance, line widths below 1.8 look the same as OpenGL.

static const char* LINE_GEOMETRY_2D_THIN_SHADER_SOURCE_HLSL = R"(
struct GSInput {
    float4 position : SV_Position;
    float4 vertexColor : COLOR;
    float2 texCoord : TEXCOORD0;
};

struct GSOutput {
    float4 position : SV_Position;
    float4 vertexColor : COLOR;
    float2 texCoord : TEXCOORD0;
    float distanceFromCenter : TEXCOORD1;
};

cbuffer TransformBuffer : register(b0) {
    float4x4 uProjection;
    float4x4 uModelView;
};

cbuffer LineWidthBuffer : register(b1) {
    float lineWidth;
    float viewportWidth;
    float viewportHeight;
    float padding;
};

[maxvertexcount(6)]
void main(line GSInput input[2], inout TriangleStream<GSOutput> triangleStream)
{
    GSOutput output = (GSOutput)0;
    
    float4 p0 = input[0].position;
    float4 p1 = input[1].position;
    
    float fPoint0w = p0.w;
    float fPoint1w = p1.w;
    float rcp0w = rcp(p0.w);
    float rcp1w = rcp(p1.w);

    //
    // Normalize by W for perspective projection

    float2 p0xy = p0.xy * rcp0w;
    float2 p1xy = p1.xy * rcp1w;

    //
    // Calculate line thickness in NDC space
    // 1.25 is a magic number that makes the lines look the same as OpenGL.

    float fThickness = (lineWidth / viewportHeight) * 1.25f;
    float rcpAspectRatio = viewportHeight / viewportWidth;

    //
    // Calculate perpendicular direction

    float2 dir = normalize(p1xy - p0xy);
    float2 perp = float2(-dir.y * fThickness, dir.x * fThickness);
    perp.x *= rcpAspectRatio;
    
    //
    // Pre-calculate vertex positions

    float2 p0PlusPerp = p0xy + perp;
    float2 p0MinusPerp = p0xy - perp;
    float2 p1PlusPerp = p1xy + perp;
    float2 p1MinusPerp = p1xy - perp;
    
    output.texCoord = float2(0.0f, 0.0f);
    output.distanceFromCenter = 1.0f;
    
    // Triangle 1
    output.vertexColor = input[0].vertexColor;
    output.position = float4(p0PlusPerp, 0.0f, 1.0f) * fPoint0w;
    triangleStream.Append(output);
    
    output.position = float4(p0MinusPerp, 0.0f, 1.0f) * fPoint0w;
    triangleStream.Append(output);
    
    output.vertexColor = input[1].vertexColor;
    output.position = float4(p1MinusPerp, 0.0f, 1.0f) * fPoint1w;
    triangleStream.Append(output);
    
    triangleStream.RestartStrip();
    
    // Triangle 2
    output.vertexColor = input[0].vertexColor;
    output.position = float4(p0PlusPerp, 0.0f, 1.0f) * fPoint0w;
    triangleStream.Append(output);
    
    output.vertexColor = input[1].vertexColor;
    output.position = float4(p1MinusPerp, 0.0f, 1.0f) * fPoint1w;
    triangleStream.Append(output);
    
    output.position = float4(p1PlusPerp, 0.0f, 1.0f) * fPoint1w;
    triangleStream.Append(output);
}
)";

// ===================================
// THICK LINE SHADER WITH MITERED CAPS 
// Performance hit is noticable here, but without miter caps
// the lines look absolutely dreadful.

static const char* LINE_GEOMETRY_2D_THICK_SHADER_SOURCE_HLSL = R"(
struct GSInput {
    float4 position : SV_Position;
    float4 vertexColor : COLOR;
    float2 texCoord : TEXCOORD0;
};

struct GSOutput {
    float4 position : SV_Position;
    float4 vertexColor : COLOR;
    float2 texCoord : TEXCOORD0;
    float distanceFromCenter : TEXCOORD1;
};

cbuffer TransformBuffer : register(b0) {
    float4x4 uProjection;
    float4x4 uModelView;
};

cbuffer LineWidthBuffer : register(b1) {
    float lineWidth;
    float viewportWidth;
    float viewportHeight;
    float padding;
};

[maxvertexcount(10)]
void main(line GSInput input[2], inout TriangleStream<GSOutput> triangleStream)
{
    GSOutput output = (GSOutput)0;
    
    float4 p0 = input[0].position;
    float4 p1 = input[1].position;
    
    float fPoint0w = p0.w;
    float fPoint1w = p1.w;
    float rcp0w = rcp(p0.w);
    float rcp1w = rcp(p1.w);

    //
    // Normalize by W for perspective projection

    float2 p0xy = p0.xy * rcp0w;
    float2 p1xy = p1.xy * rcp1w;

    //
    // Calculate line thickness in NDC space
    // 1.25 is a magic number that makes the lines look the same as OpenGL.

    float fThickness = (lineWidth / viewportHeight) * 1.25f;
    float rcpAspectRatio = viewportHeight / viewportWidth;

    //
    // Calculate perpendicular direction

    float2 dir = normalize(p1xy - p0xy);
    float2 perp = float2(-dir.y * fThickness, dir.x * fThickness);
    perp.x *= rcpAspectRatio;
    
    //
    // Pre-calculate vertex positions for main quad

    float2 p0PlusPerp = p0xy + perp;
    float2 p0MinusPerp = p0xy - perp;
    float2 p1PlusPerp = p1xy + perp;
    float2 p1MinusPerp = p1xy - perp;
    
    output.texCoord = float2(0.0f, 0.0f);
    output.distanceFromCenter = 1.0f;
    
    // Triangle 1
    output.vertexColor = input[0].vertexColor;
    output.position = float4(p0PlusPerp, 0.0f, 1.0f) * fPoint0w;
    triangleStream.Append(output);
    
    output.position = float4(p0MinusPerp, 0.0f, 1.0f) * fPoint0w;
    triangleStream.Append(output);
    
    output.vertexColor = input[1].vertexColor;
    output.position = float4(p1MinusPerp, 0.0f, 1.0f) * fPoint1w;
    triangleStream.Append(output);
    
    triangleStream.RestartStrip();
    
    // Triangle 2
    output.vertexColor = input[0].vertexColor;
    output.position = float4(p0PlusPerp, 0.0f, 1.0f) * fPoint0w;
    triangleStream.Append(output);
    
    output.vertexColor = input[1].vertexColor;
    output.position = float4(p1MinusPerp, 0.0f, 1.0f) * fPoint1w;
    triangleStream.Append(output);
    
    output.position = float4(p1PlusPerp, 0.0f, 1.0f) * fPoint1w;
    triangleStream.Append(output);
    
    triangleStream.RestartStrip();
    
    // Square cap at point 0 (1 triangle)
    output.vertexColor = input[0].vertexColor;
    output.position = float4(p0PlusPerp, 0.0f, 1.0f) * fPoint0w;
    triangleStream.Append(output);
    
    output.position = float4(p0xy, 0.0f, 1.0f) * fPoint0w;
    triangleStream.Append(output);
    
    output.position = float4(p0MinusPerp, 0.0f, 1.0f) * fPoint0w;
    triangleStream.Append(output);
    
    triangleStream.RestartStrip();
    
    // Square cap at point 1 (1 triangle)
    output.vertexColor = input[1].vertexColor;
    output.position = float4(p1PlusPerp, 0.0f, 1.0f) * fPoint1w;
    triangleStream.Append(output);
    
    output.position = float4(p1MinusPerp, 0.0f, 1.0f) * fPoint1w;
    triangleStream.Append(output);
    
    output.position = float4(p1xy, 0.0f, 1.0f) * fPoint1w;
    triangleStream.Append(output);
}
)";

#endif // RENDERER_DIRECTX11
#endif // INCLUDED_RENDER2D_LINE_GEOMETRY_SHADER_HLSL_H