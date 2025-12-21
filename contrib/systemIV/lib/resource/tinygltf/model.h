#ifndef _included_model_h
#define _included_model_h

#include "lib/math/vector3.h"
#include <vector>
#include <string>

class ModelMaterial;

struct ModelMesh {
    std::vector<float> positions;
    std::vector<float> texCoords;
    std::vector<float> normals;
    std::vector<unsigned int> indices;
    
    ModelMaterial* material;
    
    ModelMesh() : material(NULL) {}
};

class Model
{
public:
    std::vector<ModelMesh> m_meshes;
    std::vector<ModelMaterial*> m_materials;
    
    Vector3<float> m_boundsMin;
    Vector3<float> m_boundsMax;
    
    bool m_loaded;
    char m_filename[512];
    char* m_cacheKey;
    std::vector<std::string> m_vboCacheKeys;
    
public:
    Model();
    Model(const char *filename);
    virtual ~Model();
    
    bool IsLoaded() const;
    
    int GetMeshCount() const;
    const ModelMesh* GetMesh(int index) const;
    
    int GetMaterialCount() const;
    ModelMaterial* GetMaterial(int index);
    
    void  GetBounds      (Vector3<float>& outMin, Vector3<float>& outMax) const;
    float GetBoundsRadius() const;
    
    const char* GetCacheKey() const;
    int GetVBOCount() const;
    const char* GetVBOCacheKey(int index) const;
    
    void SetCacheKey(const char* key);
    void AddVBOCacheKey(const char* key);
    void ClearVBOCacheKeys();
    
    void CalculateBounds();
    
    static float FlipVCoordinate(float v) { return 1.0f - v; }
};

#endif

