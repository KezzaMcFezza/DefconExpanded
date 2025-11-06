#include "lib/universal_include.h"

#include "lib/string_utils.h"
#include "lib/render2d/renderer.h"

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
    
    m_renderer->SetShaderProgram(m_shader3DProgram);
    Set3DShaderUniforms();
    
    // Render the cached VBO
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
    
    m_megaVertex3DCount = 0;
    m_megaIndex3DCount = 0;
}

void Renderer3D::AddLineStripToMegaVBO3D(const Vector3<float>* vertices, int vertexCount) {
    if (!m_megaVBO3DActive || vertexCount < 2) return;
    
    //
    // convert color to float

    float r = m_megaVBO3DColor.GetRFloat(), g = m_megaVBO3DColor.GetGFloat();
    float b = m_megaVBO3DColor.GetBFloat(), a = m_megaVBO3DColor.GetAFloat();
    
    //
    // store starting vertex index for this line strip

    unsigned int startIndex = m_megaVertex3DCount;
    
    //
    // add vertices without duplication

    for (int i = 0; i < vertexCount; i++) {
        m_megaVertices3D[m_megaVertex3DCount] = Vertex3D(
            vertices[i].x, vertices[i].y, vertices[i].z, r, g, b, a
        );
        m_megaVertex3DCount++;
    }
    
    //
    // add indices for this line strip

    for (int i = 0; i < vertexCount; i++) {
        m_megaIndices3D[m_megaIndex3DCount++] = startIndex + i;
    }
    
    //
    // add primitive restart index to separate line strips

    m_megaIndices3D[m_megaIndex3DCount++] = 0xFFFFFFFF;
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
    
    // Store mega-VBO info
    cachedVBO->vertexCount = m_megaVertex3DCount;
    cachedVBO->indexCount = m_megaIndex3DCount;
    cachedVBO->color = m_megaVBO3DColor;
    cachedVBO->isValid = true;
    
    // Reset state
    m_megaVertex3DCount = 0;
    m_megaIndex3DCount = 0;
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