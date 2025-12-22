#include "systemiv.h"

#include "model_loader.h"
#include "model.h"
#include "model_material.h"

#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <tiny_gltf.h>

#include "lib/filesys/file_system.h"
#include "lib/filesys/filesys_utils.h"
#include "lib/filesys/binary_stream_readers.h"
#include "lib/math/matrix4f.h"
#include "lib/render/colour.h"
#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/resource/bitmap.h"

static bool ReadGLTFFile(const char* filepath, std::vector<unsigned char>& outBuffer)
{
    BinaryReader* in = g_fileSystem->GetBinaryReader(filepath);
    
    if (!in || !in->IsOpen()) {
        AppDebugOut("ModelLoader Error: Failed to open file: %s\n", filepath);
        return false;
    }
    
    int fileSize = in->GetSize();
    outBuffer.resize(fileSize);
    in->ReadBytes(fileSize, &outBuffer[0]);
    delete in;
    
    return true;
}

static bool ParseGLTFData(const char* filepath, const std::vector<unsigned char>& buffer, tinygltf::Model& outModel, std::string& outErr, std::string& outWarn)
{
    tinygltf::TinyGLTF loader;
    
    const char* extension = GetExtensionPart(filepath);
    bool ret = false;
    
    if (stricmp(extension, "glb") == 0) {
        ret = loader.LoadBinaryFromMemory(&outModel, &outErr, &outWarn, &buffer[0], (int)buffer.size(), "");
    } else {
        ret = loader.LoadASCIIFromString(&outModel, &outErr, &outWarn, (const char*)&buffer[0], (int)buffer.size(), "");
    }
    
    if (!outWarn.empty()) {
        AppDebugOut("ModelLoader Warning (%s): %s\n", filepath, outWarn.c_str());
    }
    
    if (!outErr.empty()) {
        AppDebugOut("ModelLoader Error (%s): %s\n", filepath, outErr.c_str());
        return false;
    }
    
    if (!ret) {
        AppDebugOut("ModelLoader: Failed to parse glTF data: %s\n", filepath);
        return false;
    }
    
    return true;
}

static void ExtractModelDirectory(const char* filepath, char* outDirectory, size_t directorySize)
{
    strncpy(outDirectory, filepath, directorySize - 1);
    outDirectory[directorySize - 1] = '\0';
    
    char* lastSlash = strrchr(outDirectory, '/');
    if (!lastSlash) {
        lastSlash = strrchr(outDirectory, '\\');
    }
    
    if (lastSlash) {
        *(lastSlash + 1) = '\0';
    } else {
        outDirectory[0] = '\0';
    }
}

static bool ValidateScene(const tinygltf::Model& gltfModel, int& outSceneIndex)
{
    outSceneIndex = gltfModel.defaultScene >= 0 ? gltfModel.defaultScene : 0;
    
    if (outSceneIndex < 0 || outSceneIndex >= (int)gltfModel.scenes.size()) {
        return false;
    }
    
    return true;
}

static void GetIdentityMatrix(float* outMatrix)
{
    outMatrix[0] = 1; outMatrix[1] = 0; outMatrix[2] = 0; outMatrix[3] = 0;
    outMatrix[4] = 0; outMatrix[5] = 1; outMatrix[6] = 0; outMatrix[7] = 0;
    outMatrix[8] = 0; outMatrix[9] = 0; outMatrix[10] = 1; outMatrix[11] = 0;
    outMatrix[12] = 0; outMatrix[13] = 0; outMatrix[14] = 0; outMatrix[15] = 1;
}


static void LoadPBRFactors(ModelMaterial* mat, const tinygltf::Material& gltfMat)
{
    const auto& pbr = gltfMat.pbrMetallicRoughness;
    
    if (pbr.baseColorFactor.size() >= 4) {
        mat->baseColorFactor = Colour(
            (int)(pbr.baseColorFactor[0] * 255.0),
            (int)(pbr.baseColorFactor[1] * 255.0),
            (int)(pbr.baseColorFactor[2] * 255.0),
            (int)(pbr.baseColorFactor[3] * 255.0)
        );
    }
    
    mat->metallicFactor = (float)pbr.metallicFactor;
    mat->roughnessFactor = (float)pbr.roughnessFactor;
    
    if (gltfMat.emissiveFactor.size() >= 3) {
        mat->emissiveFactor = Colour(
            (int)(gltfMat.emissiveFactor[0] * 255.0),
            (int)(gltfMat.emissiveFactor[1] * 255.0),
            (int)(gltfMat.emissiveFactor[2] * 255.0),
            255
        );
    }
}

static void LoadAlphaMode(ModelMaterial* mat, const tinygltf::Material& gltfMat)
{
    if (gltfMat.alphaMode == "OPAQUE") {
        mat->alphaMode = ModelMaterial::ALPHA_OPAQUE;
    } else if (gltfMat.alphaMode == "MASK") {
        mat->alphaMode = ModelMaterial::ALPHA_MASK;
        mat->alphaCutoff = (float)gltfMat.alphaCutoff;
    } else if (gltfMat.alphaMode == "BLEND") {
        mat->alphaMode = ModelMaterial::ALPHA_BLEND;
    }
}

static Bitmap* CreateBitmapFromImageData(const tinygltf::Image& image)
{
    Bitmap* bitmap = new Bitmap(image.width, image.height);
    
    int components = image.component;
    if (components < 1 || components > 4) {
        components = 4;
    }
    
    const unsigned char* srcData = image.image.data();
    int width = image.width;
    int height = image.height;
    
    for (int y = 0; y < height; y++) {
        int destY = height - 1 - y;
        for (int x = 0; x < width; x++) {
            int srcIdx = (y * width + x) * components;
            int destIdx = destY * width + x;
            
            int r = 255, g = 255, b = 255, a = 255;
            
            if (components >= 1) r = srcData[srcIdx];
            if (components >= 2) g = srcData[srcIdx + 1];
            if (components >= 3) b = srcData[srcIdx + 2];
            if (components >= 4) a = srcData[srcIdx + 3];
            
            bitmap->m_pixels[destIdx] = Colour(r, g, b, a);
        }
    }
    
    return bitmap;
}

static unsigned int LoadEmbeddedTexture(const tinygltf::Image& image, Image** outImage)
{
    Bitmap* bitmap = CreateBitmapFromImageData(image);
    Image* img = new Image(bitmap);
    img->MakeTexture(true, true);
    
    unsigned int textureID = img->m_textureID;
    
    if (outImage) {
        *outImage = img;
    }
    
    return textureID;
}

static unsigned int LoadExternalTexture(const char* modelDirectory, const tinygltf::Image& image, Image** outImage)
{
    char texturePath[512];
    
    if (image.uri.empty()) {
        AppDebugOut("ModelLoader: Warning - texture has no URI and no embedded data\n");
        return 0;
    }
    
    snprintf(texturePath, sizeof(texturePath), "%s%s", modelDirectory, image.uri.c_str());
    texturePath[sizeof(texturePath) - 1] = '\0';
    
    Image* img = g_resource->GetImage(texturePath);
    
    if (img && img->m_textureID > 0) {
        if (outImage) {
            *outImage = img;
        }
        return img->m_textureID;
    }
    
    AppDebugOut("ModelLoader: Warning - Failed to load texture: %s\n", texturePath);
    return 0;
}

//
// GLTF data access chain: Primitive -> Accessor -> BufferView -> Buffer
// Accessor defines the data format, BufferView defines the byte range, Buffer holds raw data

static const float* GetAccessorData(const tinygltf::Model& gltfModel, const std::string& attributeName, 
                                   const tinygltf::Primitive& primitive, const tinygltf::Accessor** outAccessor)
{
    auto it = primitive.attributes.find(attributeName);
    if (it == primitive.attributes.end()) {
        return NULL;
    }
    
    int accessorIndex = it->second;
    if (accessorIndex < 0 || accessorIndex >= (int)gltfModel.accessors.size()) {
        return NULL;
    }
    
    const tinygltf::Accessor& accessor = gltfModel.accessors[accessorIndex];
    if (accessor.bufferView < 0 || accessor.bufferView >= (int)gltfModel.bufferViews.size()) {
        return NULL;
    }
    
    const tinygltf::BufferView& bufferView = gltfModel.bufferViews[accessor.bufferView];
    if (bufferView.buffer < 0 || bufferView.buffer >= (int)gltfModel.buffers.size()) {
        return NULL;
    }
    
    const tinygltf::Buffer& buffer = gltfModel.buffers[bufferView.buffer];
    
    if (outAccessor) {
        *outAccessor = &accessor;
    }
    
    return reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
}

//
// Loads vertex positions and applies world transform immediately
// This bakes the node hierarchy transforms into the vertex data

static void LoadPositions(ModelMesh& meshData, const tinygltf::Model& gltfModel, 
                         const tinygltf::Primitive& primitive, const float* worldTransform)
{
    const tinygltf::Accessor* accessor = NULL;
    const float* positions = GetAccessorData(gltfModel, "POSITION", primitive, &accessor);
    
    if (!positions || !accessor) {
        return;
    }
    
    meshData.positions.resize(accessor->count * 3);
    for (size_t k = 0; k < accessor->count; k++) {
        meshData.positions[k * 3 + 0] = positions[k * 3 + 0];
        meshData.positions[k * 3 + 1] = positions[k * 3 + 1];
        meshData.positions[k * 3 + 2] = positions[k * 3 + 2];
        
        Matrix4f::TransformVertex(worldTransform, &meshData.positions[k * 3]);
    }
}

static void LoadNormals(ModelMesh& meshData, const tinygltf::Model& gltfModel, 
                       const tinygltf::Primitive& primitive, const float* worldTransform)
{
    const tinygltf::Accessor* accessor = NULL;
    const float* normals = GetAccessorData(gltfModel, "NORMAL", primitive, &accessor);
    
    if (!normals || !accessor) {
        return;
    }
    
    meshData.normals.resize(accessor->count * 3);
    for (size_t k = 0; k < accessor->count; k++) {
        meshData.normals[k * 3 + 0] = normals[k * 3 + 0];
        meshData.normals[k * 3 + 1] = normals[k * 3 + 1];
        meshData.normals[k * 3 + 2] = normals[k * 3 + 2];
        
        Matrix4f::TransformNormal(worldTransform, &meshData.normals[k * 3]);
    }
}

//
// GLTF supports multiple texture coordinate formats: float, normalized byte, normalized short
// byteStride allows interleaved vertex data (0 means tightly packed)

static void LoadTexCoords(ModelMesh& meshData, const tinygltf::Model& gltfModel, 
                         const tinygltf::Primitive& primitive)
{
    auto it = primitive.attributes.find("TEXCOORD_0");
    if (it == primitive.attributes.end()) {
        return;
    }
    
    int accessorIndex = it->second;
    if (accessorIndex < 0 || accessorIndex >= (int)gltfModel.accessors.size()) {
        return;
    }
    
    const tinygltf::Accessor& accessor = gltfModel.accessors[accessorIndex];
    if (accessor.bufferView < 0 || accessor.bufferView >= (int)gltfModel.bufferViews.size()) {
        return;
    }
    
    const tinygltf::BufferView& bufferView = gltfModel.bufferViews[accessor.bufferView];
    if (bufferView.buffer < 0 || bufferView.buffer >= (int)gltfModel.buffers.size()) {
        return;
    }
    
    const tinygltf::Buffer& buffer = gltfModel.buffers[bufferView.buffer];
    
    size_t byteStride = bufferView.byteStride;
    if (byteStride == 0) {
        byteStride = sizeof(float) * 2;
    }
    
    meshData.texCoords.resize(accessor.count * 2);
    
    if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
        for (size_t k = 0; k < accessor.count; k++) {
            const unsigned char* basePtr = &buffer.data[bufferView.byteOffset + accessor.byteOffset + (k * byteStride)];
            const float* texCoord = reinterpret_cast<const float*>(basePtr);
            meshData.texCoords[k * 2 + 0] = texCoord[0];
            meshData.texCoords[k * 2 + 1] = texCoord[1];
        }
    } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE && accessor.normalized) {
        for (size_t k = 0; k < accessor.count; k++) {
            const unsigned char* basePtr = &buffer.data[bufferView.byteOffset + accessor.byteOffset + (k * byteStride)];
            meshData.texCoords[k * 2 + 0] = basePtr[0] / 255.0f;
            meshData.texCoords[k * 2 + 1] = basePtr[1] / 255.0f;
        }
    } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT && accessor.normalized) {
        for (size_t k = 0; k < accessor.count; k++) {
            const unsigned char* basePtr = &buffer.data[bufferView.byteOffset + accessor.byteOffset + (k * byteStride)];
            const unsigned short* texCoord = reinterpret_cast<const unsigned short*>(basePtr);
            meshData.texCoords[k * 2 + 0] = texCoord[0] / 65535.0f;
            meshData.texCoords[k * 2 + 1] = texCoord[1] / 65535.0f;
        }
    }
}

static void LoadIndices(ModelMesh& meshData, const tinygltf::Model& gltfModel, 
                       const tinygltf::Primitive& primitive)
{
    if (primitive.indices < 0) {
        return;
    }
    
    if (primitive.indices >= (int)gltfModel.accessors.size()) {
        return;
    }
    
    const tinygltf::Accessor& accessor = gltfModel.accessors[primitive.indices];
    if (accessor.bufferView < 0 || accessor.bufferView >= (int)gltfModel.bufferViews.size()) {
        return;
    }
    
    const tinygltf::BufferView& bufferView = gltfModel.bufferViews[accessor.bufferView];
    if (bufferView.buffer < 0 || bufferView.buffer >= (int)gltfModel.buffers.size()) {
        return;
    }
    
    const tinygltf::Buffer& buffer = gltfModel.buffers[bufferView.buffer];
    
    meshData.indices.resize(accessor.count);
    
    if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
        const unsigned int* indices = reinterpret_cast<const unsigned int*>(
            &buffer.data[bufferView.byteOffset + accessor.byteOffset]
        );
        for (size_t k = 0; k < accessor.count; k++) {
            meshData.indices[k] = indices[k];
        }
    } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
        const unsigned short* indices = reinterpret_cast<const unsigned short*>(
            &buffer.data[bufferView.byteOffset + accessor.byteOffset]
        );
        for (size_t k = 0; k < accessor.count; k++) {
            meshData.indices[k] = (unsigned int)indices[k];
        }
    } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
        const unsigned char* indices = reinterpret_cast<const unsigned char*>(
            &buffer.data[bufferView.byteOffset + accessor.byteOffset]
        );
        for (size_t k = 0; k < accessor.count; k++) {
            meshData.indices[k] = (unsigned int)indices[k];
        }
    }
}

//
// Calculates face normals using cross product of triangle edges
// Accumulates normals per vertex (shared by multiple faces) for smooth shading

static void CalculateFaceNormals(ModelMesh& mesh)
{
    for (size_t i = 0; i < mesh.indices.size(); i += 3) {
        unsigned int i0 = mesh.indices[i + 0];
        unsigned int i1 = mesh.indices[i + 1];
        unsigned int i2 = mesh.indices[i + 2];
        
        Vector3<float> v0(mesh.positions[i0*3+0], mesh.positions[i0*3+1], mesh.positions[i0*3+2]);
        Vector3<float> v1(mesh.positions[i1*3+0], mesh.positions[i1*3+1], mesh.positions[i1*3+2]);
        Vector3<float> v2(mesh.positions[i2*3+0], mesh.positions[i2*3+1], mesh.positions[i2*3+2]);
        
        Vector3<float> edge1 = v1 - v0;
        Vector3<float> edge2 = v2 - v0;
        Vector3<float> normal = edge1 ^ edge2;
        
        mesh.normals[i0*3+0] += normal.x; mesh.normals[i0*3+1] += normal.y; mesh.normals[i0*3+2] += normal.z;
        mesh.normals[i1*3+0] += normal.x; mesh.normals[i1*3+1] += normal.y; mesh.normals[i1*3+2] += normal.z;
        mesh.normals[i2*3+0] += normal.x; mesh.normals[i2*3+1] += normal.y; mesh.normals[i2*3+2] += normal.z;
    }
}

static void NormalizeVertexNormals(ModelMesh& mesh)
{
    for (size_t i = 0; i < mesh.normals.size(); i += 3) {
        Vector3<float> normal(mesh.normals[i+0], mesh.normals[i+1], mesh.normals[i+2]);
        normal.Normalise();
        mesh.normals[i+0] = normal.x;
        mesh.normals[i+1] = normal.y;
        mesh.normals[i+2] = normal.z;
    }
}

static void ProcessPrimitive(ModelMesh& meshData, Model* model, const tinygltf::Model& gltfModel, 
                            const tinygltf::Primitive& primitive, const float* worldTransform)
{
    if (primitive.material >= 0 && primitive.material < (int)model->m_materials.size()) {
        meshData.material = model->m_materials[primitive.material];
    } else {
        meshData.material = ModelMaterial::GetDefaultMaterial();
    }
    
    LoadPositions(meshData, gltfModel, primitive, worldTransform);
    LoadNormals(meshData, gltfModel, primitive, worldTransform);
    LoadTexCoords(meshData, gltfModel, primitive);
    LoadIndices(meshData, gltfModel, primitive);
    
    if (meshData.normals.empty() && !meshData.positions.empty() && !meshData.indices.empty()) {
        meshData.normals.resize(meshData.positions.size(), 0.0f);
        CalculateFaceNormals(meshData);
        NormalizeVertexNormals(meshData);
    }
}

bool ModelLoader::LoadFromGLTF(Model* model, const char *filepath)
{
    if (!model) {
        return false;
    }
    
    std::vector<unsigned char> buffer;
    if (!ReadGLTFFile(filepath, buffer)) {
        return false;
    }
    
    tinygltf::Model gltfModel;
    std::string err, warn;
    if (!ParseGLTFData(filepath, buffer, gltfModel, err, warn)) {
        return false;
    }
    
    char modelDirectory[512];
    ExtractModelDirectory(filepath, modelDirectory, sizeof(modelDirectory));
    
    LoadMaterials(model, gltfModel, modelDirectory);
    
    int sceneIndex;
    if (!ValidateScene(gltfModel, sceneIndex)) {
        AppDebugOut("ModelLoader: No valid scene found in: %s\n", filepath);
        return false;
    }
    
    const tinygltf::Scene& scene = gltfModel.scenes[sceneIndex];
    
    float identityMatrix[16];
    GetIdentityMatrix(identityMatrix);
    
    for (size_t i = 0; i < scene.nodes.size(); i++) {
        ProcessNode(model, gltfModel, scene.nodes[i], identityMatrix);
    }
    
    model->m_loaded = true;
    model->CalculateBounds();
    
    return true;
}

void ModelLoader::LoadMaterials(Model* model, const tinygltf::Model& gltfModel, const char* modelDirectory)
{
    if (!model) return;
    
    model->m_materials.clear();
    
    if (gltfModel.materials.empty()) {
        ModelMaterial* defaultMat = new ModelMaterial();
        defaultMat->name = "Default";
        defaultMat->index = 0;
        model->m_materials.push_back(defaultMat);
        return;
    }
    
    for (size_t i = 0; i < gltfModel.materials.size(); i++) {
        ModelMaterial* material = LoadMaterial(gltfModel.materials[i], gltfModel, modelDirectory, (int)i);
        model->m_materials.push_back(material);
    }
}

ModelMaterial* ModelLoader::LoadMaterial(const tinygltf::Material& gltfMat, const tinygltf::Model& gltfModel, 
                                         const char* modelDirectory, int matIndex)
{
    ModelMaterial* mat = new ModelMaterial();
    mat->name = gltfMat.name;
    mat->index = matIndex;
    
    const auto& pbr = gltfMat.pbrMetallicRoughness;
    
    if (pbr.baseColorTexture.index >= 0) {
        mat->baseColorTexture.textureID = LoadTexture(gltfModel, pbr.baseColorTexture.index, modelDirectory, &mat->baseColorTexture.image);
        mat->baseColorTexture.texCoordSet = pbr.baseColorTexture.texCoord;
    }
    
    if (pbr.metallicRoughnessTexture.index >= 0) {
        mat->metallicRoughnessTexture.textureID = LoadTexture(gltfModel, pbr.metallicRoughnessTexture.index, modelDirectory, &mat->metallicRoughnessTexture.image);
        mat->metallicRoughnessTexture.texCoordSet = pbr.metallicRoughnessTexture.texCoord;
    }
    
    if (gltfMat.normalTexture.index >= 0) {
        mat->normalTexture.textureID = LoadTexture(gltfModel, gltfMat.normalTexture.index, modelDirectory, &mat->normalTexture.image);
        mat->normalTexture.texCoordSet = gltfMat.normalTexture.texCoord;
        mat->normalTexture.scale = (float)gltfMat.normalTexture.scale;
    }
    
    if (gltfMat.occlusionTexture.index >= 0) {
        mat->occlusionTexture.textureID = LoadTexture(gltfModel, gltfMat.occlusionTexture.index, modelDirectory, &mat->occlusionTexture.image);
        mat->occlusionTexture.texCoordSet = gltfMat.occlusionTexture.texCoord;
    }
    
    if (gltfMat.emissiveTexture.index >= 0) {
        mat->emissiveTexture.textureID = LoadTexture(gltfModel, gltfMat.emissiveTexture.index, modelDirectory, &mat->emissiveTexture.image);
        mat->emissiveTexture.texCoordSet = gltfMat.emissiveTexture.texCoord;
    }
    
    LoadPBRFactors(mat, gltfMat);
    LoadAlphaMode(mat, gltfMat);
    
    mat->doubleSided = gltfMat.doubleSided;
    
    AppDebugOut("ModelLoader: Loaded material '%s' (base color tex: %u, normal tex: %u)\n", 
                mat->name.c_str(), mat->baseColorTexture.textureID, mat->normalTexture.textureID);
    
    return mat;
}

unsigned int ModelLoader::LoadTexture(const tinygltf::Model& gltfModel, int textureIndex, const char* modelDirectory, Image** outImage)
{
    if (textureIndex < 0 || textureIndex >= (int)gltfModel.textures.size()) {
        return 0;
    }
    
    const tinygltf::Texture& tex = gltfModel.textures[textureIndex];
    
    if (tex.source < 0 || tex.source >= (int)gltfModel.images.size()) {
        return 0;
    }
    
    const tinygltf::Image& image = gltfModel.images[tex.source];
    
    if (image.uri.empty() && !image.image.empty() && image.width > 0 && image.height > 0) {
        return LoadEmbeddedTexture(image, outImage);
    }
    
    return LoadExternalTexture(modelDirectory, image, outImage);
}

//
// Recursively processes node hierarchy, accumulating transforms: world = parent * local

void ModelLoader::ProcessNode(Model* model, const tinygltf::Model& gltfModel, int nodeIndex, const float* parentTransform)
{
    if (!model) return;
    
    if (nodeIndex < 0 || nodeIndex >= (int)gltfModel.nodes.size()) {
        return;
    }
    
    const tinygltf::Node& node = gltfModel.nodes[nodeIndex];
    
    float localTransform[16];
    Matrix4f::BuildTransformMatrix(node.translation, node.rotation, node.scale, node.matrix, localTransform);
    
    float worldTransform[16];
    Matrix4f::Multiply(parentTransform, localTransform, worldTransform);
    
    if (node.mesh >= 0 && node.mesh < (int)gltfModel.meshes.size()) {
        const tinygltf::Mesh& mesh = gltfModel.meshes[node.mesh];
        
        for (size_t j = 0; j < mesh.primitives.size(); j++) {
            ModelMesh meshData;
            ProcessPrimitive(meshData, model, gltfModel, mesh.primitives[j], worldTransform);
            model->m_meshes.push_back(meshData);
        }
    }
    
    for (size_t i = 0; i < node.children.size(); i++) {
        ProcessNode(model, gltfModel, node.children[i], worldTransform);
    }
}

void ModelLoader::GenerateNormals(ModelMesh& mesh)
{
    mesh.normals.resize(mesh.positions.size(), 0.0f);
    CalculateFaceNormals(mesh);
    NormalizeVertexNormals(mesh);
    AppDebugOut("ModelLoader: Generated normals for mesh\n");
}

