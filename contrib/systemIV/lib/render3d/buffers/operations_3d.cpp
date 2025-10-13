#include "lib/universal_include.h"

#include <stdarg.h>

#include "lib/preferences.h"

#include "renderer/map_renderer.h"
#include "lib/render3d/renderer_3d.h"
#include "lib/render2d/renderer.h"

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

    StartFlushTiming3D("Unit_Trails_3D");
    IncrementDrawCall3D("unit_trails");
    
#ifndef TARGET_EMSCRIPTEN
    glLineWidth(g_preferences->GetFloat(PREFS_GLOBE_UNIT_TRAIL_THICKNESS, 1.0f));
#endif
    
    // use 3D shader program
    m_renderer->SetShaderProgram(m_shader3DProgram);
    Set3DShaderUniforms();
    
    m_renderer->SetVertexArray(m_effectsVAO3D);
    UploadVertexDataTo3DVBO(m_effectsVBO3D, m_unitTrailVertices3D, m_unitTrailVertexCount3D, GL_STREAM_DRAW);
    
    glDrawArrays(GL_LINES, 0, m_unitTrailVertexCount3D);
    
    m_unitTrailVertexCount3D = 0;
    
    EndFlushTiming3D("Unit_Trails_3D");
}

void Renderer3D::FlushUnitMainSprites3D() {
    if (m_unitMainVertexCount3D == 0) return;
    
    StartFlushTiming3D("Unit_Main_3D");
    IncrementDrawCall3D("unit_main_sprites");
    
    glDepthMask(GL_FALSE);
    
    m_renderer->SetShaderProgram(m_shader3DTexturedProgram);
    SetTextured3DShaderUniforms();
    
    if (m_currentUnitMainTexture3D != 0) {
        m_renderer->SetActiveTexture(GL_TEXTURE0);
        m_renderer->SetBoundTexture(m_currentUnitMainTexture3D);
    }
    
    m_renderer->SetVertexArray(m_unitVAO3D);
    UploadVertexDataTo3DVBO(m_unitVBO3D, m_unitMainVertices3D, m_unitMainVertexCount3D, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_unitMainVertexCount3D);
    glDepthMask(GL_TRUE);
    
    m_unitMainVertexCount3D = 0;
    
    EndFlushTiming3D("Unit_Main_3D");
}

void Renderer3D::FlushUnitRotating3D() {
    if (m_unitRotatingVertexCount3D == 0) return;
    
    StartFlushTiming3D("Unit_Rotating_3D");
    IncrementDrawCall3D("unit_rotating");
    
    glDepthMask(GL_FALSE);
    
    m_renderer->SetShaderProgram(m_shader3DTexturedProgram);
    SetTextured3DShaderUniforms();
    
    if (m_currentUnitRotatingTexture3D != 0) {
        m_renderer->SetActiveTexture(GL_TEXTURE0);
        m_renderer->SetBoundTexture(m_currentUnitRotatingTexture3D);
    }
    
    m_renderer->SetVertexArray(m_unitVAO3D);
    UploadVertexDataTo3DVBO(m_unitVBO3D, m_unitRotatingVertices3D, m_unitRotatingVertexCount3D, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_unitRotatingVertexCount3D);
    glDepthMask(GL_TRUE);
    
    m_unitRotatingVertexCount3D = 0;
    
    EndFlushTiming3D("Unit_Rotating_3D");
}

void Renderer3D::FlushUnitStateIcons3D() {
    if (m_unitStateVertexCount3D == 0) return;
    
    StartFlushTiming3D("Unit_StateIcons_3D");
    IncrementDrawCall3D("unit_state_icons");
    
    glDepthMask(GL_FALSE);
    
    m_renderer->SetShaderProgram(m_shader3DTexturedProgram);
    SetTextured3DShaderUniforms();
    
    if (m_currentUnitStateTexture3D != 0) {
        m_renderer->SetActiveTexture(GL_TEXTURE0);
        m_renderer->SetBoundTexture(m_currentUnitStateTexture3D);
    }
    
    m_renderer->SetVertexArray(m_unitVAO3D);
    UploadVertexDataTo3DVBO(m_unitVBO3D, m_unitStateVertices3D, m_unitStateVertexCount3D, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_unitStateVertexCount3D);
    glDepthMask(GL_TRUE);
    
    m_unitStateVertexCount3D = 0;
    
    EndFlushTiming3D("Unit_StateIcons_3D");
}

void Renderer3D::FlushUnitCounters3D() {
    if (m_unitCounterVertexCount3D == 0) return;
    
    StartFlushTiming3D("Unit_Counters_3D");
    IncrementDrawCall3D("unit_counters");
    
    m_renderer->SetShaderProgram(m_shader3DTexturedProgram);
    SetTextured3DShaderUniforms();
    
    m_renderer->SetVertexArray(m_unitVAO3D);
    UploadVertexDataTo3DVBO(m_unitVBO3D, m_unitCounterVertices3D, m_unitCounterVertexCount3D, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_unitCounterVertexCount3D);
    
    m_unitCounterVertexCount3D = 0;
    
    EndFlushTiming3D("Unit_Counters_3D");
}

void Renderer3D::FlushUnitNukeIcons3D() {
    if (m_unitNukeVertexCount3D == 0) return;
    
    StartFlushTiming3D("Unit_Nukes_3D");
    IncrementDrawCall3D("unit_nuke_icons");
    
    glDepthMask(GL_FALSE);
    
    m_renderer->SetShaderProgram(m_shader3DTexturedProgram);
    SetTextured3DShaderUniforms();
    
    if (m_currentUnitNukeTexture3D != 0) {
        m_renderer->SetActiveTexture(GL_TEXTURE0);
        m_renderer->SetBoundTexture(m_currentUnitNukeTexture3D);
    }
    
    m_renderer->SetVertexArray(m_unitVAO3D);
    UploadVertexDataTo3DVBO(m_unitVBO3D, m_unitNukeVertices3D, m_unitNukeVertexCount3D, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_unitNukeVertexCount3D);
    glDepthMask(GL_TRUE);
    
    m_unitNukeVertexCount3D = 0;
    
    EndFlushTiming3D("Unit_Nukes_3D");
}

void Renderer3D::FlushEffectsLines3D() {
    if (m_effectsLineVertexCount3D == 0) return;
    
    StartFlushTiming3D("Effects_Lines_3D");
    IncrementDrawCall3D("effects_lines");
    
    m_renderer->SetShaderProgram(m_shader3DProgram);
    Set3DShaderUniforms();
    
    m_renderer->SetVertexArray(m_effectsVAO3D);
    UploadVertexDataTo3DVBO(m_effectsVBO3D, m_effectsLineVertices3D, m_effectsLineVertexCount3D, GL_STREAM_DRAW);
    
    glDrawArrays(GL_LINES, 0, m_effectsLineVertexCount3D);
    
    m_effectsLineVertexCount3D = 0;
    
    EndFlushTiming3D("Effects_Lines_3D");
}

void Renderer3D::FlushEffectsSprites3D() {
    if (m_effectsSpriteVertexCount3D == 0) return;
    
    StartFlushTiming3D("Effects_Sprites_3D");
    IncrementDrawCall3D("effects_sprites");
    
    glDepthMask(GL_FALSE);
    
    m_renderer->SetShaderProgram(m_shader3DTexturedProgram);
    
    SetTextured3DShaderUniforms();
    
    if (m_currentEffectsSpriteTexture3D != 0) {
        m_renderer->SetActiveTexture(GL_TEXTURE0);
        m_renderer->SetBoundTexture(m_currentEffectsSpriteTexture3D);
    }
    
    m_renderer->SetVertexArray(m_starsVAO3D);
    UploadVertexDataTo3DVBO(m_starsVBO3D, m_effectsSpriteVertices3D, m_effectsSpriteVertexCount3D, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_effectsSpriteVertexCount3D);
    glDepthMask(GL_TRUE);
    
    m_effectsSpriteVertexCount3D = 0;
    
    EndFlushTiming3D("Effects_Sprites_3D");
}

void Renderer3D::FlushUnitHighlights3D() {
    if (m_unitHighlightVertexCount3D == 0) return;
    
    StartFlushTiming3D("Unit_Highlights_3D");
    IncrementDrawCall3D("unit_highlights");
    
    m_renderer->SetShaderProgram(m_shader3DTexturedProgram);
    SetTextured3DShaderUniforms();
    
    if (m_currentUnitHighlightTexture3D != 0) {
        m_renderer->SetActiveTexture(GL_TEXTURE0);
        m_renderer->SetBoundTexture(m_currentUnitHighlightTexture3D);
    }
    
    m_renderer->SetVertexArray(m_unitVAO3D);
    UploadVertexDataTo3DVBO(m_unitVBO3D, m_unitHighlightVertices3D, m_unitHighlightVertexCount3D, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_unitHighlightVertexCount3D);
    
    m_unitHighlightVertexCount3D = 0;
    
    EndFlushTiming3D("Unit_Highlights_3D");
}

void Renderer3D::FlushHealthBars3D() {
    if (m_healthBarVertexCount3D == 0) return;
    
    StartFlushTiming3D("Health_Bars_3D");
    IncrementDrawCall3D("health_bars");
    
    m_renderer->SetShaderProgram(m_shader3DProgram);
    Set3DShaderUniforms();
    
    m_renderer->SetVertexArray(m_healthVAO3D);
    UploadVertexDataTo3DVBO(m_healthVBO3D, m_healthBarVertices3D, m_healthBarVertexCount3D, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_healthBarVertexCount3D);
    
    m_healthBarVertexCount3D = 0;
    
    EndFlushTiming3D("Health_Bars_3D");
}

void Renderer3D::FlushTextBuffer3D() {
    if (m_textVertexCount3D == 0) return;
    
    StartFlushTiming3D("Text_3D");
    IncrementDrawCall3D("text");
    
    m_renderer->SetShaderProgram(m_shader3DTexturedProgram);
    SetTextured3DShaderUniforms();
    
    if (m_currentTextTexture3D != 0) {
        m_renderer->SetActiveTexture(GL_TEXTURE0);
        m_renderer->SetBoundTexture(m_currentTextTexture3D);
    }
    
    m_renderer->SetVertexArray(m_textVAO3D);
    UploadVertexDataTo3DVBO(m_textVBO3D, m_textVertices3D, m_textVertexCount3D, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_textVertexCount3D);
    
    m_textVertexCount3D = 0;
    m_currentTextTexture3D = 0;  // reset texture tracking
    
    EndFlushTiming3D("Text_3D");
}

void Renderer3D::FlushNuke3DModels3D() {
    if (m_nuke3DModelVertexCount3D == 0) return;
    
    StartFlushTiming3D("Nuke_3D_Models");
    IncrementDrawCall3D("nuke_3d_models");
    
    m_renderer->SetShaderProgram(m_shader3DProgram);
    Set3DShaderUniforms();
    
    m_renderer->SetVertexArray(m_nukeVAO3D);
    UploadVertexDataTo3DVBO(m_nukeVBO3D, m_nuke3DModelVertices3D, m_nuke3DModelVertexCount3D, GL_DYNAMIC_DRAW);
    
    // enable face culling for proper 3D model rendering
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    
    // draw as triangles
    glDrawArrays(GL_TRIANGLES, 0, m_nuke3DModelVertexCount3D);
    
    glDisable(GL_CULL_FACE);
    
    m_nuke3DModelVertexCount3D = 0;
    
    EndFlushTiming3D("Nuke_3D_Models");
}

void Renderer3D::FlushStarField3D() {
    if (m_starFieldVertexCount3D == 0) return;
    
    StartFlushTiming3D("Star_Field_3D");
    IncrementDrawCall3D("star_field");
    
    glDepthMask(GL_FALSE);  // dont write to depth buffer for stars
    
    m_renderer->SetShaderProgram(m_shader3DTexturedProgram);
    
    SetTextured3DShaderUniforms();
    
    if (m_currentStarFieldTexture3D != 0) {
        m_renderer->SetActiveTexture(GL_TEXTURE0);
        m_renderer->SetBoundTexture(m_currentStarFieldTexture3D);
    }
    
    m_renderer->SetVertexArray(m_starsVAO3D);
    UploadVertexDataTo3DVBO(m_starsVBO3D, m_starFieldVertices3D, m_starFieldVertexCount3D, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_starFieldVertexCount3D);
    glDepthMask(GL_TRUE);
    
    m_starFieldVertexCount3D = 0;
    
    EndFlushTiming3D("Star_Field_3D");
}

void Renderer3D::FlushGlobeSurface3D() {
    if (m_globeSurfaceVertexCount3D == 0) return;
    
    StartFlushTiming3D("Globe_Surface_3D");
    IncrementDrawCall3D("globe_surface");
    
    m_renderer->SetShaderProgram(m_shader3DProgram);
    Set3DShaderUniforms();
    
    m_renderer->SetVertexArray(m_globeVAO3D);
    UploadVertexDataTo3DVBO(m_globeVBO3D, m_globeSurfaceVertices3D, m_globeSurfaceVertexCount3D, GL_DYNAMIC_DRAW);
    
    m_renderer->SetBlendMode(Renderer::BlendModeNormal);
    
    glDrawArrays(GL_TRIANGLES, 0, m_globeSurfaceVertexCount3D);
    
    m_renderer->SetBlendMode(Renderer::BlendModeDisabled);
    
    m_globeSurfaceVertexCount3D = 0;
    
    EndFlushTiming3D("Globe_Surface_3D");
}

void Renderer3D::Flush3DVertices(unsigned int primitiveType) {
    if (m_vertex3DCount == 0) return;
    
    StartFlushTiming3D("Legacy_Vertices_3D");
    // Track legacy draw call for debug menu
    IncrementDrawCall3D("legacy_vertices");
    
    // Set line width for whiteboard rendering in 3D globe mode
#ifndef TARGET_EMSCRIPTEN
    if (primitiveType == GL_LINES) {
        glLineWidth(g_preferences->GetFloat(PREFS_GLOBE_WHITEBOARD_THICKNESS, 1.0f));
    }
#endif
    
    // Use 3D shader program
    m_renderer->SetShaderProgram(m_shader3DProgram);
    Set3DShaderUniforms();
    
    // Upload vertex data
    m_renderer->SetVertexArray(m_legacyVAO3D);
    UploadVertexDataTo3DVBO(m_legacyVBO3D, m_vertices3D, m_vertex3DCount, GL_DYNAMIC_DRAW);
    
    // Draw
    glDrawArrays(primitiveType, 0, m_vertex3DCount);
    
    // Reset vertex count
    m_vertex3DCount = 0;
    
    EndFlushTiming3D("Legacy_Vertices_3D");
}

void Renderer3D::Flush3DTexturedVertices() {
    if (m_vertex3DTexturedCount == 0) return;
    
    StartFlushTiming3D("Legacy_Triangles_3D");
    // Track legacy draw call for debug menu
    IncrementDrawCall3D("legacy_triangles");
    
    // Use textured 3D shader program
    m_renderer->SetShaderProgram(m_shader3DTexturedProgram);
    SetTextured3DShaderUniforms();
    
    // Bind texture
    m_renderer->SetActiveTexture(GL_TEXTURE0);
    m_renderer->SetBoundTexture(m_currentTexture3D);
    
    // Upload vertex data
    m_renderer->SetVertexArray(m_VAO3DTextured);
    UploadVertexDataTo3DVBO(m_VBO3DTextured, m_vertices3DTextured, m_vertex3DTexturedCount, GL_DYNAMIC_DRAW);
    
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
        UploadVertexDataTo3DVBO(m_VBO3DTextured, triangleVertices, 6, GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    } else {
        // Draw as triangle fan for other polygon types
        glDrawArrays(GL_TRIANGLE_FAN, 0, m_vertex3DTexturedCount);
    }
    
    // Reset vertex count
    m_vertex3DTexturedCount = 0;
    
    EndFlushTiming3D("Legacy_Triangles_3D");
}

void Renderer3D::FlushTextContext3D() {
    FlushTextBuffer3D();
}
