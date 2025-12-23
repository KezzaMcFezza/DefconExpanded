#include "systemiv.h"

#include "lib/render3d/renderer_3d.h"
#include "lib/render3d/megavbo/megavbo_3d.h"
#include "lib/string_utils.h"

Renderer3DVBO *g_renderer3dvbo = NULL;

Renderer3DVBO::Renderer3DVBO()
:   m_maxMegaVertices3D(100),
    m_maxMegaIndices3D(100),
    m_megaVBO3DActive(false),
    m_currentMegaVBO3DKey(NULL),
    m_megaVBO3DColor(0, 0, 0, 0),
    m_megaVertices3D(NULL),
    m_megaVertex3DCount(0),
    m_megaIndices3D(NULL),
    m_megaIndex3DCount(0),
    m_megaVBO3DTexturedActive(false),
    m_currentMegaVBO3DTexturedKey(NULL),
    m_currentMegaVBO3DTextureID(0),
    m_maxMegaTexturedVertices3D(100),
    m_maxMegaTexturedIndices3D(100),
    m_megaTexturedVertices3D(NULL),
    m_megaTexturedVertex3DCount(0),
    m_megaTexturedIndices3D(NULL),
    m_megaTexturedIndex3DCount(0),
    m_megaVBO3DTrianglesActive(false),
    m_currentMegaVBO3DTrianglesKey(NULL),
    m_megaVBO3DTrianglesColor(0, 0, 0, 0),
    m_maxMegaTriangleVertices3D(100),
    m_maxMegaTriangleIndices3D(100),
    m_megaTriangleVertices3D(NULL),
    m_megaTriangleVertex3DCount(0),
    m_megaTriangleIndices3D(NULL),
    m_megaTriangleIndex3DCount(0),
    m_instancedMegaVBOActive(false),
    m_currentInstancedBatchKey(NULL),
    m_currentInstancedMeshKey(NULL),
    m_maxInstances(Renderer3D::MAX_INSTANCES),
    m_instanceMatrices(NULL),
    m_instanceColors(NULL),
    m_instanceCount(0),
    m_cached3DVBOs(),
    m_cachedInstanceBatches(),
    m_protected3DVBOKeys()
{
    m_megaVertices3D = new Vertex3D[m_maxMegaVertices3D];
    m_megaIndices3D = new unsigned int[m_maxMegaIndices3D];
    
    m_megaTexturedVertices3D = new Vertex3DTextured[m_maxMegaTexturedVertices3D];
    m_megaTexturedIndices3D = new unsigned int[m_maxMegaTexturedIndices3D];
    
    m_megaTriangleVertices3D = new Vertex3D[m_maxMegaTriangleVertices3D];
    m_megaTriangleIndices3D = new unsigned int[m_maxMegaTriangleIndices3D];
    
    m_instanceMatrices = new Matrix4f[m_maxInstances];
    m_instanceColors = new Colour[m_maxInstances];
}

Renderer3DVBO::~Renderer3DVBO() 
{
    InvalidateAll3DVBOs();
    InvalidateAllInstanceBatches();
    
    if (m_megaVertices3D) {
        delete[] m_megaVertices3D;
        m_megaVertices3D = NULL;
    }
    
    if (m_megaIndices3D) {
        delete[] m_megaIndices3D;
        m_megaIndices3D = NULL;
    }
    
    if (m_megaTexturedVertices3D) {
        delete[] m_megaTexturedVertices3D;
        m_megaTexturedVertices3D = NULL;
    }
    
    if (m_megaTexturedIndices3D) {
        delete[] m_megaTexturedIndices3D;
        m_megaTexturedIndices3D = NULL;
    }
    
    if (m_megaTriangleVertices3D) {
        delete[] m_megaTriangleVertices3D;
        m_megaTriangleVertices3D = NULL;
    }
    
    if (m_megaTriangleIndices3D) {
        delete[] m_megaTriangleIndices3D;
        m_megaTriangleIndices3D = NULL;
    }
    
    if (m_instanceMatrices) {
        delete[] m_instanceMatrices;
        m_instanceMatrices = NULL;
    }
    
    if (m_instanceColors) {
        delete[] m_instanceColors;
        m_instanceColors = NULL;
    }
    
    if (m_currentMegaVBO3DKey) {
        delete[] m_currentMegaVBO3DKey;
        m_currentMegaVBO3DKey = NULL;
    }
    
    if (m_currentMegaVBO3DTexturedKey) {
        delete[] m_currentMegaVBO3DTexturedKey;
        m_currentMegaVBO3DTexturedKey = NULL;
    }
    
    if (m_currentMegaVBO3DTrianglesKey) {
        delete[] m_currentMegaVBO3DTrianglesKey;
        m_currentMegaVBO3DTrianglesKey = NULL;
    }
    
    if (m_currentInstancedBatchKey) {
        delete[] m_currentInstancedBatchKey;
        m_currentInstancedBatchKey = NULL;
    }
    
    if (m_currentInstancedMeshKey) {
        delete[] m_currentInstancedMeshKey;
        m_currentInstancedMeshKey = NULL;
    }
    
    Clear3DVBOProtection();
}


void Renderer3DVBO::InvalidateAll3DVBOs() {
    DArray<std::string> *keys = m_cached3DVBOs.ConvertIndexToDArray();
    
    for (int i = 0; i < keys->Size(); ++i) {
        if (!Is3DVBOProtected(keys->GetData(i).c_str())) {
            InvalidateCached3DVBO(keys->GetData(i).c_str());
        }
    }
    
    delete keys;
}

void Renderer3DVBO::Protect3DVBO(const char* key) {
    if (!key) return;
    if (Is3DVBOProtected(key)) return;
    m_protected3DVBOKeys.PutData(newStr(key));
}

void Renderer3DVBO::Unprotect3DVBO(const char* key) {
    if (!key) return;
    for (int i = 0; i < m_protected3DVBOKeys.Size(); ++i) {
        if (strcmp(m_protected3DVBOKeys[i], key) == 0) {
            delete[] m_protected3DVBOKeys[i];
            m_protected3DVBOKeys.RemoveData(i);
            return;
        }
    }
}

void Renderer3DVBO::Clear3DVBOProtection() {
    for (int i = 0; i < m_protected3DVBOKeys.Size(); ++i) {
        delete[] m_protected3DVBOKeys[i];
    }
    m_protected3DVBOKeys.Empty();
}

bool Renderer3DVBO::Is3DVBOProtected(const char* key) {
    if (!key) return false;
    for (int i = 0; i < m_protected3DVBOKeys.Size(); ++i) {
        if (strcmp(m_protected3DVBOKeys[i], key) == 0) {
            return true;
        }
    }
    return false;
}

void Renderer3DVBO::InvalidateCached3DVBO(const char* cacheKey) {
    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(cacheKey);
    if (tree && tree->data) {
        Cached3DVBO* cachedVBO = tree->data;

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
        m_cached3DVBOs.RemoveData(cacheKey);
    }
}

int Renderer3DVBO::GetCached3DVBOCount() {
    int count = 0;
    
    DArray<std::string> *keys = m_cached3DVBOs.ConvertIndexToDArray();
    
    for (int i = 0; i < keys->Size(); ++i) {
        BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(keys->GetData(i).c_str());
        if (tree && tree->data && tree->data->isValid) {
            count++;
        }
    }
    
    delete keys;
    return count;
}

int Renderer3DVBO::GetCached3DVBOVertexCount(const char *cacheKey) {
    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(cacheKey);
    if (tree && tree->data && tree->data->isValid) {
        return tree->data->vertexCount;
    }
    return 0;
}

int Renderer3DVBO::GetCached3DVBOIndexCount(const char *cacheKey) {
    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(cacheKey);
    if (tree && tree->data && tree->data->isValid) {
        return tree->data->indexCount;
    }
    return 0;
}

size_t Renderer3DVBO::GetCached3DVBOVertexSize(const char *cacheKey) {
    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(cacheKey);
    if (tree && tree->data && tree->data->isValid) {
        switch (tree->data->vertexType) {
            case VBO_VERTEX_3D:
                return sizeof(Vertex3D);
            case VBO_VERTEX_3D_TEXTURED:
                return sizeof(Vertex3DTextured);
            default:
                return 0;
        }
    }
    return 0;
}

DArray<std::string> *Renderer3DVBO::GetAllCached3DVBOKeys() {
    DArray<std::string> *allKeys = m_cached3DVBOs.ConvertIndexToDArray();
    DArray<std::string> *validKeys = new DArray<std::string>();
    
    for (int i = 0; i < allKeys->Size(); ++i) {
        BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(allKeys->GetData(i).c_str());
        if (tree && tree->data && tree->data->isValid) {
            validKeys->PutData(allKeys->GetData(i));
        }
    }
    
    delete allKeys;
    return validKeys;
}