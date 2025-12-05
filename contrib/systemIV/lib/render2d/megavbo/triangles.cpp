#include "lib/universal_include.h"

#include "lib/debug_utils.h"
#include "lib/string_utils.h"
#include "lib/render2d/renderer.h"

void Renderer::BeginTriangleMegaVBO(const char* megaVBOKey, Colour const &col) {
    BTree<CachedVBO*>* tree = m_cachedVBOs.LookupTree(megaVBOKey);
    if (tree && tree->data && tree->data->isValid) {
        return;
    }

    if (m_currentMegaVBOTrianglesKey) {
        delete[] m_currentMegaVBOTrianglesKey;
    }
    m_currentMegaVBOTrianglesKey = newStr(megaVBOKey);
    
    m_megaVBOTrianglesActive = true;
    m_megaVBOTrianglesColor = col;
    
    m_megaTriangleVertexCount = 0;
    m_megaTriangleIndexCount = 0;
}

void Renderer::AddTrianglesToMegaVBO(const float* vertices, int vertexCount) {
    if (!m_megaVBOTrianglesActive || vertexCount < 3) return;

    if (m_megaTriangleVertexCount + vertexCount > m_maxMegaTriangleVertices ||
        m_megaTriangleIndexCount + vertexCount > m_maxMegaTriangleIndices) {
        AppDebugOut("Warning: 2D Triangle MegaVBO overflow: Vertices: %d/%d, Indices: %d/%d\n",
                    m_megaTriangleVertexCount + vertexCount, m_maxMegaTriangleVertices,
                    m_megaTriangleIndexCount + vertexCount, m_maxMegaTriangleIndices);
        return;
    }
    
    //
    // Convert color to float

    float r = m_megaVBOTrianglesColor.m_r / 255.0f, g = m_megaVBOTrianglesColor.m_g / 255.0f;
    float b = m_megaVBOTrianglesColor.m_b / 255.0f, a = m_megaVBOTrianglesColor.m_a / 255.0f;
    
    //
    // Store starting vertex index for these triangles

    unsigned int startIndex = m_megaTriangleVertexCount;
    
    //
    // Add vertices

    for (int i = 0; i < vertexCount; i++) {
        m_megaTriangleVertices[m_megaTriangleVertexCount++] = {
            vertices[i * 2], vertices[i * 2 + 1],
            r, g, b, a,
            0.0f, 0.0f
        };
    }
    
    //
    // Add indices (3 per triangle)

    for (int i = 0; i < vertexCount; i++) {
        m_megaTriangleIndices[m_megaTriangleIndexCount++] = startIndex + i;
    }
}

void Renderer::EndTriangleMegaVBO() {
    if (!m_megaVBOTrianglesActive || !m_currentMegaVBOTrianglesKey) return;
    
    if (m_megaTriangleVertexCount < 3) {
        m_megaVBOTrianglesActive = false;
        return;
    }
    
    //
    // Create or get cached Mega-VBO

    BTree<CachedVBO*>* tree = m_cachedVBOs.LookupTree(m_currentMegaVBOTrianglesKey);
    CachedVBO* cachedVBO = NULL;
    if (!tree || !tree->data) {
        cachedVBO = new CachedVBO();
        cachedVBO->VBO = 0;
        cachedVBO->VAO = 0;
        cachedVBO->IBO = 0;
        cachedVBO->vertexCount = 0;
        cachedVBO->indexCount = 0;
        cachedVBO->isValid = false;
        m_cachedVBOs.PutData(m_currentMegaVBOTrianglesKey, cachedVBO);
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
    
    glBufferData(GL_ARRAY_BUFFER, m_megaTriangleVertexCount * sizeof(Vertex2D), m_megaTriangleVertices, GL_STATIC_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_megaTriangleIndexCount * sizeof(unsigned int), m_megaTriangleIndices, GL_STATIC_DRAW);

    cachedVBO->vertexCount = m_megaTriangleVertexCount;
    cachedVBO->indexCount = m_megaTriangleIndexCount;
    cachedVBO->color = m_megaVBOTrianglesColor;
    cachedVBO->isValid = true;
    
    m_megaTriangleVertexCount = 0;
    m_megaTriangleIndexCount = 0;
    m_megaVBOTrianglesActive = false;
}

void Renderer::RenderTriangleMegaVBO(const char* megaVBOKey) {
    BTree<CachedVBO*>* tree = m_cachedVBOs.LookupTree(megaVBOKey);
    if (!tree || !tree->data || !tree->data->isValid) {
        return;
    }
    
    CachedVBO* cachedVBO = tree->data;
    
    SetShaderProgram(m_colorShaderProgram);
    SetColorShaderUniforms();
    
    SetVertexArray(cachedVBO->VAO);
    glDrawElements(GL_TRIANGLES, cachedVBO->indexCount, GL_UNSIGNED_INT, 0);
}

bool Renderer::IsTriangleMegaVBOValid(const char* megaVBOKey) {
    BTree<CachedVBO*>* tree = m_cachedVBOs.LookupTree(megaVBOKey);
    return (tree && tree->data && tree->data->isValid);
}

void Renderer::SetTriangleMegaVBOBufferSizes(int vertexCount, int indexCount) {
    int newMaxVertices = (int)(vertexCount * 1.1f);
    int newMaxIndices = (int)(indexCount * 1.1f);
    
    InvalidateCachedVBO("Triangles");
    
    if (m_megaTriangleVertices) {
        delete[] m_megaTriangleVertices;
        m_megaTriangleVertices = NULL;
    }
    if (m_megaTriangleIndices) {
        delete[] m_megaTriangleIndices;
        m_megaTriangleIndices = NULL;
    }
    
    m_maxMegaTriangleVertices = newMaxVertices;
    m_maxMegaTriangleIndices = newMaxIndices;
    m_megaTriangleVertices = new Vertex2D[m_maxMegaTriangleVertices];
    m_megaTriangleIndices = new unsigned int[m_maxMegaTriangleIndices];
}
