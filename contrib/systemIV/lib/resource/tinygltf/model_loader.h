#ifndef _included_modelloader_h
#define _included_modelloader_h

#include "model.h"
#include <tiny_gltf.h>

class Image;

class ModelLoader
{
public:
    static bool LoadFromGLTF(Model* model, const char *filepath);
    
private:
    static void LoadMaterials(Model* model, const tinygltf::Model& gltfModel, const char* modelDirectory);
    static ModelMaterial* LoadMaterial(const tinygltf::Material& gltfMat, const tinygltf::Model& gltfModel, const char* modelDirectory, int matIndex);
    static unsigned int LoadTexture(const tinygltf::Model& gltfModel, int textureIndex, const char* modelDirectory, Image** outImage = NULL);
    static void ProcessNode(Model* model, const tinygltf::Model& gltfModel, int nodeIndex, const float* parentTransform);
    static void GenerateNormals(ModelMesh& mesh);
};

#endif

