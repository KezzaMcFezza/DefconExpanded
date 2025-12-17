#include "systemiv.h"

#include "lib/debug_utils.h"
#include "lib/string_utils.h"
#include "lib/render/renderer.h"
#include "lib/render2d/renderer_2d.h"
#include "lib/render2d/megavbo/megavbo_2d.h"

void Renderer2DVBO::BeginMegaVBO(const char* megaVBOKey, Colour const &col) {
    BTree<CachedVBO*>* tree = m_cachedVBOs.LookupTree(megaVBOKey);
    if (tree && tree->data && tree->data->isValid) {
        return;
    }
    
    if (m_currentMegaVBOKey) {
        delete[] m_currentMegaVBOKey;
    }
    m_currentMegaVBOKey = newStr(megaVBOKey);
    
    m_megaVBOActive = true;
    m_megaVBOColor = col;
    
    m_megaVertexCount = 0;
    m_megaIndexCount = 0;
}

void Renderer2DVBO::AddLineStripToMegaVBO(float* vertices, int vertexCount) {
    if (!m_megaVBOActive || vertexCount < 2) return;

    if (m_megaVertexCount + vertexCount > m_maxMegaVertices ||
        m_megaIndexCount + vertexCount + 1 > m_maxMegaIndices) {
        AppDebugOut("Warning: Coastlines/Borders VBO overflow: Vertices: %d/%d, Indices: %d/%d\n",
                    m_megaVertexCount + vertexCount, m_maxMegaVertices,
                    m_megaIndexCount + vertexCount + 1, m_maxMegaIndices);
        return;
    }

    float r = m_megaVBOColor.m_r / 255.0f, g = m_megaVBOColor.m_g / 255.0f;
    float b = m_megaVBOColor.m_b / 255.0f, a = m_megaVBOColor.m_a / 255.0f;
       
    unsigned int startIndex = m_megaVertexCount;

    //
    // Add vertices without duplication

    for (int i = 0; i < vertexCount; i++) {
        m_megaVertices[m_megaVertexCount++] = {
            vertices[i * 2], vertices[i * 2 + 1], 
            r, g, b, a, 
            0.0f, 0.0f
        };
    }

    for (int i = 0; i < vertexCount; i++) {
        m_megaIndices[m_megaIndexCount++] = startIndex + i;
    }
    
    //
    // Add primitive restart index to separate line strips

    m_megaIndices[m_megaIndexCount++] = 0xFFFFFFFF;
}

void Renderer2DVBO::EndMegaVBO() {
    if (!m_megaVBOActive || !m_currentMegaVBOKey) return;
    
    if (m_megaVertexCount < 2) {
        m_megaVBOActive = false;
        return;
    }
    
    //
    // Create or get cached Mega-VBO

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
    
    bool isNewVBO = (cachedVBO->VBO == 0);
    if (isNewVBO) {
        glGenVertexArrays(1, &cachedVBO->VAO);
        glGenBuffers(1, &cachedVBO->VBO);
        glGenBuffers(1, &cachedVBO->IBO);
        
        g_renderer->SetVertexArray(cachedVBO->VAO);
        g_renderer->SetArrayBuffer(cachedVBO->VBO);
        
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);
        
        //
        // Bind index buffer to VAO
        
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cachedVBO->IBO);
    } else {
        g_renderer->SetVertexArray(cachedVBO->VAO);
        g_renderer->SetArrayBuffer(cachedVBO->VBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cachedVBO->IBO);
    }
    
    //
    // Upload vertex data
    
    glBufferData(GL_ARRAY_BUFFER, m_megaVertexCount * sizeof(Vertex2D), m_megaVertices, GL_STATIC_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_megaIndexCount * sizeof(unsigned int), m_megaIndices, GL_STATIC_DRAW);
    
    cachedVBO->vertexCount = m_megaVertexCount;
    cachedVBO->indexCount = m_megaIndexCount;
    cachedVBO->color = m_megaVBOColor;
    cachedVBO->lineWidth = m_megaVBOWidth;
    cachedVBO->isValid = true;
    
    m_megaVertexCount = 0;
    m_megaIndexCount = 0;
    m_megaVBOActive = false;
}

void Renderer2DVBO::RenderMegaVBO(const char* megaVBOKey) {
    BTree<CachedVBO*>* tree = m_cachedVBOs.LookupTree(megaVBOKey);
    if (!tree || !tree->data || !tree->data->isValid) {
        return;
    }
    
    CachedVBO* cachedVBO = tree->data;

    g_renderer->StartFlushTiming("MegaVBO_2D");
    
    g_renderer->SetShaderProgram(g_renderer2d->m_colorShaderProgram);
    g_renderer2d->SetColorShaderUniforms();
    
#ifndef TARGET_EMSCRIPTEN
    glEnable(GL_PRIMITIVE_RESTART);
    glPrimitiveRestartIndex(0xFFFFFFFF);
#endif
    
    //
    // Render the mega-VBO with indexed drawing for emscripten

    g_renderer->SetVertexArray(cachedVBO->VAO);
    glDrawElements(GL_LINE_STRIP, cachedVBO->indexCount, GL_UNSIGNED_INT, 0);
    
#ifndef TARGET_EMSCRIPTEN
    glDisable(GL_PRIMITIVE_RESTART);
#endif
    
    g_renderer->EndFlushTiming("MegaVBO_2D");
    g_renderer2d->IncrementDrawCall("line_vbo");
}

bool Renderer2DVBO::IsMegaVBOValid(const char* megaVBOKey) {
    BTree<CachedVBO*>* tree = m_cachedVBOs.LookupTree(megaVBOKey);
    return (tree && tree->data && tree->data->isValid);
}

void Renderer2DVBO::SetMegaVBOBufferSizes(int vertexCount, int indexCount) {

    //
    // Calculate new sizes with 10% safety margin

    int newMaxVertices = (int)(vertexCount * 1.1f);
    int newMaxIndices = (int)(indexCount * 1.1f);

    InvalidateCachedVBO("MapCoastlines");
    InvalidateCachedVBO("MapBorders");
    
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
