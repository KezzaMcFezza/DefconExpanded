#include "lib/universal_include.h"

#include "lib/string_utils.h"
#include "lib/render2d/renderer.h"

#include "lib/render3d/renderer_3d.h"

void Renderer3D::BeginTriangleMegaVBO3D(const char* megaVBOKey, Colour const &col) {
    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(megaVBOKey);
    if (tree && tree->data && tree->data->isValid) {
        return;
    }

    if (m_currentMegaVBO3DTrianglesKey) {
        delete[] m_currentMegaVBO3DTrianglesKey;
    }
    m_currentMegaVBO3DTrianglesKey = newStr(megaVBOKey);
    
    m_megaVBO3DTrianglesActive = true;
    m_megaVBO3DTrianglesColor = col;
    
    m_megaTriangleVertex3DCount = 0;
    m_megaTriangleIndex3DCount = 0;
}

void Renderer3D::AddTrianglesToMegaVBO3D(const Vector3<float>* vertices, int vertexCount) {
    if (!m_megaVBO3DTrianglesActive || vertexCount < 3) return;

    if (m_megaTriangleVertex3DCount + vertexCount > m_maxMegaTriangleVertices3D ||
        m_megaTriangleIndex3DCount + vertexCount > m_maxMegaTriangleIndices3D) {
        AppDebugOut("Warning: 3D Triangle MegaVBO overflow: Vertices: %d/%d, Indices: %d/%d\n",
                    m_megaTriangleVertex3DCount + vertexCount, m_maxMegaTriangleVertices3D,
                    m_megaTriangleIndex3DCount + vertexCount, m_maxMegaTriangleIndices3D);
        return;
    }
    
    //
    // Convert color to float

    float r = m_megaVBO3DTrianglesColor.GetRFloat(), g = m_megaVBO3DTrianglesColor.GetGFloat();
    float b = m_megaVBO3DTrianglesColor.GetBFloat(), a = m_megaVBO3DTrianglesColor.GetAFloat();
    
    //
    // Store starting vertex index for these triangles

    unsigned int startIndex = m_megaTriangleVertex3DCount;
    
    //
    // Add vertices

    for (int i = 0; i < vertexCount; i++) {
        m_megaTriangleVertices3D[m_megaTriangleVertex3DCount] = Vertex3D(
            vertices[i].x, vertices[i].y, vertices[i].z, r, g, b, a
        );
        m_megaTriangleVertex3DCount++;
    }
    
    //
    // Add indices (3 per triangle)

    for (int i = 0; i < vertexCount; i++) {
        m_megaTriangleIndices3D[m_megaTriangleIndex3DCount++] = startIndex + i;
    }
}

void Renderer3D::EndTriangleMegaVBO3D() {
    if (!m_megaVBO3DTrianglesActive || !m_currentMegaVBO3DTrianglesKey) return;
    
    if (m_megaTriangleVertex3DCount < 3) {
        m_megaVBO3DTrianglesActive = false;
        return;
    }
    
    //
    // Create or get cached Mega-VBO

    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(m_currentMegaVBO3DTrianglesKey);
    Cached3DVBO* cachedVBO = NULL;
    if (!tree || !tree->data) {
        cachedVBO = new Cached3DVBO();
        cachedVBO->VBO = 0;
        cachedVBO->VAO = 0;
        cachedVBO->IBO = 0;
        cachedVBO->vertexCount = 0;
        cachedVBO->indexCount = 0;
        cachedVBO->isValid = false;
        m_cached3DVBOs.PutData(m_currentMegaVBO3DTrianglesKey, cachedVBO);
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
    
    glBufferData(GL_ARRAY_BUFFER, m_megaTriangleVertex3DCount * sizeof(Vertex3D), m_megaTriangleVertices3D, GL_STATIC_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_megaTriangleIndex3DCount * sizeof(unsigned int), m_megaTriangleIndices3D, GL_STATIC_DRAW);

    cachedVBO->vertexCount = m_megaTriangleVertex3DCount;
    cachedVBO->indexCount = m_megaTriangleIndex3DCount;
    cachedVBO->color = m_megaVBO3DTrianglesColor;
    cachedVBO->isValid = true;
    
    m_megaTriangleVertex3DCount = 0;
    m_megaTriangleIndex3DCount = 0;
    m_megaVBO3DTrianglesActive = false;
}

void Renderer3D::RenderTriangleMegaVBO3D(const char* megaVBOKey) {
    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(megaVBOKey);
    if (!tree || !tree->data || !tree->data->isValid) {
        return;
    }
    
    IncrementDrawCall3D("mega_vbo");
    
    Cached3DVBO* cachedVBO = tree->data;
    
    m_renderer->SetShaderProgram(m_shader3DProgram);
    Set3DShaderUniforms();
    
    m_renderer->SetVertexArray(cachedVBO->VAO);
    glDrawElements(GL_TRIANGLES, cachedVBO->indexCount, GL_UNSIGNED_INT, 0);
}

bool Renderer3D::IsTriangleMegaVBO3DValid(const char* megaVBOKey) {
    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(megaVBOKey);
    return (tree && tree->data && tree->data->isValid);
}

void Renderer3D::SetTriangleMegaVBO3DBufferSizes(int vertexCount, int indexCount) {
    int newMaxVertices = (int)(vertexCount * 1.1f);
    int newMaxIndices = (int)(indexCount * 1.1f);
    
    InvalidateCached3DVBO("CullingSphere");
    
    if (m_megaTriangleVertices3D) {
        delete[] m_megaTriangleVertices3D;
        m_megaTriangleVertices3D = NULL;
    }
    if (m_megaTriangleIndices3D) {
        delete[] m_megaTriangleIndices3D;
        m_megaTriangleIndices3D = NULL;
    }
    
    m_maxMegaTriangleVertices3D = newMaxVertices;
    m_maxMegaTriangleIndices3D = newMaxIndices;
    m_megaTriangleVertices3D = new Vertex3D[m_maxMegaTriangleVertices3D];
    m_megaTriangleIndices3D = new unsigned int[m_maxMegaTriangleIndices3D];
}
