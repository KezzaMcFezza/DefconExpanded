#include "systemiv.h"

#include "lib/debug_utils.h"
#include "lib/string_utils.h"
#include "lib/render/renderer.h"
#include "lib/render3d/renderer_3d.h"
#include "lib/render3d/megavbo/megavbo_3d.h"
#include "lib/math/matrix4f.h"

// ============================================================================
// INSTANCING SYSTEM
// ============================================================================

void Renderer3DVBO::BeginInstancedMegaVBO(const char* batchKey, const char* meshKey) {
    if (!batchKey || !meshKey) {
        return;
    }
    
    //
    // Only skip building if we are not currently building
    
    if (!m_instancedMegaVBOActive) {
        BTree<CachedInstanceBatch*>* tree = m_cachedInstanceBatches.LookupTree(batchKey);
        if (tree && tree->data && tree->data->isValid) {
            return;
        }
    }
    
    //
    // Verify the mesh exists
    
    if (!IsAny3DVBOValid(meshKey)) {
        // Continue anyway, mesh might be created later
    }
    
    //
    // Clean up previous keys if still active
    
    if (m_currentInstancedBatchKey) {
        delete[] m_currentInstancedBatchKey;
    }
    if (m_currentInstancedMeshKey) {
        delete[] m_currentInstancedMeshKey;
    }
    
    m_currentInstancedBatchKey = newStr(batchKey);
    m_currentInstancedMeshKey = newStr(meshKey);
    m_instancedMegaVBOActive = true;
    m_instanceCount = 0;
}

bool Renderer3DVBO::AddInstance(const Matrix4f& matrix, const Colour& color) {
    if (!m_instancedMegaVBOActive) {
        return false;
    }
    
    if (m_instanceCount >= m_maxInstances) {
        return false;
    }
    
    m_instanceMatrices[m_instanceCount] = matrix;
    m_instanceColors[m_instanceCount] = color;
    m_instanceCount++;
    
    return true;
}

bool Renderer3DVBO::AddInstanceIfNotFull(const Matrix4f& matrix, const Colour& color) {
    if (!m_instancedMegaVBOActive) {
        return false;
    }
    
    if (m_instanceCount >= m_maxInstances) {
        return false;
    }
    
    m_instanceMatrices[m_instanceCount] = matrix;
    m_instanceColors[m_instanceCount] = color;
    m_instanceCount++;
    
    return true;
}

void Renderer3DVBO::EndInstancedMegaVBO() {
    if (!m_instancedMegaVBOActive || !m_currentInstancedBatchKey) {
        return;
    }
    
    if (m_instanceCount == 0) {
        m_instancedMegaVBOActive = false;
        return;
    }
    
    //
    // Create or get cached instance batch
    
    BTree<CachedInstanceBatch*>* tree = m_cachedInstanceBatches.LookupTree(m_currentInstancedBatchKey);
    CachedInstanceBatch* batch = NULL;
    
    if (!tree || !tree->data) {
        //
        // Create new batch
        
        batch = new CachedInstanceBatch();
        batch->meshKey = newStr(m_currentInstancedMeshKey);
        batch->maxInstances = m_maxInstances;
        batch->matrices = new Matrix4f[m_maxInstances];
        batch->colors = new Colour[m_maxInstances];
        batch->instanceCount = 0;
        batch->isValid = false;
        
        m_cachedInstanceBatches.PutData(m_currentInstancedBatchKey, batch);
    } else {

        //
        // Update existing batch
        
        batch = tree->data;
        
        //
        // If mesh changed, update it

        if (strcmp(batch->meshKey, m_currentInstancedMeshKey) != 0) {
            delete[] batch->meshKey;
            batch->meshKey = newStr(m_currentInstancedMeshKey);
        }
        
        //
        // Resize if needed

        if (batch->maxInstances < m_instanceCount) {
            delete[] batch->matrices;
            delete[] batch->colors;
            
            batch->maxInstances = m_instanceCount;
            batch->matrices = new Matrix4f[batch->maxInstances];
            batch->colors = new Colour[batch->maxInstances];
        }
    }
    
    for (int i = 0; i < m_instanceCount; i++) {
        batch->matrices[i] = m_instanceMatrices[i];
        batch->colors[i] = m_instanceColors[i];
    }
    
    batch->instanceCount = m_instanceCount;
    batch->isValid = true;
    
    m_instanceCount = 0;
    m_instancedMegaVBOActive = false;
}

void Renderer3DVBO::RenderInstancedMegaVBO3D(const char* batchKey) {
    if (!batchKey) {
        return;
    }
    
    BTree<CachedInstanceBatch*>* tree = m_cachedInstanceBatches.LookupTree(batchKey);
    if (!tree || !tree->data || !tree->data->isValid) {
        return;
    }
    
    CachedInstanceBatch* batch = tree->data;
    
    if (!IsAny3DVBOValid(batch->meshKey)) {
        return;
    }
    
    if (batch->instanceCount <= 0) {
        return;
    }
    
    BTree<Cached3DVBO*>* vboTree = m_cached3DVBOs.LookupTree(batch->meshKey);
    if (!vboTree || !vboTree->data || !vboTree->data->isValid) {
        return;
    }
    
    Cached3DVBO* cachedVBO = vboTree->data;
    
    g_renderer->StartFlushTiming("MegaVBO_Instanced_3D");
    
    g_renderer->SetShaderProgram(g_renderer3d->m_shader3DModelProgram);
    g_renderer3d->Set3DModelShaderUniformsInstanced(batch->matrices, batch->colors, batch->instanceCount);
    
    g_renderer->SetVertexArray(cachedVBO->VAO);
    g_renderer->SetArrayBuffer(cachedVBO->VBO);
    
    //
    // For textured VBOs, we need to remap.
    // location 0 = position, location 1 = color (from location 2)
    
    if (cachedVBO->vertexType == VBO_VERTEX_3D_TEXTURED) {
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3DTextured), (void*)0);
        glEnableVertexAttribArray(0);
        
        glDisableVertexAttribArray(1);
        
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex3DTextured), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(1);
        
        glDisableVertexAttribArray(3);
    }
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cachedVBO->IBO);
    glDrawElementsInstanced(GL_TRIANGLES, cachedVBO->indexCount, GL_UNSIGNED_INT, 0, batch->instanceCount);
    
    g_renderer->EndFlushTiming("MegaVBO_Instanced_3D");
    g_renderer3d->IncrementDrawCall3D("triangle_vbo");
}

bool Renderer3DVBO::IsInstancedMegaVBOValid(const char* batchKey) {
    if (!batchKey) {
        return false;
    }
    
    BTree<CachedInstanceBatch*>* tree = m_cachedInstanceBatches.LookupTree(batchKey);
    return (tree && tree->data && tree->data->isValid);
}

void Renderer3DVBO::InvalidateInstancedMegaVBO(const char* batchKey) {
    if (!batchKey) {
        return;
    }
    
    BTree<CachedInstanceBatch*>* tree = m_cachedInstanceBatches.LookupTree(batchKey);
    if (tree && tree->data) {
        CachedInstanceBatch* batch = tree->data;
        
        if (batch->meshKey) {
            delete[] batch->meshKey;
        }
        if (batch->matrices) {
            delete[] batch->matrices;
        }
        if (batch->colors) {
            delete[] batch->colors;
        }
        
        delete batch;
        m_cachedInstanceBatches.RemoveData(batchKey);
    }
}

void Renderer3DVBO::InvalidateAllInstanceBatches() {
    DArray<std::string>* keys = m_cachedInstanceBatches.ConvertIndexToDArray();
    
    for (int i = 0; i < keys->Size(); ++i) {
        InvalidateInstancedMegaVBO(keys->GetData(i).c_str());
    }
    
    delete keys;
}

void Renderer3DVBO::InvalidateInstanceBatchPrefix(const char* batchKeyPrefix) {
    if (!batchKeyPrefix) return;
    
    DArray<std::string>* keys = m_cachedInstanceBatches.ConvertIndexToDArray();
    size_t prefixLen = strlen(batchKeyPrefix);
    
    for (int i = 0; i < keys->Size(); ++i) {
        const std::string &key = keys->GetData(i);
        if (strncmp(key.c_str(), batchKeyPrefix, prefixLen) == 0) {
            InvalidateInstancedMegaVBO(key.c_str());
        }
    }
    
    delete keys;
}

int Renderer3DVBO::GetCachedInstanceBatchCount() {
    int count = 0;
    
    DArray<std::string>* keys = m_cachedInstanceBatches.ConvertIndexToDArray();
    
    for (int i = 0; i < keys->Size(); ++i) {
        BTree<CachedInstanceBatch*>* tree = m_cachedInstanceBatches.LookupTree(keys->GetData(i).c_str());
        if (tree && tree->data && tree->data->isValid) {
            count++;
        }
    }
    
    delete keys;
    return count;
}