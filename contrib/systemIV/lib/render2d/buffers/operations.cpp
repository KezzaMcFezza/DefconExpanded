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

void Renderer::BeginHealthBarBatch() {
    m_healthBarVertexCount = 0;
}

void Renderer::BeginCircleFillBatch() {
    m_circleFillVertexCount = 0;
}

void Renderer::BeginRectFillBatch() {
    m_rectFillVertexCount = 0;
}

void Renderer::BeginEclipseRectBatch() {
    m_eclipseRectVertexCount = 0;
}

void Renderer::BeginEclipseRectFillBatch() {
    m_eclipseRectFillVertexCount = 0;
}

void Renderer::BeginEclipseTriangleFillBatch() {
    m_eclipseTriangleFillVertexCount = 0;
}

void Renderer::BeginEclipseLineBatch() {
    m_eclipseLineVertexCount = 0;
}

void Renderer::BeginEclipseSpriteBatch() {
    m_eclipseSpriteVertexCount = 0;
    m_currentEclipseSpriteTexture = 0;
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

void Renderer::EndHealthBarBatch() {
    if (m_healthBarVertexCount > 0) {
        FlushHealthBars();
    }
}

void Renderer::EndCircleFillBatch() {
    if (m_circleFillVertexCount > 0) {
        FlushCircleFills();
    }
}

void Renderer::EndRectFillBatch() {
    if (m_rectFillVertexCount > 0) {
        FlushRectFills();
    }
}

void Renderer::EndEclipseRectBatch() {
    if (m_eclipseRectVertexCount > 0) {
        FlushEclipseRects();
    }
}

void Renderer::EndEclipseRectFillBatch() {
    if (m_eclipseRectFillVertexCount > 0) {
        FlushEclipseRectFills();
    }
}

void Renderer::EndEclipseTriangleFillBatch() {
    if (m_eclipseTriangleFillVertexCount > 0) {
        FlushEclipseTriangleFills();
    }
}

void Renderer::EndEclipseLineBatch() {
    if (m_eclipseLineVertexCount > 0) {
        FlushEclipseLines();
    }
}

void Renderer::EndEclipseSpriteBatch() {
    if (m_eclipseSpriteVertexCount > 0) {
        FlushEclipseSprites();
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

void Renderer::FlushCircleFillsIfFull(int verticesNeeded) {
    if (m_circleFillVertexCount + verticesNeeded > MAX_CIRCLE_FILL_VERTICES) {
        FlushCircleFills();
    }
}

void Renderer::FlushRectFillsIfFull(int verticesNeeded) {
    if (m_rectFillVertexCount + verticesNeeded > MAX_RECT_FILL_VERTICES) {
        FlushRectFills();
    }
}

void Renderer::FlushEclipseRectsIfFull(int segmentsNeeded) {
    if (m_eclipseRectVertexCount + segmentsNeeded > MAX_ECLIPSE_RECT_VERTICES) {
        FlushEclipseRects();
    }
}

void Renderer::FlushEclipseRectFillsIfFull(int verticesNeeded) {
    if (m_eclipseRectFillVertexCount + verticesNeeded > MAX_ECLIPSE_RECTFILL_VERTICES) {
        FlushEclipseRectFills();
    }
}

void Renderer::FlushEclipseTriangleFillsIfFull(int verticesNeeded) {
    if (m_eclipseTriangleFillVertexCount + verticesNeeded > MAX_ECLIPSE_TRIANGLEFILL_VERTICES) {
        FlushEclipseTriangleFills();
    }
}

void Renderer::FlushEclipseLinesIfFull(int segmentsNeeded) {
    if (m_eclipseLineVertexCount + segmentsNeeded > MAX_ECLIPSE_LINE_VERTICES) {
        FlushEclipseLines();
    }
}

void Renderer::FlushEclipseSpritesIfFull(int verticesNeeded) {
    if (m_eclipseSpriteVertexCount + verticesNeeded > MAX_ECLIPSE_SPRITE_VERTICES) {
        FlushEclipseSprites();
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
    
#ifndef TARGET_EMSCRIPTEN
    SetLineWidth(g_preferences->GetFloat(PREFS_GRAPHICS_UNIT_TRAIL_THICKNESS)); 
#endif
    
    SetShaderProgram(m_colorShaderProgram);
    SetColorShaderUniforms();
    
    SetVertexArray(m_effectsVAO);
    SetArrayBuffer(m_effectsVBO);
    UploadVertexDataToVBO(m_effectsVBO, m_lineVertices, m_lineVertexCount, GL_DYNAMIC_DRAW); 
    
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
    
    SetVertexArray(m_unitVAO);
    SetArrayBuffer(m_unitVBO);
    UploadVertexDataToVBO(m_unitVBO, m_staticSpriteVertices, m_staticSpriteVertexCount, GL_STREAM_DRAW);   
    
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
    
    SetVertexArray(m_unitVAO);
    SetArrayBuffer(m_unitVBO);
    UploadVertexDataToVBO(m_unitVBO, m_rotatingSpriteVertices, m_rotatingSpriteVertexCount, GL_STREAM_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_rotatingSpriteVertexCount);
    
    m_rotatingSpriteVertexCount = 0;
    
    EndFlushTiming("Unit_Rotating");
}

// flush the buffer
void Renderer::FlushHealthBars() {
    if (m_healthBarVertexCount == 0) return;
    
    StartFlushTiming("Health_Bars");
    IncrementDrawCall("health_bars");
    
    SetShaderProgram(m_colorShaderProgram);
    SetColorShaderUniforms();
    
    SetVertexArray(m_healthVAO);
    SetArrayBuffer(m_healthVBO);
    UploadVertexDataToVBO(m_healthVBO, m_healthBarVertices, m_healthBarVertexCount, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_healthBarVertexCount);
    
    m_healthBarVertexCount = 0;
    
    EndFlushTiming("Health_Bars");
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

void Renderer::FlushEclipseRects() {
    if (m_eclipseRectVertexCount == 0) return;
    
    StartFlushTiming("Eclipse_Rects");
    IncrementDrawCall("eclipse_rects");
    
#ifndef TARGET_EMSCRIPTEN
    SetLineWidth(1.0f);
#endif
    
    SetShaderProgram(m_colorShaderProgram);
    SetColorShaderUniforms();
    
    SetVertexArray(m_eclipseLinesVAO);
    SetArrayBuffer(m_eclipseLinesVBO);
    UploadVertexDataToVBO(m_eclipseLinesVBO, m_eclipseRectVertices, m_eclipseRectVertexCount, GL_STREAM_DRAW);
    
    glDrawArrays(GL_LINES, 0, m_eclipseRectVertexCount);
    
    m_eclipseRectVertexCount = 0;
    
    EndFlushTiming("Eclipse_Rects");
}

void Renderer::FlushEclipseRectFills() {
    if (m_eclipseRectFillVertexCount == 0) return;
    
    StartFlushTiming("Eclipse_RectFills");
    IncrementDrawCall("eclipse_rectfills");
    
    SetShaderProgram(m_colorShaderProgram);
    SetColorShaderUniforms();
    
    SetVertexArray(m_eclipseFillsVAO);
    SetArrayBuffer(m_eclipseFillsVBO);
    UploadVertexDataToVBO(m_eclipseFillsVBO, m_eclipseRectFillVertices, m_eclipseRectFillVertexCount, GL_STREAM_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_eclipseRectFillVertexCount);
    
    m_eclipseRectFillVertexCount = 0;
    
    EndFlushTiming("Eclipse_RectFills");
}

void Renderer::FlushEclipseTriangleFills() {
    if (m_eclipseTriangleFillVertexCount == 0) return;
    
    StartFlushTiming("Eclipse_TriangleFills");
    IncrementDrawCall("eclipse_trianglefills");
    
    SetShaderProgram(m_colorShaderProgram);
    SetColorShaderUniforms();
    
    SetVertexArray(m_eclipseFillsVAO);
    SetArrayBuffer(m_eclipseFillsVBO);
    UploadVertexDataToVBO(m_eclipseFillsVBO, m_eclipseTriangleFillVertices, m_eclipseTriangleFillVertexCount, GL_STREAM_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_eclipseTriangleFillVertexCount);
    
    m_eclipseTriangleFillVertexCount = 0;
    
    EndFlushTiming("Eclipse_TriangleFills");
}

void Renderer::FlushEclipseLines() {
    if (m_eclipseLineVertexCount == 0) return;
    
    StartFlushTiming("Eclipse_Lines");
    IncrementDrawCall("eclipse_lines");
    
#ifndef TARGET_EMSCRIPTEN
    SetLineWidth(1.0f);  // Set once during flush
#endif
    
    SetShaderProgram(m_colorShaderProgram);
    SetColorShaderUniforms();
    
    SetVertexArray(m_eclipseLinesVAO);
    SetArrayBuffer(m_eclipseLinesVBO);
    UploadVertexDataToVBO(m_eclipseLinesVBO, m_eclipseLineVertices, m_eclipseLineVertexCount, GL_STREAM_DRAW);
    
    glDrawArrays(GL_LINES, 0, m_eclipseLineVertexCount);
    
    m_eclipseLineVertexCount = 0;
    
    EndFlushTiming("Eclipse_Lines");
}

void Renderer::FlushEclipseSprites() {
    if (m_eclipseSpriteVertexCount == 0) return;
    
    StartFlushTiming("Eclipse_Sprites");
    IncrementDrawCall("eclipse_sprites");
    
    SetShaderProgram(m_textureShaderProgram);
    SetTextureShaderUniforms();
    
    if (m_currentEclipseSpriteTexture != 0) {
        SetActiveTexture(GL_TEXTURE0);
        SetBoundTexture(m_currentEclipseSpriteTexture);
    }
    
    SetVertexArray(m_eclipseSpritesVAO);
    SetArrayBuffer(m_eclipseSpritesVBO);
    UploadVertexDataToVBO(m_eclipseSpritesVBO, m_eclipseSpriteVertices, m_eclipseSpriteVertexCount, GL_STREAM_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_eclipseSpriteVertexCount);
    
    m_eclipseSpriteVertexCount = 0;
    m_currentEclipseSpriteTexture = 0;
    
    EndFlushTiming("Eclipse_Sprites");
}