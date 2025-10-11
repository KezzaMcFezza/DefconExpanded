#include "lib/universal_include.h"

#include <stdio.h>
#include <fstream>
#include <unrar/unrar.h>

#include "lib/resource/bitmap.h"
#include "lib/resource/resource.h"
#include "lib/resource/bitmapfont.h"
#include "lib/resource/image.h"
#include "lib/filesys/binary_stream_readers.h"
#include "lib/filesys/file_system.h"
#include "lib/render2d/renderer.h"
#include "lib/render3d/renderer_3d.h"
#include "app/modsystem.h"

#include "sprite_atlas.h"



Resource *g_resource = NULL;



Resource::Resource()
{
    m_spriteAtlasManager = NULL;
}

void Resource::InitializeAtlases()
{
    if (!g_spriteAtlasManager) {
        g_spriteAtlasManager = new SpriteAtlasManager();
        g_spriteAtlasManager->Initialize();
    }
}

void Resource::Restart()
{
    Shutdown();
    
    //
    // recreate and rebuild atlases for mods

    g_spriteAtlasManager = new SpriteAtlasManager();
    g_spriteAtlasManager->Initialize();
    
    //
    // invalidate VBOs when resources restart

    extern Renderer *g_renderer;
    extern Renderer3D *g_renderer3d;

    if (g_renderer) {
        g_renderer->InvalidateAllVBOs();
        AppDebugOut("Resource restart: Invalidated all 2D VBOs for mod loading\n");
    }

    if (g_renderer3d) {
        g_renderer3d->InvalidateAll3DVBOs();
        AppDebugOut("Resource restart: Invalidated all 3D VBOs for mod loading\n");
    }
}

void Resource::Shutdown()
{

    //
    // Shutdown atlases first

    if (g_spriteAtlasManager) {
        delete g_spriteAtlasManager;
        g_spriteAtlasManager = NULL;
    }

    //
    // Image Cache

    DArray<Image *> *images = m_imageCache.ConvertToDArray();
    for( int i = 0; i < images->Size(); ++i )
    {
        Image *image = images->GetData(i);
        delete image;
    }
    delete images;
    m_imageCache.Empty();


    //
    // BitmapFont cache

    DArray<BitmapFont *> *fonts = m_bitmapFontCache.ConvertToDArray();
    for( int i = 0; i < fonts->Size(); ++i )
    {
        BitmapFont *font = fonts->GetData(i);
        delete font;
    }
    delete fonts;
    m_bitmapFontCache.Empty();


    //
    // TestBitmapFont cache

    m_testBitmapFontCache.Empty();
}


Image *Resource::GetImage( const char *filename )
{   
    if( !filename ) return NULL;
    
    char fullFilename[512];
    sprintf( fullFilename, "data/%s", filename );

    Image *image = m_imageCache.GetData( fullFilename );
    if( image )
    {
        return image;
    }
    
    //
    // check if this is a mod graphic
    
    extern class ModSystem *g_modSystem;
    bool isModGraphic = g_modSystem && g_modSystem->IsModGraphic(filename);
    
    //
    // try runtime atlas ONLY if not a mod graphic and atlas is initialized
    
    if (!isModGraphic && g_spriteAtlasManager && g_spriteAtlasManager->IsInitialized()) {
        RuntimeTextureAtlas* atlas = NULL;
        const PackedSprite* sprite = g_spriteAtlasManager->GetSpriteFromAnyAtlas(filename, &atlas);
        
        if (sprite && atlas) {

            //
            // create AtlasImage from runtime-packed sprite
            
            AtlasImage* atlasImage = new AtlasImage((PackedSprite*)sprite, atlas);
            image = atlasImage;
            
            m_imageCache.PutData( fullFilename, image );
            return image;
        }
    }
    
    image = new Image( fullFilename );
    m_imageCache.PutData( fullFilename, image );
    image->MakeTexture( true, true );
    return image;
}


BitmapFont *Resource::GetBitmapFont ( const char *filename )
{
    if( !filename ) return NULL;

    char fullFilename[512];
    snprintf( fullFilename, sizeof(fullFilename), "data/%s", filename );
    fullFilename[ sizeof(fullFilename) - 1 ] = '\x0';

    BitmapFont *font = m_bitmapFontCache.GetData( fullFilename );
    if( font )
    {
        return font;
    }
    else
    {
        font = new BitmapFont( fullFilename );
        m_bitmapFontCache.PutData( fullFilename, font );
        return font;
    }
}


bool Resource::TestBitmapFont ( const char *filename )
{
    if( !filename ) return false;

    char fullFilename[512];
    snprintf( fullFilename, sizeof(fullFilename), "data/%s", filename );
    fullFilename[ sizeof(fullFilename) - 1 ] = '\x0';

    BitmapFont *font = m_bitmapFontCache.GetData( fullFilename );
    if( font )
    {
        return true;
    }

    bool fontNotFound = m_testBitmapFontCache.GetData( fullFilename );
    if( fontNotFound )
    {
        return false;
    }

    BinaryReader *reader = g_fileSystem->GetBinaryReader( fullFilename );
    if( reader )
    {
        delete reader;
        return true;
    }
    m_testBitmapFontCache.PutData( fullFilename, true );
    return false;
}