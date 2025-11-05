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
    FlushLine3D();
    FlushStaticSprites3D();
    FlushRotatingSprite3D();
    FlushTextBuffer3D();
    FlushNuke3DModels3D();
}

void Renderer3D::FlushAllUnitBuffers3D() {
    FlushLine3D();
    FlushStaticSprites3D();
    FlushRotatingSprite3D();
}

void Renderer3D::FlushAllNuke3DModelBuffers3D() {
    FlushNuke3DModels3D();
}

// ============================================================================
// BEGIN SCENES
// ============================================================================

void Renderer3D::BeginLineBatch3D() {
    // trail segments will be batched until EndLineBatch3D()
}

void Renderer3D::BeginStaticSpriteBatch3D() {
    m_staticSpriteVertexCount3D = 0;
    m_currentStaticSpriteTexture3D = 0;
}

void Renderer3D::BeginRotatingSpriteBatch3D() {
    m_rotatingSpriteVertexCount3D = 0;
    m_currentRotatingSpriteTexture3D = 0;
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

void Renderer3D::EndLineBatch3D() {
    FlushLine3D();
}

void Renderer3D::EndStaticSpriteBatch3D() {
    if (m_staticSpriteVertexCount3D > 0) {
        FlushStaticSprites3D();
    }
}

void Renderer3D::EndRotatingSpriteBatch3D() {
    if (m_rotatingSpriteVertexCount3D > 0) {
        FlushRotatingSprite3D();
    }
}

void Renderer3D::EndMapTextBatch3D() {
    EndTextBatch3D();
}

void Renderer3D::EndFrameTextBatch3D() {
    // flush any remaining text at the end of the frame
    EndTextBatch3D();
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

void Renderer3D::FlushLine3DIfFull(int segmentsNeeded) {
    if (m_lineVertexCount3D + segmentsNeeded > MAX_LINE_VERTICES_3D) {
        FlushLine3D();
    }
}

void Renderer3D::FlushStaticSprites3DIfFull(int verticesNeeded) {
    if (m_staticSpriteVertexCount3D + verticesNeeded > MAX_STATIC_SPRITE_VERTICES_3D) {
        FlushStaticSprites3D();
    }
}

void Renderer3D::FlushRotatingSprite3DIfFull(int verticesNeeded) {
    if (m_rotatingSpriteVertexCount3D + verticesNeeded > MAX_ROTATING_SPRITE_VERTICES_3D) {
        FlushRotatingSprite3D();
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

void Renderer3D::FlushLine3D() {
    if (m_lineVertexCount3D == 0) return;

    StartFlushTiming3D("Batched_Lines_3D");
    IncrementDrawCall3D("batched_lines");
    
#ifndef TARGET_EMSCRIPTEN
    glLineWidth(g_preferences->GetFloat(PREFS_GLOBE_UNIT_TRAIL_THICKNESS, 1.0f));
#endif
    
    // use 3D shader program
    m_renderer->SetShaderProgram(m_shader3DProgram);
    Set3DShaderUniforms();
    
    m_renderer->SetVertexArray(m_lineVAO3D);
    UploadVertexDataTo3DVBO(m_lineVBO3D, m_lineVertices3D, m_lineVertexCount3D, GL_STREAM_DRAW);
    
    glDrawArrays(GL_LINES, 0, m_lineVertexCount3D);
    
    m_lineVertexCount3D = 0;
    
    EndFlushTiming3D("Batched_Lines_3D");
}

void Renderer3D::FlushStaticSprites3D() {
    if (m_staticSpriteVertexCount3D == 0) return;
    
    StartFlushTiming3D("Static_Sprite_3D");
    IncrementDrawCall3D("Static_Sprite_sprites");
    
    glDepthMask(GL_FALSE);
    
    m_renderer->SetShaderProgram(m_shader3DTexturedProgram);
    SetTextured3DShaderUniforms();
    
    if (m_currentStaticSpriteTexture3D != 0) {
        m_renderer->SetActiveTexture(GL_TEXTURE0);
        m_renderer->SetBoundTexture(m_currentStaticSpriteTexture3D);
    }
    
    m_renderer->SetVertexArray(m_spriteVAO3D);
    UploadVertexDataTo3DVBO(m_spriteVBO3D, m_staticSpriteVertices3D, m_staticSpriteVertexCount3D, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_staticSpriteVertexCount3D);
    glDepthMask(GL_TRUE);
    
    m_staticSpriteVertexCount3D = 0;
    
    EndFlushTiming3D("Static_Sprite_3D");
}

void Renderer3D::FlushRotatingSprite3D() {
    if (m_rotatingSpriteVertexCount3D == 0) return;
    
    StartFlushTiming3D("Unit_Rotating_3D");
    IncrementDrawCall3D("unit_rotating");
    
    glDepthMask(GL_FALSE);
    
    m_renderer->SetShaderProgram(m_shader3DTexturedProgram);
    SetTextured3DShaderUniforms();
    
    if (m_currentRotatingSpriteTexture3D != 0) {
        m_renderer->SetActiveTexture(GL_TEXTURE0);
        m_renderer->SetBoundTexture(m_currentRotatingSpriteTexture3D);
    }
    
    m_renderer->SetVertexArray(m_spriteVAO3D);
    UploadVertexDataTo3DVBO(m_spriteVBO3D, m_rotatingSpriteVertices3D, m_rotatingSpriteVertexCount3D, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_rotatingSpriteVertexCount3D);
    glDepthMask(GL_TRUE);
    
    m_rotatingSpriteVertexCount3D = 0;
    
    EndFlushTiming3D("Unit_Rotating_3D");
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
