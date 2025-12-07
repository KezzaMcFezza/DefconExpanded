 
/*
 * ===============
 * RESOURCE SYSTEM
 * ===============
 *
 * Manages a cache of all game resources
 * Will load on request
 *
 */

#ifndef _included_resource_h
#define _included_resource_h

#include "lib/tosser/btree.h"

// Forward declarations
class Image;
class BitmapFont;
class SpriteAtlasManager;
class Model3D;



class Resource
{
protected:
    BTree   <Image *>           m_imageCache;
    BTree   <BitmapFont *>      m_bitmapFontCache;
    BTree   <bool>              m_testBitmapFontCache;
    BTree   <Model3D *>         m_model3DCache;

    SpriteAtlasManager* m_spriteAtlasManager;

public:
    Resource();
    ~Resource();

    void            InitializeAtlases();
    void            Restart();
    void            Shutdown();

    Image           *GetImage           ( const char *_filename );
    void            UnloadImage         ( const char *_filename );
    BitmapFont      *GetBitmapFont      ( const char *_filename );
    bool            TestBitmapFont      ( const char *_filename );
    
    Model3D         *GetModel3D         ( const char *_filename );
    void            UnloadModel3D       ( const char *_filename );
};


extern Resource *g_resource;

#endif

