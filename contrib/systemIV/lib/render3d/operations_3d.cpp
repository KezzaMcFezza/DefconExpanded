#include "systemiv.h"

#include <stdarg.h>

#include "lib/preferences.h"

#include "lib/render3d/renderer_3d.h"
#include "lib/render/renderer.h"
#include "lib/render2d/renderer_2d.h"

extern Renderer3D *g_renderer3d;

// ============================================================================
// BEGIN SCENES
// ============================================================================

void Renderer3D::BeginLineBatch3D() {
    m_lineVertexCount3D = 0;
    m_currentLineWidth3D = 1.0f;
}

void Renderer3D::BeginStaticSpriteBatch3D() {
    m_staticSpriteVertexCount3D = 0;
    m_currentStaticSpriteTexture3D = 0;
}

void Renderer3D::BeginRotatingSpriteBatch3D() {
    m_rotatingSpriteVertexCount3D = 0;
    m_currentRotatingSpriteTexture3D = 0;
}

void Renderer3D::BeginTextBatch3D() {
    for (int i = 0; i < Renderer::MAX_FONT_ATLASES; i++) {
        m_fontBatches3D[i].vertexCount = 0;
        m_fontBatches3D[i].textureID = 0;
    }
    
    m_currentFontBatchIndex3D = 0;
    m_textVertices3D = m_fontBatches3D[0].vertices;
    m_textVertexCount3D = 0;
    m_currentTextTexture3D = 0;
}

void Renderer3D::BeginCircleBatch3D() {
    m_circleVertexCount3D = 0;
    m_currentCircleWidth3D = 1.0f;
}

void Renderer3D::BeginCircleFillBatch3D() {
    m_circleFillVertexCount3D = 0;
}

void Renderer3D::BeginRectBatch3D() {
    m_rectVertexCount3D = 0;
    m_currentRectWidth3D = 1.0f;
}

void Renderer3D::BeginRectFillBatch3D() {
    m_rectFillVertexCount3D = 0;
}

void Renderer3D::BeginTriangleFillBatch3D() {
    m_triangleFillVertexCount3D = 0;
}

// ============================================================================
// END SCENES
// ============================================================================

void Renderer3D::EndLineBatch3D() {
    if (m_lineVertexCount3D > 0) {
        FlushLine3D();
    }
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

void Renderer3D::EndTextBatch3D() {
    int activeBatches = 0;
    
    for (int i = 0; i < Renderer::MAX_FONT_ATLASES; i++) {
        if (m_fontBatches3D[i].vertexCount > 0) {
            activeBatches++;
            
            m_textVertices3D = m_fontBatches3D[i].vertices;
            m_textVertexCount3D = m_fontBatches3D[i].vertexCount;
            m_currentTextTexture3D = m_fontBatches3D[i].textureID;
            
            FlushTextBuffer3D();
            
            m_fontBatches3D[i].vertexCount = 0;
        }
    }
    
    if (activeBatches > m_activeFontBatches3D) {
        m_activeFontBatches3D = activeBatches;
    }
    
    m_currentFontBatchIndex3D = 0;
    m_textVertices3D = m_fontBatches3D[0].vertices;
    m_textVertexCount3D = 0;
    m_currentTextTexture3D = 0;
}

void Renderer3D::EndCircleBatch3D() {
    if (m_circleVertexCount3D > 0) {
        FlushCircles3D();
    }
}

void Renderer3D::EndCircleFillBatch3D() {
    if (m_circleFillVertexCount3D > 0) {
        FlushCircleFills3D();
    }
}

void Renderer3D::EndRectBatch3D() {
    if (m_rectVertexCount3D > 0) {
        FlushRects3D();
    }
}

void Renderer3D::EndRectFillBatch3D() {
    if (m_rectFillVertexCount3D > 0) {
        FlushRectFills3D();
    }
}

void Renderer3D::EndTriangleFillBatch3D() {
    if (m_triangleFillVertexCount3D > 0) {
        FlushTriangleFills3D();
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

void Renderer3D::FlushCircles3DIfFull(int verticesNeeded) {
    if (m_circleVertexCount3D + verticesNeeded > MAX_CIRCLE_VERTICES_3D) {
        FlushCircles3D();
    }
}

void Renderer3D::FlushCircleFills3DIfFull(int verticesNeeded) {
    if (m_circleFillVertexCount3D + verticesNeeded > MAX_CIRCLE_FILL_VERTICES_3D) {
        FlushCircleFills3D();
    }
}

void Renderer3D::FlushRects3DIfFull(int verticesNeeded) {
    if (m_rectVertexCount3D + verticesNeeded > MAX_RECT_VERTICES_3D) {
        FlushRects3D();
    }
}

void Renderer3D::FlushRectFills3DIfFull(int verticesNeeded) {
    if (m_rectFillVertexCount3D + verticesNeeded > MAX_RECT_FILL_VERTICES_3D) {
        FlushRectFills3D();
    }
}

void Renderer3D::FlushTriangleFills3DIfFull(int verticesNeeded) {
    if (m_triangleFillVertexCount3D + verticesNeeded > MAX_TRIANGLE_FILL_VERTICES_3D) {
        FlushTriangleFills3D();
    }
}

// ============================================================================
// CORE FLUSH FUNCTIONS
// ============================================================================

void Renderer3D::FlushLine3D() {
    if (m_lineVertexCount3D == 0) return;

    g_renderer->StartFlushTiming("Batched_Lines_3D");
    IncrementDrawCall3D("batched_lines");
    
#ifndef TARGET_EMSCRIPTEN
    g_renderer->SetLineWidth(m_currentLineWidth3D);
#endif
    
    // use 3D shader program
    g_renderer->SetShaderProgram(m_shader3DProgram);
    Set3DShaderUniforms();
    
    g_renderer->SetVertexArray(m_lineVAO3D);
    UploadVertexDataTo3DVBO(m_lineVBO3D, m_lineVertices3D, m_lineVertexCount3D, GL_STREAM_DRAW);
    
    glDrawArrays(GL_LINES, 0, m_lineVertexCount3D);
    
    m_lineVertexCount3D = 0;
    
    g_renderer->EndFlushTiming("Batched_Lines_3D");
}

void Renderer3D::FlushStaticSprites3D() {
    if (m_staticSpriteVertexCount3D == 0) return;
    
    g_renderer->StartFlushTiming("Static_Sprite_3D");
    IncrementDrawCall3D("static_sprites");
    
    g_renderer->SetDepthMask(false);
    
    g_renderer->SetShaderProgram(m_shader3DTexturedProgram);
    SetTextured3DShaderUniforms();
    
    if (m_currentStaticSpriteTexture3D != 0) {
        g_renderer->SetActiveTexture(GL_TEXTURE0);
        g_renderer->SetBoundTexture(m_currentStaticSpriteTexture3D);
    }
    
    g_renderer->SetVertexArray(m_spriteVAO3D);
    UploadVertexDataTo3DVBO(m_spriteVBO3D, m_staticSpriteVertices3D, m_staticSpriteVertexCount3D, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_staticSpriteVertexCount3D);
    g_renderer->SetDepthMask(true);
    
    m_staticSpriteVertexCount3D = 0;
    
    g_renderer->EndFlushTiming("Static_Sprite_3D");
}

void Renderer3D::FlushRotatingSprite3D() {
    if (m_rotatingSpriteVertexCount3D == 0) return;
    
    g_renderer->StartFlushTiming("Unit_Rotating_3D");
    IncrementDrawCall3D("rotating_sprites");
    
    glDepthMask(GL_FALSE);
    
    g_renderer->SetShaderProgram(m_shader3DTexturedProgram);
    SetTextured3DShaderUniforms();
    
    if (m_currentRotatingSpriteTexture3D != 0) {
        g_renderer->SetActiveTexture(GL_TEXTURE0);
        g_renderer->SetBoundTexture(m_currentRotatingSpriteTexture3D);
    }
    
    g_renderer->SetVertexArray(m_spriteVAO3D);
    UploadVertexDataTo3DVBO(m_spriteVBO3D, m_rotatingSpriteVertices3D, m_rotatingSpriteVertexCount3D, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_rotatingSpriteVertexCount3D);
    g_renderer->SetDepthMask(true);
    
    m_rotatingSpriteVertexCount3D = 0;
    
    g_renderer->EndFlushTiming("Unit_Rotating_3D");
}

void Renderer3D::FlushTextBuffer3D() {
    if (m_textVertexCount3D == 0) return;
    
    g_renderer->StartFlushTiming("Text_3D");
    IncrementDrawCall3D("text");
    
    //
    // save current blend state

    int currentBlendSrc = g_renderer->m_currentBlendSrcFactor;
    int currentBlendDst = g_renderer->m_currentBlendDstFactor;
    
    //
    // disable depth mask for transparent text rendering

    g_renderer->SetDepthMask(false);
    
    //
    // set correct blend mode for text rendering (additive for smooth fonts)

    if (g_renderer->m_blendMode != Renderer::BlendModeSubtractive) {
        g_renderer->SetBlendMode(Renderer::BlendModeAdditive);
    }
    
    g_renderer->SetShaderProgram(m_shader3DTexturedProgram);
    SetTextured3DShaderUniforms();
    
    if (m_currentTextTexture3D != 0) {
        g_renderer->SetActiveTexture(GL_TEXTURE0);
        g_renderer->SetBoundTexture(m_currentTextTexture3D);
    }
    
    g_renderer->SetVertexArray(m_textVAO3D);
    UploadVertexDataTo3DVBO(m_textVBO3D, m_textVertices3D, m_textVertexCount3D, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_textVertexCount3D);
    
    //
    // restore previous blend state

    g_renderer->SetBlendFunc(currentBlendSrc, currentBlendDst);
    
    m_textVertexCount3D = 0;
    m_currentTextTexture3D = 0;
    
    g_renderer->EndFlushTiming("Text_3D");
}

void Renderer3D::FlushCircles3D() {
    if (m_circleVertexCount3D == 0) return;
    
    g_renderer->StartFlushTiming("Circles_3D");
    IncrementDrawCall3D("circles");
    
#ifndef TARGET_EMSCRIPTEN
    g_renderer->SetLineWidth(m_currentCircleWidth3D);
#endif
    
    g_renderer->SetShaderProgram(m_shader3DProgram);
    Set3DShaderUniforms();
    
    g_renderer->SetVertexArray(m_circleVAO3D);
    UploadVertexDataTo3DVBO(m_circleVBO3D, m_circleVertices3D, m_circleVertexCount3D, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_LINES, 0, m_circleVertexCount3D);
    
    m_circleVertexCount3D = 0;
    
    g_renderer->EndFlushTiming("Circles_3D");
}

void Renderer3D::FlushCircleFills3D() {
    if (m_circleFillVertexCount3D == 0) return;
    
    g_renderer->StartFlushTiming("Circle_Fills_3D");
    IncrementDrawCall3D("circle_fills");
    
    g_renderer->SetShaderProgram(m_shader3DProgram);
    Set3DShaderUniforms();
    
    g_renderer->SetVertexArray(m_circleFillVAO3D);
    UploadVertexDataTo3DVBO(m_circleFillVBO3D, m_circleFillVertices3D, m_circleFillVertexCount3D, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_circleFillVertexCount3D);
    
    m_circleFillVertexCount3D = 0;
    
    g_renderer->EndFlushTiming("Circle_Fills_3D");
}

void Renderer3D::FlushRects3D() {
    if (m_rectVertexCount3D == 0) return;
    
    g_renderer->StartFlushTiming("Rects_3D");
    IncrementDrawCall3D("rects");
    
#ifndef TARGET_EMSCRIPTEN
    g_renderer->SetLineWidth(m_currentRectWidth3D);
#endif
    
    g_renderer->SetShaderProgram(m_shader3DProgram);
    Set3DShaderUniforms();
    
    g_renderer->SetVertexArray(m_rectVAO3D);
    UploadVertexDataTo3DVBO(m_rectVBO3D, m_rectVertices3D, m_rectVertexCount3D, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_LINES, 0, m_rectVertexCount3D);
    
    m_rectVertexCount3D = 0;
    
    g_renderer->EndFlushTiming("Rects_3D");
}

void Renderer3D::FlushRectFills3D() {
    if (m_rectFillVertexCount3D == 0) return;
    
    g_renderer->StartFlushTiming("Rect_Fills_3D");
    IncrementDrawCall3D("rect_fills");
    
    g_renderer->SetShaderProgram(m_shader3DProgram);
    Set3DShaderUniforms();
    
    g_renderer->SetVertexArray(m_rectFillVAO3D);
    UploadVertexDataTo3DVBO(m_rectFillVBO3D, m_rectFillVertices3D, m_rectFillVertexCount3D, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_rectFillVertexCount3D);
    
    m_rectFillVertexCount3D = 0;
    
    g_renderer->EndFlushTiming("Rect_Fills_3D");
}

void Renderer3D::FlushTriangleFills3D() {
    if (m_triangleFillVertexCount3D == 0) return;
    
    g_renderer->StartFlushTiming("Triangle_Fills_3D");
    IncrementDrawCall3D("triangle_fills");
    
    g_renderer->SetDepthMask(false);
    
    g_renderer->SetShaderProgram(m_shader3DProgram);
    Set3DShaderUniforms();
    
    g_renderer->SetVertexArray(m_triangleFillVAO3D);
    UploadVertexDataTo3DVBO(m_triangleFillVBO3D, m_triangleFillVertices3D, m_triangleFillVertexCount3D, GL_DYNAMIC_DRAW);
    
    g_renderer->SetCullFace(true, GL_BACK);
    glCullFace(GL_BACK);
    
    glDrawArrays(GL_TRIANGLES, 0, m_triangleFillVertexCount3D);
    
    g_renderer->SetCullFace(false, GL_BACK);
    g_renderer->SetDepthMask(true);
    
    m_triangleFillVertexCount3D = 0;
    
    g_renderer->EndFlushTiming("Triangle_Fills_3D");
}

void Renderer3D::Flush3DVertices(unsigned int primitiveType) {
    if (m_vertex3DCount == 0) return;
    
    g_renderer->StartFlushTiming("Immediate_Vertices_3D");
    IncrementDrawCall3D("immediate_vertices");
    
    g_renderer->SetShaderProgram(m_shader3DProgram);
    Set3DShaderUniforms();
    
    g_renderer->SetVertexArray(m_immediateVAO3D);
    UploadVertexDataTo3DVBO(m_immediateVBO3D, m_vertices3D, m_vertex3DCount, GL_DYNAMIC_DRAW);
    
    glDrawArrays(primitiveType, 0, m_vertex3DCount);
    
    m_vertex3DCount = 0;
    
    g_renderer->EndFlushTiming("Immediate_Vertices_3D");
}

void Renderer3D::Flush3DTexturedVertices() {
    if (m_vertex3DTexturedCount == 0) return;
    
    g_renderer->StartFlushTiming("Immediate_Triangles_3D");
    IncrementDrawCall3D("immediate_triangles");
    
    g_renderer->SetShaderProgram(m_shader3DTexturedProgram);
    SetTextured3DShaderUniforms();
    
    g_renderer->SetActiveTexture(GL_TEXTURE0);
    g_renderer->SetBoundTexture(m_currentTexture3D);
    
    g_renderer->SetVertexArray(m_VAO3DTextured);
    UploadVertexDataTo3DVBO(m_VBO3DTextured, m_vertices3DTextured, m_vertex3DTexturedCount, GL_DYNAMIC_DRAW);
    
    if (m_vertex3DTexturedCount == 4) {
        Vertex3DTextured triangleVertices[6];
        
        // First triangle
        triangleVertices[0] = m_vertices3DTextured[0]; // bottom-left
        triangleVertices[1] = m_vertices3DTextured[1]; // bottom-right  
        triangleVertices[2] = m_vertices3DTextured[2]; // top-right
        
        // Second triangle
        triangleVertices[3] = m_vertices3DTextured[0]; // bottom-left
        triangleVertices[4] = m_vertices3DTextured[2]; // top-right
        triangleVertices[5] = m_vertices3DTextured[3]; // top-left
        

        UploadVertexDataTo3DVBO(m_VBO3DTextured, triangleVertices, 6, GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    } else {
        glDrawArrays(GL_TRIANGLE_FAN, 0, m_vertex3DTexturedCount);
    }
    
    m_vertex3DTexturedCount = 0;
    
    g_renderer->EndFlushTiming("Immediate_Triangles_3D");
}
