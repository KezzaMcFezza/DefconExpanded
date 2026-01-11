#include "systemiv.h"

#include "lib/debug/debug_utils.h"
#include "lib/string_utils.h"
#include "lib/render/renderer.h"
#include "lib/render3d/renderer_3d.h"
#include "lib/render3d/megavbo/megavbo_3d.h"

void MegaVBO3D::BeginMegaVBO3D(const char* megaVBOKey, Colour const &col) 
{
    Cached3DVBO* cachedVBO = m_cached3DVBOs.GetData(megaVBOKey, nullptr);
    if (cachedVBO && cachedVBO->isValid) {
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

void MegaVBO3D::AddLineStripToMegaVBO3D(const Vector3<float>* vertices, int vertexCount) 
{
    if (!m_megaVBO3DActive || vertexCount < 2) return;

    if (m_megaVertex3DCount + vertexCount > m_maxMegaVertices3D ||
        m_megaIndex3DCount + vertexCount + 1 > m_maxMegaIndices3D) {
        AppDebugOut("Warning: 3D MegaVBO overflow: Vertices: %d/%d, Indices: %d/%d\n",
                    m_megaVertex3DCount + vertexCount, m_maxMegaVertices3D,
                    m_megaIndex3DCount + vertexCount + 1, m_maxMegaIndices3D);
        return;
    }
    
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

void MegaVBO3D::EndMegaVBO3D() 
{
    if (!m_megaVBO3DActive || !m_currentMegaVBO3DKey) return;
    
    if (m_megaVertex3DCount < 2) {
        m_megaVBO3DActive = false;
        return;
    }
    
    //
    // Create or get cached Mega-VBO

    Cached3DVBO* cachedVBO = m_cached3DVBOs.GetData(m_currentMegaVBO3DKey, nullptr);
    if (!cachedVBO) {
        cachedVBO = new Cached3DVBO();
        cachedVBO->VBO = 0;
        cachedVBO->VAO = 0;
        cachedVBO->IBO = 0;
        cachedVBO->vertexCount = 0;
        cachedVBO->indexCount = 0;
        cachedVBO->vertexType = VBO_VERTEX_3D;
        cachedVBO->isValid = false;
        m_cached3DVBOs.PutData(m_currentMegaVBO3DKey, cachedVBO);
    }
    
    bool isNewVBO = (cachedVBO->VBO == 0);
    if (isNewVBO) {
        cachedVBO->VAO = g_renderer3d->CreateMegaVBOVertexArray3D();
        cachedVBO->VBO = g_renderer3d->CreateMegaVBOVertexBuffer3D(m_megaVertex3DCount * sizeof(Vertex3D), BUFFER_USAGE_STATIC_DRAW);
        cachedVBO->IBO = g_renderer3d->CreateMegaVBOIndexBuffer3D(m_megaIndex3DCount * sizeof(unsigned int), BUFFER_USAGE_STATIC_DRAW);
        
        g_renderer3d->SetupMegaVBOVertexAttributes3D(cachedVBO->VAO, cachedVBO->VBO, cachedVBO->IBO);
    } 
    else 
    {
        g_renderer->SetVertexArray(cachedVBO->VAO);
        g_renderer->SetArrayBuffer(cachedVBO->VBO);
    }
    
    g_renderer->SetVertexArray(cachedVBO->VAO);
    g_renderer3d->UploadMegaVBOVertexData3D(cachedVBO->VBO, m_megaVertices3D, m_megaVertex3DCount, BUFFER_USAGE_STATIC_DRAW);
    g_renderer3d->UploadMegaVBOIndexData3D(cachedVBO->IBO, m_megaIndices3D, m_megaIndex3DCount, BUFFER_USAGE_STATIC_DRAW);

    cachedVBO->vertexCount = m_megaVertex3DCount;
    cachedVBO->indexCount = m_megaIndex3DCount;
    cachedVBO->color = m_megaVBO3DColor;
    cachedVBO->vertexType = VBO_VERTEX_3D;
    cachedVBO->isValid = true;
    
    m_megaVertex3DCount = 0;
    m_megaIndex3DCount = 0;
    m_megaVBO3DActive = false;
}

void MegaVBO3D::RenderMegaVBO3D(const char* megaVBOKey) 
{
    Cached3DVBO* cachedVBO = m_cached3DVBOs.GetData(megaVBOKey, nullptr);
    if (!cachedVBO || !cachedVBO->isValid) {
        return;
    }
    
    g_renderer->StartFlushTiming("MegaVBO_3D");
    
    g_renderer->SetShaderProgram(g_renderer3d->m_shader3DProgram);
    g_renderer3d->Set3DShaderUniforms();
    
    g_renderer3d->EnableMegaVBOPrimitiveRestart3D(0xFFFFFFFF);
    
    //
    // Render the mega-VBO with indexed drawing

    g_renderer->SetVertexArray(cachedVBO->VAO);
    g_renderer3d->DrawMegaVBOIndexed3D(PRIMITIVE_LINE_STRIP, cachedVBO->indexCount);
    
    g_renderer3d->DisableMegaVBOPrimitiveRestart3D();
    
    g_renderer->EndFlushTiming("MegaVBO_3D");
    g_renderer3d->IncrementDrawCall3D("line_vbo");
}

bool MegaVBO3D::IsMegaVBO3DValid(const char* megaVBOKey) 
{
    Cached3DVBO* cachedVBO = m_cached3DVBOs.GetData(megaVBOKey, nullptr);
    return (cachedVBO && cachedVBO->isValid);
}

void MegaVBO3D::SetMegaVBO3DBufferSizes(int vertexCount, int indexCount, const char *cacheKey) 
{

    //
    // Calculate new sizes with 10% safety margin

    int newMaxVertices = (int)(vertexCount * 1.1f);
    int newMaxIndices = (int)(indexCount * 1.1f);

    if (cacheKey) {
        InvalidateCached3DVBO(cacheKey);
    }
    
    //
    // free existing CPU buffers

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