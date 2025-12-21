#ifndef _included_modelbuilder_h
#define _included_modelbuilder_h

#include "model.h"
#include "model_material.h"
#include <vector>

class ModelBuilder
{
public:
    static void BuildModelVBO(Model* model);
    
private:
    struct MaterialGroup {
        ModelMaterial* material;
        std::vector<size_t> meshIndices;
        int totalVertices;
        int totalIndices;
    };

    static std::vector<MaterialGroup> GroupMeshesByMaterial(Model* model);
    
    static char* GetModelCacheKey     (const Model* model);
    static bool ShouldUseTexturedVBO  (const MaterialGroup& group, Model* model);
    static void BuildTexturedVBO      (Model* model, const MaterialGroup& group, const char* vboCacheKey);
    static void BuildNonTexturedVBO   (Model* model, const MaterialGroup& group, const char* vboCacheKey);
    static void GenerateVBOCacheKey   (const Model* model, const MaterialGroup& group, char* outKey, size_t keySize);
};

#endif

