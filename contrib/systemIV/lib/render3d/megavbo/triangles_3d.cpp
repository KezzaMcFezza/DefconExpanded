#include "systemiv.h"

#include "lib/debug_utils.h"
#include "lib/string_utils.h"
#include "lib/render/renderer.h"
#include "lib/render3d/renderer_3d.h"
#include "lib/render3d/megavbo/megavbo_3d.h"
#include "lib/math/matrix4f.h"

void Renderer3DVBO::BeginTriangleMegaVBO3D(const char* megaVBOKey, Colour const &col) {
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

void Renderer3DVBO::AddTrianglesToMegaVBO3D(const Vector3<float>* vertices, int vertexCount) {
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

void Renderer3DVBO::AddTrianglesToMegaVBO3DWithIndices(const Vector3<float>* vertices, int vertexCount, const unsigned int* indices, int indexCount) {
    if (!m_megaVBO3DTrianglesActive || vertexCount < 3 || indexCount < 3) return;

    if (m_megaTriangleVertex3DCount + vertexCount > m_maxMegaTriangleVertices3D ||
        m_megaTriangleIndex3DCount + indexCount > m_maxMegaTriangleIndices3D) {
        AppDebugOut("Warning: 3D Triangle MegaVBO overflow: Vertices: %d/%d, Indices: %d/%d\n",
                    m_megaTriangleVertex3DCount + vertexCount, m_maxMegaTriangleVertices3D,
                    m_megaTriangleIndex3DCount + indexCount, m_maxMegaTriangleIndices3D);
        return;
    }

    float r = m_megaVBO3DTrianglesColor.GetRFloat(), g = m_megaVBO3DTrianglesColor.GetGFloat();
    float b = m_megaVBO3DTrianglesColor.GetBFloat(), a = m_megaVBO3DTrianglesColor.GetAFloat();

    unsigned int startIndex = m_megaTriangleVertex3DCount;
    
    //
    // Add vertices but only unique vertices, indices will reference them

    for (int i = 0; i < vertexCount; i++) {
        m_megaTriangleVertices3D[m_megaTriangleVertex3DCount] = Vertex3D(
            vertices[i].x, vertices[i].y, vertices[i].z, r, g, b, a
        );
        m_megaTriangleVertex3DCount++;
    }

    for (int i = 0; i < indexCount; i++) {
        m_megaTriangleIndices3D[m_megaTriangleIndex3DCount++] = startIndex + indices[i];
    }
}

void Renderer3DVBO::EndTriangleMegaVBO3D() {
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
        cachedVBO->vertexType = VBO_VERTEX_3D;
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
        
        g_renderer->SetVertexArray(cachedVBO->VAO);
        g_renderer->SetArrayBuffer(cachedVBO->VBO);
        
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cachedVBO->IBO);
    } else {
        g_renderer->SetVertexArray(cachedVBO->VAO);
        g_renderer->SetArrayBuffer(cachedVBO->VBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cachedVBO->IBO);
    }
    
    glBufferData(GL_ARRAY_BUFFER, m_megaTriangleVertex3DCount * sizeof(Vertex3D), m_megaTriangleVertices3D, GL_STATIC_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_megaTriangleIndex3DCount * sizeof(unsigned int), m_megaTriangleIndices3D, GL_STATIC_DRAW);

    cachedVBO->vertexCount = m_megaTriangleVertex3DCount;
    cachedVBO->indexCount = m_megaTriangleIndex3DCount;
    cachedVBO->color = m_megaVBO3DTrianglesColor;
    cachedVBO->vertexType = VBO_VERTEX_3D;
    cachedVBO->isValid = true;
    
    m_megaTriangleVertex3DCount = 0;
    m_megaTriangleIndex3DCount = 0;
    m_megaVBO3DTrianglesActive = false;
}

void Renderer3DVBO::RenderTriangleMegaVBO3D(const char* megaVBOKey) {
    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(megaVBOKey);
    if (!tree || !tree->data || !tree->data->isValid) {
        return;
    }
    
    Cached3DVBO* cachedVBO = tree->data;
    
    g_renderer->StartFlushTiming("MegaVBO_Triangles_3D");
    
    g_renderer->SetShaderProgram(g_renderer3d->m_shader3DProgram);
    g_renderer3d->Set3DShaderUniforms();
    
    g_renderer->SetVertexArray(cachedVBO->VAO);
    glDrawElements(GL_TRIANGLES, cachedVBO->indexCount, GL_UNSIGNED_INT, 0);
    
    g_renderer->EndFlushTiming("MegaVBO_Triangles_3D");
    g_renderer3d->IncrementDrawCall3D("triangle_vbo");
}

void Renderer3DVBO::RenderTriangleMegaVBO3DWithMatrix(const char* megaVBOKey, const Matrix4f& modelMatrix, const Colour& modelColor) {
    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(megaVBOKey);
    if (!tree || !tree->data || !tree->data->isValid) {
        return;
    }
    
    Cached3DVBO* cachedVBO = tree->data;
    
    g_renderer->StartFlushTiming("MegaVBO_Triangles_Matrix_3D");
    
    g_renderer->SetShaderProgram(g_renderer3d->m_shader3DModelProgram);
    g_renderer3d->Set3DModelShaderUniforms(modelMatrix, modelColor);
    
    g_renderer->SetVertexArray(cachedVBO->VAO);
    glDrawElements(GL_TRIANGLES, cachedVBO->indexCount, GL_UNSIGNED_INT, 0);
    
    g_renderer->EndFlushTiming("MegaVBO_Triangles_Matrix_3D");
    g_renderer3d->IncrementDrawCall3D("triangle_vbo");
}

bool Renderer3DVBO::IsTriangleMegaVBO3DValid(const char* megaVBOKey) {
    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(megaVBOKey);
    return (tree && tree->data && tree->data->isValid);
}

void Renderer3DVBO::SetTriangleMegaVBO3DBufferSizes(int vertexCount, int indexCount, const char *cacheKey) {
    int newMaxVertices = (int)(vertexCount * 1.1f);
    int newMaxIndices = (int)(indexCount * 1.1f);
    
    if (cacheKey) {
        InvalidateCached3DVBO(cacheKey);
    }
    
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