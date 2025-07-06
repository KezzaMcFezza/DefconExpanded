/*
 * =====
 * IMAGE
 * =====
 *
 * A wrapper class for Bitmap Image objects
 * that have been converted into Textures
 *
 */


#ifndef _included_image_h
#define _included_image_h

class Bitmap;
class Colour;
struct AtlasCoord;

#include "lib/resource/resource.h"
#include "lib/render/colour.h"


class Image
{
public:
    Bitmap      *m_bitmap;
    unsigned int m_textureID;
    bool        m_mipmapping;
    
public:
    Image( char *filename );
    Image( Bitmap *_bitmap );
    virtual ~Image();

    virtual int     Width();
    virtual int     Height();    

    void    MakeTexture( bool mipmapping, bool masked );
    
    Colour  GetColour( int pixelX, int pixelY );
};

class AtlasImage : public Image
{
private:
    const AtlasCoord* m_atlasCoord;
    static Image* s_atlasTexture;      // shared atlas texture instance
    static int s_atlasRefCount;        // reference counting for atlas texture

public:
    AtlasImage(const AtlasCoord* atlasCoord);
    virtual ~AtlasImage();
    
    // override dimensions to return atlas sprite dimensions
    int Width() override;
    int Height() override;
    
    // get the atlas coordinates
    const AtlasCoord* GetAtlasCoord() const { return m_atlasCoord; }
    
    // get the shared atlas texture ID
    unsigned int GetAtlasTextureID() const;
    
    // static methods for atlas texture management
    static void InitializeAtlasTexture();
    static void CleanupAtlasTexture();
};



#endif
