#ifndef _included_modelmaterial_h
#define _included_modelmaterial_h

#include "lib/render/colour.h"
#include "lib/math/vector2.h"
#include <string>

class Image;

class ModelMaterial {
public:
    
    struct TextureInfo {
        Image* image;               // Using your existing Image class
        unsigned int textureID;     // OpenGL texture ID
        int texCoordSet;            // Which UV channel (0 or 1)
        float scale;                // UV scale
        Vector2 offset;             // UV offset
        
        TextureInfo() 
            : image(NULL), textureID(0), texCoordSet(0), 
              scale(1.0f), offset(0.0f, 0.0f) {
        }
    };
    
    enum AlphaMode {
        ALPHA_OPAQUE,       // Fully opaque
        ALPHA_MASK,         // Binary transparency (use alphaCutoff)
        ALPHA_BLEND         // Blended transparency
    };
    
    //
    // PBR Texture maps
    
    TextureInfo baseColorTexture;           // Albedo/diffuse color
    TextureInfo normalTexture;              // Normal map
    TextureInfo metallicRoughnessTexture;   // Metallic (B) + Roughness (G)
    TextureInfo occlusionTexture;           // Ambient occlusion (R)
    TextureInfo emissiveTexture;            // Emissive/glow map
    
    //
    // PBR Materials

    Colour baseColorFactor;         // Base color tint
    float metallicFactor;           // Metallic value [0-1]
    float roughnessFactor;          // Roughness value [0-1]
    Colour emissiveFactor;          // Emissive color/intensity
    
    AlphaMode alphaMode;
    float alphaCutoff;              // For ALPHA_MASK mode
    bool doubleSided;               // Disable backface culling
    std::string name;
    int index;                      // Index in model's material array
    
    ModelMaterial();
    ~ModelMaterial();
    
    bool HasBaseColorTexture        () const { return baseColorTexture.textureID != 0; }
    bool HasNormalTexture           () const { return normalTexture.textureID != 0; }
    bool HasMetallicRoughnessTexture() const { return metallicRoughnessTexture.textureID != 0; }
    bool HasOcclusionTexture        () const { return occlusionTexture.textureID != 0; }
    bool HasEmissiveTexture         () const { return emissiveTexture.textureID != 0; }
    
    unsigned int GetBaseColorTextureID        () const { return baseColorTexture.textureID; }
    unsigned int GetNormalTextureID           () const { return normalTexture.textureID; }
    unsigned int GetMetallicRoughnessTextureID() const { return metallicRoughnessTexture.textureID; }
    unsigned int GetOcclusionTextureID        () const { return occlusionTexture.textureID; }
    unsigned int GetEmissiveTextureID         () const { return emissiveTexture.textureID; }
    
    bool IsTransparent() const { 
        return (alphaMode == ALPHA_BLEND) || 
               (alphaMode == ALPHA_MASK && baseColorFactor.m_a < 255);
    }
    
    static ModelMaterial* GetDefaultMaterial();
    
private:
    static ModelMaterial* s_defaultMaterial;
};

#endif