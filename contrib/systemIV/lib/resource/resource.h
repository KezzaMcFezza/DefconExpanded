 
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
class Model;



class Resource
{
protected:
    BTree   <Image *>           m_imageCache;
    BTree   <BitmapFont *>      m_bitmapFontCache;
    BTree   <bool>              m_testBitmapFontCache;
    BTree   <Model *>         m_modelCache;

    SpriteAtlasManager* m_spriteAtlasManager;

    Image   *TryLoadFromAtlas   ( const char *_filename, float uvAdjustX = 0.0f, float uvAdjustY = 0.0f );

public:
    Resource();
    ~Resource();

    void            InitializeAtlases();
    void            Restart();
    void            Shutdown();

    Image           *GetImage           ( const char *_filename );
    Image           *GetImage           ( const char *_filename, float uvAdjustX, float uvAdjustY );
    void            UnloadImage         ( const char *_filename );
    void            UnloadImage         ( const char *_filename, float uvAdjustX, float uvAdjustY );
    BitmapFont      *GetBitmapFont      ( const char *_filename );
    bool            TestBitmapFont      ( const char *_filename );
    
    Model           *GetModel           ( const char *_filename );
    void            UnloadModel         ( const char *_filename );
};


extern Resource *g_resource;

#endif

