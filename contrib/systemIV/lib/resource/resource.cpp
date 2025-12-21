#include "systemiv.h"

#include <stdio.h>
#include <fstream>
#include <unrar/unrar.h>

#include "lib/resource/bitmap.h"
#include "lib/resource/resource.h"
#include "lib/resource/bitmapfont.h"
#include "lib/resource/image.h"
#include "lib/resource/tinygltf/model.h"
#include "lib/resource/tinygltf/model_loader.h"
#include "lib/resource/tinygltf/model_builder.h"
#include "lib/filesys/binary_stream_readers.h"
#include "lib/filesys/file_system.h"
#include "lib/render/renderer.h"
#include "lib/render2d/renderer_2d.h"
#include "lib/render3d/renderer_3d.h"
#include "lib/render2d/megavbo/megavbo_2d.h"
#include "lib/render3d/megavbo/megavbo_3d.h"

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
    // Model cache

    DArray<Model *> *models = m_modelCache.ConvertToDArray();
    for( int i = 0; i < models->Size(); ++i )
    {
        Model *model = models->GetData(i);
        delete model;
    }
    delete models;
    m_modelCache.Empty();
}


void Resource::UnloadImage( const char *filename )
{
    if( !filename ) return;
    
    char fullFilename[512];
    sprintf( fullFilename, "%s/%s", GetDataRoot(), filename );
    
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

void Resource::UnloadImage( const char *filename, float uvAdjustX, float uvAdjustY )
{
    if( !filename ) return;
    
    char fullFilename[512];
    sprintf( fullFilename, "%s/%s", GetDataRoot(), filename );
    
    char cacheKey[512];
    if( uvAdjustX != 0.0f || uvAdjustY != 0.0f )
    {
        sprintf( cacheKey, "%s|uv%.6f|uv%.6f", fullFilename, uvAdjustX, uvAdjustY );
    }
    else
    {
        strcpy( cacheKey, fullFilename );
    }
    
    Image *image = m_imageCache.GetData( cacheKey );
    if( image )
    {
        delete image;
        m_imageCache.RemoveData( cacheKey );
    }
    
#ifndef NO_UNRAR
    g_fileSystem->UnloadArchiveFile( fullFilename );
#endif
}

Image *Resource::TryLoadFromAtlas( const char *filename, float uvAdjustX, float uvAdjustY )
{
    if( !filename ) return NULL;
    
    if (g_spriteAtlasManager && g_spriteAtlasManager->IsInitialized()) {
        char dataPath[512];
        sprintf(dataPath, "%s/%s", GetDataRoot(), filename);
        
        RuntimeTextureAtlas* atlas = NULL;
        const PackedSprite* sprite = g_spriteAtlasManager->GetSpriteFromAnyAtlas(dataPath, &atlas);
        
        if (sprite && atlas) {
            AtlasImage* atlasImage = new AtlasImage((PackedSprite*)sprite, atlas);
            atlasImage->SetUVAdjust( uvAdjustX, uvAdjustY );
            return atlasImage;
        }
    }
    
    return NULL;
}

Image *Resource::GetImage( const char *filename )
{   
    if( !filename ) return NULL;
    
    char fullFilename[512];
    sprintf( fullFilename, "%s/%s", GetDataRoot(), filename );

    Image *image = m_imageCache.GetData( fullFilename );
    if( image )
    {
        return image;
    }
    
    image = TryLoadFromAtlas( filename );
    if( image )
    {
        m_imageCache.PutData( fullFilename, image );
        return image;
    }
    
    image = new Image( fullFilename );
    m_imageCache.PutData( fullFilename, image );
    image->MakeTexture( true, true );
    return image;
}

Image *Resource::GetImage( const char *filename, float uvAdjustX, float uvAdjustY )
{   
    if( !filename ) return NULL;
    
    //
    // Create cache key that includes UV adjustments
    // Images with different UV adjustments are cached separately
    
    char fullFilename[512];
    sprintf( fullFilename, "%s/%s", GetDataRoot(), filename );
    
    char cacheKey[512];
    if( uvAdjustX != 0.0f || uvAdjustY != 0.0f )
    {
        sprintf( cacheKey, "%s|uv%.6f|uv%.6f", fullFilename, uvAdjustX, uvAdjustY );
    }
    else
    {
        strcpy( cacheKey, fullFilename );
    }
    
    Image *image = m_imageCache.GetData( cacheKey );
    if( image )
    {
        return image;
    }
    
    if( uvAdjustX == 0.0f && uvAdjustY == 0.0f )
    {
        return GetImage( filename );
    }
    
    image = TryLoadFromAtlas( filename, uvAdjustX, uvAdjustY );
    if( image )
    {
        m_imageCache.PutData( cacheKey, image );
        return image;
    }
    
    image = new Image( fullFilename );
    image->MakeTexture( true, true );
    image->SetUVAdjust( uvAdjustX, uvAdjustY );
    m_imageCache.PutData( cacheKey, image );
    return image;
}


BitmapFont *Resource::GetBitmapFont ( const char *filename )
{
    if( !filename ) return NULL;

    char fullFilename[512];
    snprintf( fullFilename, sizeof(fullFilename), "%s/%s", GetDataRoot(), filename );
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
    snprintf( fullFilename, sizeof(fullFilename), "%s/%s", GetDataRoot(), filename );
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

Model *Resource::GetModel( const char *filename )
{
    if( !filename ) return NULL;
    
    char fullFilename[512];
    snprintf( fullFilename, sizeof(fullFilename), "%s/%s", GetDataRoot(), filename );
    fullFilename[ sizeof(fullFilename) - 1 ] = '\x0';
    
    Model *model = m_modelCache.GetData( fullFilename );
    if( model )
    {
        return model;
    }
    
    model = new Model( fullFilename );
    
    if( !ModelLoader::LoadFromGLTF( model, fullFilename ) )
    {
        AppDebugOut("Model: Failed to load model: %s\n", fullFilename);
    }
    else
    { 
        ModelBuilder::BuildModelVBO( model );
    }
    
    m_modelCache.PutData( fullFilename, model );
    return model;
}


void Resource::UnloadModel( const char *filename )
{
    if( !filename ) return;
    
    char fullFilename[512];
    snprintf( fullFilename, sizeof(fullFilename), "%s/%s", GetDataRoot(), filename );
    fullFilename[ sizeof(fullFilename) - 1 ] = '\x0';
    
    Model *model = m_modelCache.GetData( fullFilename );
    if( model )
    {
        delete model;
        m_modelCache.RemoveData( fullFilename );
    }
    
#ifndef NO_UNRAR
    g_fileSystem->UnloadArchiveFile( filename );
#endif
}