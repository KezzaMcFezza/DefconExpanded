#include "lib/universal_include.h"

#include "lib/render2d/renderer.h"

void Renderer::InvalidateAllVBOs() {
    
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

void Renderer::ProtectVBO(const char* key) {
    if (!key) return;
    if (IsVBOProtected(key)) return;
    m_protectedVBOKeys.PutData(newStr(key));
}

void Renderer::UnprotectVBO(const char* key) {
    if (!key) return;
    for (int i = 0; i < m_protectedVBOKeys.Size(); ++i) {
        if (strcmp(m_protectedVBOKeys[i], key) == 0) {
            delete[] m_protectedVBOKeys[i];
            m_protectedVBOKeys.RemoveData(i);
            return;
        }
    }
}

void Renderer::ClearVBOProtection() {
    for (int i = 0; i < m_protectedVBOKeys.Size(); ++i) {
        delete[] m_protectedVBOKeys[i];
    }
    m_protectedVBOKeys.Empty();
}

bool Renderer::IsVBOProtected(const char* key) {
    if (!key) return false;
    for (int i = 0; i < m_protectedVBOKeys.Size(); ++i) {
        if (strcmp(m_protectedVBOKeys[i], key) == 0) {
            return true;
        }
    }
    return false;
}

void Renderer::InvalidateCachedVBO(const char* cacheKey) {
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

int Renderer::GetCachedVBOCount() {
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

int Renderer::GetCachedVBOVertexCount(const char *cacheKey) {
    BTree<CachedVBO*>* tree = m_cachedVBOs.LookupTree(cacheKey);
    if (tree && tree->data && tree->data->isValid) {
        return tree->data->vertexCount;
    }
    return 0;
}

int Renderer::GetCachedVBOIndexCount(const char *cacheKey) {
    BTree<CachedVBO*>* tree = m_cachedVBOs.LookupTree(cacheKey);
    if (tree && tree->data && tree->data->isValid) {
        return tree->data->indexCount;
    }
    return 0;
}
