#include "lib/universal_include.h"

#include <stdio.h>
#include <fstream>
#include <unrar/unrar.h>

#include "lib/resource/bitmap.h"
#include "lib/resource/resource.h"
#include "lib/resource/bitmapfont.h"
#include "lib/resource/image.h"
#include "lib/render/sprite_atlas.h"

#include "lib/filesys/binary_stream_readers.h"
#include "lib/filesys/file_system.h"



Resource *g_resource = NULL;



Resource::Resource()
{
}

void Resource::Restart()
{
    Shutdown();
}

void Resource::Shutdown()
{
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
    // Display list cache - OpenGL 3.3 Core Profile: Display lists not available
    // Modern equivalent: VBO caching system (already implemented in renderer)

    // Note: Display lists have been replaced with VBO caching system in the modern renderer
    // No cleanup needed as display lists are not used in OpenGL 3.3 Core Profile
    m_displayLists.Empty();
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
    else
    {
        // check if this is an atlas sprite first
        if( SpriteAtlas::IsAtlasSprite(filename) )
        {
            const AtlasCoord* coord = SpriteAtlas::GetSpriteCoord(filename);
            if( coord )
            {
                image = new AtlasImage(coord);
                m_imageCache.PutData( fullFilename, image );
                return image;
            }
        }
        
        // not an atlas sprite, load normally
        image = new Image( fullFilename );
        m_imageCache.PutData( fullFilename, image );
        image->MakeTexture( true, true );
        return image;
    }
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


bool Resource::GetDisplayList( const char *_name, unsigned int &_listId )
{
    // OpenGL 3.3 Core Profile: Display lists not available
    // Modern equivalent: VBO caching system (implemented in renderer)
    
    BTree<unsigned int> *data = m_displayLists.LookupTree(_name);

    if( data )
    {
        _listId = data->data;
        return false;
    }

    // OpenGL 3.3 Core Profile: Use VBO caching instead of display lists
    // Return a dummy ID and indicate that caching should be handled by the modern renderer
    _listId = 0; // Dummy ID - actual caching handled by VBO system
    m_displayLists.PutData( _name, _listId );

    return true; // Always return true to indicate "new" (but VBO system handles actual caching)
}


void Resource::DeleteDisplayList( const char *_name )
{
    BTree<unsigned int> *data = m_displayLists.LookupTree(_name);

    if( data )
    {
        m_displayLists.RemoveData( _name );
    }
}

