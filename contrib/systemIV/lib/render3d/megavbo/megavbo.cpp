#include "lib/universal_include.h"

#include "lib/render3d/renderer_3d.h"

void Renderer3D::InvalidateAll3DVBOs() {
    DArray<char*> *keys = m_cached3DVBOs.ConvertIndexToDArray();
    
    for (int i = 0; i < keys->Size(); ++i) {
        InvalidateCached3DVBO(keys->GetData(i));
    }
    
    delete keys;
}

void Renderer3D::InvalidateCached3DVBO(const char* cacheKey) {
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

int Renderer3D::GetCached3DVBOCount() {
    int count = 0;
    
    DArray<char*> *keys = m_cached3DVBOs.ConvertIndexToDArray();
    
    for (int i = 0; i < keys->Size(); ++i) {
        BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(keys->GetData(i));
        if (tree && tree->data && tree->data->isValid) {
            count++;
        }
    }
    
    delete keys;
    return count;
}

int Renderer3D::GetCached3DVBOVertexCount(const char *cacheKey) {
    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(cacheKey);
    if (tree && tree->data && tree->data->isValid) {
        return tree->data->vertexCount;
    }
    return 0;
}

int Renderer3D::GetCached3DVBOIndexCount(const char *cacheKey) {
    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(cacheKey);
    if (tree && tree->data && tree->data->isValid) {
        return tree->data->indexCount;
    }
    return 0;
}