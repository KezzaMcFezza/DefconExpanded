#include "systemiv.h"

#include "model_builder.h"
#include "model.h"
#include "model_material.h"

#include "lib/render3d/megavbo/megavbo_3d.h"
#include "lib/render/colour.h"
#include "lib/math/vector3.h"
#include <stdio.h>

static bool ValidateModelCacheKey(Model* model)
{
    if (!model->m_cacheKey) {
        if (!model->m_filename[0]) {
            return false;
        }
        char* cacheKey = newStr(model->m_filename);
        model->SetCacheKey(cacheKey);
        if (cacheKey) {
            delete[] cacheKey;
        }
    }
    
    return model->m_cacheKey != NULL;
}

//
// Defaults to up-facing normal if mesh doesnt provide normals

static void CreateTexturedVertices(const ModelMesh& mesh, Vertex3DTextured* outVertices, const Colour& materialColor)
{
    int vertexCount = (int)mesh.positions.size() / 3;
    bool meshHasTexCoords = !mesh.texCoords.empty() && mesh.texCoords.size() >= (size_t)(vertexCount * 2);
    bool hasNormals = !mesh.normals.empty() && mesh.normals.size() >= (size_t)(vertexCount * 3);
    
    for (int j = 0; j < vertexCount; j++) {
        float x = mesh.positions[j * 3 + 0];
        float y = mesh.positions[j * 3 + 1];
        float z = mesh.positions[j * 3 + 2];
        
        float nx = 0.0f, ny = 1.0f, nz = 0.0f;
        if (hasNormals) {
            nx = mesh.normals[j * 3 + 0];
            ny = mesh.normals[j * 3 + 1];
            nz = mesh.normals[j * 3 + 2];
        }
        
        float u = 0.0f, v = 0.0f;
        if (meshHasTexCoords) {
            u = mesh.texCoords[j * 2 + 0];
            v = mesh.texCoords[j * 2 + 1];
        }
        
        outVertices[j] = Vertex3DTextured(x, y, z, nx, ny, nz, materialColor, u, v);
    }
}

static void CreateNonTexturedVertices(const ModelMesh& mesh, Vector3<float>* outVertices)
{
    int vertexCount = (int)mesh.positions.size() / 3;
    
    for (int j = 0; j < vertexCount; j++) {
        outVertices[j] = Vector3<float>(
            mesh.positions[j * 3 + 0],
            mesh.positions[j * 3 + 1],
            mesh.positions[j * 3 + 2]
        );
    }
}

//
// Checks VBO cache first to avoid rebuilding existing buffers
// Non textured VBOs only resize if needed

static void BuildVBOForGroup(Model* model, const std::vector<size_t>& meshIndices, ModelMaterial* material, 
                             int totalVertices, int totalIndices, const char* vboCacheKey, bool useTexturedVBO)
{
    if (useTexturedVBO) {
        if (g_renderer3dvbo->IsTexturedMegaVBO3DValid(vboCacheKey)) {
            model->AddVBOCacheKey(vboCacheKey);
            return;
        }
        
        g_renderer3dvbo->SetTexturedMegaVBO3DBufferSizes(totalVertices, totalIndices, vboCacheKey);
        
        unsigned int textureID = material->GetBaseColorTextureID();
        g_renderer3dvbo->BeginTexturedMegaVBO3D(vboCacheKey, textureID);
        
        Colour materialColor = material->baseColorFactor;
        
        for (size_t i = 0; i < meshIndices.size(); i++) {
            const ModelMesh& mesh = model->m_meshes[meshIndices[i]];
            
            int vertexCount = (int)mesh.positions.size() / 3;
            int indexCount = (int)mesh.indices.size();
            
            Vertex3DTextured* vertices = new Vertex3DTextured[vertexCount];
            CreateTexturedVertices(mesh, vertices, materialColor);
            
            g_renderer3dvbo->AddTexturedTrianglesToMegaVBO3DWithIndices(vertices, vertexCount, &mesh.indices[0], indexCount);
            
            delete[] vertices;
        }
        
        g_renderer3dvbo->EndTexturedMegaVBO3D();
    } else {
        if (g_renderer3dvbo->IsTriangleMegaVBO3DValid(vboCacheKey)) {
            model->AddVBOCacheKey(vboCacheKey);
            return;
        }
        
        int currentMaxVertices = g_renderer3dvbo->GetMegaTriangleBufferVertexCount3D();
        int currentMaxIndices = g_renderer3dvbo->GetMegaTriangleBufferIndexCount3D();
        
        if (totalVertices > currentMaxVertices || totalIndices > currentMaxIndices) {
            g_renderer3dvbo->SetTriangleMegaVBO3DBufferSizes(totalVertices, totalIndices, vboCacheKey);
        }
        
        Colour materialColor = material->baseColorFactor;
        g_renderer3dvbo->BeginTriangleMegaVBO3D(vboCacheKey, materialColor);
        
        for (size_t i = 0; i < meshIndices.size(); i++) {
            const ModelMesh& mesh = model->m_meshes[meshIndices[i]];
            
            int vertexCount = (int)mesh.positions.size() / 3;
            int indexCount = (int)mesh.indices.size();
            
            Vector3<float>* vertices = new Vector3<float>[vertexCount];
            CreateNonTexturedVertices(mesh, vertices);
            
            g_renderer3dvbo->AddTrianglesToMegaVBO3DWithIndices(vertices, vertexCount, &mesh.indices[0], indexCount);
            
            delete[] vertices;
        }
        
        g_renderer3dvbo->EndTriangleMegaVBO3D();
    }
    
    model->AddVBOCacheKey(vboCacheKey);
}

void ModelBuilder::BuildModelVBO(Model* model)
{
    if (!model || !model->IsLoaded() || model->m_meshes.empty()) {
        return;
    }
    
    if (!ValidateModelCacheKey(model)) {
        return;
    }
    
    model->ClearVBOCacheKeys();
    
    std::vector<MaterialGroup> materialGroups = GroupMeshesByMaterial(model);
    
    char vboCacheKey[1024];
    for (size_t i = 0; i < materialGroups.size(); i++) {
        const MaterialGroup& group = materialGroups[i];
        
        GenerateVBOCacheKey(model, group, vboCacheKey, sizeof(vboCacheKey));
        
        bool useTexturedVBO = ShouldUseTexturedVBO(group, model);
        BuildVBOForGroup(model, group.meshIndices, group.material, group.totalVertices, group.totalIndices, vboCacheKey, useTexturedVBO);
        
        AppDebugOut("ModelBuilder: Built VBO for material '%s' (%d verts, %d indices)\n", 
                    group.material->name.c_str(), group.totalVertices, group.totalIndices);
    }
}

//
// Groups meshes by material to batch rendering and reduce state changes
// Accumulates vertex/index counts to pre-size VBO buffers efficiently

std::vector<ModelBuilder::MaterialGroup> ModelBuilder::GroupMeshesByMaterial(Model* model)
{
    std::vector<MaterialGroup> materialGroups;
    
    for (size_t i = 0; i < model->m_meshes.size(); i++) {
        const ModelMesh& mesh = model->m_meshes[i];
        
        if (mesh.positions.empty() || mesh.indices.empty()) {
            continue;
        }
        
        ModelMaterial* mat = mesh.material ? mesh.material : ModelMaterial::GetDefaultMaterial();
        
        bool found = false;
        for (size_t j = 0; j < materialGroups.size(); j++) {
            if (materialGroups[j].material == mat) {
                materialGroups[j].meshIndices.push_back(i);
                materialGroups[j].totalVertices += (int)mesh.positions.size() / 3;
                materialGroups[j].totalIndices += (int)mesh.indices.size();
                found = true;
                break;
            }
        }
        
        if (!found) {
            MaterialGroup group;
            group.material = mat;
            group.meshIndices.push_back(i);
            group.totalVertices = (int)mesh.positions.size() / 3;
            group.totalIndices = (int)mesh.indices.size();
            materialGroups.push_back(group);
        }
    }
    
    return materialGroups;
}

bool ModelBuilder::ShouldUseTexturedVBO(const MaterialGroup& group, Model* model)
{
    if (!group.material->HasBaseColorTexture()) {
        return false;
    }
    
    for (size_t i = 0; i < group.meshIndices.size(); i++) {
        const ModelMesh& mesh = model->m_meshes[group.meshIndices[i]];
        int vertexCount = (int)mesh.positions.size() / 3;
        if (!mesh.texCoords.empty() && mesh.texCoords.size() >= (size_t)(vertexCount * 2)) {
            return true;
        }
    }
    
    return false;
}

void ModelBuilder::GenerateVBOCacheKey(const Model* model, const MaterialGroup& group, char* outKey, size_t keySize)
{
    snprintf(outKey, keySize, "%s_mat%d", model->m_cacheKey, group.material->index);
    outKey[keySize - 1] = '\0';
}

void ModelBuilder::BuildTexturedVBO(Model* model, const MaterialGroup& group, const char* vboCacheKey)
{
    g_renderer3dvbo->SetTexturedMegaVBO3DBufferSizes(group.totalVertices, group.totalIndices, vboCacheKey);
    
    unsigned int textureID = group.material->GetBaseColorTextureID();
    g_renderer3dvbo->BeginTexturedMegaVBO3D(vboCacheKey, textureID);
    
    Colour materialColor = group.material->baseColorFactor;
    
    for (size_t i = 0; i < group.meshIndices.size(); i++) {
        const ModelMesh& mesh = model->m_meshes[group.meshIndices[i]];
        
        int vertexCount = (int)mesh.positions.size() / 3;
        int indexCount = (int)mesh.indices.size();
        
        Vertex3DTextured* vertices = new Vertex3DTextured[vertexCount];
        CreateTexturedVertices(mesh, vertices, materialColor);
        
        g_renderer3dvbo->AddTexturedTrianglesToMegaVBO3DWithIndices(vertices, vertexCount, &mesh.indices[0], indexCount);
        
        delete[] vertices;
    }
    
    g_renderer3dvbo->EndTexturedMegaVBO3D();
}

void ModelBuilder::BuildNonTexturedVBO(Model* model, const MaterialGroup& group, const char* vboCacheKey)
{ 
    int currentMaxVertices = g_renderer3dvbo->GetMegaTriangleBufferVertexCount3D();
    int currentMaxIndices = g_renderer3dvbo->GetMegaTriangleBufferIndexCount3D();
    
    if (group.totalVertices > currentMaxVertices || group.totalIndices > currentMaxIndices) {
        g_renderer3dvbo->SetTriangleMegaVBO3DBufferSizes(group.totalVertices, group.totalIndices, vboCacheKey);
    }
    
    Colour materialColor = group.material->baseColorFactor;
    g_renderer3dvbo->BeginTriangleMegaVBO3D(vboCacheKey, materialColor);
    
    for (size_t i = 0; i < group.meshIndices.size(); i++) {
        const ModelMesh& mesh = model->m_meshes[group.meshIndices[i]];
        
        int vertexCount = (int)mesh.positions.size() / 3;
        int indexCount = (int)mesh.indices.size();
        
        Vector3<float>* vertices = new Vector3<float>[vertexCount];
        CreateNonTexturedVertices(mesh, vertices);
        
        g_renderer3dvbo->AddTrianglesToMegaVBO3DWithIndices(vertices, vertexCount, &mesh.indices[0], indexCount);
        
        delete[] vertices;
    }
    
    g_renderer3dvbo->EndTriangleMegaVBO3D();
}

char* ModelBuilder::GetModelCacheKey(const Model* model)
{
    if (!model) return NULL;
    return newStr(model->m_filename);
}

