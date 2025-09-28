#include "lib/universal_include.h"

#include "lib/debug_utils.h"
#include "lib/string_utils.h"

#include "renderer_3d.h"

// ============================================================================
// VBO CACHING SYSTEM FOR 3D
// High-performance rendering for static 3D geometry like globe coastlines
// ============================================================================

void Renderer3D::BeginCachedGeometry3D(const char* cacheKey, Colour const &col) {
    // Check if this VBO already exists and is valid
    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(cacheKey);
    if (tree && tree->data && tree->data->isValid) {
        // VBO already exists, no need to rebuild
        return;
    }
    
    // Start building cached geometry
    BeginLineStrip3D(col);
}

void Renderer3D::CachedLineStrip3D(const Vector3<float>* vertices, int vertexCount) {
    for (int i = 0; i < vertexCount; i++) {
        LineStripVertex3D(vertices[i]);
    }
}

void Renderer3D::EndCachedGeometry3D() {
    // Implementation will be completed when we convert the actual globe rendering
    // For now, this is a placeholder
    EndLineStrip3D();
}

void Renderer3D::RenderCachedGeometry3D(const char* cacheKey) {
    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(cacheKey);
    if (!tree || !tree->data || !tree->data->isValid) {
        return; // VBO doesn't exist or is invalid
    }
    
    Cached3DVBO* cachedVBO = tree->data;
    
    // Use 3D shader
    glUseProgram(m_shader3DProgram);
    
    // Set uniforms
    int projLoc = glGetUniformLocation(m_shader3DProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_shader3DProgram, "uModelView");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix3D.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix3D.m);
    
    // Render the cached VBO
    glBindVertexArray(cachedVBO->VAO);
    glDrawArrays(GL_LINES, 0, cachedVBO->vertexCount);
    glBindVertexArray(0);
    glUseProgram(0);
}

void Renderer3D::InvalidateCached3DVBO(const char* cacheKey) {
    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(cacheKey);
    if (tree && tree->data) {
        Cached3DVBO* cachedVBO = tree->data;
        cachedVBO->isValid = false;
        if (cachedVBO->VBO != 0) {
            glDeleteBuffers(1, &cachedVBO->VBO);
            cachedVBO->VBO = 0;
        }
        if (cachedVBO->VAO != 0) {
            glDeleteVertexArrays(1, &cachedVBO->VAO);
            cachedVBO->VAO = 0;
        }
    }
}

bool Renderer3D::IsCachedGeometry3DValid(const char* cacheKey) {
    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(cacheKey);
    return (tree && tree->data && tree->data->isValid);
}

// ============================================================================
// MEGA-VBO BATCHING SYSTEM FOR 3D
// Maximum performance for rendering thousands of 3D line strips in single draw call
// ============================================================================

void Renderer3D::BeginMegaVBO3D(const char* megaVBOKey, Colour const &col) {
    // Check if this mega-VBO already exists and is valid
    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(megaVBOKey);
    if (tree && tree->data && tree->data->isValid) {
        // Mega-VBO already exists, no need to rebuild
        return;
    }
    
    // Store mega-VBO key for building
    if (m_currentMegaVBO3DKey) {
        delete[] m_currentMegaVBO3DKey;
    }
    m_currentMegaVBO3DKey = newStr(megaVBOKey);
    
    // Store mega-VBO state
    m_megaVBO3DActive = true;
    m_megaVBO3DColor = col;
    
    // Clear mega vertex buffer
    m_megaVertex3DCount = 0;
}

void Renderer3D::AddLineStripToMegaVBO3D(const Vector3<float>* vertices, int vertexCount) {
    if (!m_megaVBO3DActive || vertexCount < 2) return;
    
    // Convert color to float
    float r = m_megaVBO3DColor.m_r / 255.0f, g = m_megaVBO3DColor.m_g / 255.0f;
    float b = m_megaVBO3DColor.m_b / 255.0f, a = m_megaVBO3DColor.m_a / 255.0f;
    
    // Convert line strip to line segments and add to mega buffer
    for (int i = 0; i < vertexCount - 1; i++) {
        if (m_megaVertex3DCount + 2 >= MAX_MEGA_3D_VERTICES) {
            return;
        }
        
        // Line from vertex i to vertex i+1
        // First vertex of line segment
        m_megaVertices3D[m_megaVertex3DCount] = Vertex3D(
            vertices[i].x, vertices[i].y, vertices[i].z, r, g, b, a
        );
        m_megaVertex3DCount++;
        
        // Second vertex of line segment
        m_megaVertices3D[m_megaVertex3DCount] = Vertex3D(
            vertices[i + 1].x, vertices[i + 1].y, vertices[i + 1].z, r, g, b, a
        );
        m_megaVertex3DCount++;
    }
}

void Renderer3D::EndMegaVBO3D() {
    if (!m_megaVBO3DActive || !m_currentMegaVBO3DKey) return;
    
    if (m_megaVertex3DCount < 2) {
        // Not enough vertices for lines
        m_megaVBO3DActive = false;
        return;
    }
    
    // Create or get cached mega-VBO
    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(m_currentMegaVBO3DKey);
    Cached3DVBO* cachedVBO = NULL;
    if (!tree || !tree->data) {
        cachedVBO = new Cached3DVBO();
        cachedVBO->VBO = 0;
        cachedVBO->VAO = 0;
        cachedVBO->vertexCount = 0;
        cachedVBO->isValid = false;
        m_cached3DVBOs.PutData(m_currentMegaVBO3DKey, cachedVBO);
    } else {
        cachedVBO = tree->data;
    }
    
    // Create VBO if it doesn't exist
    if (cachedVBO->VBO == 0) {
        glGenVertexArrays(1, &cachedVBO->VAO);
        glGenBuffers(1, &cachedVBO->VBO);
    }
    
    // Upload mega data to VBO
    glBindVertexArray(cachedVBO->VAO);
    glBindBuffer(GL_ARRAY_BUFFER, cachedVBO->VBO);
    glBufferData(GL_ARRAY_BUFFER, m_megaVertex3DCount * sizeof(Vertex3D), m_megaVertices3D, GL_STATIC_DRAW);
    
    // Set up vertex attributes (3D version)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    
    // Store mega-VBO info
    cachedVBO->vertexCount = m_megaVertex3DCount;
    cachedVBO->color = m_megaVBO3DColor;
    cachedVBO->isValid = true;
    
    // Reset state
    m_megaVertex3DCount = 0;
    m_megaVBO3DActive = false;
}

void Renderer3D::RenderMegaVBO3D(const char* megaVBOKey) {
    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(megaVBOKey);
    if (!tree || !tree->data || !tree->data->isValid) {
        return; // Mega-VBO doesn't exist or is invalid
    }
    
    // Track VBO draw call for debug menu
    IncrementDrawCall3D("mega_vbo");
    
    Cached3DVBO* cachedVBO = tree->data;
    
    // Use 3D shader
    glUseProgram(m_shader3DProgram);
    
    // Set uniforms
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
    
    // Render the mega-VBO with single draw call
    glBindVertexArray(cachedVBO->VAO);
    glDrawArrays(GL_LINES, 0, cachedVBO->vertexCount);
    glBindVertexArray(0);
    glUseProgram(0);
}

bool Renderer3D::IsMegaVBO3DValid(const char* megaVBOKey) {
    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(megaVBOKey);
    return (tree && tree->data && tree->data->isValid);
}

void Renderer3D::InvalidateAll3DVBOs() {
    DArray<Cached3DVBO*> *allVBOs = m_cached3DVBOs.ConvertToDArray();
    for (int i = 0; i < allVBOs->Size(); ++i) {
        Cached3DVBO* cachedVBO = allVBOs->GetData(i);
        if (cachedVBO) {
            cachedVBO->isValid = false;
            if (cachedVBO->VBO != 0) {
                glDeleteBuffers(1, &cachedVBO->VBO);
                cachedVBO->VBO = 0;
            }
            if (cachedVBO->VAO != 0) {
                glDeleteVertexArrays(1, &cachedVBO->VAO);
                cachedVBO->VAO = 0;
            }
        }
    }
    delete allVBOs;
    AppDebugOut("Invalidated all cached 3D VBOs\n");
}
