#include "lib/universal_include.h"
  
#include <stdio.h>

#include "lib/render/colour.h"
#include "lib/filesys/file_system.h"
#include "lib/filesys/filesys_utils.h"
#include "lib/filesys/binary_stream_readers.h"

#include "bitmap.h"
#include "image.h"
#include "sprite_atlas.h"


Image::Image( char *filename )
:   m_textureID(-1),
    m_mipmapping(false)
{
    BinaryReader *in = g_fileSystem->GetBinaryReader(filename);

    if( in && in->IsOpen() )
    {
        char *extension = (char *)GetExtensionPart(filename);
        AppAssert(stricmp(extension, "bmp") == 0);
        m_bitmap = new Bitmap(in, extension);
        delete in;
    }
    else
    {
        // Failed to load the bitmap, so create an obvious ERROR bitmap
        m_bitmap = new Bitmap(32,32);        
        m_bitmap->Clear( Colour(255,0,0) );

        for( int x = 1; x < 30; ++x )
        {
            m_bitmap->PutPixel( x, x, Colour(0,255,0) );
            m_bitmap->PutPixel( x-1, x, Colour(0,255,0) );
            m_bitmap->PutPixel( x+1, x, Colour(0,255,0) );

            m_bitmap->PutPixel( 31-x, x, Colour(0,255,0) );
            m_bitmap->PutPixel( 31-x-1, x, Colour(0,255,0) );
            m_bitmap->PutPixel( 31-x+1, x, Colour(0,255,0) );
        }
    }
}


Image::Image( Bitmap *_bitmap )
:   m_textureID(-1),
    m_mipmapping(false),
    m_bitmap(_bitmap)
{
}

Image::~Image()
{
    if( m_textureID > -1 )
    {
        glDeleteTextures( 1, (GLuint *) &m_textureID );
    }
}

int Image::Width()
{
    return m_bitmap->m_width;
}

int Image::Height()
{
    return m_bitmap->m_height;
}


void Image::MakeTexture( bool mipmapping, bool masked )
{
    if( m_textureID == -1 )
    {
        m_mipmapping = mipmapping;
        if( masked ) m_bitmap->ConvertPinkToTransparent();
        m_textureID = m_bitmap->ConvertToTexture(mipmapping);
    }
}

Colour Image::GetColour( int pixelX, int pixelY )
{
    return m_bitmap->GetPixelClipped( pixelX, pixelY );
}


// static members for shared atlas texture
Image* AtlasImage::s_atlasTexture = NULL;
int AtlasImage::s_atlasRefCount = 0;
bool AtlasImage::s_atlasLoadFailed = false; 

AtlasImage::AtlasImage(const AtlasCoord* atlasCoord)
:   Image((Bitmap*)NULL),  // dont load individual bitmap
    m_atlasCoord(atlasCoord)
{
    // only initialize shared atlas texture if needed
    if (s_atlasTexture == NULL) {
        InitializeAtlasTexture();
    }
    s_atlasRefCount++;
    
    // copy atlas texture ID to this instance
    m_textureID = s_atlasTexture->m_textureID;
    m_mipmapping = s_atlasTexture->m_mipmapping;
    
    // dont create individual bitmap, we use the shared atlas
    m_bitmap = NULL;
}

AtlasImage::~AtlasImage()
{
    m_textureID = -1;
    
    s_atlasRefCount--;
    if (s_atlasRefCount <= 0) {
        CleanupAtlasTexture();
    }
}

int AtlasImage::Width()
{
    if (m_atlasCoord) {
        return m_atlasCoord->width;
    }
    return 0;
}

int AtlasImage::Height()
{
    if (m_atlasCoord) {
        return m_atlasCoord->height;
    }
    return 0;
}

unsigned int AtlasImage::GetAtlasTextureID() const
{
    return s_atlasTexture ? s_atlasTexture->m_textureID : 0;
}

void AtlasImage::InitializeAtlasTexture()
{
    if (s_atlasTexture != NULL) {
        return; 
    }
    
    // Reset failure flag
    s_atlasLoadFailed = false;
    
    // Load the atlas texture using the standard image loading
    char atlasPath[256];
    strcpy(atlasPath, "data/");
    strcat(atlasPath, SpriteAtlas::GetAtlasTexturePath());
    
    s_atlasTexture = new Image(atlasPath);
    
    // Check if loading actually failed (Image constructor creates ERROR bitmap)
    // We can detect this by checking if the bitmap is 32x32 (ERROR bitmap size)
    if (s_atlasTexture->Width() == 32 && s_atlasTexture->Height() == 32) {
        s_atlasLoadFailed = true;
        AppDebugOut("Atlas texture loading failed: %s\n", atlasPath);
    } else {
        s_atlasTexture->MakeTexture(true, true);
        s_atlasRefCount = 0;
        AppDebugOut("Atlas texture loaded successfully: %s (%dx%d)\n", 
                   atlasPath, s_atlasTexture->Width(), s_atlasTexture->Height());
    }
}

void AtlasImage::CleanupAtlasTexture()
{
    if (s_atlasTexture != NULL && s_atlasRefCount <= 0) {
        delete s_atlasTexture;
        s_atlasTexture = NULL;
        s_atlasRefCount = 0;
        s_atlasLoadFailed = false;  // Reset failure flag
    }
}
