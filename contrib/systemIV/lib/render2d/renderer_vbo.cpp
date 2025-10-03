#include "lib/universal_include.h"

#include "lib/debug_utils.h"
#include "lib/string_utils.h"

#include "lib/render2d/renderer.h"

// External globals  
extern Renderer *g_renderer;

// ============================================================================
// VBO CACHING SYSTEM
// High-performance rendering for static geometry like coastlines
// ============================================================================

void Renderer::BeginCachedLineStrip(const char* cacheKey, Colour const &col, float lineWidth) {
    // Check if this VBO already exists and is valid
    BTree<CachedVBO*>* tree = m_cachedVBOs.LookupTree(cacheKey);
    if (tree && tree->data && tree->data->isValid) {
        return; // VBO already exists, no need to rebuild
    }
    
    // Store cache key for building
    if (m_currentCacheKey) {
        delete[] m_currentCacheKey;
    }
    
    if (m_currentMegaVBOKey) {
        delete[] m_currentMegaVBOKey;
    }
    
    if (m_megaVertices) {
        delete[] m_megaVertices;
    }
    m_currentCacheKey = newStr(cacheKey);
    
    // Store cached line strip state
    m_cachedLineStripActive = true;
    m_cachedLineStripColor = col;
    m_cachedLineStripWidth = lineWidth;
    
    // Clear vertex buffer for new cached line strip
    m_lineVertexCount = 0;
}

void Renderer::CachedLineStripVertex(float x, float y) {
    if (!m_cachedLineStripActive) return;
    
    // Check for buffer overflow
    if (m_lineVertexCount >= MAX_VERTICES) {
        AppDebugOut("Warning: Cached line strip exceeded vertex buffer size\n");
        return;
    }
    
    // Convert color to float
    float r = m_cachedLineStripColor.m_r / 255.0f, g = m_cachedLineStripColor.m_g / 255.0f;
    float b = m_cachedLineStripColor.m_b / 255.0f, a = m_cachedLineStripColor.m_a / 255.0f;
    
    // Add vertex to cached line strip buffer
    m_lineVertices[m_lineVertexCount++] = {x, y, r, g, b, a, 0.0f, 0.0f};
}

void Renderer::EndCachedLineStrip() {
    if (!m_cachedLineStripActive || !m_currentCacheKey) return;
    
    // Convert the accumulated vertices to GL_LINES format and store in VBO
    if (m_lineVertexCount < 2) {
        m_cachedLineStripActive = false;
        return;
    }
    
    // Create or get cached VBO
    BTree<CachedVBO*>* tree = m_cachedVBOs.LookupTree(m_currentCacheKey);
    CachedVBO* cachedVBO = NULL;
    if (!tree || !tree->data) {
        cachedVBO = new CachedVBO();
        cachedVBO->VBO = 0;
        cachedVBO->VAO = 0;
        cachedVBO->vertexCount = 0;
        cachedVBO->isValid = false;
        m_cachedVBOs.PutData(m_currentCacheKey, cachedVBO);
    } else {
        cachedVBO = tree->data;
    }
    
    // Create line segments from line strip
    int lineVertexCount = (m_lineVertexCount - 1) * 2;
    Vertex2D* lineVertices = new Vertex2D[lineVertexCount];
    
    int lineIndex = 0;
    for (int i = 0; i < m_lineVertexCount - 1; i++) {
        // Line from vertex i to vertex i+1
        lineVertices[lineIndex++] = m_lineVertices[i];
        lineVertices[lineIndex++] = m_lineVertices[i + 1];
    }
    
    bool isNewVBO = (cachedVBO->VBO == 0);
    if (isNewVBO) {
        glGenVertexArrays(1, &cachedVBO->VAO);
        glGenBuffers(1, &cachedVBO->VBO);
        
        glBindVertexArray(cachedVBO->VAO);
        glBindBuffer(GL_ARRAY_BUFFER, cachedVBO->VBO);
        
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);
    } else {
        glBindVertexArray(cachedVBO->VAO);
        glBindBuffer(GL_ARRAY_BUFFER, cachedVBO->VBO);
    }
    
#ifdef TARGET_EMSCRIPTEN
    if (isNewVBO || cachedVBO->vertexCount != lineVertexCount) {
        glBufferData(GL_ARRAY_BUFFER, lineVertexCount * sizeof(Vertex2D), lineVertices, GL_STATIC_DRAW);
    } else {
        glBufferSubData(GL_ARRAY_BUFFER, 0, lineVertexCount * sizeof(Vertex2D), lineVertices);
    }
#else
    glBufferData(GL_ARRAY_BUFFER, lineVertexCount * sizeof(Vertex2D), lineVertices, GL_STATIC_DRAW);
#endif
    
    // Store VBO info
    cachedVBO->vertexCount = lineVertexCount;
    cachedVBO->color = m_cachedLineStripColor;
    cachedVBO->lineWidth = m_cachedLineStripWidth;
    cachedVBO->isValid = true;
    
    // Clean up
    delete[] lineVertices;
    m_lineVertexCount = 0;
    m_cachedLineStripActive = false;
}

void Renderer::RenderCachedLineStrip(const char* cacheKey) {
    BTree<CachedVBO*>* tree = m_cachedVBOs.LookupTree(cacheKey);
    if (!tree || !tree->data || !tree->data->isValid) {
        return; // VBO doesn't exist or is invalid
    }
    CachedVBO* cachedVBO = tree->data;
    
    glUseProgram(m_colorShaderProgram);
    SetColorShaderUniforms();
    
    // Render the cached VBO
    glBindVertexArray(cachedVBO->VAO);
    glDrawArrays(GL_LINES, 0, cachedVBO->vertexCount);
}

void Renderer::InvalidateCachedVBO(const char* cacheKey) {
    BTree<CachedVBO*>* tree = m_cachedVBOs.LookupTree(cacheKey);
    if (tree && tree->data) {
        CachedVBO* cachedVBO = tree->data;

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
        
        //
        // delete the struct and remove from BTree to free CPU memory

        delete cachedVBO;
        m_cachedVBOs.RemoveData(cacheKey);
    }
}

// ============================================================================
// MEGA-VBO BATCHING SYSTEM
// Maximum performance for rendering thousands of line strips in single draw call
// ============================================================================

void Renderer::BeginMegaVBO(const char* megaVBOKey, Colour const &col) {
    // Check if this mega-VBO already exists and is valid
    BTree<CachedVBO*>* tree = m_cachedVBOs.LookupTree(megaVBOKey);
    if (tree && tree->data && tree->data->isValid) {
        return; // Mega-VBO already exists, no need to rebuild
    }
    
    // Store mega-VBO key for building
    if (m_currentMegaVBOKey) {
        delete[] m_currentMegaVBOKey;
    }
    m_currentMegaVBOKey = newStr(megaVBOKey);
    
    // Store mega-VBO state
    m_megaVBOActive = true;
    m_megaVBOColor = col;
    
    // Clear mega vertex and index buffers
    m_megaVertexCount = 0;
    m_megaIndexCount = 0;
}

void Renderer::AddLineStripToMegaVBO(float* vertices, int vertexCount) {
    if (!m_megaVBOActive || vertexCount < 2) return;
    
    //
    // convert color to float

    float r = m_megaVBOColor.m_r / 255.0f, g = m_megaVBOColor.m_g / 255.0f;
    float b = m_megaVBOColor.m_b / 255.0f, a = m_megaVBOColor.m_a / 255.0f;
    
    //
    // store starting vertex index for this line strip
        
    unsigned int startIndex = m_megaVertexCount;

    //
    // add vertices without duplication

    for (int i = 0; i < vertexCount; i++) {
        m_megaVertices[m_megaVertexCount++] = {
            vertices[i * 2], vertices[i * 2 + 1], 
            r, g, b, a, 
            0.0f, 0.0f
        };
    }
    
    //
    // add indices for this line strip

    for (int i = 0; i < vertexCount; i++) {
        m_megaIndices[m_megaIndexCount++] = startIndex + i;
    }
    
    //
    // add primitive restart index to separate line strips

    m_megaIndices[m_megaIndexCount++] = 0xFFFFFFFF;
}

void Renderer::EndMegaVBO() {
    if (!m_megaVBOActive || !m_currentMegaVBOKey) return;
    
    if (m_megaVertexCount < 2) {
        m_megaVBOActive = false;
        return;
    }
    
    // Create or get cached mega-VBO
    BTree<CachedVBO*>* tree = m_cachedVBOs.LookupTree(m_currentMegaVBOKey);
    CachedVBO* cachedVBO = NULL;
    if (!tree || !tree->data) {
        cachedVBO = new CachedVBO();
        cachedVBO->VBO = 0;
        cachedVBO->VAO = 0;
        cachedVBO->IBO = 0;
        cachedVBO->vertexCount = 0;
        cachedVBO->indexCount = 0;
        cachedVBO->isValid = false;
        m_cachedVBOs.PutData(m_currentMegaVBOKey, cachedVBO);
    } else {
        cachedVBO = tree->data;
    }
    
    if (cachedVBO->VBO == 0) {
        glGenVertexArrays(1, &cachedVBO->VAO);
        glGenBuffers(1, &cachedVBO->VBO);
        glGenBuffers(1, &cachedVBO->IBO);
        
        glBindVertexArray(cachedVBO->VAO);
        glBindBuffer(GL_ARRAY_BUFFER, cachedVBO->VBO);
        
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);
        
        //
        // bind index buffer to VAO
        
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cachedVBO->IBO);
    } else {
        glBindVertexArray(cachedVBO->VAO);
        glBindBuffer(GL_ARRAY_BUFFER, cachedVBO->VBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cachedVBO->IBO);
    }
    
    //
    // upload vertex data
    
#ifdef TARGET_EMSCRIPTEN
    if (cachedVBO->vertexCount != m_megaVertexCount) {
        glBufferData(GL_ARRAY_BUFFER, m_megaVertexCount * sizeof(Vertex2D), m_megaVertices, GL_STATIC_DRAW);
    } else {
        glBufferSubData(GL_ARRAY_BUFFER, 0, m_megaVertexCount * sizeof(Vertex2D), m_megaVertices);
    }
#else
    glBufferData(GL_ARRAY_BUFFER, m_megaVertexCount * sizeof(Vertex2D), m_megaVertices, GL_STATIC_DRAW);
#endif
    
    //
    // upload index data
    
#ifdef TARGET_EMSCRIPTEN
    if (cachedVBO->indexCount != m_megaIndexCount) {
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_megaIndexCount * sizeof(unsigned int), m_megaIndices, GL_STATIC_DRAW);
    } else {
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, m_megaIndexCount * sizeof(unsigned int), m_megaIndices);
    }
#else
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_megaIndexCount * sizeof(unsigned int), m_megaIndices, GL_STATIC_DRAW);
#endif
    
    // Store mega-VBO info
    cachedVBO->vertexCount = m_megaVertexCount;
    cachedVBO->indexCount = m_megaIndexCount;
    cachedVBO->color = m_megaVBOColor;
    cachedVBO->lineWidth = m_megaVBOWidth;
    cachedVBO->isValid = true;
    
#ifdef EMSCRIPTEN_DEBUG
    AppDebugOut("Created mega-VBO '%s' with %d vertices\n", m_currentMegaVBOKey, m_megaVertexCount);
#endif
    
    // Reset state
    m_megaVertexCount = 0;
    m_megaIndexCount = 0;
    m_megaVBOActive = false;
}

void Renderer::RenderMegaVBO(const char* megaVBOKey) {
    BTree<CachedVBO*>* tree = m_cachedVBOs.LookupTree(megaVBOKey);
    if (!tree || !tree->data || !tree->data->isValid) {
        return; // Mega-VBO doesn't exist or is invalid
    }
    
    CachedVBO* cachedVBO = tree->data;
    
    glUseProgram(m_colorShaderProgram);
    SetColorShaderUniforms();
    
#ifndef TARGET_EMSCRIPTEN
    glEnable(GL_PRIMITIVE_RESTART);
    glPrimitiveRestartIndex(0xFFFFFFFF);
#endif
    
    //
    // render the mega-VBO with indexed drawing for emscripten

    glBindVertexArray(cachedVBO->VAO);
    glDrawElements(GL_LINE_STRIP, cachedVBO->indexCount, GL_UNSIGNED_INT, 0);
    
#ifndef TARGET_EMSCRIPTEN
    glDisable(GL_PRIMITIVE_RESTART);
#endif
}

bool Renderer::IsMegaVBOValid(const char* megaVBOKey) {
    BTree<CachedVBO*>* tree = m_cachedVBOs.LookupTree(megaVBOKey);
    return (tree && tree->data && tree->data->isValid);
}

void Renderer::InvalidateAllVBOs() {
    
    //
    // collect all keys first (can't modify BTree while iterating)

    DArray<char*> *keys = m_cachedVBOs.ConvertIndexToDArray();
    
    //
    // delete each VBO 

    for (int i = 0; i < keys->Size(); ++i) {
        InvalidateCachedVBO(keys->GetData(i));
    }
    
    delete keys;
}

void Renderer::SetMegaVBOBufferSizes(int vertexCount, int indexCount) {

    //
    // calculate new sizes with 10% safety margin

    int newMaxVertices = (int)(vertexCount * 1.1f);
    int newMaxIndices = (int)(indexCount * 1.1f);
    
    //
    // delete all mega-VBO GPU/CPU resources when resizing

    InvalidateCachedVBO("all_coastlines");
    InvalidateCachedVBO("all_borders");
    
    //
    // free existing CPU buffers

    if (m_megaVertices) {
        delete[] m_megaVertices;
        m_megaVertices = NULL;
    }
    if (m_megaIndices) {
        delete[] m_megaIndices;
        m_megaIndices = NULL;
    }
    
    m_maxMegaVertices = newMaxVertices;
    m_maxMegaIndices = newMaxIndices;
    m_megaVertices = new Vertex2D[m_maxMegaVertices];
    m_megaIndices = new unsigned int[m_maxMegaIndices];
}