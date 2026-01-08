#include "systemiv.h"

#include <stdarg.h>

#include "lib/resource/bitmapfont.h"
#include "lib/preferences.h"

#include "lib/render/renderer.h"
#include "lib/render2d/renderer_2d.h"

void Renderer2D::FlushIfTextureChanged(unsigned int newTextureID, bool useTexture) {
    if (!m_batchingTextures) {
        FlushTriangles(useTexture);
        return;
    }
    
    if (m_triangleVertexCount > 0 && g_renderer->GetCurrentBoundTexture() != newTextureID) {
        FlushTriangles(useTexture);
    }
}

// ============================================================================
// BEGIN SCENES
// ============================================================================

void Renderer2D::BeginTextBatch() {
    for (int i = 0; i < Renderer::MAX_FONT_ATLASES; i++) {
        m_fontBatches[i].vertexCount = 0;
        m_fontBatches[i].textureID = 0;
    }
    
    m_currentFontBatchIndex = 0;
    m_textVertices = m_fontBatches[0].vertices;
    m_textVertexCount = 0;
    m_currentTextTexture = 0;
}

void Renderer2D::BeginLineBatch() {
    m_lineVertexCount = 0;
    m_currentLineWidth = 1.0f;
}

void Renderer2D::BeginStaticSpriteBatch() {
    m_staticSpriteVertexCount = 0;
    m_currentStaticSpriteTexture = 0;
}

void Renderer2D::BeginRotatingSpriteBatch() {
    m_rotatingSpriteVertexCount = 0;
    m_currentRotatingSpriteTexture = 0;
}

void Renderer2D::BeginCircleBatch() {
    m_circleVertexCount = 0;
    m_currentCircleWidth = 1.0f;
}

void Renderer2D::BeginCircleFillBatch() {
    m_circleFillVertexCount = 0;
}

void Renderer2D::BeginRectBatch() {
    m_rectVertexCount = 0;
    m_currentRectWidth = 1.0f;
}

void Renderer2D::BeginRectFillBatch() {
    m_rectFillVertexCount = 0;
}

void Renderer2D::BeginTriangleFillBatch() {
    m_triangleFillVertexCount = 0;
}

// ============================================================================
// END SCENES
// ============================================================================

void Renderer2D::EndTextBatch() {
    int activeBatches = 0;
    
    for (int i = 0; i < Renderer::MAX_FONT_ATLASES; i++) {
        if (m_fontBatches[i].vertexCount > 0) {
            
            activeBatches++;
            
            //
            // temporarily switch for flushing
            
            m_textVertices = m_fontBatches[i].vertices;
            m_textVertexCount = m_fontBatches[i].vertexCount;
            m_currentTextTexture = m_fontBatches[i].textureID;
            
            FlushTextBuffer();
            
            //
            // mark it as flushed

            m_fontBatches[i].vertexCount = 0;
        }
    }
    
    //
    // Track max active font batches this frame
    
    if (activeBatches > m_activeFontBatches) {
        m_activeFontBatches = activeBatches;
    }
    
    m_currentFontBatchIndex = 0;
    m_textVertices = m_fontBatches[0].vertices;
    m_textVertexCount = 0;
    m_currentTextTexture = 0;
}

void Renderer2D::EndLineBatch() {
    if (m_lineVertexCount > 0) {
        FlushLines();
    }
}

void Renderer2D::EndStaticSpriteBatch() {
    if (m_staticSpriteVertexCount > 0) {
        FlushStaticSprites();
    }
}

void Renderer2D::EndRotatingSpriteBatch() {
    if (m_rotatingSpriteVertexCount > 0) {
        FlushRotatingSprite();
    }
}

void Renderer2D::EndCircleBatch() {
    if (m_circleVertexCount > 0) {
        FlushCircles();
    }
}

void Renderer2D::EndCircleFillBatch() {
    if (m_circleFillVertexCount > 0) {
        FlushCircleFills();
    }
}

void Renderer2D::EndRectBatch() {
    if (m_rectVertexCount > 0) {
        FlushRects();
    }
}

void Renderer2D::EndRectFillBatch() {
    if (m_rectFillVertexCount > 0) {
        FlushRectFills();
    }
}

void Renderer2D::EndTriangleFillBatch() {
    if (m_triangleFillVertexCount > 0) {
        FlushTriangleFills();
    }
}

void Renderer2D::FlushAllBatches()
{
    EndRectFillBatch();
    EndTriangleFillBatch();
    EndStaticSpriteBatch();
    EndRotatingSpriteBatch();
    EndLineBatch();
    EndCircleBatch();
    EndCircleFillBatch();
    EndRectBatch();
    EndTextBatch();
}

// ============================================================================
// NOW FLUSH IF FULL
// ============================================================================

void Renderer2D::FlushTextBufferIfFull(int charactersNeeded) {
    int verticesNeeded = charactersNeeded * 6;
    if (m_textVertexCount + verticesNeeded > MAX_TEXT_VERTICES) {
        FlushTextBuffer();
    }
}

void Renderer2D::FlushLinesIfFull(int segmentsNeeded) {
    if (m_lineVertexCount + segmentsNeeded > MAX_LINE_VERTICES) {
        FlushLines();
    }
}

void Renderer2D::FlushStaticSpritesIfFull(int verticesNeeded) {
    if (m_staticSpriteVertexCount + verticesNeeded > MAX_STATIC_SPRITE_VERTICES) {
        FlushStaticSprites();
    }
}

void Renderer2D::FlushRotatingSpritesIfFull(int verticesNeeded) {
    if (m_rotatingSpriteVertexCount + verticesNeeded > MAX_ROTATING_SPRITE_VERTICES) {
        FlushRotatingSprite();
    }
}

void Renderer2D::FlushCirclesIfFull(int verticesNeeded) {
    if (m_circleVertexCount + verticesNeeded > MAX_CIRCLE_VERTICES) {
        FlushCircles();
    }
}

void Renderer2D::FlushCircleFillsIfFull(int verticesNeeded) {
    if (m_circleFillVertexCount + verticesNeeded > MAX_CIRCLE_FILL_VERTICES) {
        FlushCircleFills();
    }
}

void Renderer2D::FlushRectsIfFull(int verticesNeeded) {
    if (m_rectVertexCount + verticesNeeded > MAX_RECT_VERTICES) {
        FlushRects();
    }
}

void Renderer2D::FlushRectFillsIfFull(int verticesNeeded) {
    if (m_rectFillVertexCount + verticesNeeded > MAX_RECT_FILL_VERTICES) {
        FlushRectFills();
    }
}

void Renderer2D::FlushTriangleFillsIfFull(int verticesNeeded) {
    if (m_triangleFillVertexCount + verticesNeeded > MAX_TRIANGLE_FILL_VERTICES) {
        FlushTriangleFills();
    }
}
