#include "lib/universal_include.h"

#include <stdarg.h>

#include "lib/preferences.h"

#include "renderer/map_renderer.h"
#include "lib/render3d/renderer_3d.h"

extern Renderer3D *g_renderer3d;

// ============================================================================
// FLUSH ALL SPECIALIZED BUFFERS
// ============================================================================

void Renderer3D::FlushAllSpecializedBuffers3D() {
    FlushStarField3D();
    FlushGlobeSurface3D();
    FlushUnitTrails3D();
    FlushUnitMainSprites3D();
    FlushUnitRotating3D();
    FlushUnitStateIcons3D();
    FlushUnitNukeIcons3D();
    FlushUnitHighlights3D();
    FlushUnitCounters3D();
    FlushEffectsLines3D();
    FlushEffectsSprites3D();
    FlushHealthBars3D();
    FlushTextBuffer3D();
    FlushNuke3DModels3D();
}

void Renderer3D::FlushAllUnitBuffers3D() {
    FlushUnitTrails3D();
    FlushUnitMainSprites3D();
    FlushUnitRotating3D();
    FlushUnitStateIcons3D();
    FlushUnitNukeIcons3D();
    FlushUnitHighlights3D();
    FlushUnitCounters3D();
}

void Renderer3D::FlushAllEffectBuffers3D() {
    FlushEffectsLines3D();
    FlushEffectsSprites3D();
}

void Renderer3D::FlushAllNuke3DModelBuffers3D() {
    FlushNuke3DModels3D();
}

// ============================================================================
// BEGIN SCENES
// ============================================================================

void Renderer3D::BeginUnitTrailBatch3D() {
    // trail segments will be batched until EndUnitTrailBatch3D()
}

void Renderer3D::BeginUnitMainBatch3D() {
    m_unitMainVertexCount3D = 0;
    m_currentUnitMainTexture3D = 0;
}

void Renderer3D::BeginUnitRotatingBatch3D() {
    m_unitRotatingVertexCount3D = 0;
    m_currentUnitRotatingTexture3D = 0;
}

void Renderer3D::BeginUnitStateBatch3D() {
    m_unitStateVertexCount3D = 0;
    m_currentUnitStateTexture3D = 0;
}

void Renderer3D::BeginUnitCounterBatch3D() {
    m_unitCounterVertexCount3D = 0;
    m_currentUnitCounterTexture3D = 0;
}

void Renderer3D::BeginUnitNukeBatch3D() {
    m_unitNukeVertexCount3D = 0;
    m_currentUnitNukeTexture3D = 0;
}

void Renderer3D::BeginEffectsLineBatch3D() {
    // Effects line segments will be batched until EndEffectsLineBatch3D()
}

void Renderer3D::BeginEffectsSpriteBatch3D() {
    m_effectsSpriteVertexCount3D = 0;
    m_currentEffectsSpriteTexture3D = 0;
}

void Renderer3D::BeginUnitHighlightBatch3D() {
    m_unitHighlightVertexCount3D = 0;
    m_currentUnitHighlightTexture3D = 0;
}

void Renderer3D::BeginMapTextBatch3D() {
    BeginTextBatch3D();
}

void Renderer3D::BeginFrameTextBatch3D() {
    // reset text buffer for the new frame
    m_textVertexCount3D = 0;
    m_currentTextTexture3D = 0;
    BeginTextBatch3D();
}

void Renderer3D::BeginHealthBarBatch3D() {
    m_healthBarVertexCount3D = 0;
}

void Renderer3D::BeginTextBatch3D() {
    m_textVertexCount3D = 0;
    m_currentTextTexture3D = 0;
}

void Renderer3D::BeginNuke3DModelBatch3D() {
    m_nuke3DModelVertexCount3D = 0;
}

void Renderer3D::BeginStarFieldBatch3D() {
    m_starFieldVertexCount3D = 0;
    m_currentStarFieldTexture3D = 0;
}

void Renderer3D::BeginGlobeSurfaceBatch3D() {
    m_globeSurfaceVertexCount3D = 0;
}

// ============================================================================
// END SCENES
// ============================================================================

void Renderer3D::EndUnitTrailBatch3D() {
    FlushUnitTrails3D();
}

void Renderer3D::EndUnitMainBatch3D() {
    if (m_unitMainVertexCount3D > 0) {
        FlushUnitMainSprites3D();
    }
}

void Renderer3D::EndUnitRotatingBatch3D() {
    if (m_unitRotatingVertexCount3D > 0) {
        FlushUnitRotating3D();
    }
}

void Renderer3D::EndUnitStateBatch3D() {
    if (m_unitStateVertexCount3D > 0) {
        FlushUnitStateIcons3D();
    }
}

void Renderer3D::EndUnitCounterBatch3D() {
    if (m_unitCounterVertexCount3D > 0) {
        FlushUnitCounters3D();
    }
}

void Renderer3D::EndUnitNukeBatch3D() {
    if (m_unitNukeVertexCount3D > 0) {
        FlushUnitNukeIcons3D();
    }
}

void Renderer3D::EndEffectsLineBatch3D() {
    FlushEffectsLines3D();
}

void Renderer3D::EndEffectsSpriteBatch3D() {
    if (m_effectsSpriteVertexCount3D > 0) {
        FlushEffectsSprites3D();
    }
}

void Renderer3D::EndUnitHighlightBatch3D() {
    if (m_unitHighlightVertexCount3D > 0) {
        FlushUnitHighlights3D();
    }
}

void Renderer3D::EndMapTextBatch3D() {
    EndTextBatch3D();
}

void Renderer3D::EndFrameTextBatch3D() {
    // flush any remaining text at the end of the frame
    EndTextBatch3D();
}

void Renderer3D::EndHealthBarBatch3D() {
    if (m_healthBarVertexCount3D > 0) {
        FlushHealthBars3D();
    }
}

void Renderer3D::EndTextBatch3D() {
    FlushTextContext3D();
}

void Renderer3D::EndNuke3DModelBatch3D() {
    FlushNuke3DModels3D();
}

void Renderer3D::EndStarFieldBatch3D() {
    if (m_starFieldVertexCount3D > 0) {
        FlushStarField3D();
    }
}

void Renderer3D::EndGlobeSurfaceBatch3D() {
    if (m_globeSurfaceVertexCount3D > 0) {
        FlushGlobeSurface3D();
    }
}

// ============================================================================
// FLUSH IF FULL
// ============================================================================

void Renderer3D::FlushUnitTrails3DIfFull(int segmentsNeeded) {
    if (m_unitTrailVertexCount3D + segmentsNeeded > MAX_UNIT_TRAIL_VERTICES_3D) {
        FlushUnitTrails3D();
    }
}

void Renderer3D::FlushUnitMainSprites3DIfFull(int verticesNeeded) {
    if (m_unitMainVertexCount3D + verticesNeeded > MAX_UNIT_MAIN_VERTICES_3D) {
        FlushUnitMainSprites3D();
    }
}

void Renderer3D::FlushUnitRotating3DIfFull(int verticesNeeded) {
    if (m_unitRotatingVertexCount3D + verticesNeeded > MAX_UNIT_ROTATING_VERTICES_3D) {
        FlushUnitRotating3D();
    }
}

void Renderer3D::FlushUnitStateIcons3DIfFull(int verticesNeeded) {
    if (m_unitStateVertexCount3D + verticesNeeded > MAX_UNIT_STATE_VERTICES_3D) {
        FlushUnitStateIcons3D();
    }
}

void Renderer3D::FlushUnitNukeIcons3DIfFull(int verticesNeeded) {
    if (m_unitNukeVertexCount3D + verticesNeeded > MAX_UNIT_NUKE_VERTICES_3D) {
        FlushUnitNukeIcons3D();
    }
}

void Renderer3D::FlushEffectsLines3DIfFull(int segmentsNeeded) {
    if (m_effectsLineVertexCount3D + segmentsNeeded > MAX_EFFECTS_LINE_VERTICES_3D) {
        FlushEffectsLines3D();
    }
}

void Renderer3D::FlushStarField3DIfFull(int verticesNeeded) {
    if (m_starFieldVertexCount3D + verticesNeeded > MAX_STAR_FIELD_VERTICES_3D) {
        FlushStarField3D();
    }
}

void Renderer3D::FlushGlobeSurface3DIfFull(int verticesNeeded) {
    if (m_globeSurfaceVertexCount3D + verticesNeeded > MAX_GLOBE_SURFACE_VERTICES_3D) {
        FlushGlobeSurface3D();
    }
}

void Renderer3D::FlushNuke3DModels3DIfFull(int verticesNeeded) {
    if (m_nuke3DModelVertexCount3D + verticesNeeded >= MAX_NUKE_3D_MODEL_VERTICES_3D) {
        FlushNuke3DModels3D();
    }
}

// ============================================================================
// CORE FLUSH FUNCTIONS
// ============================================================================

void Renderer3D::FlushUnitTrails3D() {
    if (m_unitTrailVertexCount3D == 0) return;

    IncrementDrawCall3D("unit_trails");
    
#ifndef TARGET_EMSCRIPTEN
    glLineWidth(g_preferences->GetFloat(PREFS_GLOBE_UNIT_TRAIL_THICKNESS, 1.0f));
#endif
    
    // use 3D shader program
    glUseProgram(m_shader3DProgram);
    
    int projLoc = glGetUniformLocation(m_shader3DProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_shader3DProgram, "uModelView");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix3D.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix3D.m);
    
    // set fog uniforms
    SetFogUniforms3D(m_shader3DProgram);
    
    glBindVertexArray(m_VAO3D);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO3D);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_unitTrailVertexCount3D * sizeof(Vertex3D), m_unitTrailVertices3D);
    
    glDrawArrays(GL_LINES, 0, m_unitTrailVertexCount3D);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_unitTrailVertexCount3D = 0;
}

void Renderer3D::FlushUnitMainSprites3D() {
    if (m_unitMainVertexCount3D == 0) return;
    
    IncrementDrawCall3D("unit_main_sprites");
    
    glDepthMask(GL_FALSE);
    
    glUseProgram(m_shader3DTexturedProgram);
    
    if (m_currentUnitMainTexture3D != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_currentUnitMainTexture3D);
    }
    
    int projLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uModelView");
    int texLoc = glGetUniformLocation(m_shader3DTexturedProgram, "ourTexture");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix3D.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix3D.m);
    glUniform1i(texLoc, 0);
    
    SetFogUniforms3D(m_shader3DTexturedProgram);
    
    glBindVertexArray(m_VAO3DTextured);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO3DTextured);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_unitMainVertexCount3D * sizeof(Vertex3DTextured), m_unitMainVertices3D);
    
    glDrawArrays(GL_TRIANGLES, 0, m_unitMainVertexCount3D);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    glDepthMask(GL_TRUE);
    
    m_unitMainVertexCount3D = 0;
}

void Renderer3D::FlushUnitRotating3D() {
    if (m_unitRotatingVertexCount3D == 0) return;
    
    IncrementDrawCall3D("unit_rotating");
    
    glDepthMask(GL_FALSE);
    
    glUseProgram(m_shader3DTexturedProgram);
    
    if (m_currentUnitRotatingTexture3D != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_currentUnitRotatingTexture3D);
    }
    
    int projLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uModelView");
    int texLoc = glGetUniformLocation(m_shader3DTexturedProgram, "ourTexture");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix3D.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix3D.m);
    glUniform1i(texLoc, 0);
    
    SetFogUniforms3D(m_shader3DTexturedProgram);
    
    glBindVertexArray(m_VAO3DTextured);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO3DTextured);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_unitRotatingVertexCount3D * sizeof(Vertex3DTextured), m_unitRotatingVertices3D);
    
    glDrawArrays(GL_TRIANGLES, 0, m_unitRotatingVertexCount3D);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    glDepthMask(GL_TRUE);
    
    m_unitRotatingVertexCount3D = 0;
}

void Renderer3D::FlushUnitStateIcons3D() {
    if (m_unitStateVertexCount3D == 0) return;
    
    IncrementDrawCall3D("unit_state_icons");
    
    glDepthMask(GL_FALSE);
    
    glUseProgram(m_shader3DTexturedProgram);
    
    int projLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uModelView");
    int texLoc = glGetUniformLocation(m_shader3DTexturedProgram, "ourTexture");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix3D.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix3D.m);
    glUniform1i(texLoc, 0);
    
    SetFogUniforms3D(m_shader3DTexturedProgram);
    
    glBindVertexArray(m_VAO3DTextured);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO3DTextured);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_unitStateVertexCount3D * sizeof(Vertex3DTextured), m_unitStateVertices3D);
    
    glDrawArrays(GL_TRIANGLES, 0, m_unitStateVertexCount3D);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    glDepthMask(GL_TRUE);
    
    m_unitStateVertexCount3D = 0;
}

void Renderer3D::FlushUnitCounters3D() {
    if (m_unitCounterVertexCount3D == 0) return;
    
    IncrementDrawCall3D("unit_counters");
    
    glUseProgram(m_shader3DTexturedProgram);
    
    int projLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uModelView");
    int texLoc = glGetUniformLocation(m_shader3DTexturedProgram, "ourTexture");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix3D.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix3D.m);
    glUniform1i(texLoc, 0);
    
    SetFogUniforms3D(m_shader3DTexturedProgram);
    
    glBindVertexArray(m_VAO3DTextured);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO3DTextured);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_unitCounterVertexCount3D * sizeof(Vertex3DTextured), m_unitCounterVertices3D);
    
    glDrawArrays(GL_TRIANGLES, 0, m_unitCounterVertexCount3D);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_unitCounterVertexCount3D = 0;
}

void Renderer3D::FlushUnitNukeIcons3D() {
    if (m_unitNukeVertexCount3D == 0) return;
    
    IncrementDrawCall3D("unit_nuke_icons");
    
    glDepthMask(GL_FALSE);
    
    glUseProgram(m_shader3DTexturedProgram);
    
    int projLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uModelView");
    int texLoc = glGetUniformLocation(m_shader3DTexturedProgram, "ourTexture");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix3D.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix3D.m);
    glUniform1i(texLoc, 0);
    
    SetFogUniforms3D(m_shader3DTexturedProgram);
    
    glBindVertexArray(m_VAO3DTextured);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO3DTextured);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_unitNukeVertexCount3D * sizeof(Vertex3DTextured), m_unitNukeVertices3D);
    
    glDrawArrays(GL_TRIANGLES, 0, m_unitNukeVertexCount3D);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    glDepthMask(GL_TRUE);
    
    m_unitNukeVertexCount3D = 0;
}

void Renderer3D::FlushEffectsLines3D() {
    if (m_effectsLineVertexCount3D == 0) return;
    
    IncrementDrawCall3D("effects_lines");
    
    glUseProgram(m_shader3DProgram);
    
    int projLoc = glGetUniformLocation(m_shader3DProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_shader3DProgram, "uModelView");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix3D.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix3D.m);
    
    SetFogUniforms3D(m_shader3DProgram);
    
    glBindVertexArray(m_VAO3D);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO3D);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_effectsLineVertexCount3D * sizeof(Vertex3D), m_effectsLineVertices3D);
    
    glDrawArrays(GL_LINES, 0, m_effectsLineVertexCount3D);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_effectsLineVertexCount3D = 0;
}

void Renderer3D::FlushEffectsSprites3D() {
    if (m_effectsSpriteVertexCount3D == 0) return;
    
    IncrementDrawCall3D("effects_sprites");
    
    glDepthMask(GL_FALSE);
    
    glUseProgram(m_shader3DTexturedProgram);
    
    if (m_currentEffectsSpriteTexture3D != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_currentEffectsSpriteTexture3D);
    }
    
    int projLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uModelView");
    int texLoc = glGetUniformLocation(m_shader3DTexturedProgram, "ourTexture");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix3D.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix3D.m);
    glUniform1i(texLoc, 0);
    
    SetFogUniforms3D(m_shader3DTexturedProgram);
    
    glBindVertexArray(m_VAO3DTextured);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO3DTextured);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_effectsSpriteVertexCount3D * sizeof(Vertex3DTextured), m_effectsSpriteVertices3D);
    
    glDrawArrays(GL_TRIANGLES, 0, m_effectsSpriteVertexCount3D);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    glDepthMask(GL_TRUE);
    
    m_effectsSpriteVertexCount3D = 0;
}

void Renderer3D::FlushUnitHighlights3D() {
    if (m_unitHighlightVertexCount3D == 0) return;
    
    IncrementDrawCall3D("unit_highlights");
    
    glUseProgram(m_shader3DTexturedProgram);
    
    int projLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uModelView");
    int texLoc = glGetUniformLocation(m_shader3DTexturedProgram, "ourTexture");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix3D.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix3D.m);
    glUniform1i(texLoc, 0);
    
    SetFogUniforms3D(m_shader3DTexturedProgram);
    
    glBindVertexArray(m_VAO3DTextured);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO3DTextured);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_unitHighlightVertexCount3D * sizeof(Vertex3DTextured), m_unitHighlightVertices3D);
    
    glDrawArrays(GL_TRIANGLES, 0, m_unitHighlightVertexCount3D);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_unitHighlightVertexCount3D = 0;
}

void Renderer3D::FlushHealthBars3D() {
    if (m_healthBarVertexCount3D == 0) return;
    
    IncrementDrawCall3D("health_bars");
    
    glUseProgram(m_shader3DProgram);
    
    int projLoc = glGetUniformLocation(m_shader3DProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_shader3DProgram, "uModelView");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix3D.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix3D.m);
    
    // Set fog uniforms
    SetFogUniforms3D(m_shader3DProgram);
    
    glBindVertexArray(m_VAO3D);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO3D);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_healthBarVertexCount3D * sizeof(Vertex3D), m_healthBarVertices3D);
    
    glDrawArrays(GL_TRIANGLES, 0, m_healthBarVertexCount3D);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_healthBarVertexCount3D = 0;
}

void Renderer3D::FlushTextBuffer3D() {
    if (m_textVertexCount3D == 0) return;
    
    IncrementDrawCall3D("text");
    
    glUseProgram(m_shader3DTexturedProgram);
    
    int projLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uModelView");
    int texLoc = glGetUniformLocation(m_shader3DTexturedProgram, "ourTexture");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix3D.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix3D.m);
    glUniform1i(texLoc, 0);
    
    // set fog 
    SetFogUniforms3D(m_shader3DTexturedProgram);
    
    // bind the current text texture
    if (m_currentTextTexture3D != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_currentTextTexture3D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    }
    
    glBindVertexArray(m_VAO3DTextured);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO3DTextured);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_textVertexCount3D * sizeof(Vertex3DTextured), m_textVertices3D);
    
    glDrawArrays(GL_TRIANGLES, 0, m_textVertexCount3D);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_textVertexCount3D = 0;
    m_currentTextTexture3D = 0;  // reset texture tracking
}

void Renderer3D::FlushNuke3DModels3D() {
    if (m_nuke3DModelVertexCount3D == 0) return;
    
    IncrementDrawCall3D("nuke_3d_models");
    
    glUseProgram(m_shader3DProgram);
    
    int projLoc = glGetUniformLocation(m_shader3DProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_shader3DProgram, "uModelView");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix3D.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix3D.m);
    
    SetFogUniforms3D(m_shader3DProgram);
    
    glBindVertexArray(m_VAO3D);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO3D);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_nuke3DModelVertexCount3D * sizeof(Vertex3D), m_nuke3DModelVertices3D);
    
    // enable face culling for proper 3D model rendering
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    
    // draw as triangles
    glDrawArrays(GL_TRIANGLES, 0, m_nuke3DModelVertexCount3D);
    
    glDisable(GL_CULL_FACE);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_nuke3DModelVertexCount3D = 0;
}

void Renderer3D::FlushStarField3D() {
    if (m_starFieldVertexCount3D == 0) return;
    
    IncrementDrawCall3D("star_field");
    
    glDepthMask(GL_FALSE);  // dont write to depth buffer for stars
    
    glUseProgram(m_shader3DTexturedProgram);
    
    if (m_currentStarFieldTexture3D != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_currentStarFieldTexture3D);
    }
    
    int projLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uModelView");
    int texLoc = glGetUniformLocation(m_shader3DTexturedProgram, "ourTexture");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix3D.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix3D.m);
    glUniform1i(texLoc, 0);
    
    SetFogUniforms3D(m_shader3DTexturedProgram);
    
    glBindVertexArray(m_VAO3DTextured);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO3DTextured);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_starFieldVertexCount3D * sizeof(Vertex3DTextured), m_starFieldVertices3D);
    
    glDrawArrays(GL_TRIANGLES, 0, m_starFieldVertexCount3D);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    glDepthMask(GL_TRUE);
    
    m_starFieldVertexCount3D = 0;
}

void Renderer3D::FlushGlobeSurface3D() {
    if (m_globeSurfaceVertexCount3D == 0) return;
    
    IncrementDrawCall3D("globe_surface");
    
    glUseProgram(m_shader3DProgram);
    
    int projLoc = glGetUniformLocation(m_shader3DProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_shader3DProgram, "uModelView");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix3D.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix3D.m);
    
    SetFogUniforms3D(m_shader3DProgram);
    
    glBindVertexArray(m_VAO3D);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO3D);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_globeSurfaceVertexCount3D * sizeof(Vertex3D), m_globeSurfaceVertices3D);
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glDrawArrays(GL_TRIANGLES, 0, m_globeSurfaceVertexCount3D);
    
    glDisable(GL_BLEND);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_globeSurfaceVertexCount3D = 0;
}

void Renderer3D::Flush3DVertices(unsigned int primitiveType) {
    if (m_vertex3DCount == 0) return;
    
    // Track legacy draw call for debug menu
    IncrementDrawCall3D("legacy_vertices");
    
    // Set line width for whiteboard rendering in 3D globe mode
#ifndef TARGET_EMSCRIPTEN
    if (primitiveType == GL_LINES) {
        glLineWidth(g_preferences->GetFloat(PREFS_GLOBE_WHITEBOARD_THICKNESS, 1.0f));
    }
#endif
    
    // Use 3D shader program
    glUseProgram(m_shader3DProgram);
    
    // Set matrix uniforms
    int projLoc = glGetUniformLocation(m_shader3DProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_shader3DProgram, "uModelView");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix3D.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix3D.m);
    
    // Set fog uniforms (both distance-based and orientation-based)
    int fogEnabledLoc = glGetUniformLocation(m_shader3DProgram, "uFogEnabled");
    int fogOrientationLoc = glGetUniformLocation(m_shader3DProgram, "uFogOrientationBased");
    int fogStartLoc = glGetUniformLocation(m_shader3DProgram, "uFogStart");
    int fogEndLoc = glGetUniformLocation(m_shader3DProgram, "uFogEnd");
    int fogColorLoc = glGetUniformLocation(m_shader3DProgram, "uFogColor");
    int cameraPosLoc = glGetUniformLocation(m_shader3DProgram, "uCameraPos");
    
    glUniform1i(fogEnabledLoc, m_fogEnabled ? 1 : 0);
    glUniform1i(fogOrientationLoc, m_fogOrientationBased ? 1 : 0);
    glUniform1f(fogStartLoc, m_fogStart);
    glUniform1f(fogEndLoc, m_fogEnd);
    glUniform4f(fogColorLoc, m_fogColor[0], m_fogColor[1], m_fogColor[2], m_fogColor[3]);
    glUniform3f(cameraPosLoc, m_cameraPos[0], m_cameraPos[1], m_cameraPos[2]);
    
    // Upload vertex data
    glBindVertexArray(m_VAO3D);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO3D);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_vertex3DCount * sizeof(Vertex3D), m_vertices3D);
    
    // Draw
    glDrawArrays(primitiveType, 0, m_vertex3DCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    // Reset vertex count
    m_vertex3DCount = 0;
}

void Renderer3D::Flush3DTexturedVertices() {
    if (m_vertex3DTexturedCount == 0) return;
    
    // Track legacy draw call for debug menu
    IncrementDrawCall3D("legacy_triangles");
    
    // Use textured 3D shader program
    glUseProgram(m_shader3DTexturedProgram);
    
    // Set matrix uniforms
    int projLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uModelView");
    int texLoc = glGetUniformLocation(m_shader3DTexturedProgram, "ourTexture");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix3D.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix3D.m);
    glUniform1i(texLoc, 0);
    
    // Set fog uniforms (both distance-based and orientation-based)
    int fogEnabledLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uFogEnabled");
    int fogOrientationLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uFogOrientationBased");
    int fogStartLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uFogStart");
    int fogEndLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uFogEnd");
    int fogColorLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uFogColor");
    int cameraPosLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uCameraPos");
    
    glUniform1i(fogEnabledLoc, m_fogEnabled ? 1 : 0);
    glUniform1i(fogOrientationLoc, m_fogOrientationBased ? 1 : 0);
    glUniform1f(fogStartLoc, m_fogStart);
    glUniform1f(fogEndLoc, m_fogEnd);
    glUniform4f(fogColorLoc, m_fogColor[0], m_fogColor[1], m_fogColor[2], m_fogColor[3]);
    glUniform3f(cameraPosLoc, m_cameraPos[0], m_cameraPos[1], m_cameraPos[2]);
    
    // Bind texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_currentTexture3D);
    
    // Set proper texture parameters for clean rendering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // Upload vertex data
    glBindVertexArray(m_VAO3DTextured);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO3DTextured);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_vertex3DTexturedCount * sizeof(Vertex3DTextured), m_vertices3DTextured);
    
    // Draw as triangles (convert quad to two triangles with proper winding)
    if (m_vertex3DTexturedCount == 4) {
        // Ensure counter-clockwise winding order for proper face culling
        // Expected vertex order: 0=bottom-left, 1=bottom-right, 2=top-right, 3=top-left
        Vertex3DTextured triangleVertices[6];
        
        // First triangle: bottom-left -> bottom-right -> top-right
        triangleVertices[0] = m_vertices3DTextured[0]; // bottom-left
        triangleVertices[1] = m_vertices3DTextured[1]; // bottom-right  
        triangleVertices[2] = m_vertices3DTextured[2]; // top-right
        
        // Second triangle: bottom-left -> top-right -> top-left
        triangleVertices[3] = m_vertices3DTextured[0]; // bottom-left
        triangleVertices[4] = m_vertices3DTextured[2]; // top-right
        triangleVertices[5] = m_vertices3DTextured[3]; // top-left
        
        // Upload triangle data
        glBufferSubData(GL_ARRAY_BUFFER, 0, 6 * sizeof(Vertex3DTextured), triangleVertices);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    } else {
        // Draw as triangle fan for other polygon types
        glDrawArrays(GL_TRIANGLE_FAN, 0, m_vertex3DTexturedCount);
    }
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    // Reset vertex count
    m_vertex3DTexturedCount = 0;
}

void Renderer3D::FlushTextContext3D() {
    FlushTextBuffer3D();
}
