#include "lib/universal_include.h"

#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/math/vector3.h"
#include "lib/render/colour.h"

#include "lib/render2d/renderer.h"

extern Renderer *g_renderer;

// ============================================================================
// BATCHED ECLIPSE SPRITE RENDERING
// ============================================================================

void Renderer::EclipseSprite(Image *src, float x, float y, Colour const &col) {
    float w = src->Width();
    float h = src->Height();
    EclipseSprite(src, x, y, w, h, col);
}

void Renderer::EclipseSprite(Image *src, float x, float y, float w, float h, Colour const &col) {
    // Check for texture change and flush if needed
    unsigned int textureID = GetEffectiveTextureID(src);
    if (m_eclipseSpriteVertexCount > 0 && m_currentEclipseSpriteTexture != textureID) {
        FlushEclipseSprites();
    }
    
    //
    // check for blend mode change and flush 
    
    static int s_lastSpriteBlendMode = -1;
    if (m_eclipseSpriteVertexCount > 0 && s_lastSpriteBlendMode != -1 && s_lastSpriteBlendMode != m_blendMode) {
        FlushEclipseSprites();
    }
    s_lastSpriteBlendMode = m_blendMode;
    
    // Check for buffer overflow and flush if needed
    FlushEclipseSpritesIfFull(6);
    
    // Set current texture for batching
    m_currentEclipseSpriteTexture = textureID;
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    // Get UV coordinates - atlas sprites use specific regions, others use full texture
    float u1, v1, u2, v2;
    GetImageUVCoords(src, u1, v1, u2, v2);
    
    // First triangle: TL, TR, BR
    m_eclipseSpriteVertices[m_eclipseSpriteVertexCount++] = {x, y, r, g, b, a, u1, v2};
    m_eclipseSpriteVertices[m_eclipseSpriteVertexCount++] = {x + w, y, r, g, b, a, u2, v2};
    m_eclipseSpriteVertices[m_eclipseSpriteVertexCount++] = {x + w, y + h, r, g, b, a, u2, v1};
    
    // Second triangle: TL, BR, BL
    m_eclipseSpriteVertices[m_eclipseSpriteVertexCount++] = {x, y, r, g, b, a, u1, v2};
    m_eclipseSpriteVertices[m_eclipseSpriteVertexCount++] = {x + w, y + h, r, g, b, a, u2, v1};
    m_eclipseSpriteVertices[m_eclipseSpriteVertexCount++] = {x, y + h, r, g, b, a, u1, v1};
}

void Renderer::EclipseSprite(Image *src, float x, float y, float w, float h, Colour const &col, float angle) {
    
    //
    // check for texture change and flush if needed

    unsigned int textureID = GetEffectiveTextureID(src);
    if (m_eclipseSpriteVertexCount > 0 && m_currentEclipseSpriteTexture != textureID) {
        FlushEclipseSprites();
    }
    
    static int s_lastSpriteBlendMode = -1;
    if (m_eclipseSpriteVertexCount > 0 && s_lastSpriteBlendMode != -1 && s_lastSpriteBlendMode != m_blendMode) {
        FlushEclipseSprites();
    }
    s_lastSpriteBlendMode = m_blendMode;
    
    //
    // check for buffer overflow and flush if needed

    FlushEclipseSpritesIfFull(6);
    
    //
    // set current texture for batching

    m_currentEclipseSpriteTexture = textureID;
    
    // calculate rotated vertices
    Vector3<float> vert1(-w, +h, 0);
    Vector3<float> vert2(+w, +h, 0);
    Vector3<float> vert3(+w, -h, 0);
    Vector3<float> vert4(-w, -h, 0);

    vert1.RotateAroundZ(angle);
    vert2.RotateAroundZ(angle);
    vert3.RotateAroundZ(angle);
    vert4.RotateAroundZ(angle);

    vert1 += Vector3<float>(x, y, 0);
    vert2 += Vector3<float>(x, y, 0);
    vert3 += Vector3<float>(x, y, 0);
    vert4 += Vector3<float>(x, y, 0);

    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    // get UV coordinates, atlas sprites use specific regions, others use full texture
    float u1, v1, u2, v2;
    GetImageUVCoords(src, u1, v1, u2, v2);
    
    // first triangle: vert1, vert2, vert3
    m_eclipseSpriteVertices[m_eclipseSpriteVertexCount++] = {vert1.x, vert1.y, r, g, b, a, u1, v2};
    m_eclipseSpriteVertices[m_eclipseSpriteVertexCount++] = {vert2.x, vert2.y, r, g, b, a, u2, v2};
    m_eclipseSpriteVertices[m_eclipseSpriteVertexCount++] = {vert3.x, vert3.y, r, g, b, a, u2, v1};
    
    // second triangle: vert1, vert3, vert4
    m_eclipseSpriteVertices[m_eclipseSpriteVertexCount++] = {vert1.x, vert1.y, r, g, b, a, u1, v2};
    m_eclipseSpriteVertices[m_eclipseSpriteVertexCount++] = {vert3.x, vert3.y, r, g, b, a, u2, v1};
    m_eclipseSpriteVertices[m_eclipseSpriteVertexCount++] = {vert4.x, vert4.y, r, g, b, a, u1, v1};
}

// ============================================================================
// ECLIPSE UI PRIMITIVE RENDERING
// ============================================================================

void Renderer::EclipseRect(float x, float y, float w, float h, Colour const &col, float lineWidth) {
    FlushEclipseRectsIfFull(8);
        
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
        
    // Create 4 lines to form rectangle outline - same pattern as EffectsRect
    // Top line
    m_eclipseRectVertices[m_eclipseRectVertexCount++] = {x, y, r, g, b, a, 0.0f, 0.0f};
    m_eclipseRectVertices[m_eclipseRectVertexCount++] = {x + w, y, r, g, b, a, 0.0f, 0.0f};

    // Right line
    m_eclipseRectVertices[m_eclipseRectVertexCount++] = {x + w, y, r, g, b, a, 0.0f, 0.0f};
    m_eclipseRectVertices[m_eclipseRectVertexCount++] = {x + w, y + h, r, g, b, a, 0.0f, 0.0f};

    // Bottom line
    m_eclipseRectVertices[m_eclipseRectVertexCount++] = {x + w, y + h, r, g, b, a, 0.0f, 0.0f};
    m_eclipseRectVertices[m_eclipseRectVertexCount++] = {x, y + h, r, g, b, a, 0.0f, 0.0f};

    // Left line
    m_eclipseRectVertices[m_eclipseRectVertexCount++] = {x, y + h, r, g, b, a, 0.0f, 0.0f};
    m_eclipseRectVertices[m_eclipseRectVertexCount++] = {x, y, r, g, b, a, 0.0f, 0.0f};
}
void Renderer::EclipseRectFill(float x, float y, float w, float h, Colour const &col) {
    EclipseRectFill(x, y, w, h, col, col, col, col);
}

void Renderer::EclipseRectFill(float x, float y, float w, float h, Colour const &col1, Colour const &col2, bool horizontal) {
    if (horizontal) {
        EclipseRectFill(x, y, w, h, col1, col1, col2, col2);
    } else {
        EclipseRectFill(x, y, w, h, col1, col2, col2, col1);
    }
}

void Renderer::EclipseRectFill(float x, float y, float w, float h, Colour const &colTL, Colour const &colTR, 
                               Colour const &colBR, Colour const &colBL) {
    FlushEclipseRectFillsIfFull(6);
    
    float rTL = colTL.m_r / 255.0f, gTL = colTL.m_g / 255.0f, bTL = colTL.m_b / 255.0f, aTL = colTL.m_a / 255.0f;
    float rTR = colTR.m_r / 255.0f, gTR = colTR.m_g / 255.0f, bTR = colTR.m_b / 255.0f, aTR = colTR.m_a / 255.0f;
    float rBR = colBR.m_r / 255.0f, gBR = colBR.m_g / 255.0f, bBR = colBR.m_b / 255.0f, aBR = colBR.m_a / 255.0f;
    float rBL = colBL.m_r / 255.0f, gBL = colBL.m_g / 255.0f, bBL = colBL.m_b / 255.0f, aBL = colBL.m_a / 255.0f;
    
    // First triangle: TL, TR, BR - same pattern as RectFill
    m_eclipseRectFillVertices[m_eclipseRectFillVertexCount++] = {x, y, rTL, gTL, bTL, aTL, 0.0f, 0.0f};
    m_eclipseRectFillVertices[m_eclipseRectFillVertexCount++] = {x + w, y, rTR, gTR, bTR, aTR, 1.0f, 0.0f};
    m_eclipseRectFillVertices[m_eclipseRectFillVertexCount++] = {x + w, y + h, rBR, gBR, bBR, aBR, 1.0f, 1.0f};
    
    // Second triangle: TL, BR, BL
    m_eclipseRectFillVertices[m_eclipseRectFillVertexCount++] = {x, y, rTL, gTL, bTL, aTL, 0.0f, 0.0f};
    m_eclipseRectFillVertices[m_eclipseRectFillVertexCount++] = {x + w, y + h, rBR, gBR, bBR, aBR, 1.0f, 1.0f};
    m_eclipseRectFillVertices[m_eclipseRectFillVertexCount++] = {x, y + h, rBL, gBL, bBL, aBL, 0.0f, 1.0f};
}

void Renderer::EclipseTriangleFill(float x1, float y1, float x2, float y2, float x3, float y3, Colour const &col) {
    EclipseTriangleFill(x1, y1, x2, y2, x3, y3, col, col, col);
}

void Renderer::EclipseTriangleFill(float x1, float y1, float x2, float y2, float x3, float y3, 
                                  Colour const &col1, Colour const &col2, Colour const &col3) {
    FlushEclipseTriangleFillsIfFull(3);
    
    float r1 = col1.m_r / 255.0f, g1 = col1.m_g / 255.0f, b1 = col1.m_b / 255.0f, a1 = col1.m_a / 255.0f;
    float r2 = col2.m_r / 255.0f, g2 = col2.m_g / 255.0f, b2 = col2.m_b / 255.0f, a2 = col2.m_a / 255.0f;
    float r3 = col3.m_r / 255.0f, g3 = col3.m_g / 255.0f, b3 = col3.m_b / 255.0f, a3 = col3.m_a / 255.0f;
    
    // Single triangle with three vertices
    m_eclipseTriangleFillVertices[m_eclipseTriangleFillVertexCount++] = {x1, y1, r1, g1, b1, a1, 0.0f, 0.0f};
    m_eclipseTriangleFillVertices[m_eclipseTriangleFillVertexCount++] = {x2, y2, r2, g2, b2, a2, 0.0f, 0.0f};
    m_eclipseTriangleFillVertices[m_eclipseTriangleFillVertexCount++] = {x3, y3, r3, g3, b3, a3, 0.0f, 0.0f};
}

void Renderer::EclipseLine(float x1, float y1, float x2, float y2, Colour const &col, float lineWidth) {
    FlushEclipseLinesIfFull(2);
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    m_eclipseLineVertices[m_eclipseLineVertexCount++] = {x1, y1, r, g, b, a, 0.0f, 0.0f};
    m_eclipseLineVertices[m_eclipseLineVertexCount++] = {x2, y2, r, g, b, a, 0.0f, 0.0f};
}

void Renderer::BeginEclipseLines(Colour const &col, float lineWidth) {
    m_currentEclipseLineColor = col;
}

void Renderer::EclipseLine(float x, float y) {
    if (m_eclipseLineVertexCount + 1 > MAX_ECLIPSE_LINE_VERTICES) {
        FlushEclipseLines();
    }
    
    float r = m_currentEclipseLineColor.m_r / 255.0f, g = m_currentEclipseLineColor.m_g / 255.0f;
    float b = m_currentEclipseLineColor.m_b / 255.0f, a = m_currentEclipseLineColor.m_a / 255.0f;
    
    m_eclipseLineVertices[m_eclipseLineVertexCount++] = {x, y, r, g, b, a, 0.0f, 0.0f};
}

void Renderer::EndEclipseLines() {
    if (m_eclipseLineVertexCount < 2) {
        m_eclipseLineVertexCount = 0;
        return;
    }
    
    //
    // convert line strip to individual line segments using pre-allocated buffer
    
    int tempLineVertexCount = (m_eclipseLineVertexCount - 1) * 2;
    Vertex2D* tempLineVertices = m_lineConversionBuffer;
    
    int lineIndex = 0;
    for (int i = 0; i < m_eclipseLineVertexCount - 1; i++) {
        tempLineVertices[lineIndex++] = m_eclipseLineVertices[i];
        tempLineVertices[lineIndex++] = m_eclipseLineVertices[i + 1];
    }
    
    m_eclipseLineVertexCount = 0;
    
    for (int i = 0; i < tempLineVertexCount; i++) {
        if (m_eclipseLineVertexCount >= MAX_ECLIPSE_LINE_VERTICES) {
            FlushEclipseLines();
        }
        m_eclipseLineVertices[m_eclipseLineVertexCount++] = tempLineVertices[i];
    }
    
    FlushEclipseLines();
}
