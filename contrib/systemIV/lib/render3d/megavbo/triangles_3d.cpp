#include "systemiv.h"

#include "lib/debug_utils.h"
#include "lib/string_utils.h"
#include "lib/render/renderer.h"
#include "lib/render3d/renderer_3d.h"
#include "lib/render3d/megavbo/megavbo_3d.h"
#include "lib/math/matrix4f.h"

void MegaVBO3D::BeginTriangleMegaVBO3D(const char* megaVBOKey, Colour const &col) 
{
    Cached3DVBO* cachedVBO = m_cached3DVBOs.GetData(megaVBOKey, nullptr);
    if (cachedVBO && cachedVBO->isValid) {
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

void MegaVBO3D::AddTrianglesToMegaVBO3D(const Vector3<float>* vertices, int vertexCount) 
{
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

void MegaVBO3D::AddTrianglesToMegaVBO3DWithIndices(const Vector3<float>* vertices, 
                                                       int vertexCount, 
                                                       const unsigned int* indices, 
                                                       int indexCount) 
{
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

void MegaVBO3D::EndTriangleMegaVBO3D() 
{
    if (!m_megaVBO3DTrianglesActive || !m_currentMegaVBO3DTrianglesKey) return;
    
    if (m_megaTriangleVertex3DCount < 3) {
        m_megaVBO3DTrianglesActive = false;
        return;
    }
    
    //
    // Create or get cached Mega-VBO

    Cached3DVBO* cachedVBO = m_cached3DVBOs.GetData(m_currentMegaVBO3DTrianglesKey, nullptr);
    if (!cachedVBO) {
        cachedVBO = new Cached3DVBO();
        cachedVBO->VBO = 0;
        cachedVBO->VAO = 0;
        cachedVBO->IBO = 0;
        cachedVBO->vertexCount = 0;
        cachedVBO->indexCount = 0;
        cachedVBO->vertexType = VBO_VERTEX_3D;
        cachedVBO->isValid = false;
        m_cached3DVBOs.PutData(m_currentMegaVBO3DTrianglesKey, cachedVBO);
    }
    
    bool isNewVBO = (cachedVBO->VBO == 0);
    if (isNewVBO) {
        cachedVBO->VAO = g_renderer3d->CreateMegaVBOVertexArray3D();
        cachedVBO->VBO = g_renderer3d->CreateMegaVBOVertexBuffer3D(m_megaTriangleVertex3DCount * sizeof(Vertex3D), BUFFER_USAGE_STATIC_DRAW);
        cachedVBO->IBO = g_renderer3d->CreateMegaVBOIndexBuffer3D(m_megaTriangleIndex3DCount * sizeof(unsigned int), BUFFER_USAGE_STATIC_DRAW);
        
        g_renderer3d->SetupMegaVBOVertexAttributes3D(cachedVBO->VAO, cachedVBO->VBO, cachedVBO->IBO);
    } 
    else 
    {
        g_renderer->SetVertexArray(cachedVBO->VAO);
        g_renderer->SetArrayBuffer(cachedVBO->VBO);
    }
    
    g_renderer->SetVertexArray(cachedVBO->VAO);
    g_renderer3d->UploadMegaVBOVertexData3D(cachedVBO->VBO, m_megaTriangleVertices3D, m_megaTriangleVertex3DCount, BUFFER_USAGE_STATIC_DRAW);
    g_renderer3d->UploadMegaVBOIndexData3D(cachedVBO->IBO, m_megaTriangleIndices3D, m_megaTriangleIndex3DCount, BUFFER_USAGE_STATIC_DRAW);

    cachedVBO->vertexCount = m_megaTriangleVertex3DCount;
    cachedVBO->indexCount = m_megaTriangleIndex3DCount;
    cachedVBO->color = m_megaVBO3DTrianglesColor;
    cachedVBO->vertexType = VBO_VERTEX_3D;
    cachedVBO->isValid = true;
    
    m_megaTriangleVertex3DCount = 0;
    m_megaTriangleIndex3DCount = 0;
    m_megaVBO3DTrianglesActive = false;
}

void MegaVBO3D::RenderTriangleMegaVBO3D(const char* megaVBOKey) 
{
    Cached3DVBO* cachedVBO = m_cached3DVBOs.GetData(megaVBOKey, nullptr);
    if (!cachedVBO || !cachedVBO->isValid) {
        return;
    }
    
    g_renderer->StartFlushTiming("MegaVBO_Triangles_3D");
    
    g_renderer->SetShaderProgram(g_renderer3d->m_shader3DProgram);
    g_renderer3d->Set3DShaderUniforms();
    
    g_renderer->SetVertexArray(cachedVBO->VAO);
    g_renderer3d->DrawMegaVBOIndexed3D(PRIMITIVE_TRIANGLES, cachedVBO->indexCount);
    
    g_renderer->EndFlushTiming("MegaVBO_Triangles_3D");
    g_renderer3d->IncrementDrawCall3D("triangle_vbo");
}

void MegaVBO3D::RenderTriangleMegaVBO3DWithMatrix(const char* megaVBOKey, 
                                                      const Matrix4f& modelMatrix, 
                                                      const Colour& modelColor) 
{
    Cached3DVBO* cachedVBO = m_cached3DVBOs.GetData(megaVBOKey, nullptr);
    if (!cachedVBO || !cachedVBO->isValid) {
        return;
    }
    
    g_renderer->StartFlushTiming("MegaVBO_Triangles_Matrix_3D");
    
    g_renderer->SetShaderProgram(g_renderer3d->m_shader3DModelProgram);
    g_renderer3d->Set3DModelShaderUniforms(modelMatrix, modelColor);
    
    g_renderer->SetVertexArray(cachedVBO->VAO);
    g_renderer3d->DrawMegaVBOIndexed3D(PRIMITIVE_TRIANGLES, cachedVBO->indexCount);
    
    g_renderer->EndFlushTiming("MegaVBO_Triangles_Matrix_3D");
    g_renderer3d->IncrementDrawCall3D("triangle_vbo");
}

bool MegaVBO3D::IsTriangleMegaVBO3DValid(const char* megaVBOKey) 
{
    Cached3DVBO* cachedVBO = m_cached3DVBOs.GetData(megaVBOKey, nullptr);
    return (cachedVBO && cachedVBO->isValid);
}

bool MegaVBO3D::IsAny3DVBOValid(const char* cacheKey) 
{
    Cached3DVBO* cachedVBO = m_cached3DVBOs.GetData(cacheKey, nullptr);
    return (cachedVBO && cachedVBO->isValid);
}

void MegaVBO3D::SetTriangleMegaVBO3DBufferSizes(int vertexCount, 
                                                    int indexCount, 
                                                    const char *cacheKey) 
{
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