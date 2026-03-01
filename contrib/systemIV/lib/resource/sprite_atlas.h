#ifndef _included_sprite_atlas_h
#define _included_sprite_atlas_h

#include "lib/tosser/hash_table.h"
#include "lib/tosser/llist.h"
#include "lib/resource/image.h"

class Bitmap;

struct AtlasCoord {
    float u1, v1, u2, v2;  // UV coordinates, normalized 0.0 - 1.0
    int width, height;     // original sprite dimensions
    int pixelX, pixelY;    // pixel coordinates in atlas for blitting
    
    constexpr AtlasCoord() : u1(0), v1(0), u2(0), v2(0), width(0), height(0), pixelX(0), pixelY(0) {}
    
    AtlasCoord(int x, int y, int w, int h, int atlasWidth, int atlasHeight);
};

struct PackedSprite {
    char* filename;                 // Full path like "graphics/bomber.bmp"
    AtlasCoord coord;               // UV coordinates in atlas
    Bitmap* sourceBitmap;           // Original bitmap (temporary, deleted after packing)
    
    PackedSprite();
    ~PackedSprite();
};

class RuntimeTextureAtlas {
private:
    int m_width, m_height;                      // Final atlas dimensions
    unsigned int m_textureID;                   // OpenGL texture ID
    Bitmap* m_atlasBitmap;                      // Combined atlas bitmap
    HashTable<PackedSprite*> m_spriteMap;       // filename -> PackedSprite lookup
    char* m_name;                               // Atlas name for debugging
    
    bool PackSprites              (PackedSprite** sprites, int spriteCount, 
                                   int maxWidth = 4096, int maxHeight = 4096);
    void BlitSpritesToAtlas       (PackedSprite** sprites, int spriteCount);
    void ExtrudeSpriteEdges       (Bitmap* src, int x, int y, int w, int h);
    void CreateGLTexture          ();
    void LoadSprites              (const char* directory, 
                                   const char* const *excludeList, int excludeCount,
                                   PackedSprite*** outSprites, int* outCount);
    
public:
    RuntimeTextureAtlas           (const char* name);
    ~RuntimeTextureAtlas          ();
    
    bool BuildFromDirectory       (const char* directory, 
                                   const char* const *excludeList = nullptr,
                                   int excludeCount = 0);
    
    const PackedSprite* GetSprite (const char* filename) const;
    
    int GetWidth             () const;
    int GetHeight            () const;
    int GetSpriteCount       () const;
    bool HasSprite           (const char* filename) const;

    unsigned int GetTextureID() const;
    const char* GetName      () const;
};

//
// atlas image class

class AtlasImage : public Image {
private:
    PackedSprite* m_packedSprite;
    RuntimeTextureAtlas* m_atlas;
    
public:
    AtlasImage(PackedSprite* sprite, RuntimeTextureAtlas* atlas);
    virtual ~AtlasImage();
    virtual int Width();
    virtual int Height();
    
    const AtlasCoord* GetAtlasCoord() const;
    const char* GetFilename() const;
    
    unsigned int GetAtlasTextureID() const;
    
    RuntimeTextureAtlas* GetAtlas() const;
};

//
// global atlas manager

class SpriteAtlasManager {
private:
    LList<RuntimeTextureAtlas*> m_atlases;
    bool m_initialized;
    
public:
    SpriteAtlasManager();
    ~SpriteAtlasManager();
    
    bool Initialize();
    void Rebuild();
    void Shutdown();
    bool IsAtlasSprite                       (const char* filename) const;

    const PackedSprite* GetSpriteFromAnyAtlas(const char* filename, 
                                               RuntimeTextureAtlas** outAtlas) const;
    
    bool IsInitialized                       () const;
};

extern SpriteAtlasManager* g_spriteAtlasManager;

#endif