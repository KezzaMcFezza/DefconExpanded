#include "lib/universal_include.h"

#include <stdio.h>
#include <fstream>
#include <unrar/unrar.h>

#include "lib/resource/bitmap.h"
#include "lib/resource/resource.h"
#include "lib/resource/bitmapfont.h"
#include "lib/resource/image.h"
#include "lib/resource/model3d.h"
#include "lib/filesys/binary_stream_readers.h"
#include "lib/filesys/file_system.h"
#include "lib/render/renderer.h"
#include "lib/render2d/renderer_2d.h"
#include "lib/render3d/renderer_3d.h"
#include "lib/render2d/megavbo/megavbo_2d.h"
#include "lib/render3d/megavbo/megavbo_3d.h"
#include "app/modsystem.h"

#include "sprite_atlas.h"



Resource *g_resource = NULL;



Resource::Resource()
{
    m_spriteAtlasManager = NULL;
}

Resource::~Resource()
{
    Shutdown();
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

    if (g_renderer2d) {
        g_renderer2dvbo->InvalidateAllVBOs();
        AppDebugOut("Resource restart: Invalidated all 2D VBOs for mod loading\n");
    }

    if (g_renderer3d) {
        g_renderer3dvbo->InvalidateAll3DVBOs();
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

    //
    // Model3D cache

    DArray<Model3D *> *models = m_model3DCache.ConvertToDArray();
    for( int i = 0; i < models->Size(); ++i )
    {
        Model3D *model = models->GetData(i);
        delete model;
    }
    delete models;
    m_model3DCache.Empty();
}


void Resource::UnloadImage( const char *filename )
{
    if( !filename ) return;
    
    char fullFilename[512];
    sprintf( fullFilename, "data/%s", filename );
    
    Image *image = m_imageCache.GetData( fullFilename );
    if( image )
    {
        delete image;
        m_imageCache.RemoveData( fullFilename );
    }
    
#ifndef NO_UNRAR
    g_fileSystem->UnloadArchiveFile( fullFilename );
#endif
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

Model3D *Resource::GetModel3D( const char *filename )
{
    if( !filename ) return NULL;
    
    char fullFilename[512];
    sprintf( fullFilename, "data/models/%s", filename );
    
    Model3D *model = m_model3DCache.GetData( fullFilename );
    if( model )
    {
        return model;
    }
    
    model = new Model3D( fullFilename );
    
    if( !model->IsLoaded() )
    {
        AppDebugOut("Model3D: Failed to load model: %s\n", fullFilename);
    }
    else
    { 
        model->BuildModelVBO();
    }
    
    m_model3DCache.PutData( fullFilename, model );
    return model;
}


void Resource::UnloadModel3D( const char *filename )
{
    if( !filename ) return;
    
    char fullFilename[512];
    sprintf( fullFilename, "data/models/%s", filename );
    
    Model3D *model = m_model3DCache.GetData( fullFilename );
    if( model )
    {
        delete model;
        m_model3DCache.RemoveData( fullFilename );
    }
    
#ifndef NO_UNRAR
    g_fileSystem->UnloadArchiveFile( fullFilename );
#endif
}