#include "systemiv.h"

#include <stdarg.h>

#include "lib/resource/bitmapfont.h"
#include "lib/preferences.h"

#include "renderer/map_renderer.h"
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

// ============================================================================
// CORE FLUSH FUNCTIONS
// ============================================================================

void Renderer2D::FlushTriangles(bool useTexture) {
    if (m_triangleVertexCount == 0) return;
    
    g_renderer->StartFlushTiming("Immediate_Triangles"); 
    IncrementDrawCall("immediate_triangles");
    
    unsigned int shaderToUse = useTexture ? m_textureShaderProgram : m_colorShaderProgram;
    g_renderer->SetShaderProgram(shaderToUse);
    
    if (useTexture) {
        SetTextureShaderUniforms();
    } else {
        SetColorShaderUniforms();
    }
    
    g_renderer->SetVertexArray(m_immediateVAO);    
    g_renderer->SetArrayBuffer(m_immediateVBO);
    UploadVertexDataToVBO(m_immediateVBO, m_triangleVertices, m_triangleVertexCount, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_triangleVertexCount);
    
    m_triangleVertexCount = 0;
    
    g_renderer->EndFlushTiming("Immediate_Triangles");
}

void Renderer2D::FlushTextBuffer() {
    if (m_textVertexCount == 0) return;
    
    g_renderer->StartFlushTiming("Text");
    IncrementDrawCall("text");
    
    // save current blend state
    int currentBlendSrc = g_renderer->m_currentBlendSrcFactor;
    int currentBlendDst = g_renderer->m_currentBlendDstFactor;
    
    // set correct blend mode for text rendering (additive for smooth fonts)
    if (g_renderer->m_blendMode != Renderer::BlendModeSubtractive) {
        g_renderer->SetBlendMode(Renderer::BlendModeAdditive);
    }
    
    g_renderer->SetShaderProgram(m_textureShaderProgram);
    SetTextureShaderUniforms();
    
    // bind the current text texture
    if (m_currentTextTexture != 0) {
        g_renderer->SetActiveTexture(GL_TEXTURE0);
        g_renderer->SetBoundTexture(m_currentTextTexture);
    }
    
    g_renderer->SetVertexArray(m_textVAO);
    g_renderer->SetArrayBuffer(m_textVBO);
    UploadVertexDataToVBO(m_textVBO, m_textVertices, m_textVertexCount, GL_DYNAMIC_DRAW);  
    
    glDrawArrays(GL_TRIANGLES, 0, m_textVertexCount);
    
    // restore previous blend state
    g_renderer->SetBlendFunc(currentBlendSrc, currentBlendDst);
    
    m_textVertexCount = 0;
    m_currentTextTexture = 0;  // reset texture tracking
    
    g_renderer->EndFlushTiming("Text");
}

void Renderer2D::FlushLines() {
    if (m_lineVertexCount == 0) return;
    
    g_renderer->StartFlushTiming("Lines");
    IncrementDrawCall("lines");
    
#ifndef TARGET_EMSCRIPTEN
    g_renderer->SetLineWidth(m_currentLineWidth);
#endif
    
    g_renderer->SetShaderProgram(m_colorShaderProgram);
    SetColorShaderUniforms();
    
    g_renderer->SetVertexArray(m_lineVAO);
    g_renderer->SetArrayBuffer(m_lineVBO);
    UploadVertexDataToVBO(m_lineVBO, m_lineVertices, m_lineVertexCount, GL_DYNAMIC_DRAW); 
    
    glDrawArrays(GL_LINES, 0, m_lineVertexCount);
    
    m_lineVertexCount = 0;
    
    g_renderer->EndFlushTiming("Lines"); 
}


void Renderer2D::FlushStaticSprites() {
    if (m_staticSpriteVertexCount == 0) return;
    
    g_renderer->StartFlushTiming("Static_Sprite");
    IncrementDrawCall("static_sprites");
    
    g_renderer->SetShaderProgram(m_textureShaderProgram);
    SetTextureShaderUniforms();
    
    // bind texture here during flush
    if (m_currentStaticSpriteTexture != 0) {
        g_renderer->SetActiveTexture(GL_TEXTURE0);
        g_renderer->SetBoundTexture(m_currentStaticSpriteTexture);
    }
    
    g_renderer->SetVertexArray(m_spriteVAO);
    g_renderer->SetArrayBuffer(m_spriteVBO);
    UploadVertexDataToVBO(m_spriteVBO, m_staticSpriteVertices, m_staticSpriteVertexCount, GL_STREAM_DRAW);   
    
    glDrawArrays(GL_TRIANGLES, 0, m_staticSpriteVertexCount);
    
    m_staticSpriteVertexCount = 0;
    
    g_renderer->EndFlushTiming("Static_Sprite");
}

void Renderer2D::FlushRotatingSprite() {
    if (m_rotatingSpriteVertexCount == 0) return;
    
    g_renderer->StartFlushTiming("Rotating_Sprite");
    IncrementDrawCall("rotating_sprites");
    
    g_renderer->SetShaderProgram(m_textureShaderProgram);
    SetTextureShaderUniforms();
    
    if (m_currentRotatingSpriteTexture != 0) {
        g_renderer->SetActiveTexture(GL_TEXTURE0);
        g_renderer->SetBoundTexture(m_currentRotatingSpriteTexture);
    }
    
    g_renderer->SetVertexArray(m_rotatingSpriteVAO);
    g_renderer->SetArrayBuffer(m_rotatingSpriteVBO);
    UploadVertexDataToVBO(m_rotatingSpriteVBO, m_rotatingSpriteVertices, m_rotatingSpriteVertexCount, GL_STREAM_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_rotatingSpriteVertexCount);
    
    m_rotatingSpriteVertexCount = 0;
    
    g_renderer->EndFlushTiming("Rotating_Sprite");
}

void Renderer2D::FlushCircles() {
    if (m_circleVertexCount == 0) return;
    
    g_renderer->StartFlushTiming("Circles");
    IncrementDrawCall("circles");
    
#ifndef TARGET_EMSCRIPTEN
    g_renderer->SetLineWidth(m_currentCircleWidth);
#endif
    
    g_renderer->SetShaderProgram(m_colorShaderProgram);
    SetColorShaderUniforms();
    
    g_renderer->SetVertexArray(m_circleVAO);
    g_renderer->SetArrayBuffer(m_circleVBO);
    UploadVertexDataToVBO(m_circleVBO, m_circleVertices, m_circleVertexCount, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_LINES, 0, m_circleVertexCount);
    
    m_circleVertexCount = 0;
    
    g_renderer->EndFlushTiming("Circles");
}

void Renderer2D::FlushCircleFills() {
    if (m_circleFillVertexCount == 0) return;
    
    g_renderer->StartFlushTiming("Circle_Fills");
    IncrementDrawCall("circle_fills");
    
    g_renderer->SetShaderProgram(m_colorShaderProgram);
    SetColorShaderUniforms();
    
    g_renderer->SetVertexArray(m_circleFillVAO);
    g_renderer->SetArrayBuffer(m_circleFillVBO);
    UploadVertexDataToVBO(m_circleFillVBO, m_circleFillVertices, m_circleFillVertexCount, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_circleFillVertexCount);
    
    m_circleFillVertexCount = 0;
    
    g_renderer->EndFlushTiming("Circle_Fills");
}

void Renderer2D::FlushRects() {
    if (m_rectVertexCount == 0) return;
    
    g_renderer->StartFlushTiming("Rects");
    IncrementDrawCall("rects");
    
#ifndef TARGET_EMSCRIPTEN
    g_renderer->SetLineWidth(m_currentRectWidth);
#endif
    
    g_renderer->SetShaderProgram(m_colorShaderProgram);
    SetColorShaderUniforms();
    
    g_renderer->SetVertexArray(m_rectVAO);
    g_renderer->SetArrayBuffer(m_rectVBO);
    UploadVertexDataToVBO(m_rectVBO, m_rectVertices, m_rectVertexCount, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_LINES, 0, m_rectVertexCount);
    
    m_rectVertexCount = 0;
    
    g_renderer->EndFlushTiming("Rects");
}

void Renderer2D::FlushRectFills() {
    if (m_rectFillVertexCount == 0) return;
    
    g_renderer->StartFlushTiming("Rect_Fills");
    IncrementDrawCall("rect_fills");
    
    g_renderer->SetShaderProgram(m_colorShaderProgram);
    SetColorShaderUniforms();
    
    g_renderer->SetVertexArray(m_rectFillVAO);
    g_renderer->SetArrayBuffer(m_rectFillVBO);
    UploadVertexDataToVBO(m_rectFillVBO, m_rectFillVertices, m_rectFillVertexCount, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_rectFillVertexCount);
    
    m_rectFillVertexCount = 0;
    
    g_renderer->EndFlushTiming("Rect_Fills");
}

void Renderer2D::FlushTriangleFills() {
    if (m_triangleFillVertexCount == 0) return;
    
    g_renderer->StartFlushTiming("Triangle_Fills");
    IncrementDrawCall("triangle_fills");
    
    g_renderer->SetShaderProgram(m_colorShaderProgram);
    SetColorShaderUniforms();
    
    g_renderer->SetVertexArray(m_triangleFillVAO);
    g_renderer->SetArrayBuffer(m_triangleFillVBO);
    UploadVertexDataToVBO(m_triangleFillVBO, m_triangleFillVertices, m_triangleFillVertexCount, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_triangleFillVertexCount);
    
    m_triangleFillVertexCount = 0;
    
    g_renderer->EndFlushTiming("Triangle_Fills");
}