#include "lib/universal_include.h"

#include "lib/string_utils.h"
#include "lib/render2d/renderer.h"

#include "renderer_3d.h"

// ============================================================================
// VBO CACHING SYSTEM FOR 3D
// For static 3D geometry
// ============================================================================

void Renderer3D::BeginCachedGeometry3D(const char* cacheKey, Colour const &col) {

    //
    // Check if this VBO already exists and is valid

    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(cacheKey);
    if (tree && tree->data && tree->data->isValid) {
        return;
    }
    
    BeginLineStrip3D(col);
}

void Renderer3D::CachedLineStrip3D(const Vector3<float>* vertices, int vertexCount) {
    for (int i = 0; i < vertexCount; i++) {
        LineStripVertex3D(vertices[i]);
    }
}

void Renderer3D::EndCachedGeometry3D() {
    EndLineStrip3D();
}

void Renderer3D::RenderCachedGeometry3D(const char* cacheKey) {
    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(cacheKey);
    if (!tree || !tree->data || !tree->data->isValid) {
        return;
    }
    
    Cached3DVBO* cachedVBO = tree->data;
    
    m_renderer->SetShaderProgram(m_shader3DProgram);
    Set3DShaderUniforms();
    
    m_renderer->SetVertexArray(cachedVBO->VAO);
    glDrawArrays(GL_LINES, 0, cachedVBO->vertexCount);
}

void Renderer3D::InvalidateCached3DVBO(const char* cacheKey) {
    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(cacheKey);
    if (tree && tree->data) {
        Cached3DVBO* cachedVBO = tree->data;

        if (cachedVBO->VBO != 0) {
            glDeleteBuffers(1, &cachedVBO->VBO);
            cachedVBO->VBO = 0;
        }
        if (cachedVBO->IBO != 0) {
            glDeleteBuffers(1, &cachedVBO->IBO);
            cachedVBO->IBO = 0;
        }
        if (cachedVBO->VAO != 0) {
            glDeleteVertexArrays(1, &cachedVBO->VAO);
            cachedVBO->VAO = 0;
        }
        
        delete cachedVBO;
        m_cached3DVBOs.RemoveData(cacheKey);
    }
}

bool Renderer3D::IsCachedGeometry3DValid(const char* cacheKey) {
    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(cacheKey);
    return (tree && tree->data && tree->data->isValid);
}

void Renderer3D::BeginMegaVBO3D(const char* megaVBOKey, Colour const &col) {
    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(megaVBOKey);
    if (tree && tree->data && tree->data->isValid) {
        return;
    }

    if (m_currentMegaVBO3DKey) {
        delete[] m_currentMegaVBO3DKey;
    }
    m_currentMegaVBO3DKey = newStr(megaVBOKey);
    
    m_megaVBO3DActive = true;
    m_megaVBO3DColor = col;
    
    m_megaVertex3DCount = 0;
    m_megaIndex3DCount = 0;
}

void Renderer3D::AddLineStripToMegaVBO3D(const Vector3<float>* vertices, int vertexCount) {
    if (!m_megaVBO3DActive || vertexCount < 2) return;
    
    //
    // Convert color to float

    float r = m_megaVBO3DColor.GetRFloat(), g = m_megaVBO3DColor.GetGFloat();
    float b = m_megaVBO3DColor.GetBFloat(), a = m_megaVBO3DColor.GetAFloat();
    
    //
    // Store starting vertex index for this line strip

    unsigned int startIndex = m_megaVertex3DCount;
    
    //
    // Add vertices without duplication

    for (int i = 0; i < vertexCount; i++) {
        m_megaVertices3D[m_megaVertex3DCount] = Vertex3D(
            vertices[i].x, vertices[i].y, vertices[i].z, r, g, b, a
        );
        m_megaVertex3DCount++;
    }
    
    //
    // Add indices for this line strip

    for (int i = 0; i < vertexCount; i++) {
        m_megaIndices3D[m_megaIndex3DCount++] = startIndex + i;
    }
    
    //
    // Add primitive restart index to separate line strips

    m_megaIndices3D[m_megaIndex3DCount++] = 0xFFFFFFFF;
}

void Renderer3D::EndMegaVBO3D() {
    if (!m_megaVBO3DActive || !m_currentMegaVBO3DKey) return;
    
    if (m_megaVertex3DCount < 2) {
        m_megaVBO3DActive = false;
        return;
    }
    
    //
    // Create or get cached Mega-VBO

    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(m_currentMegaVBO3DKey);
    Cached3DVBO* cachedVBO = NULL;
    if (!tree || !tree->data) {
        cachedVBO = new Cached3DVBO();
        cachedVBO->VBO = 0;
        cachedVBO->VAO = 0;
        cachedVBO->IBO = 0;
        cachedVBO->vertexCount = 0;
        cachedVBO->indexCount = 0;
        cachedVBO->isValid = false;
        m_cached3DVBOs.PutData(m_currentMegaVBO3DKey, cachedVBO);
    } else {
        cachedVBO = tree->data;
    }
    
    bool isNewVBO = (cachedVBO->VBO == 0);
    if (isNewVBO) {
        glGenVertexArrays(1, &cachedVBO->VAO);
        glGenBuffers(1, &cachedVBO->VBO);
        glGenBuffers(1, &cachedVBO->IBO);
        
        glBindVertexArray(cachedVBO->VAO);
        glBindBuffer(GL_ARRAY_BUFFER, cachedVBO->VBO);
        
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cachedVBO->IBO);
    } else {
        glBindVertexArray(cachedVBO->VAO);
        glBindBuffer(GL_ARRAY_BUFFER, cachedVBO->VBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cachedVBO->IBO);
    }
    
    glBufferData(GL_ARRAY_BUFFER, m_megaVertex3DCount * sizeof(Vertex3D), m_megaVertices3D, GL_STATIC_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_megaIndex3DCount * sizeof(unsigned int), m_megaIndices3D, GL_STATIC_DRAW);

    cachedVBO->vertexCount = m_megaVertex3DCount;
    cachedVBO->indexCount = m_megaIndex3DCount;
    cachedVBO->color = m_megaVBO3DColor;
    cachedVBO->isValid = true;
    
    m_megaVertex3DCount = 0;
    m_megaIndex3DCount = 0;
    m_megaVBO3DActive = false;
}

void Renderer3D::RenderMegaVBO3D(const char* megaVBOKey) {
    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(megaVBOKey);
    if (!tree || !tree->data || !tree->data->isValid) {
        return;
    }
    
    IncrementDrawCall3D("mega_vbo");
    
    Cached3DVBO* cachedVBO = tree->data;
    
    m_renderer->SetShaderProgram(m_shader3DProgram);
    Set3DShaderUniforms();
    
#ifndef TARGET_EMSCRIPTEN
    glEnable(GL_PRIMITIVE_RESTART);
    glPrimitiveRestartIndex(0xFFFFFFFF);
#endif
    m_renderer->SetVertexArray(cachedVBO->VAO);
    glDrawElements(GL_LINE_STRIP, cachedVBO->indexCount, GL_UNSIGNED_INT, 0);
    
#ifndef TARGET_EMSCRIPTEN
    glDisable(GL_PRIMITIVE_RESTART);
#endif
}

bool Renderer3D::IsMegaVBO3DValid(const char* megaVBOKey) {
    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(megaVBOKey);
    return (tree && tree->data && tree->data->isValid);
}

void Renderer3D::InvalidateAll3DVBOs() {
    DArray<char*> *keys = m_cached3DVBOs.ConvertIndexToDArray();
    
    for (int i = 0; i < keys->Size(); ++i) {
        InvalidateCached3DVBO(keys->GetData(i));
    }
    
    delete keys;
}

void Renderer3D::SetMegaVBO3DBufferSizes(int vertexCount, int indexCount) {
    int newMaxVertices = (int)(vertexCount * 1.1f);
    int newMaxIndices = (int)(indexCount * 1.1f);
    
    InvalidateCached3DVBO("GlobeCoastlines");
    InvalidateCached3DVBO("GlobeBorders");
    InvalidateCached3DVBO("GlobeGridlines");
    
    if (m_megaVertices3D) {
        delete[] m_megaVertices3D;
        m_megaVertices3D = NULL;
    }
    if (m_megaIndices3D) {
        delete[] m_megaIndices3D;
        m_megaIndices3D = NULL;
    }
    
    m_maxMegaVertices3D = newMaxVertices;
    m_maxMegaIndices3D = newMaxIndices;
    m_megaVertices3D = new Vertex3D[m_maxMegaVertices3D];
    m_megaIndices3D = new unsigned int[m_maxMegaIndices3D];
}

// ============================================================================
// TEXTURED MEGA-VBO BATCHING SYSTEM
// ============================================================================

void Renderer3D::BeginTexturedMegaVBO3D(const char* megaVBOKey, unsigned int textureID) {
    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(megaVBOKey);
    if (tree && tree->data && tree->data->isValid) {
        return;
    }
    
    if (m_currentMegaVBO3DTexturedKey) {
        delete[] m_currentMegaVBO3DTexturedKey;
    }

    m_currentMegaVBO3DTexturedKey = newStr(megaVBOKey);

    m_megaVBO3DTexturedActive = true;
    m_currentMegaVBO3DTextureID = textureID;
    
    m_megaTexturedVertex3DCount = 0;
    m_megaTexturedIndex3DCount = 0;
}

void Renderer3D::AddTexturedQuadsToMegaVBO3D(const Vertex3DTextured* vertices, int vertexCount, int quadCount) {
    if (!m_megaVBO3DTexturedActive || vertexCount < 4) return;
    
    //
    // Store starting vertex index for this set of quads

    unsigned int startIndex = m_megaTexturedVertex3DCount;
    
    //
    // Add vertices

    for (int i = 0; i < vertexCount; i++) {
        m_megaTexturedVertices3D[m_megaTexturedVertex3DCount] = vertices[i];
        m_megaTexturedVertex3DCount++;
    }
    
    //
    // Add indices for triangles (6 indices per quad = 2 triangles)

    for (int q = 0; q < quadCount; q++) {
        int baseIdx = startIndex + (q * 6);
        
        // First triangle
        m_megaTexturedIndices3D[m_megaTexturedIndex3DCount++] = baseIdx + 0;
        m_megaTexturedIndices3D[m_megaTexturedIndex3DCount++] = baseIdx + 1;
        m_megaTexturedIndices3D[m_megaTexturedIndex3DCount++] = baseIdx + 2;
        
        // Second triangle
        m_megaTexturedIndices3D[m_megaTexturedIndex3DCount++] = baseIdx + 3;
        m_megaTexturedIndices3D[m_megaTexturedIndex3DCount++] = baseIdx + 4;
        m_megaTexturedIndices3D[m_megaTexturedIndex3DCount++] = baseIdx + 5;
    }
}

void Renderer3D::EndTexturedMegaVBO3D() {
    if (!m_megaVBO3DTexturedActive || !m_currentMegaVBO3DTexturedKey) return;
    
    if (m_megaTexturedVertex3DCount < 4) {
        m_megaVBO3DTexturedActive = false;
        return;
    }
    
    //
    // Create or get cached Mega-VBO

    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(m_currentMegaVBO3DTexturedKey);
    Cached3DVBO* cachedVBO = NULL;
    if (!tree || !tree->data) {
        cachedVBO = new Cached3DVBO();
        cachedVBO->VBO = 0;
        cachedVBO->VAO = 0;
        cachedVBO->IBO = 0;
        cachedVBO->vertexCount = 0;
        cachedVBO->indexCount = 0;
        cachedVBO->isValid = false;
        m_cached3DVBOs.PutData(m_currentMegaVBO3DTexturedKey, cachedVBO);
    } else {
        cachedVBO = tree->data;
    }
    
    bool isNewVBO = (cachedVBO->VBO == 0);
    if (isNewVBO) {
        glGenVertexArrays(1, &cachedVBO->VAO);
        glGenBuffers(1, &cachedVBO->VBO);
        glGenBuffers(1, &cachedVBO->IBO);
        
        glBindVertexArray(cachedVBO->VAO);
        glBindBuffer(GL_ARRAY_BUFFER, cachedVBO->VBO);
        
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3DTextured), (void*)0);
        glEnableVertexAttribArray(0);
        
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex3DTextured), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex3DTextured), (void*)(7 * sizeof(float)));
        glEnableVertexAttribArray(2);
        
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cachedVBO->IBO);
    } else {
        glBindVertexArray(cachedVBO->VAO);
        glBindBuffer(GL_ARRAY_BUFFER, cachedVBO->VBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cachedVBO->IBO);
    }
    
    glBufferData(GL_ARRAY_BUFFER, m_megaTexturedVertex3DCount * sizeof(Vertex3DTextured), m_megaTexturedVertices3D, GL_STATIC_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_megaTexturedIndex3DCount * sizeof(unsigned int), m_megaTexturedIndices3D, GL_STATIC_DRAW);
    
    //
    // Store mega-VBO info with texture

    cachedVBO->vertexCount = m_megaTexturedVertex3DCount;
    cachedVBO->indexCount = m_megaTexturedIndex3DCount;
    cachedVBO->isValid = true;
    
    //
    // Store texture ID for rendering

    cachedVBO->color.m_r = (m_currentMegaVBO3DTextureID >> 24) & 0xFF;
    cachedVBO->color.m_g = (m_currentMegaVBO3DTextureID >> 16) & 0xFF;
    cachedVBO->color.m_b = (m_currentMegaVBO3DTextureID >> 8) & 0xFF;
    cachedVBO->color.m_a = m_currentMegaVBO3DTextureID & 0xFF;
    
    //
    // Reset

    m_megaTexturedVertex3DCount = 0;
    m_megaTexturedIndex3DCount = 0;
    m_megaVBO3DTexturedActive = false;
}

void Renderer3D::RenderTexturedMegaVBO3D(const char* megaVBOKey) {
    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(megaVBOKey);
    if (!tree || !tree->data || !tree->data->isValid) {
        return;
    }
    
    IncrementDrawCall3D("mega_vbo_textured");
    
    Cached3DVBO* cachedVBO = tree->data;
    
    //
    // Extract texture ID from color storage

    unsigned int textureID = ((unsigned int)cachedVBO->color.m_r << 24) |
                             ((unsigned int)cachedVBO->color.m_g << 16) |
                             ((unsigned int)cachedVBO->color.m_b << 8) |
                             ((unsigned int)cachedVBO->color.m_a);
    
    glDepthMask(GL_FALSE);
    
    m_renderer->SetShaderProgram(m_shader3DTexturedProgram);
    SetTextured3DShaderUniforms();
    
    if (textureID != 0) {
        m_renderer->SetActiveTexture(GL_TEXTURE0);
        m_renderer->SetBoundTexture(textureID);
    }
    
    m_renderer->SetVertexArray(cachedVBO->VAO);
    glDrawElements(GL_TRIANGLES, cachedVBO->indexCount, GL_UNSIGNED_INT, 0);
    
    glDepthMask(GL_TRUE);
}

bool Renderer3D::IsTexturedMegaVBO3DValid(const char* megaVBOKey) {
    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(megaVBOKey);
    return (tree && tree->data && tree->data->isValid);
}

void Renderer3D::SetTexturedMegaVBO3DBufferSizes(int vertexCount, int indexCount) {
    int newMaxVertices = (int)(vertexCount * 1.1f);
    int newMaxIndices = (int)(indexCount * 1.1f);
    
    InvalidateCached3DVBO("Starfield");
    
    if (m_megaTexturedVertices3D) {
        delete[] m_megaTexturedVertices3D;
        m_megaTexturedVertices3D = NULL;
    }
    if (m_megaTexturedIndices3D) {
        delete[] m_megaTexturedIndices3D;
        m_megaTexturedIndices3D = NULL;
    }
    
    m_maxMegaTexturedVertices3D = newMaxVertices;
    m_maxMegaTexturedIndices3D = newMaxIndices;
    m_megaTexturedVertices3D = new Vertex3DTextured[m_maxMegaTexturedVertices3D];
    m_megaTexturedIndices3D = new unsigned int[m_maxMegaTexturedIndices3D];
}

// ============================================================================
// VBO CACHE STATISTICS
// ============================================================================

int Renderer3D::GetCached3DVBOCount() {
    int count = 0;
    
    DArray<char*> *keys = m_cached3DVBOs.ConvertIndexToDArray();
    
    for (int i = 0; i < keys->Size(); ++i) {
        BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(keys->GetData(i));
        if (tree && tree->data && tree->data->isValid) {
            count++;
        }
    }
    
    delete keys;
    return count;
}

int Renderer3D::GetCached3DVBOVertexCount(const char *cacheKey) {
    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(cacheKey);
    if (tree && tree->data && tree->data->isValid) {
        return tree->data->vertexCount;
    }
    return 0;
}

int Renderer3D::GetCached3DVBOIndexCount(const char *cacheKey) {
    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(cacheKey);
    if (tree && tree->data && tree->data->isValid) {
        return tree->data->indexCount;
    }
    return 0;
}