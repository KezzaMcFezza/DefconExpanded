#include "lib/universal_include.h"

#include <stdarg.h>

#include "lib/debug_utils.h"
#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/resource/bitmapfont.h"
#include "lib/math/vector3.h"
#include "lib/math/math_utils.h"
#include "lib/string_utils.h"
#include "lib/resource/sprite_atlas.h"
#include "lib/render/colour.h"
#include "lib/preferences.h"

#include "renderer/map_renderer.h"
#include "lib/render2d/renderer.h"

extern Renderer *g_renderer;

// ============================================================================
// FLUSH ALL SPECIALIZED BUFFERS
// ============================================================================

void Renderer::FlushAllSpecializedBuffers() {
    FlushUITriangles();
    FlushUILines();
    FlushTextBuffer();
    FlushUnitTrails();
    FlushUnitMainSprites();
    FlushUnitRotating();
    FlushUnitHighlights();
    FlushUnitStateIcons();
    FlushUnitCounters();
    FlushUnitNukeIcons();
    FlushHealthBars();
    FlushEffectsLines();
    FlushEffectsRects();
    FlushEffectsSprites();
    FlushEclipseRects();
    FlushEclipseRectFills();
    FlushEclipseLines();
    FlushEclipseSprites();
}

void Renderer::FlushAllUnitBuffers() {
    FlushUnitTrails();
    FlushUnitMainSprites();
    FlushUnitRotating();
    FlushUnitHighlights();
    FlushUnitStateIcons();
    FlushUnitCounters();
    FlushUnitNukeIcons();
    FlushHealthBars();
}

void Renderer::FlushAllEffectBuffers() {
    FlushEffectsLines();
    FlushEffectsRects();
    FlushEffectsSprites();
}

void Renderer::FlushUIContext() {
    FlushUITriangles();
    FlushUILines();
}

void Renderer::FlushAllBuffers() {
    FlushTriangles(false);
    FlushLines();
}

void Renderer::FlushTextContext() {
    FlushTextBuffer();
}

//
// this function is problematic but necessary without a texture atlas

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

void Renderer::BeginUnitTrailBatch() {
    m_unitTrailVertexCount = 0;
}

void Renderer::BeginUnitMainBatch() {
    m_unitMainVertexCount = 0;
    m_currentUnitMainTexture = 0;
}

void Renderer::BeginUnitRotatingBatch() {
    m_unitRotatingVertexCount = 0;
    m_currentUnitRotatingTexture = 0;
}

void Renderer::BeginEffectsCircleBatch() {
    m_effectsCircleFillVertexCount = 0;
    m_effectsCircleOutlineVertexCount = 0;
    m_effectsCircleOutlineThickVertexCount = 0;
}

void Renderer::BeginUnitStateBatch() {
    m_unitStateVertexCount = 0;
    m_currentUnitStateTexture = 0;
}

void Renderer::BeginUnitCounterBatch() {
    m_unitCounterVertexCount = 0;
    m_currentUnitCounterTexture = 0;
}

void Renderer::BeginUnitNukeBatch() {
    m_unitNukeVertexCount = 0;
    m_currentUnitNukeTexture = 0;
}

void Renderer::BeginEffectsLineBatch() {
    m_effectsLineVertexCount = 0;
}

void Renderer::BeginEffectsSpriteBatch() {
    m_effectsSpriteVertexCount = 0;
    m_currentEffectsSpriteTexture = 0;
}

void Renderer::BeginUnitHighlightBatch() {
    m_unitHighlightVertexCount = 0;
    m_currentUnitHighlightTexture = 0;
}

void Renderer::BeginMapTextBatch() {
    BeginTextBatch();
}

void Renderer::BeginFrameTextBatch() {
    m_textVertexCount = 0;
    m_currentTextTexture = 0;
    BeginTextBatch();
}

void Renderer::BeginHealthBarBatch() {
    m_healthBarVertexCount = 0;
}

void Renderer::BeginUITriangleBatch() {
    m_uiTriangleVertexCount = 0;
}

void Renderer::BeginUILineBatch() {
    m_uiLineVertexCount = 0;
}

void Renderer::BeginUIBatch() {
    BeginUITriangleBatch();
    BeginUILineBatch();
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

void Renderer::EndUnitTrailBatch() {
    FlushUnitTrails();
}

void Renderer::EndUnitMainBatch() {
    if (m_unitMainVertexCount > 0) {
        FlushUnitMainSprites();
    }
}

void Renderer::EndUnitRotatingBatch() {
    if (m_unitRotatingVertexCount > 0) {
        FlushUnitRotating();
    }
}

void Renderer::EndEffectsCircleBatch() {
    FlushEffectsCircleFills();           
    FlushEffectsCircleOutlinesThick();   
    FlushEffectsCircleOutlines();        
}

void Renderer::EndUnitStateBatch() {
    if (m_unitStateVertexCount > 0) {
        FlushUnitStateIcons();
    }
}

void Renderer::EndUnitCounterBatch() {
    if (m_unitCounterVertexCount > 0) {
        FlushUnitCounters();
    }
}

void Renderer::EndUnitNukeBatch() {
    if (m_unitNukeVertexCount > 0) {
        FlushUnitNukeIcons();
    }
}

void Renderer::EndEffectsLineBatch() {
    FlushEffectsLines();
    FlushEffectsRects();
}

void Renderer::EndEffectsSpriteBatch() {
    if (m_effectsSpriteVertexCount > 0) {
        FlushEffectsSprites();
    }
}

void Renderer::EndUnitHighlightBatch() {
    if (m_unitHighlightVertexCount > 0) {
        FlushUnitHighlights();
    }
}

void Renderer::EndMapTextBatch() {
    EndTextBatch();
}

void Renderer::EndFrameTextBatch() {
    // flush any remaining text at the end of the frame
    EndTextBatch();
}

void Renderer::EndHealthBarBatch() {
    if (m_healthBarVertexCount > 0) {
        FlushHealthBars();
    }
}

void Renderer::EndUITriangleBatch() {
    if (m_uiTriangleVertexCount > 0) {
        FlushUITriangles();
    }
}

void Renderer::EndUILineBatch() {
    if (m_uiLineVertexCount > 0) {
        FlushUILines();
    }
}

void Renderer::EndUIBatch() {
    EndUITriangleBatch();
    EndUILineBatch();
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

void Renderer::FlushUnitTrailsIfFull(int segmentsNeeded) {
    if (m_unitTrailVertexCount + segmentsNeeded > MAX_UNIT_TRAIL_VERTICES) {
        FlushUnitTrails();
    }
}

void Renderer::FlushUnitMainSpritesIfFull(int verticesNeeded) {
    if (m_unitMainVertexCount + verticesNeeded > MAX_UNIT_MAIN_VERTICES) {
        FlushUnitMainSprites();
    }
}

void Renderer::FlushUnitRotatingIfFull(int verticesNeeded) {
    if (m_unitRotatingVertexCount + verticesNeeded > MAX_UNIT_ROTATING_VERTICES) {
        FlushUnitRotating();
    }
}

void Renderer::FlushUnitStateIconsIfFull(int verticesNeeded) {
    if (m_unitStateVertexCount + verticesNeeded > MAX_UNIT_STATE_VERTICES) {
        FlushUnitStateIcons();
    }
}

void Renderer::FlushUnitNukeIconsIfFull(int verticesNeeded) {
    if (m_unitNukeVertexCount + verticesNeeded > MAX_UNIT_NUKE_VERTICES) {
        FlushUnitNukeIcons();
    }
}

void Renderer::FlushEffectsLinesIfFull(int segmentsNeeded) {
    if (m_effectsLineVertexCount + segmentsNeeded > MAX_EFFECTS_LINE_VERTICES) {
        FlushEffectsLines();
    }
}

void Renderer::FlushEffectsRectsIfFull(int segmentsNeeded) {
    if (m_effectsRectVertexCount + segmentsNeeded > MAX_EFFECTS_LINE_VERTICES) {
        FlushEffectsRects();
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

void Renderer::FlushVertices(unsigned int primitiveType, bool useTexture) {
    if (primitiveType == GL_TRIANGLES) {
        FlushTriangles(useTexture);
    } else if (primitiveType == GL_LINES) {
        FlushLines();
    }
}

void Renderer::FlushUITriangles() {
    if (m_uiTriangleVertexCount == 0) return;
    
    StartFlushTiming("UI_Triangles"); 
    IncrementDrawCall("ui_triangles");
    
    SetShaderProgram(m_colorShaderProgram);
    SetColorShaderUniforms();
    
    SetVertexArray(m_uiVAO);
    SetArrayBuffer(m_uiVBO);
    UploadVertexDataToVBO(m_uiVBO, m_uiTriangleVertices, m_uiTriangleVertexCount, GL_STREAM_DRAW);
        
    glDrawArrays(GL_TRIANGLES, 0, m_uiTriangleVertexCount);
    
    m_uiTriangleVertexCount = 0;
    
    EndFlushTiming("UI_Triangles");
}

void Renderer::FlushUILines() {
    if (m_uiLineVertexCount == 0) return;
    
    StartFlushTiming("UI_Lines");
    IncrementDrawCall("ui_lines");
    
    SetShaderProgram(m_colorShaderProgram);
    SetColorShaderUniforms();
    
    SetVertexArray(m_uiVAO);
    SetArrayBuffer(m_uiVBO);
    UploadVertexDataToVBO(m_uiVBO, m_uiLineVertices, m_uiLineVertexCount, GL_STREAM_DRAW);
           
    glDrawArrays(GL_LINES, 0, m_uiLineVertexCount);
    
    m_uiLineVertexCount = 0;
    
    EndFlushTiming("UI_Lines");
}

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

void Renderer::FlushLines() {
    if (m_lineVertexCount == 0) return;
    
    StartFlushTiming("Legacy_Lines");
    IncrementDrawCall("legacy_lines");
    
    SetShaderProgram(m_colorShaderProgram);
    SetColorShaderUniforms();
    
    SetVertexArray(m_legacyVAO);
    SetArrayBuffer(m_legacyVBO);
    UploadVertexDataToVBO(m_legacyVBO, m_lineVertices, m_lineVertexCount, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_LINES, 0, m_lineVertexCount);
    
    m_lineVertexCount = 0;
    
    EndFlushTiming("Legacy_Lines"); 
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

void Renderer::FlushUnitTrails() {
    if (m_unitTrailVertexCount == 0) return;
    
    StartFlushTiming("Unit_Trails");
    IncrementDrawCall("unit_trails");
    
#ifndef TARGET_EMSCRIPTEN
    SetLineWidth(g_preferences->GetFloat(PREFS_GRAPHICS_UNIT_TRAIL_THICKNESS)); 
#endif
    
    SetShaderProgram(m_colorShaderProgram);
    SetColorShaderUniforms();
    
    SetVertexArray(m_effectsVAO);
    SetArrayBuffer(m_effectsVBO);
    UploadVertexDataToVBO(m_effectsVBO, m_unitTrailVertices, m_unitTrailVertexCount, GL_DYNAMIC_DRAW); 
    
    glDrawArrays(GL_LINES, 0, m_unitTrailVertexCount);
    
    m_unitTrailVertexCount = 0;
    
    EndFlushTiming("Unit_Trails"); 
}


void Renderer::FlushUnitMainSprites() {
    if (m_unitMainVertexCount == 0) return;
    
    StartFlushTiming("Unit_Main");
    IncrementDrawCall("unit_main_sprites");
    
    SetShaderProgram(m_textureShaderProgram);
    SetTextureShaderUniforms();
    
    // bind texture here during flush
    if (m_currentUnitMainTexture != 0) {
        SetActiveTexture(GL_TEXTURE0);
        SetBoundTexture(m_currentUnitMainTexture);
    }
    
    SetVertexArray(m_unitVAO);
    SetArrayBuffer(m_unitVBO);
    UploadVertexDataToVBO(m_unitVBO, m_unitMainVertices, m_unitMainVertexCount, GL_STREAM_DRAW);   
    
    glDrawArrays(GL_TRIANGLES, 0, m_unitMainVertexCount);
    
    m_unitMainVertexCount = 0;
    
    EndFlushTiming("Unit_Main");
}

void Renderer::FlushUnitRotating() {
    if (m_unitRotatingVertexCount == 0) return;
    
    StartFlushTiming("Unit_Rotating");
    IncrementDrawCall("unit_rotating");
    
    SetShaderProgram(m_textureShaderProgram);
    SetTextureShaderUniforms();
    
    if (m_currentUnitRotatingTexture != 0) {
        SetActiveTexture(GL_TEXTURE0);
        SetBoundTexture(m_currentUnitRotatingTexture);
    }
    
    SetVertexArray(m_unitVAO);
    SetArrayBuffer(m_unitVBO);
    UploadVertexDataToVBO(m_unitVBO, m_unitRotatingVertices, m_unitRotatingVertexCount, GL_STREAM_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_unitRotatingVertexCount);
    
    m_unitRotatingVertexCount = 0;
    
    EndFlushTiming("Unit_Rotating");
}

void Renderer::FlushEffectsCircleFills() {
    if (m_effectsCircleFillVertexCount == 0) return;
    
    IncrementDrawCall("effects_circle_fills");
    StartFlushTiming("Effects_CircleFills");

    SetShaderProgram(m_colorShaderProgram);
    SetColorShaderUniforms();
    
    SetVertexArray(m_effectsVAO);
    SetArrayBuffer(m_effectsVBO);
    UploadVertexDataToVBO(m_effectsVBO, m_radarFillVertices, m_effectsCircleFillVertexCount, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_effectsCircleFillVertexCount);
    
    m_effectsCircleFillVertexCount = 0;

    EndFlushTiming("Effects_CircleFills");
}

void Renderer::FlushEffectsCircleOutlines() {
    if (m_effectsCircleOutlineVertexCount == 0) return;
    
    IncrementDrawCall("effects_circle_outlines_thin");
    StartFlushTiming("Effects_CircleOutlinesThin");

#ifndef TARGET_EMSCRIPTEN
    SetLineWidth(1.0f); 
#endif
    
    SetShaderProgram(m_colorShaderProgram);
    SetColorShaderUniforms();
    
    SetVertexArray(m_effectsVAO);
    SetArrayBuffer(m_effectsVBO);
    UploadVertexDataToVBO(m_effectsVBO, m_effectsCircleOutlineVertices, m_effectsCircleOutlineVertexCount, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_LINES, 0, m_effectsCircleOutlineVertexCount);
    
    m_effectsCircleOutlineVertexCount = 0;

    EndFlushTiming("Effects_CircleOutlinesThin");
}

void Renderer::FlushEffectsCircleOutlinesThick() {
    if (m_effectsCircleOutlineThickVertexCount == 0) return;
    
    IncrementDrawCall("effects_circle_outlines_thick");
    StartFlushTiming("Effects_CircleOutlinesThick");

#ifndef TARGET_EMSCRIPTEN
    SetLineWidth(3.0f); // Own radar thick lines
#endif
    
    SetShaderProgram(m_colorShaderProgram);
    SetColorShaderUniforms();
    
    SetVertexArray(m_effectsVAO);
    SetArrayBuffer(m_effectsVBO);
    UploadVertexDataToVBO(m_effectsVBO, m_effectsCircleOutlineThickVertices, m_effectsCircleOutlineThickVertexCount, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_LINES, 0, m_effectsCircleOutlineThickVertexCount);
    
    m_effectsCircleOutlineThickVertexCount = 0;

    EndFlushTiming("Effects_CircleOutlinesThick");
}

void Renderer::FlushUnitStateIcons() {
    if (m_unitStateVertexCount == 0) return;
    
    StartFlushTiming("Unit_StateIcons");
    IncrementDrawCall("unit_state_icons");

    SetShaderProgram(m_textureShaderProgram);
    SetTextureShaderUniforms();
    
    if (m_currentUnitStateTexture != 0) {
        SetActiveTexture(GL_TEXTURE0);
        SetBoundTexture(m_currentUnitStateTexture);
    }
    
    SetVertexArray(m_unitVAO);
    SetArrayBuffer(m_unitVBO);
    UploadVertexDataToVBO(m_unitVBO, m_unitStateVertices, m_unitStateVertexCount, GL_STREAM_DRAW); 
        
    glDrawArrays(GL_TRIANGLES, 0, m_unitStateVertexCount);
    
    m_unitStateVertexCount = 0;
    
    EndFlushTiming("Unit_StateIcons");
}

void Renderer::FlushUnitCounters() {
    if (m_unitCounterVertexCount == 0) return;
    
    StartFlushTiming("Unit_Counters");
    IncrementDrawCall("unit_counters");
    
    SetShaderProgram(m_textureShaderProgram);
    SetTextureShaderUniforms();
    
    SetVertexArray(m_unitVAO);
    SetArrayBuffer(m_unitVBO);
    UploadVertexDataToVBO(m_unitVBO, m_unitCounterVertices, m_unitCounterVertexCount, GL_STREAM_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_unitCounterVertexCount);
    
    m_unitCounterVertexCount = 0;
    
    EndFlushTiming("Unit_Counters");
}

void Renderer::FlushUnitNukeIcons() {
    if (m_unitNukeVertexCount == 0) return;
    
    StartFlushTiming("Unit_Nukes");
    IncrementDrawCall("unit_nuke_icons");
    
    SetShaderProgram(m_textureShaderProgram);
    SetTextureShaderUniforms();

    if (m_currentUnitNukeTexture != 0) {
        SetActiveTexture(GL_TEXTURE0);
        SetBoundTexture(m_currentUnitNukeTexture);
    }
    
    SetVertexArray(m_unitVAO);
    SetArrayBuffer(m_unitVBO);
    UploadVertexDataToVBO(m_unitVBO, m_unitNukeVertices, m_unitNukeVertexCount, GL_STREAM_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_unitNukeVertexCount);
    
    m_unitNukeVertexCount = 0;
    
    EndFlushTiming("Unit_Nukes");
}

void Renderer::FlushEffectsLines() {
    if (m_effectsLineVertexCount == 0) return;
    
    StartFlushTiming("Effects_Lines");
    IncrementDrawCall("effects_lines");

#ifndef TARGET_EMSCRIPTEN
    SetLineWidth(1.0f);  // Set once during flush, not per line
#endif
    
    SetShaderProgram(m_colorShaderProgram);
    SetColorShaderUniforms();
    
    SetVertexArray(m_effectsVAO);
    SetArrayBuffer(m_effectsVBO);
    UploadVertexDataToVBO(m_effectsVBO, m_effectsLineVertices, m_effectsLineVertexCount, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_LINES, 0, m_effectsLineVertexCount);
    
    m_effectsLineVertexCount = 0;
    
    EndFlushTiming("Effects_Lines");
}

void Renderer::FlushEffectsRects() {
    if (m_effectsRectVertexCount == 0) return;
    
    StartFlushTiming("Effects_Rects");
    IncrementDrawCall("effects_rects");

#ifndef TARGET_EMSCRIPTEN
    SetLineWidth(1.0f);
#endif
    
    SetShaderProgram(m_colorShaderProgram);
    SetColorShaderUniforms();
    
    SetVertexArray(m_effectsVAO);
    SetArrayBuffer(m_effectsVBO);
    UploadVertexDataToVBO(m_effectsVBO, m_effectsRectVertices, m_effectsRectVertexCount, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_LINES, 0, m_effectsRectVertexCount);
    
    m_effectsRectVertexCount = 0;
    
    EndFlushTiming("Effects_Rects");
}

void Renderer::FlushEffectsSprites() {
    if (m_effectsSpriteVertexCount == 0) return;
    
    StartFlushTiming("Effects_Sprites");
    IncrementDrawCall("effects_sprites");
    
    SetShaderProgram(m_textureShaderProgram);
    SetTextureShaderUniforms();
    
    if (m_currentEffectsSpriteTexture != 0) {
        SetActiveTexture(GL_TEXTURE0);
        SetBoundTexture(m_currentEffectsSpriteTexture);
    }
    
    SetVertexArray(m_effectsVAO);
    SetArrayBuffer(m_effectsVBO);
    UploadVertexDataToVBO(m_effectsVBO, m_effectsSpriteVertices, m_effectsSpriteVertexCount, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_effectsSpriteVertexCount);
    
    m_effectsSpriteVertexCount = 0;
    
    EndFlushTiming("Effects_Sprites");
}

void Renderer::FlushUnitHighlights() {
    if (m_unitHighlightVertexCount == 0) return;
    
    StartFlushTiming("Unit_Highlights");
    IncrementDrawCall("unit_highlights");
    
    SetShaderProgram(m_textureShaderProgram);
    SetTextureShaderUniforms();
    
    if (m_currentUnitHighlightTexture != 0) {
        SetActiveTexture(GL_TEXTURE0);
        SetBoundTexture(m_currentUnitHighlightTexture);
    }
    
    SetVertexArray(m_unitVAO);
    SetArrayBuffer(m_unitVBO);
    UploadVertexDataToVBO(m_unitVBO, m_unitHighlightVertices, m_unitHighlightVertexCount, GL_STREAM_DRAW);
 
    glDrawArrays(GL_TRIANGLES, 0, m_unitHighlightVertexCount);
    
    m_unitHighlightVertexCount = 0;
    
    EndFlushTiming("Unit_Highlights");
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

void Renderer::FlushWhiteboard() {
    if (m_whiteboardVertexCount == 0) return;
    
    StartFlushTiming("Whiteboard");
    IncrementDrawCall("whiteboard");
    
#ifndef TARGET_EMSCRIPTEN
    SetLineWidth(g_preferences->GetFloat(PREFS_GRAPHICS_WHITEBOARD_THICKNESS));
#endif
    
    SetShaderProgram(m_colorShaderProgram);
    SetColorShaderUniforms();
    
    SetVertexArray(m_legacyVAO);
    SetArrayBuffer(m_legacyVBO);
    UploadVertexDataToVBO(m_legacyVBO, m_whiteboardVertices, m_whiteboardVertexCount, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_LINES, 0, m_whiteboardVertexCount);
    
    m_whiteboardVertexCount = 0;
    
    EndFlushTiming("Whiteboard");
}