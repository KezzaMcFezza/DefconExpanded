#include "systemiv.h"

#include "lib/debug_utils.h"
#include "lib/string_utils.h"
#include "lib/render/renderer.h"
#include "lib/render2d/renderer_2d.h"
#include "lib/render2d/megavbo/megavbo_2d.h"

void MegaVBO2D::BeginMegaVBO(const char* megaVBOKey, Colour const &col) 
{
    CachedVBO* cachedVBO = m_cachedVBOs.GetData(megaVBOKey, nullptr);
    if (cachedVBO && cachedVBO->isValid) {
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

void MegaVBO2D::AddLineStripToMegaVBO(float* vertices, int vertexCount) 
{
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

void MegaVBO2D::EndMegaVBO() 
{
    if (!m_megaVBOActive || !m_currentMegaVBOKey) return;
    
    if (m_megaVertexCount < 2) {
        m_megaVBOActive = false;
        return;
    }
    
    //
    // Create or get cached Mega-VBO

    CachedVBO* cachedVBO = m_cachedVBOs.GetData(m_currentMegaVBOKey, nullptr);
    if (!cachedVBO) {
        cachedVBO = new CachedVBO();
        cachedVBO->VBO = 0;
        cachedVBO->VAO = 0;
        cachedVBO->IBO = 0;
        cachedVBO->vertexCount = 0;
        cachedVBO->indexCount = 0;
        cachedVBO->vertexType = VBO_VERTEX_2D;
        cachedVBO->isValid = false;
        m_cachedVBOs.PutData(m_currentMegaVBOKey, cachedVBO);
    }
    
    bool isNewVBO = (cachedVBO->VBO == 0);
    if (isNewVBO) {
        cachedVBO->VAO = g_renderer2d->CreateMegaVBOVertexArray();
        cachedVBO->VBO = g_renderer2d->CreateMegaVBOVertexBuffer(m_megaVertexCount * sizeof(Vertex2D), BUFFER_USAGE_STATIC_DRAW);
        cachedVBO->IBO = g_renderer2d->CreateMegaVBOIndexBuffer(m_megaIndexCount * sizeof(unsigned int), BUFFER_USAGE_STATIC_DRAW);
        
        g_renderer2d->SetupMegaVBOVertexAttributes2D(cachedVBO->VAO, cachedVBO->VBO, cachedVBO->IBO);
    } 
    else 
    {
        g_renderer->SetVertexArray(cachedVBO->VAO);
        g_renderer->SetArrayBuffer(cachedVBO->VBO);
    }
    
    //
    // Upload vertex data
    
    g_renderer->SetVertexArray(cachedVBO->VAO);
    g_renderer2d->UploadMegaVBOVertexData(cachedVBO->VBO, m_megaVertices, m_megaVertexCount, BUFFER_USAGE_STATIC_DRAW);
    g_renderer2d->UploadMegaVBOIndexData(cachedVBO->IBO, m_megaIndices, m_megaIndexCount, BUFFER_USAGE_STATIC_DRAW);
    
    cachedVBO->vertexCount = m_megaVertexCount;
    cachedVBO->indexCount = m_megaIndexCount;
    cachedVBO->color = m_megaVBOColor;
    cachedVBO->lineWidth = m_megaVBOWidth;
    cachedVBO->vertexType = VBO_VERTEX_2D;
    cachedVBO->isValid = true;
    
    m_megaVertexCount = 0;
    m_megaIndexCount = 0;
    m_megaVBOActive = false;
}

void MegaVBO2D::RenderMegaVBO(const char* megaVBOKey) 
{
    CachedVBO* cachedVBO = m_cachedVBOs.GetData(megaVBOKey, nullptr);
    if (!cachedVBO || !cachedVBO->isValid) {
        return;
    }

    g_renderer->StartFlushTiming("MegaVBO_2D");
    
    g_renderer->SetShaderProgram(g_renderer2d->m_colorShaderProgram);
    g_renderer2d->SetColorShaderUniforms();
    
    g_renderer2d->EnableMegaVBOPrimitiveRestart(0xFFFFFFFF);
    
    //
    // Render the mega-VBO with indexed drawing

    g_renderer->SetVertexArray(cachedVBO->VAO);
    g_renderer2d->DrawMegaVBOIndexed(PRIMITIVE_LINE_STRIP, cachedVBO->indexCount);
    
    g_renderer2d->DisableMegaVBOPrimitiveRestart();
    
    g_renderer->EndFlushTiming("MegaVBO_2D");
    g_renderer2d->IncrementDrawCall("line_vbo");
}

bool MegaVBO2D::IsMegaVBOValid(const char* megaVBOKey) {
    CachedVBO* cachedVBO = m_cachedVBOs.GetData(megaVBOKey, nullptr);
    return (cachedVBO && cachedVBO->isValid);
}

void MegaVBO2D::SetMegaVBOBufferSizes(int vertexCount, int indexCount, const char *cacheKey) 
{

    //
    // Calculate new sizes with 10% safety margin

    int newMaxVertices = (int)(vertexCount * 1.1f);
    int newMaxIndices = (int)(indexCount * 1.1f);

    if (cacheKey) {
        InvalidateCachedVBO(cacheKey);
    }
    
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
