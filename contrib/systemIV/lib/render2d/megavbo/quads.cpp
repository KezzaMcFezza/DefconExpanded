#include "lib/universal_include.h"

#include "lib/debug_utils.h"
#include "lib/string_utils.h"
#include "lib/render2d/renderer.h"

void Renderer::BeginTexturedMegaVBO(const char* megaVBOKey, unsigned int textureID) {
    BTree<CachedVBO*>* tree = m_cachedVBOs.LookupTree(megaVBOKey);
    if (tree && tree->data && tree->data->isValid) {
        return;
    }
    
    if (m_currentMegaVBOTexturedKey) {
        delete[] m_currentMegaVBOTexturedKey;
    }

    m_currentMegaVBOTexturedKey = newStr(megaVBOKey);

    m_megaVBOTexturedActive = true;
    m_currentMegaVBOTextureID = textureID;
    
    m_megaTexturedVertexCount = 0;
    m_megaTexturedIndexCount = 0;
}

void Renderer::AddTexturedQuadsToMegaVBO(const Vertex2D* vertices, int vertexCount, int quadCount) {
    if (!m_megaVBOTexturedActive || vertexCount < 4) return;

    int indicesNeeded = quadCount * 6;
    if (m_megaTexturedVertexCount + vertexCount > m_maxMegaTexturedVertices ||
        m_megaTexturedIndexCount + indicesNeeded > m_maxMegaTexturedIndices) {
        AppDebugOut("Warning: 2D Textured MegaVBO overflow: Vertices: %d/%d, Indices: %d/%d\n",
                    m_megaTexturedVertexCount + vertexCount, m_maxMegaTexturedVertices,
                    m_megaTexturedIndexCount + indicesNeeded, m_maxMegaTexturedIndices);
        return;
    }
    
    //
    // Store starting vertex index for this set of quads

    unsigned int startIndex = m_megaTexturedVertexCount;
    
    //
    // Add vertices

    for (int i = 0; i < vertexCount; i++) {
        m_megaTexturedVertices[m_megaTexturedVertexCount] = vertices[i];
        m_megaTexturedVertexCount++;
    }
    
    //
    // Add indices for triangles (6 indices per quad = 2 triangles)

    for (int q = 0; q < quadCount; q++) {
        int baseIdx = startIndex + (q * 4);
        
        // First triangle
        m_megaTexturedIndices[m_megaTexturedIndexCount++] = baseIdx + 0;
        m_megaTexturedIndices[m_megaTexturedIndexCount++] = baseIdx + 1;
        m_megaTexturedIndices[m_megaTexturedIndexCount++] = baseIdx + 2;
        
        // Second triangle
        m_megaTexturedIndices[m_megaTexturedIndexCount++] = baseIdx + 0;
        m_megaTexturedIndices[m_megaTexturedIndexCount++] = baseIdx + 2;
        m_megaTexturedIndices[m_megaTexturedIndexCount++] = baseIdx + 3;
    }
}

void Renderer::EndTexturedMegaVBO() {
    if (!m_megaVBOTexturedActive || !m_currentMegaVBOTexturedKey) return;
    
    if (m_megaTexturedVertexCount < 4) {
        m_megaVBOTexturedActive = false;
        return;
    }
    
    //
    // Create or get cached Mega-VBO

    BTree<CachedVBO*>* tree = m_cachedVBOs.LookupTree(m_currentMegaVBOTexturedKey);
    CachedVBO* cachedVBO = NULL;
    if (!tree || !tree->data) {
        cachedVBO = new CachedVBO();
        cachedVBO->VBO = 0;
        cachedVBO->VAO = 0;
        cachedVBO->IBO = 0;
        cachedVBO->vertexCount = 0;
        cachedVBO->indexCount = 0;
        cachedVBO->isValid = false;
        m_cachedVBOs.PutData(m_currentMegaVBOTexturedKey, cachedVBO);
    } else {
        cachedVBO = tree->data;
    }
    
    bool isNewVBO = (cachedVBO->VBO == 0);
    if (isNewVBO) {
        glGenVertexArrays(1, &cachedVBO->VAO);
        glGenBuffers(1, &cachedVBO->VBO);
        glGenBuffers(1, &cachedVBO->IBO);
        
        SetVertexArray(cachedVBO->VAO);
        SetArrayBuffer(cachedVBO->VBO);
        
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);
        
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cachedVBO->IBO);
    } else {
        SetVertexArray(cachedVBO->VAO);
        SetArrayBuffer(cachedVBO->VBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cachedVBO->IBO);
    }
    
    glBufferData(GL_ARRAY_BUFFER, m_megaTexturedVertexCount * sizeof(Vertex2D), m_megaTexturedVertices, GL_STATIC_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_megaTexturedIndexCount * sizeof(unsigned int), m_megaTexturedIndices, GL_STATIC_DRAW);

    cachedVBO->vertexCount = m_megaTexturedVertexCount;
    cachedVBO->indexCount = m_megaTexturedIndexCount;
    cachedVBO->isValid = true;

    cachedVBO->color.m_r = (m_currentMegaVBOTextureID >> 24) & 0xFF;
    cachedVBO->color.m_g = (m_currentMegaVBOTextureID >> 16) & 0xFF;
    cachedVBO->color.m_b = (m_currentMegaVBOTextureID >> 8) & 0xFF;
    cachedVBO->color.m_a = m_currentMegaVBOTextureID & 0xFF;

    m_megaTexturedVertexCount = 0;
    m_megaTexturedIndexCount = 0;
    m_megaVBOTexturedActive = false;
}

void Renderer::RenderTexturedMegaVBO(const char* megaVBOKey) {
    BTree<CachedVBO*>* tree = m_cachedVBOs.LookupTree(megaVBOKey);
    if (!tree || !tree->data || !tree->data->isValid) {
        return;
    }
    
    CachedVBO* cachedVBO = tree->data;
    
    //
    // Extract texture ID from color storage

    unsigned int textureID = ((unsigned int)cachedVBO->color.m_r << 24) |
                             ((unsigned int)cachedVBO->color.m_g << 16) |
                             ((unsigned int)cachedVBO->color.m_b << 8) |
                             ((unsigned int)cachedVBO->color.m_a);
    
    SetShaderProgram(m_textureShaderProgram);
    SetTextureShaderUniforms();
    
    if (textureID != 0) {
        SetActiveTexture(GL_TEXTURE0);
        SetBoundTexture(textureID);
    }
    
    SetVertexArray(cachedVBO->VAO);
    glDrawElements(GL_TRIANGLES, cachedVBO->indexCount, GL_UNSIGNED_INT, 0);
}

bool Renderer::IsTexturedMegaVBOValid(const char* megaVBOKey) {
    BTree<CachedVBO*>* tree = m_cachedVBOs.LookupTree(megaVBOKey);
    return (tree && tree->data && tree->data->isValid);
}

void Renderer::SetTexturedMegaVBOBufferSizes(int vertexCount, int indexCount) {
    int newMaxVertices = (int)(vertexCount * 1.1f);
    int newMaxIndices = (int)(indexCount * 1.1f);
    
    InvalidateCachedVBO("TexturedQuads");
    
    if (m_megaTexturedVertices) {
        delete[] m_megaTexturedVertices;
        m_megaTexturedVertices = NULL;
    }
    if (m_megaTexturedIndices) {
        delete[] m_megaTexturedIndices;
        m_megaTexturedIndices = NULL;
    }
    
    m_maxMegaTexturedVertices = newMaxVertices;
    m_maxMegaTexturedIndices = newMaxIndices;
    m_megaTexturedVertices = new Vertex2D[m_maxMegaTexturedVertices];
    m_megaTexturedIndices = new unsigned int[m_maxMegaTexturedIndices];
}
