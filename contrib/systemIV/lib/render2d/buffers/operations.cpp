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

void Renderer::FlushTextContext() {
    FlushTextBuffer();
}

// ============================================================================
// BEGIN SCENES
// ============================================================================

void Renderer::BeginTextBatch() {
    m_allowImmedateFlush = false;
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

// ============================================================================
// END SCENES
// ============================================================================

void Renderer::EndTextBatch() {
    FlushTextContext();
    m_allowImmedateFlush = true;
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

// ============================================================================
// NOW FLUSH IF FULL
// ============================================================================

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

// ============================================================================
// CORE FLUSH FUNCTIONS
// ============================================================================

void Renderer::FlushUITriangles() {
    if (m_uiTriangleVertexCount == 0) return;
    
    IncrementDrawCall("ui_triangles");
    
    glUseProgram(m_colorShaderProgram);
    
    int projLoc = glGetUniformLocation(m_colorShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_colorShaderProgram, "uModelView");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    
    
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_uiTriangleVertexCount * sizeof(Vertex2D), m_uiTriangleVertices);
        
    glDrawArrays(GL_TRIANGLES, 0, m_uiTriangleVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_uiTriangleVertexCount = 0;
}

void Renderer::FlushUILines() {
    if (m_uiLineVertexCount == 0) return;
    
    IncrementDrawCall("ui_lines");
    
    glUseProgram(m_colorShaderProgram);
    
    int projLoc = glGetUniformLocation(m_colorShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_colorShaderProgram, "uModelView");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_uiLineVertexCount * sizeof(Vertex2D), m_uiLineVertices);
           
    glDrawArrays(GL_LINES, 0, m_uiLineVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_uiLineVertexCount = 0;
}

void Renderer::FlushTextBuffer() {
    if (m_textVertexCount == 0) return;
    
    IncrementDrawCall("text");
    
    // save current blend state
    int currentBlendSrc = m_blendSrcFactor;
    int currentBlendDst = m_blendDstFactor;
    
    // set correct blend mode for text rendering (additive for smooth fonts)
    if (m_blendMode != BlendModeSubtractive) {
        SetBlendFunc(GL_SRC_ALPHA, GL_ONE);
    }
    
    glUseProgram(m_textureShaderProgram);
    
    int projLoc = glGetUniformLocation(m_textureShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_textureShaderProgram, "uModelView");
    int texLoc = glGetUniformLocation(m_textureShaderProgram, "ourTexture");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    glUniform1i(texLoc, 0);
    
    // bind the current text texture
    if (m_currentTextTexture != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_currentTextTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    }
    
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_textVertexCount * sizeof(Vertex2D), m_textVertices);  
    
    glDrawArrays(GL_TRIANGLES, 0, m_textVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    // restore previous blend state
    SetBlendFunc(currentBlendSrc, currentBlendDst);
    
    m_textVertexCount = 0;
    m_currentTextTexture = 0;  // reset texture tracking
}

void Renderer::FlushUnitTrails() {
    if (m_unitTrailVertexCount == 0) return;
    
    IncrementDrawCall("unit_trails");
    
// im not sure where the linewidth was defined before the 
// refactor but here seems like a good place to set line width
#ifndef TARGET_EMSCRIPTEN
    glLineWidth(g_preferences->GetFloat(PREFS_GRAPHICS_UNIT_TRAIL_THICKNESS)); 
#endif
    
    glUseProgram(m_colorShaderProgram);
    
    int projLoc = glGetUniformLocation(m_colorShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_colorShaderProgram, "uModelView");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_unitTrailVertexCount * sizeof(Vertex2D), m_unitTrailVertices); 
    
    glDrawArrays(GL_LINES, 0, m_unitTrailVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_unitTrailVertexCount = 0;
}


void Renderer::FlushUnitMainSprites() {
    if (m_unitMainVertexCount == 0) return;
    
    IncrementDrawCall("unit_main_sprites");
    
    glUseProgram(m_textureShaderProgram);
    
    if (m_currentUnitMainTexture != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_currentUnitMainTexture);
    }
    
    int projLoc = glGetUniformLocation(m_textureShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_textureShaderProgram, "uModelView");
    int texLoc = glGetUniformLocation(m_textureShaderProgram, "ourTexture");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    glUniform1i(texLoc, 0);
    
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    
    
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_unitMainVertexCount * sizeof(Vertex2D), m_unitMainVertices);   
    
    glDrawArrays(GL_TRIANGLES, 0, m_unitMainVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_unitMainVertexCount = 0;
}

void Renderer::FlushUnitRotating() {
    if (m_unitRotatingVertexCount == 0) return;
    
    IncrementDrawCall("unit_rotating");
    
    glUseProgram(m_textureShaderProgram);
    
    if (m_currentUnitRotatingTexture != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_currentUnitRotatingTexture);
    }
    
    int projLoc = glGetUniformLocation(m_textureShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_textureShaderProgram, "uModelView");
    int texLoc = glGetUniformLocation(m_textureShaderProgram, "ourTexture");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    glUniform1i(texLoc, 0);
    
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_unitRotatingVertexCount * sizeof(Vertex2D), m_unitRotatingVertices);
    
    glDrawArrays(GL_TRIANGLES, 0, m_unitRotatingVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_unitRotatingVertexCount = 0;
}

void Renderer::FlushEffectsCircleFills() {
    if (m_effectsCircleFillVertexCount == 0) return;
    
    IncrementDrawCall("effects_circle_fills");
    
    glUseProgram(m_colorShaderProgram);
    
    int projLoc = glGetUniformLocation(m_colorShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_colorShaderProgram, "uModelView");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_effectsCircleFillVertexCount * sizeof(Vertex2D), m_radarFillVertices);
    
    glDrawArrays(GL_TRIANGLES, 0, m_effectsCircleFillVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_effectsCircleFillVertexCount = 0;
}

void Renderer::FlushEffectsCircleOutlines() {
    if (m_effectsCircleOutlineVertexCount == 0) return;
    
    IncrementDrawCall("effects_circle_outlines_thin");

#ifndef TARGET_EMSCRIPTEN
    glLineWidth(1.0f); 
#endif
    
    glUseProgram(m_colorShaderProgram);
    
    int projLoc = glGetUniformLocation(m_colorShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_colorShaderProgram, "uModelView");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_effectsCircleOutlineVertexCount * sizeof(Vertex2D), m_effectsCircleOutlineVertices);
    
    glDrawArrays(GL_LINES, 0, m_effectsCircleOutlineVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_effectsCircleOutlineVertexCount = 0;
}

void Renderer::FlushEffectsCircleOutlinesThick() {
    if (m_effectsCircleOutlineThickVertexCount == 0) return;
    
    IncrementDrawCall("effects_circle_outlines_thick");

#ifndef TARGET_EMSCRIPTEN
    glLineWidth(3.0f); // Own radar thick lines
#endif
    
    glUseProgram(m_colorShaderProgram);
    
    int projLoc = glGetUniformLocation(m_colorShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_colorShaderProgram, "uModelView");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_effectsCircleOutlineThickVertexCount * sizeof(Vertex2D), m_effectsCircleOutlineThickVertices);
    
    glDrawArrays(GL_LINES, 0, m_effectsCircleOutlineThickVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_effectsCircleOutlineThickVertexCount = 0;
}

void Renderer::FlushUnitStateIcons() {
    if (m_unitStateVertexCount == 0) return;
    
    IncrementDrawCall("unit_state_icons");
    
    glUseProgram(m_textureShaderProgram);

    if (m_currentUnitStateTexture != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_currentUnitStateTexture);
    }
    
    int projLoc = glGetUniformLocation(m_textureShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_textureShaderProgram, "uModelView");
    int texLoc = glGetUniformLocation(m_textureShaderProgram, "ourTexture");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    glUniform1i(texLoc, 0);
    
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_unitStateVertexCount * sizeof(Vertex2D), m_unitStateVertices); 
        
    glDrawArrays(GL_TRIANGLES, 0, m_unitStateVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_unitStateVertexCount = 0;
}

void Renderer::FlushUnitCounters() {
    if (m_unitCounterVertexCount == 0) return;
    
    IncrementDrawCall("unit_counters");
    
    glUseProgram(m_textureShaderProgram);
    
    int projLoc = glGetUniformLocation(m_textureShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_textureShaderProgram, "uModelView");
    int texLoc = glGetUniformLocation(m_textureShaderProgram, "ourTexture");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    glUniform1i(texLoc, 0);
    
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_unitCounterVertexCount * sizeof(Vertex2D), m_unitCounterVertices);
    
    glDrawArrays(GL_TRIANGLES, 0, m_unitCounterVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_unitCounterVertexCount = 0;
}

void Renderer::FlushUnitNukeIcons() {
    if (m_unitNukeVertexCount == 0) return;
    
    IncrementDrawCall("unit_nuke_icons");
    
    glUseProgram(m_textureShaderProgram);

    if (m_currentUnitNukeTexture != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_currentUnitNukeTexture);
    }
    
    int projLoc = glGetUniformLocation(m_textureShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_textureShaderProgram, "uModelView");
    int texLoc = glGetUniformLocation(m_textureShaderProgram, "ourTexture");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    glUniform1i(texLoc, 0);
    
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_unitNukeVertexCount * sizeof(Vertex2D), m_unitNukeVertices);
    
    glDrawArrays(GL_TRIANGLES, 0, m_unitNukeVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_unitNukeVertexCount = 0;
}

void Renderer::FlushEffectsLines() {
    if (m_effectsLineVertexCount == 0) return;
    
    IncrementDrawCall("effects_lines");

// im not sure where the linewidth was defined before the 
// refactor but here seems like a good place to set line width
#ifndef TARGET_EMSCRIPTEN
    glLineWidth(1.5f);
#endif
    
    glUseProgram(m_colorShaderProgram);
    
    int projLoc = glGetUniformLocation(m_colorShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_colorShaderProgram, "uModelView");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_effectsLineVertexCount * sizeof(Vertex2D), m_effectsLineVertices);
    
    glDrawArrays(GL_LINES, 0, m_effectsLineVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_effectsLineVertexCount = 0;
}

void Renderer::FlushEffectsRects() {
    if (m_effectsRectVertexCount == 0) return;
    
    IncrementDrawCall("effects_rects");

// im not sure where the linewidth was defined before the 
// refactor but here seems like a good place to set line width
#ifndef TARGET_EMSCRIPTEN
    glLineWidth(1.5f);
#endif
    
    glUseProgram(m_colorShaderProgram);
    
    int projLoc = glGetUniformLocation(m_colorShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_colorShaderProgram, "uModelView");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_effectsRectVertexCount * sizeof(Vertex2D), m_effectsRectVertices);
    
    glDrawArrays(GL_LINES, 0, m_effectsRectVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_effectsRectVertexCount = 0;
}

void Renderer::FlushEffectsSprites() {
    if (m_effectsSpriteVertexCount == 0) return;
    
    IncrementDrawCall("effects_sprites");
    
    glUseProgram(m_textureShaderProgram);
    
    if (m_currentEffectsSpriteTexture != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_currentEffectsSpriteTexture);
    }
    
    int projLoc = glGetUniformLocation(m_textureShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_textureShaderProgram, "uModelView");
    int texLoc = glGetUniformLocation(m_textureShaderProgram, "ourTexture");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    glUniform1i(texLoc, 0);
    
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_effectsSpriteVertexCount * sizeof(Vertex2D), m_effectsSpriteVertices);
    
    glDrawArrays(GL_TRIANGLES, 0, m_effectsSpriteVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_effectsSpriteVertexCount = 0;
}

void Renderer::FlushUnitHighlights() {
    if (m_unitHighlightVertexCount == 0) return;
    
    IncrementDrawCall("unit_highlights");
    
    glUseProgram(m_textureShaderProgram);
    
    int projLoc = glGetUniformLocation(m_textureShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_textureShaderProgram, "uModelView");
    int texLoc = glGetUniformLocation(m_textureShaderProgram, "ourTexture");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    glUniform1i(texLoc, 0);
    
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_unitHighlightVertexCount * sizeof(Vertex2D), m_unitHighlightVertices);
 
    glDrawArrays(GL_TRIANGLES, 0, m_unitHighlightVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_unitHighlightVertexCount = 0;
}

// flush the buffer
void Renderer::FlushHealthBars() {
    if (m_healthBarVertexCount == 0) return;
    
    IncrementDrawCall("health_bars");
    
    glUseProgram(m_colorShaderProgram);
    
    int projLoc = glGetUniformLocation(m_colorShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_colorShaderProgram, "uModelView");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_healthBarVertexCount * sizeof(Vertex2D), m_healthBarVertices);
    
    glDrawArrays(GL_TRIANGLES, 0, m_healthBarVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_healthBarVertexCount = 0;
}