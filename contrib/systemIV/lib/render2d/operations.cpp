#include "lib/universal_include.h"

#include <stdarg.h>

#include "lib/resource/bitmapfont.h"
#include "lib/preferences.h"

#include "renderer/map_renderer.h"
#include "lib/render2d/renderer.h"

extern Renderer *g_renderer;

void Renderer::FlushIfTextureChanged(unsigned int newTextureID, bool useTexture) {
    if (!m_batchingTextures) {
        FlushTriangles(useTexture);
        return;
    }
    
    if (m_triangleVertexCount > 0 && m_currentBoundTexture != newTextureID) {
        FlushTriangles(useTexture);
    }
    
    m_currentBoundTexture = newTextureID;
}

// ============================================================================
// BEGIN SCENES
// ============================================================================

void Renderer::BeginTextBatch() {
    for (int i = 0; i < MAX_FONT_ATLASES; i++) {
        m_fontBatches[i].vertexCount = 0;
        m_fontBatches[i].textureID = 0;
    }
    
    m_currentFontBatchIndex = 0;
    m_textVertices = m_fontBatches[0].vertices;
    m_textVertexCount = 0;
    m_currentTextTexture = 0;
}

void Renderer::BeginLineBatch() {
    m_lineVertexCount = 0;
}

void Renderer::BeginStaticSpriteBatch() {
    m_staticSpriteVertexCount = 0;
    m_currentStaticSpriteTexture = 0;
}

void Renderer::BeginRotatingSpriteBatch() {
    m_rotatingSpriteVertexCount = 0;
    m_currentRotatingSpriteTexture = 0;
}

void Renderer::BeginCircleBatch() {
    m_circleVertexCount = 0;
}

void Renderer::BeginCircleFillBatch() {
    m_circleFillVertexCount = 0;
}

void Renderer::BeginRectBatch() {
    m_rectVertexCount = 0;
}

void Renderer::BeginRectFillBatch() {
    m_rectFillVertexCount = 0;
}

void Renderer::BeginTriangleFillBatch() {
    m_triangleFillVertexCount = 0;
}

// ============================================================================
// END SCENES
// ============================================================================

void Renderer::EndTextBatch() {
    for (int i = 0; i < MAX_FONT_ATLASES; i++) {
        if (m_fontBatches[i].vertexCount > 0) {
            
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
    
    m_currentFontBatchIndex = 0;
    m_textVertices = m_fontBatches[0].vertices;
    m_textVertexCount = 0;
    m_currentTextTexture = 0;
}

void Renderer::EndLineBatch() {
    if (m_lineVertexCount > 0) {
        FlushLines();
    }
}

void Renderer::EndStaticSpriteBatch() {
    if (m_staticSpriteVertexCount > 0) {
        FlushStaticSprites();
    }
}

void Renderer::EndRotatingSpriteBatch() {
    if (m_rotatingSpriteVertexCount > 0) {
        FlushRotatingSprite();
    }
}

void Renderer::EndCircleBatch() {
    if (m_circleVertexCount > 0) {
        FlushCircles();
    }
}

void Renderer::EndCircleFillBatch() {
    if (m_circleFillVertexCount > 0) {
        FlushCircleFills();
    }
}

void Renderer::EndRectBatch() {
    if (m_rectVertexCount > 0) {
        FlushRects();
    }
}

void Renderer::EndRectFillBatch() {
    if (m_rectFillVertexCount > 0) {
        FlushRectFills();
    }
}

void Renderer::EndTriangleFillBatch() {
    if (m_triangleFillVertexCount > 0) {
        FlushTriangleFills();
    }
}

// ============================================================================
// NOW FLUSH IF FULL
// ============================================================================

void Renderer::FlushTextBufferIfFull(int charactersNeeded) {
    int verticesNeeded = charactersNeeded * 6;
    if (m_textVertexCount + verticesNeeded > MAX_TEXT_VERTICES) {
        FlushTextBuffer();
    }
}

void Renderer::FlushLinesIfFull(int segmentsNeeded) {
    if (m_lineVertexCount + segmentsNeeded > MAX_LINE_VERTICES) {
        FlushLines();
    }
}

void Renderer::FlushStaticSpritesIfFull(int verticesNeeded) {
    if (m_staticSpriteVertexCount + verticesNeeded > MAX_STATIC_SPRITE_VERTICES) {
        FlushStaticSprites();
    }
}

void Renderer::FlushRotatingSpritesIfFull(int verticesNeeded) {
    if (m_rotatingSpriteVertexCount + verticesNeeded > MAX_ROTATING_SPRITE_VERTICES) {
        FlushRotatingSprite();
    }
}

void Renderer::FlushCirclesIfFull(int verticesNeeded) {
    if (m_circleVertexCount + verticesNeeded > MAX_CIRCLE_VERTICES) {
        FlushCircles();
    }
}

void Renderer::FlushCircleFillsIfFull(int verticesNeeded) {
    if (m_circleFillVertexCount + verticesNeeded > MAX_CIRCLE_FILL_VERTICES) {
        FlushCircleFills();
    }
}

void Renderer::FlushRectsIfFull(int verticesNeeded) {
    if (m_rectVertexCount + verticesNeeded > MAX_RECT_VERTICES) {
        FlushRects();
    }
}

void Renderer::FlushRectFillsIfFull(int verticesNeeded) {
    if (m_rectFillVertexCount + verticesNeeded > MAX_RECT_FILL_VERTICES) {
        FlushRectFills();
    }
}

void Renderer::FlushTriangleFillsIfFull(int verticesNeeded) {
    if (m_triangleFillVertexCount + verticesNeeded > MAX_TRIANGLE_FILL_VERTICES) {
        FlushTriangleFills();
    }
}

// ============================================================================
// CORE FLUSH FUNCTIONS
// ============================================================================

void Renderer::FlushTriangles(bool useTexture) {
    if (m_triangleVertexCount == 0) return;
    
    StartFlushTiming("Legacy_Triangles"); 
    IncrementDrawCall("legacy_triangles");
    
    unsigned int shaderToUse = useTexture ? m_textureShaderProgram : m_colorShaderProgram;
    SetShaderProgram(shaderToUse);
    
    if (useTexture) {
        SetTextureShaderUniforms();
    } else {
        SetColorShaderUniforms();
    }
    
    SetVertexArray(m_legacyVAO);    
    SetArrayBuffer(m_legacyVBO);
    UploadVertexDataToVBO(m_legacyVBO, m_triangleVertices, m_triangleVertexCount, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_triangleVertexCount);
    
    m_triangleVertexCount = 0;
    
    EndFlushTiming("Legacy_Triangles");
}

void Renderer::FlushTextBuffer() {
    if (m_textVertexCount == 0) return;
    
    StartFlushTiming("Text");
    IncrementDrawCall("text");
    
    // save current blend state
    int currentBlendSrc = m_blendSrcFactor;
    int currentBlendDst = m_blendDstFactor;
    
    // set correct blend mode for text rendering (additive for smooth fonts)
    if (m_blendMode != BlendModeSubtractive) {
        SetBlendMode(BlendModeAdditive);
    }
    
    SetShaderProgram(m_textureShaderProgram);
    SetTextureShaderUniforms();
    
    // bind the current text texture
    if (m_currentTextTexture != 0) {
        SetActiveTexture(GL_TEXTURE0);
        SetBoundTexture(m_currentTextTexture);
    }
    
    SetVertexArray(m_textVAO);
    SetArrayBuffer(m_textVBO);
    UploadVertexDataToVBO(m_textVBO, m_textVertices, m_textVertexCount, GL_DYNAMIC_DRAW);  
    
    glDrawArrays(GL_TRIANGLES, 0, m_textVertexCount);
    
    // restore previous blend state
    SetBlendFunc(currentBlendSrc, currentBlendDst);
    
    m_textVertexCount = 0;
    m_currentTextTexture = 0;  // reset texture tracking
    
    EndFlushTiming("Text");
}

void Renderer::FlushLines() {
    if (m_lineVertexCount == 0) return;
    
    StartFlushTiming("Lines");
    IncrementDrawCall("lines");
    
    SetShaderProgram(m_colorShaderProgram);
    SetColorShaderUniforms();
    
    SetVertexArray(m_lineVAO);
    SetArrayBuffer(m_lineVBO);
    UploadVertexDataToVBO(m_lineVBO, m_lineVertices, m_lineVertexCount, GL_DYNAMIC_DRAW); 
    
    glDrawArrays(GL_LINES, 0, m_lineVertexCount);
    
    m_lineVertexCount = 0;
    
    EndFlushTiming("Lines"); 
}


void Renderer::FlushStaticSprites() {
    if (m_staticSpriteVertexCount == 0) return;
    
    StartFlushTiming("Static_Sprite");
    IncrementDrawCall("static_sprites");
    
    SetShaderProgram(m_textureShaderProgram);
    SetTextureShaderUniforms();
    
    // bind texture here during flush
    if (m_currentStaticSpriteTexture != 0) {
        SetActiveTexture(GL_TEXTURE0);
        SetBoundTexture(m_currentStaticSpriteTexture);
    }
    
    SetVertexArray(m_spriteVAO);
    SetArrayBuffer(m_spriteVBO);
    UploadVertexDataToVBO(m_spriteVBO, m_staticSpriteVertices, m_staticSpriteVertexCount, GL_STREAM_DRAW);   
    
    glDrawArrays(GL_TRIANGLES, 0, m_staticSpriteVertexCount);
    
    m_staticSpriteVertexCount = 0;
    
    EndFlushTiming("Static_Sprite");
}

void Renderer::FlushRotatingSprite() {
    if (m_rotatingSpriteVertexCount == 0) return;
    
    StartFlushTiming("Unit_Rotating");
    IncrementDrawCall("rotating_sprites");
    
    SetShaderProgram(m_textureShaderProgram);
    SetTextureShaderUniforms();
    
    if (m_currentRotatingSpriteTexture != 0) {
        SetActiveTexture(GL_TEXTURE0);
        SetBoundTexture(m_currentRotatingSpriteTexture);
    }
    
    SetVertexArray(m_spriteVAO);
    SetArrayBuffer(m_spriteVBO);
    UploadVertexDataToVBO(m_spriteVBO, m_rotatingSpriteVertices, m_rotatingSpriteVertexCount, GL_STREAM_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_rotatingSpriteVertexCount);
    
    m_rotatingSpriteVertexCount = 0;
    
    EndFlushTiming("Unit_Rotating");
}

void Renderer::FlushCircles() {
    if (m_circleVertexCount == 0) return;
    
    StartFlushTiming("Circles");
    IncrementDrawCall("circles");
    
    SetShaderProgram(m_colorShaderProgram);
    SetColorShaderUniforms();
    
    SetVertexArray(m_circleVAO);
    SetArrayBuffer(m_circleVBO);
    UploadVertexDataToVBO(m_circleVBO, m_circleVertices, m_circleVertexCount, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_LINES, 0, m_circleVertexCount);
    
    m_circleVertexCount = 0;
    
    EndFlushTiming("Circles");
}

void Renderer::FlushCircleFills() {
    if (m_circleFillVertexCount == 0) return;
    
    StartFlushTiming("Circle_Fills");
    IncrementDrawCall("circle_fills");
    
    SetShaderProgram(m_colorShaderProgram);
    SetColorShaderUniforms();
    
    SetVertexArray(m_circleFillVAO);
    SetArrayBuffer(m_circleFillVBO);
    UploadVertexDataToVBO(m_circleFillVBO, m_circleFillVertices, m_circleFillVertexCount, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_circleFillVertexCount);
    
    m_circleFillVertexCount = 0;
    
    EndFlushTiming("Circle_Fills");
}

void Renderer::FlushRects() {
    if (m_rectVertexCount == 0) return;
    
    StartFlushTiming("Rects");
    IncrementDrawCall("rects");
    
    SetShaderProgram(m_colorShaderProgram);
    SetColorShaderUniforms();
    
    SetVertexArray(m_rectVAO);
    SetArrayBuffer(m_rectVBO);
    UploadVertexDataToVBO(m_rectVBO, m_rectVertices, m_rectVertexCount, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_LINES, 0, m_rectVertexCount);
    
    m_rectVertexCount = 0;
    
    EndFlushTiming("Rects");
}

void Renderer::FlushRectFills() {
    if (m_rectFillVertexCount == 0) return;
    
    StartFlushTiming("Rect_Fills");
    IncrementDrawCall("rect_fills");
    
    SetShaderProgram(m_colorShaderProgram);
    SetColorShaderUniforms();
    
    SetVertexArray(m_rectFillVAO);
    SetArrayBuffer(m_rectFillVBO);
    UploadVertexDataToVBO(m_rectFillVBO, m_rectFillVertices, m_rectFillVertexCount, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_rectFillVertexCount);
    
    m_rectFillVertexCount = 0;
    
    EndFlushTiming("Rect_Fills");
}

void Renderer::FlushTriangleFills() {
    if (m_triangleFillVertexCount == 0) return;
    
    StartFlushTiming("Triangle_Fills");
    IncrementDrawCall("triangle_fills");
    
    SetShaderProgram(m_colorShaderProgram);
    SetColorShaderUniforms();
    
    SetVertexArray(m_triangleFillVAO);
    SetArrayBuffer(m_triangleFillVBO);
    UploadVertexDataToVBO(m_triangleFillVBO, m_triangleFillVertices, m_triangleFillVertexCount, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_triangleFillVertexCount);
    
    m_triangleFillVertexCount = 0;
    
    EndFlushTiming("Triangle_Fills");
}