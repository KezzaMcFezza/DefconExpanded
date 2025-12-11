#include "lib/universal_include.h"

#include <stdio.h>
#include <string.h>
#include <algorithm>

#include <tiny_gltf.h>

#include "lib/filesys/file_system.h"
#include "lib/filesys/filesys_utils.h"
#include "lib/filesys/binary_stream_readers.h"
#include "lib/render3d/renderer_3d.h"
#include "lib/render3d/megavbo/megavbo_3d.h"
#include "lib/render/colour.h"
#include "model3d.h"


Model3D::Model3D( const char *filename )
:   m_boundsMin(0,0,0),
    m_boundsMax(0,0,0),
    m_loaded(false),
    m_cacheKey(NULL)
{
    strncpy(m_filename, filename, sizeof(m_filename) - 1);
    m_filename[sizeof(m_filename) - 1] = '\0';
    
    m_loaded = LoadFromGLTF(filename);
    
    if (m_loaded) {
        CalculateBounds();
    }
}

Model3D::~Model3D()
{
    m_meshes.clear();
    
    if (m_cacheKey) {
        delete[] m_cacheKey;
        m_cacheKey = NULL;
    }
}

bool Model3D::IsLoaded() const
{
    return m_loaded;
}

int Model3D::GetMeshCount() const
{
    return (int)m_meshes.size();
}

const Model3DMesh* Model3D::GetMesh(int index) const
{
    if (index >= 0 && index < (int)m_meshes.size()) {
        return &m_meshes[index];
    }
    return NULL;
}

void Model3D::GetBounds(Vector3<float>& outMin, Vector3<float>& outMax) const
{
    outMin = m_boundsMin;
    outMax = m_boundsMax;
}

float Model3D::GetBoundsRadius() const
{
    Vector3<float> size = m_boundsMax - m_boundsMin;
    return size.Mag() * 0.5f;
}


bool Model3D::LoadFromGLTF(const char *filepath)
{
    BinaryReader *in = g_fileSystem->GetBinaryReader(filepath);
    
    if (!in || !in->IsOpen()) {
        AppDebugOut("Model3D Error: Failed to open file: %s\n", filepath);
        return false;
    }
    
    int fileSize = in->GetSize();
    std::vector<unsigned char> buffer(fileSize);
    in->ReadBytes(fileSize, &buffer[0]);
    delete in;
    
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;
    
    bool ret = false;
    
    const char *extension = GetExtensionPart(filepath);
    if (stricmp(extension, "glb") == 0) {
        ret = loader.LoadBinaryFromMemory(&model, &err, &warn, &buffer[0], fileSize, "");
    } else {
        ret = loader.LoadASCIIFromString(&model, &err, &warn, (const char*)&buffer[0], fileSize, "");
    }
    
    if (!warn.empty()) {
        AppDebugOut("Model3D Warning (%s): %s\n", filepath, warn.c_str());
    }
    
    if (!err.empty()) {
        AppDebugOut("Model3D Error (%s): %s\n", filepath, err.c_str());
        return false;
    }
    
    if (!ret) {
        AppDebugOut("Model3D: Failed to parse glTF data: %s\n", filepath);
        return false;
    }
    
    //
    // Process the scene graph with transforms
    // Use the default scene or first scene

    int sceneIndex = model.defaultScene >= 0 ? model.defaultScene : 0;
    
    if (sceneIndex < 0 || sceneIndex >= (int)model.scenes.size()) {
        AppDebugOut("Model3D: No valid scene found in: %s\n", filepath);
        return false;
    }
    
    const tinygltf::Scene& scene = model.scenes[sceneIndex];
    
    //
    // Identity matrix for root nodes

    float identityMatrix[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
    
    for (size_t i = 0; i < scene.nodes.size(); i++) {
        ProcessNode(model, scene.nodes[i], identityMatrix);
    }
    
    return true;
}

void Model3D::ProcessNode(const tinygltf::Model& model, int nodeIndex, const float* parentTransform)
{
    if (nodeIndex < 0 || nodeIndex >= (int)model.nodes.size()) {
        return;
    }
    
    const tinygltf::Node& node = model.nodes[nodeIndex];
    
    //
    // Build this node's local transform

    float localTransform[16];
    Matrix4f::BuildTransformMatrix(node.translation, node.rotation, node.scale, node.matrix, localTransform);
    
    //
    // Accumulate with parent transform

    float worldTransform[16];
    Matrix4f::Multiply(parentTransform, localTransform, worldTransform);
    
    //
    // Process mesh if this node has one

    if (node.mesh >= 0 && node.mesh < (int)model.meshes.size()) {
        const tinygltf::Mesh& mesh = model.meshes[node.mesh];
        
        for (size_t j = 0; j < mesh.primitives.size(); j++) {
            const tinygltf::Primitive& primitive = mesh.primitives[j];
            
            Model3DMesh meshData;
            
            //
            // Load positions
            
            if (primitive.attributes.find("POSITION") != primitive.attributes.end()) {
                const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("POSITION")];
                const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
                const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
                
                const float* positions = reinterpret_cast<const float*>(
                    &buffer.data[bufferView.byteOffset + accessor.byteOffset]
                );
                
                meshData.positions.resize(accessor.count * 3);
                for (size_t k = 0; k < accessor.count; k++) {
                    meshData.positions[k * 3 + 0] = positions[k * 3 + 0];
                    meshData.positions[k * 3 + 1] = positions[k * 3 + 1];
                    meshData.positions[k * 3 + 2] = positions[k * 3 + 2];
                    
                    Matrix4f::TransformVertex(worldTransform, &meshData.positions[k * 3]);
                }
            }
            
            //
            // Load indices
            
            if (primitive.indices >= 0) {
                const tinygltf::Accessor& accessor = model.accessors[primitive.indices];
                const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
                const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
                
                meshData.indices.resize(accessor.count);
                
                if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                    const unsigned short* indices = reinterpret_cast<const unsigned short*>(
                        &buffer.data[bufferView.byteOffset + accessor.byteOffset]
                    );
                    for (size_t k = 0; k < accessor.count; k++) {
                        meshData.indices[k] = indices[k];
                    }
                } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                    const unsigned int* indices = reinterpret_cast<const unsigned int*>(
                        &buffer.data[bufferView.byteOffset + accessor.byteOffset]
                    );
                    for (size_t k = 0; k < accessor.count; k++) {
                        meshData.indices[k] = indices[k];
                    }
                } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                    const unsigned char* indices = reinterpret_cast<const unsigned char*>(
                        &buffer.data[bufferView.byteOffset + accessor.byteOffset]
                    );
                    for (size_t k = 0; k < accessor.count; k++) {
                        meshData.indices[k] = indices[k];
                    }
                }
            }
            
            m_meshes.push_back(meshData);
        }
    }
    
    //
    // Recursively process child nodes
    
    for (size_t i = 0; i < node.children.size(); i++) {
        ProcessNode(model, node.children[i], worldTransform);
    }
}

void Model3D::CalculateBounds()
{
    if (m_meshes.empty()) {
        return;
    }
    
    bool first = true;
    
    for (size_t i = 0; i < m_meshes.size(); i++) {
        const Model3DMesh& mesh = m_meshes[i];
        
        for (size_t j = 0; j < mesh.positions.size(); j += 3) {
            Vector3<float> pos(mesh.positions[j], mesh.positions[j+1], mesh.positions[j+2]);
            
            if (first) {
                m_boundsMin = pos;
                m_boundsMax = pos;
                first = false;
            } else {
                m_boundsMin.x = std::min(m_boundsMin.x, pos.x);
                m_boundsMin.y = std::min(m_boundsMin.y, pos.y);
                m_boundsMin.z = std::min(m_boundsMin.z, pos.z);
                
                m_boundsMax.x = std::max(m_boundsMax.x, pos.x);
                m_boundsMax.y = std::max(m_boundsMax.y, pos.y);
                m_boundsMax.z = std::max(m_boundsMax.z, pos.z);
            }
        }
    }
}

//
// The cachekey for this VBO is the full filename path
// This allows mods to have models with the same name without conflicts

char* Model3D::GetModelCacheKey() const {
    return newStr(m_filename);
}

const char* Model3D::GetCacheKey() const {
    return m_cacheKey ? m_cacheKey : "";
}

//
// Build the VBO for this model

void Model3D::BuildModelVBO() {
    if (!m_loaded || m_meshes.empty()) {
        return;
    }
    
    //
    // Generate and store cache key if not already set
    
    if (!m_cacheKey) {
        m_cacheKey = GetModelCacheKey();
    }
    
    if (!m_cacheKey) {
        return;
    }
    
    if (g_renderer3dvbo->IsTriangleMegaVBO3DValid(m_cacheKey)) {
        return;
    }
    
    //
    // Calculate total vertex and index counts
    
    int totalVertices = 0;
    int totalIndices = 0;
    
    for (size_t i = 0; i < m_meshes.size(); i++) {
        totalVertices += (int)m_meshes[i].positions.size() / 3;
        totalIndices += (int)m_meshes[i].indices.size();
    }
    
    if (totalVertices == 0 || totalIndices == 0) {
        return;
    }
    
    //
    // Set buffer sizes if needed
    
    int currentMaxVertices = g_renderer3dvbo->GetMegaTriangleBufferVertexCount3D();
    int currentMaxIndices = g_renderer3dvbo->GetMegaTriangleBufferIndexCount3D();
    
    if (totalVertices > currentMaxVertices || totalIndices > currentMaxIndices) {
        g_renderer3dvbo->SetTriangleMegaVBO3DBufferSizes(totalVertices, totalIndices);
    }
    
    //
    // Begin building VBO, color will be set per-instance at render time.
    // The default color is white
    
    Colour defaultColor(255, 255, 255, 255);
    g_renderer3dvbo->BeginTriangleMegaVBO3D(m_cacheKey, defaultColor);
    
    //
    // Add all meshes to VBO using models index buffers
    
    for (size_t i = 0; i < m_meshes.size(); i++) {
        const Model3DMesh& mesh = m_meshes[i];
        
        if (mesh.positions.empty() || mesh.indices.empty()) {
            continue;
        }
        
        int vertexCount = (int)mesh.positions.size() / 3;
        int indexCount = (int)mesh.indices.size();
        
        Vector3<float>* vertices = new Vector3<float>[vertexCount];
        
        for (int j = 0; j < vertexCount; j++) {
            vertices[j].x = mesh.positions[j * 3 + 0];
            vertices[j].y = mesh.positions[j * 3 + 1];
            vertices[j].z = mesh.positions[j * 3 + 2];
        }
        
        //
        // Use models index buffer to properly reference vertices
        
        g_renderer3dvbo->AddTrianglesToMegaVBO3DWithIndices(vertices, vertexCount, &mesh.indices[0], indexCount);
        
        delete[] vertices;
    }
    
    g_renderer3dvbo->EndTriangleMegaVBO3D();
}