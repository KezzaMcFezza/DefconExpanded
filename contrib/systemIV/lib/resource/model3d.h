#ifndef _included_model3d_h
#define _included_model3d_h

#include "lib/math/vector3.h"
#include "lib/math/matrix4f.h"
#include <tiny_gltf.h>
#include <vector>

struct Model3DMesh {
    std::vector<float> positions;
    std::vector<unsigned int> indices;
};

class Model3D
{
public:
    std::vector<Model3DMesh> m_meshes;
    
    Vector3<float> m_boundsMin;
    Vector3<float> m_boundsMax;
    
    bool m_loaded;
    char m_filename[512];
    
public:
    Model3D( const char *filename );
    virtual ~Model3D();
    
    bool IsLoaded() const;
    
    int GetMeshCount() const;
    const Model3DMesh* GetMesh(int index) const;
    
    void  GetBounds      (Vector3<float>& outMin, Vector3<float>& outMax) const;
    float GetBoundsRadius() const;
    
    void BuildModelVBO();
    
private:
    bool LoadFromGLTF     (const char *filepath);
    void CalculateBounds  ();
    void ProcessNode      (const tinygltf::Model& model, int nodeIndex, const float* parentTransform);
    char* GetModelCacheKey() const;
};

#endif