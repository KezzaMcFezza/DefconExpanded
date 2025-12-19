#include "systemiv.h"

#include "lib/debug_utils.h"
#include "lib/string_utils.h"
#include "lib/render/renderer.h"
#include "lib/render3d/renderer_3d.h"
#include "lib/render3d/megavbo/megavbo_3d.h"

void Renderer3DVBO::BeginMegaVBO3D(const char* megaVBOKey, Colour const &col) {
    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(megaVBOKey);
    if (tree && tree->data && tree->data->isValid) {
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

void Renderer3DVBO::AddLineStripToMegaVBO3D(const Vector3<float>* vertices, int vertexCount) {
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

void Renderer3DVBO::EndMegaVBO3D() {
    if (!m_megaVBO3DActive || !m_currentMegaVBO3DKey) return;
    
    if (m_megaVertex3DCount < 2) {
        m_megaVBO3DActive = false;
        return;
    }
    
    //
    // Create or get cached Mega-VBO

    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(m_currentMegaVBO3DKey);
    Cached3DVBO* cachedVBO = NULL;
    if (!tree || !tree->data) {
        cachedVBO = new Cached3DVBO();
        cachedVBO->VBO = 0;
        cachedVBO->VAO = 0;
        cachedVBO->IBO = 0;
        cachedVBO->vertexCount = 0;
        cachedVBO->indexCount = 0;
        cachedVBO->isValid = false;
        m_cached3DVBOs.PutData(m_currentMegaVBO3DKey, cachedVBO);
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
        
        //
        // Bind index buffer to VAO
        
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cachedVBO->IBO);
    } else {
        g_renderer->SetVertexArray(cachedVBO->VAO);
        g_renderer->SetArrayBuffer(cachedVBO->VBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cachedVBO->IBO);
    }
    
    glBufferData(GL_ARRAY_BUFFER, m_megaVertex3DCount * sizeof(Vertex3D), m_megaVertices3D, GL_STATIC_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_megaIndex3DCount * sizeof(unsigned int), m_megaIndices3D, GL_STATIC_DRAW);

    cachedVBO->vertexCount = m_megaVertex3DCount;
    cachedVBO->indexCount = m_megaIndex3DCount;
    cachedVBO->color = m_megaVBO3DColor;
    cachedVBO->isValid = true;
    
    m_megaVertex3DCount = 0;
    m_megaIndex3DCount = 0;
    m_megaVBO3DActive = false;
}

void Renderer3DVBO::RenderMegaVBO3D(const char* megaVBOKey) {
    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(megaVBOKey);
    if (!tree || !tree->data || !tree->data->isValid) {
        return;
    }
    
    Cached3DVBO* cachedVBO = tree->data;
    
    g_renderer->StartFlushTiming("MegaVBO_3D");
    
    g_renderer->SetShaderProgram(g_renderer3d->m_shader3DProgram);
    g_renderer3d->Set3DShaderUniforms();
    
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
    
    g_renderer->EndFlushTiming("MegaVBO_3D");
    g_renderer3d->IncrementDrawCall3D("line_vbo");
}

bool Renderer3DVBO::IsMegaVBO3DValid(const char* megaVBOKey) {
    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(megaVBOKey);
    return (tree && tree->data && tree->data->isValid);
}

void Renderer3DVBO::SetMegaVBO3DBufferSizes(int vertexCount, int indexCount) {

    //
    // Calculate new sizes with 10% safety margin

    int newMaxVertices = (int)(vertexCount * 1.1f);
    int newMaxIndices = (int)(indexCount * 1.1f);

    InvalidateCached3DVBO("GlobeCoastlines");
    InvalidateCached3DVBO("GlobeBorders");
    InvalidateCached3DVBO("GlobeGridlines");
    
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