#include "systemiv.h"

#include <algorithm>

#include "lib/render/renderer.h"
#include "lib/render2d/renderer_2d.h"
#include "lib/render2d/megavbo/megavbo_2d.h"
#include "lib/string_utils.h"

Renderer2DVBO *g_renderer2dvbo = NULL;

Renderer2DVBO::Renderer2DVBO()
:   m_maxMegaVertices(100),
    m_maxMegaIndices(100),
    m_megaVBOActive(false),
    m_currentMegaVBOKey(NULL),
    m_megaVBOColor(0, 0, 0, 0),
    m_megaVBOWidth(1.0f),
    m_megaVertices(NULL),
    m_megaVertexCount(0),
    m_megaIndices(NULL),
    m_megaIndexCount(0),
    m_megaVBOTexturedActive(false),
    m_currentMegaVBOTexturedKey(NULL),
    m_currentMegaVBOTextureID(0),
    m_maxMegaTexturedVertices(100),
    m_maxMegaTexturedIndices(100),
    m_megaTexturedVertices(NULL),
    m_megaTexturedVertexCount(0),
    m_megaTexturedIndices(NULL),
    m_megaTexturedIndexCount(0),
    m_megaVBOTrianglesActive(false),
    m_currentMegaVBOTrianglesKey(NULL),
    m_megaVBOTrianglesColor(0, 0, 0, 0),
    m_maxMegaTriangleVertices(100),
    m_maxMegaTriangleIndices(100),
    m_megaTriangleVertices(NULL),
    m_megaTriangleVertexCount(0),
    m_megaTriangleIndices(NULL),
    m_megaTriangleIndexCount(0),
    m_cachedVBOs(),
    m_protectedVBOKeys()
{
    m_megaVertices = new Vertex2D[m_maxMegaVertices];
    m_megaIndices = new unsigned int[m_maxMegaIndices];
    
    m_megaTexturedVertices = new Vertex2D[m_maxMegaTexturedVertices];
    m_megaTexturedIndices = new unsigned int[m_maxMegaTexturedIndices];
    
    m_megaTriangleVertices = new Vertex2D[m_maxMegaTriangleVertices];
    m_megaTriangleIndices = new unsigned int[m_maxMegaTriangleIndices];
}

Renderer2DVBO::~Renderer2DVBO() 
{
    InvalidateAllVBOs();
    
    if (m_megaVertices) {
        delete[] m_megaVertices;
        m_megaVertices = NULL;
    }
    
    if (m_megaIndices) {
        delete[] m_megaIndices;
        m_megaIndices = NULL;
    }
    
    if (m_megaTexturedVertices) {
        delete[] m_megaTexturedVertices;
        m_megaTexturedVertices = NULL;
    }
    
    if (m_megaTexturedIndices) {
        delete[] m_megaTexturedIndices;
        m_megaTexturedIndices = NULL;
    }
    
    if (m_megaTriangleVertices) {
        delete[] m_megaTriangleVertices;
        m_megaTriangleVertices = NULL;
    }
    
    if (m_megaTriangleIndices) {
        delete[] m_megaTriangleIndices;
        m_megaTriangleIndices = NULL;
    }
    
    if (m_currentMegaVBOKey) {
        delete[] m_currentMegaVBOKey;
        m_currentMegaVBOKey = NULL;
    }
    
    if (m_currentMegaVBOTexturedKey) {
        delete[] m_currentMegaVBOTexturedKey;
        m_currentMegaVBOTexturedKey = NULL;
    }
    
    if (m_currentMegaVBOTrianglesKey) {
        delete[] m_currentMegaVBOTrianglesKey;
        m_currentMegaVBOTrianglesKey = NULL;
    }
    
    ClearVBOProtection();
}


void Renderer2DVBO::InvalidateAllVBOs() {
    
    DArray<char*> *keys = m_cachedVBOs.ConvertIndexToDArray();
    
    //
    // Delete each VBO, but skip protected ones

    for (int i = 0; i < keys->Size(); ++i) {
        if (!IsVBOProtected(keys->GetData(i))) {
            InvalidateCachedVBO(keys->GetData(i));
        }
    }
    
    delete keys;
}

void Renderer2DVBO::ProtectVBO(const char* key) {
    if (!key) return;
    if (IsVBOProtected(key)) return;
    m_protectedVBOKeys.PutData(newStr(key));
}

void Renderer2DVBO::UnprotectVBO(const char* key) {
    if (!key) return;
    for (int i = 0; i < m_protectedVBOKeys.Size(); ++i) {
        if (strcmp(m_protectedVBOKeys[i], key) == 0) {
            delete[] m_protectedVBOKeys[i];
            m_protectedVBOKeys.RemoveData(i);
            return;
        }
    }
}

void Renderer2DVBO::ClearVBOProtection() {
    for (int i = 0; i < m_protectedVBOKeys.Size(); ++i) {
        delete[] m_protectedVBOKeys[i];
    }
    m_protectedVBOKeys.Empty();
}

bool Renderer2DVBO::IsVBOProtected(const char* key) {
    if (!key) return false;
    for (int i = 0; i < m_protectedVBOKeys.Size(); ++i) {
        if (strcmp(m_protectedVBOKeys[i], key) == 0) {
            return true;
        }
    }
    return false;
}

void Renderer2DVBO::InvalidateCachedVBO(const char* cacheKey) {
    BTree<CachedVBO*>* tree = m_cachedVBOs.LookupTree(cacheKey);
    if (tree && tree->data) {
        CachedVBO* cachedVBO = tree->data;

        if (cachedVBO->VBO != 0) {
            glDeleteBuffers(1, &cachedVBO->VBO);
            cachedVBO->VBO = 0;
        }
        if (cachedVBO->IBO != 0) {
            glDeleteBuffers(1, &cachedVBO->IBO);
            cachedVBO->IBO = 0;
        }
        if (cachedVBO->VAO != 0) {
            glDeleteVertexArrays(1, &cachedVBO->VAO);
            cachedVBO->VAO = 0;
        }

        delete cachedVBO;
        m_cachedVBOs.RemoveData(cacheKey);
    }
}

int Renderer2DVBO::GetCachedVBOCount() {
    int count = 0;
    
    //
    // Iterate through cached VBOs and count valid ones
    
    DArray<char*> *keys = m_cachedVBOs.ConvertIndexToDArray();
    
    for (int i = 0; i < keys->Size(); ++i) {
        BTree<CachedVBO*>* tree = m_cachedVBOs.LookupTree(keys->GetData(i));
        if (tree && tree->data && tree->data->isValid) {
            count++;
        }
    }
    
    delete keys;
    return count;
}

int Renderer2DVBO::GetCachedVBOVertexCount(const char *cacheKey) {
    BTree<CachedVBO*>* tree = m_cachedVBOs.LookupTree(cacheKey);
    if (tree && tree->data && tree->data->isValid) {
        return tree->data->vertexCount;
    }
    return 0;
}

int Renderer2DVBO::GetCachedVBOIndexCount(const char *cacheKey) {
    BTree<CachedVBO*>* tree = m_cachedVBOs.LookupTree(cacheKey);
    if (tree && tree->data && tree->data->isValid) {
        return tree->data->indexCount;
    }
    return 0;
}