#include "systemiv.h"

#include "lib/debug_utils.h"
#include "lib/string_utils.h"
#include "lib/render/renderer.h"
#include "lib/render2d/renderer_2d.h"
#include "lib/render2d/megavbo/megavbo_2d.h"

void MegaVBO2D::BeginTexturedMegaVBO(const char* megaVBOKey, unsigned int textureID) 
{
    CachedVBO* cachedVBO = m_cachedVBOs.GetData(megaVBOKey, nullptr);
    if (cachedVBO && cachedVBO->isValid) {
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

void MegaVBO2D::AddTexturedQuadsToMegaVBO(const Vertex2D* vertices, int vertexCount, int quadCount) {
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

void MegaVBO2D::EndTexturedMegaVBO() 
{
    if (!m_megaVBOTexturedActive || !m_currentMegaVBOTexturedKey) return;
    
    if (m_megaTexturedVertexCount < 4) {
        m_megaVBOTexturedActive = false;
        return;
    }
    
    //
    // Create or get cached Mega-VBO

    CachedVBO* cachedVBO = m_cachedVBOs.GetData(m_currentMegaVBOTexturedKey, nullptr);
    if (!cachedVBO) {
        cachedVBO = new CachedVBO();
        cachedVBO->VBO = 0;
        cachedVBO->VAO = 0;
        cachedVBO->IBO = 0;
        cachedVBO->vertexCount = 0;
        cachedVBO->indexCount = 0;
        cachedVBO->vertexType = VBO_VERTEX_2D;
        cachedVBO->isValid = false;
        m_cachedVBOs.PutData(m_currentMegaVBOTexturedKey, cachedVBO);
    }
    
    bool isNewVBO = (cachedVBO->VBO == 0);
    if (isNewVBO) {
        cachedVBO->VAO = g_renderer2d->CreateMegaVBOVertexArray();
        cachedVBO->VBO = g_renderer2d->CreateMegaVBOVertexBuffer(m_megaTexturedVertexCount * sizeof(Vertex2D), BUFFER_USAGE_STATIC_DRAW);
        cachedVBO->IBO = g_renderer2d->CreateMegaVBOIndexBuffer(m_megaTexturedIndexCount * sizeof(unsigned int), BUFFER_USAGE_STATIC_DRAW);
        
        g_renderer2d->SetupMegaVBOVertexAttributes2D(cachedVBO->VAO, cachedVBO->VBO, cachedVBO->IBO);
    } 
    else 
    {
        g_renderer->SetVertexArray(cachedVBO->VAO);
        g_renderer->SetArrayBuffer(cachedVBO->VBO);
    }
    
    g_renderer->SetVertexArray(cachedVBO->VAO);
    g_renderer2d->UploadMegaVBOVertexData(cachedVBO->VBO, m_megaTexturedVertices, m_megaTexturedVertexCount, BUFFER_USAGE_STATIC_DRAW);
    g_renderer2d->UploadMegaVBOIndexData(cachedVBO->IBO, m_megaTexturedIndices, m_megaTexturedIndexCount, BUFFER_USAGE_STATIC_DRAW);

    cachedVBO->vertexCount = m_megaTexturedVertexCount;
    cachedVBO->indexCount = m_megaTexturedIndexCount;
    cachedVBO->vertexType = VBO_VERTEX_2D;
    cachedVBO->isValid = true;

    cachedVBO->color.m_r = (m_currentMegaVBOTextureID >> 24) & 0xFF;
    cachedVBO->color.m_g = (m_currentMegaVBOTextureID >> 16) & 0xFF;
    cachedVBO->color.m_b = (m_currentMegaVBOTextureID >> 8) & 0xFF;
    cachedVBO->color.m_a = m_currentMegaVBOTextureID & 0xFF;

    m_megaTexturedVertexCount = 0;
    m_megaTexturedIndexCount = 0;
    m_megaVBOTexturedActive = false;
}

void MegaVBO2D::RenderTexturedMegaVBO(const char* megaVBOKey) 
{
    CachedVBO* cachedVBO = m_cachedVBOs.GetData(megaVBOKey, nullptr);
    if (!cachedVBO || !cachedVBO->isValid) {
        return;
    }
    
    g_renderer->StartFlushTiming("MegaVBO_Textured_2D");
    
    //
    // Extract texture ID from color storage

    unsigned int textureID = ((unsigned int)cachedVBO->color.m_r << 24) |
                             ((unsigned int)cachedVBO->color.m_g << 16) |
                             ((unsigned int)cachedVBO->color.m_b << 8) |
                             ((unsigned int)cachedVBO->color.m_a);
    
    g_renderer->SetShaderProgram(g_renderer2d->m_textureShaderProgram);
    g_renderer2d->SetTextureShaderUniforms();
    
    if (textureID != 0) {
        g_renderer->SetActiveTexture(0);
        g_renderer->SetBoundTexture(textureID);
    }
    
    g_renderer->SetVertexArray(cachedVBO->VAO);
    g_renderer2d->DrawMegaVBOIndexed(PRIMITIVE_TRIANGLES, cachedVBO->indexCount);
    
    g_renderer->EndFlushTiming("MegaVBO_Textured_2D");
    g_renderer2d->IncrementDrawCall("quad_vbo");
}

bool MegaVBO2D::IsTexturedMegaVBOValid(const char* megaVBOKey) 
{
    CachedVBO* cachedVBO = m_cachedVBOs.GetData(megaVBOKey, nullptr);
    return (cachedVBO && cachedVBO->isValid);
}

void MegaVBO2D::SetTexturedMegaVBOBufferSizes(int vertexCount, 
                                                  int indexCount, 
                                                  const char *cacheKey) 
{
    int newMaxVertices = (int)(vertexCount * 1.1f);
    int newMaxIndices = (int)(indexCount * 1.1f);
    
    if (cacheKey) {
        InvalidateCachedVBO(cacheKey);
    }
    
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
