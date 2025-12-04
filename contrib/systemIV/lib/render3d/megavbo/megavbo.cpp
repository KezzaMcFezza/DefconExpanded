#include "lib/universal_include.h"

#include "lib/render3d/renderer_3d.h"

void Renderer3D::InvalidateAll3DVBOs() {
    DArray<char*> *keys = m_cached3DVBOs.ConvertIndexToDArray();
    
    for (int i = 0; i < keys->Size(); ++i) {
        if (!Is3DVBOProtected(keys->GetData(i))) {
            InvalidateCached3DVBO(keys->GetData(i));
        }
    }
    
    delete keys;
}

void Renderer3D::Protect3DVBO(const char* key) {
    if (!key) return;
    if (Is3DVBOProtected(key)) return;
    m_protected3DVBOKeys.PutData(newStr(key));
}

void Renderer3D::Unprotect3DVBO(const char* key) {
    if (!key) return;
    for (int i = 0; i < m_protected3DVBOKeys.Size(); ++i) {
        if (strcmp(m_protected3DVBOKeys[i], key) == 0) {
            delete[] m_protected3DVBOKeys[i];
            m_protected3DVBOKeys.RemoveData(i);
            return;
        }
    }
}

void Renderer3D::Clear3DVBOProtection() {
    for (int i = 0; i < m_protected3DVBOKeys.Size(); ++i) {
        delete[] m_protected3DVBOKeys[i];
    }
    m_protected3DVBOKeys.Empty();
}

bool Renderer3D::Is3DVBOProtected(const char* key) {
    if (!key) return false;
    for (int i = 0; i < m_protected3DVBOKeys.Size(); ++i) {
        if (strcmp(m_protected3DVBOKeys[i], key) == 0) {
            return true;
        }
    }
    return false;
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