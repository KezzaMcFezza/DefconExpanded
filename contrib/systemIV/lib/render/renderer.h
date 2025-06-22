/*
 * ========
 * RENDERER
 * ========
 *
 */

#ifndef _included_renderer_h
#define _included_renderer_h

#include "lib/tosser/btree.h"
#include "lib/math/vector3.h"

#include "colour.h"

class Image;
class BitmapFont;

#define     White           Colour(255,255,255)
#define     Black           Colour(0,0,0)

#define     LightGray       Colour(200,200,200)
#define     DarkGray        Colour(100,100,100)

// Modern OpenGL support structures
struct Matrix4f {
    float m[16];
    
    Matrix4f();
    void LoadIdentity();
    void Ortho(float left, float right, float bottom, float top, float nearZ, float farZ);
    void Multiply(const Matrix4f& other);
};

struct Vertex2D {
    float x, y;
    float r, g, b, a;
    float u, v;  // texture coordinates
};

class Renderer
{
protected:
    char    *m_defaultFontName;
    char    *m_defaultFontFilename;
    bool    m_defaultFontLanguageSpecific;
    char    *m_currentFontName;
    char    *m_currentFontFilename;
    bool    m_currentFontLanguageSpecific;
    bool    m_horizFlip;
    bool    m_fixedWidth;
    bool    m_negative;
    BTree <float> m_fontSpacings;

    // Modern OpenGL 3.3 Core Profile support
    unsigned int m_shaderProgram;
    unsigned int m_colorShaderProgram;
    unsigned int m_textureShaderProgram;
    unsigned int m_VAO;
    unsigned int m_VBO;
    
    Matrix4f m_projectionMatrix;
    Matrix4f m_modelViewMatrix;
    
    int m_projMatrixLocation;
    int m_modelViewMatrixLocation;
    int m_colorLocation;
    int m_textureLocation;
    
    // Dynamic vertex buffer for rendering
    static const int MAX_VERTICES = 10000;
    Vertex2D m_vertices[MAX_VERTICES];
    int m_vertexCount;
    
    // Line rendering state
    Colour m_currentLineColor;
    
    // Line strip rendering state
    bool m_lineStripActive;
    Colour m_lineStripColor;
    float m_lineStripWidth;
    
    // Cached line strip state
    bool m_cachedLineStripActive;
    char* m_currentCacheKey;
    Colour m_cachedLineStripColor;
    float m_cachedLineStripWidth;
    
    // Mega-VBO state
    bool m_megaVBOActive;
    char* m_currentMegaVBOKey;
    Colour m_megaVBOColor;
    float m_megaVBOWidth;
    static const int MAX_MEGA_VERTICES = 100000;  // Much larger buffer
    Vertex2D* m_megaVertices;
    int m_megaVertexCount;
    
    // VBO caching system for performance (replaces display lists)
    struct CachedVBO {
        unsigned int VBO;
        unsigned int VAO;
        int vertexCount;
        Colour color;
        float lineWidth;
        bool isValid;
    };
    
    BTree<CachedVBO*> m_cachedVBOs;
    
    // Font/texture batching state - for Phase 5.1 performance optimization
    unsigned int m_currentBoundTexture;  // Track current texture for batching
    bool m_batchingTextures;             // Enable intelligent batching
    
    // Helper methods for modern OpenGL
    void InitializeShaders();
    void SetupVertexArrays();
    void FlushIfTextureChanged(unsigned int newTextureID, bool useTexture);  // New batching helper

public:
    // Public shader creation for 3D renderer
    unsigned int CreateShader(const char* vertexSource, const char* fragmentSource);
    
    // Public flush method for font batching optimization
    void FlushVertices(unsigned int primitiveType, bool useTexture = false);
    
    // Safety controls for batching system
    void SetTextureBatching(bool enabled) { m_batchingTextures = enabled; }
    bool IsTextureBatchingEnabled() const { return m_batchingTextures; }

protected:

public:
    float   m_alpha;
    int     m_colourDepth;
    int     m_mouseMode;
    int     m_blendMode;

	int   m_blendSrcFactor;
	int   m_blendDstFactor;
    
    enum
    {
        BlendModeDisabled,
        BlendModeNormal,
        BlendModeAdditive,
        BlendModeSubtractive
    };

public:
    Renderer();
    virtual ~Renderer();

    void    Shutdown            ();
    
    void    Set2DViewport       ( float l, float r, float b, float t,
                                    int x, int y, int w, int h );
    void    Reset2DViewport     ();

    void    BeginScene          ();
    void    ClearScreen         ( bool _colour, bool _depth );

    void    SaveScreenshot      ();


    //
    // Rendering modes

    void    SetBlendMode        ( int _blendMode );
	void	SetBlendFunc        ( int srcFactor, int dstFactor );
    void    SetDepthBuffer      ( bool _enabled, bool _clearNow );

    //
    // Text output

    void    SetDefaultFont      ( const char *font, const char *_langName = NULL );
    void    SetFontSpacing      ( const char *font, float _spacing );
    float   GetFontSpacing      ( const char *font );
    void    SetFont             ( const char *font = NULL, 
                                  bool horizFlip = false, 
                                  bool negative = false,
                                  bool fixedWidth = false,
                                  const char *_langName = NULL );
    void    SetFont             ( const char *font, 
                                  const char *_langName );
    bool    IsFontLanguageSpecific ();
    BitmapFont *GetBitmapFont   ();

    void    Text                ( float x, float y, Colour const &col, float size, const char *text, ... );
    void    TextCentre          ( float x, float y, Colour const &col, float size, const char *text, ... );
    void    TextRight           ( float x, float y, Colour const &col, float size, const char *text, ... );
    void    TextSimple          ( float x, float y, Colour const &col, float size, const char *text );
    void    TextCentreSimple    ( float x, float y, Colour const &col, float size, const char *text );
    void    TextRightSimple     ( float x, float y, Colour const &col, float size, const char *text );
    
    float   TextWidth           ( const char *text, float size );
    float   TextWidth           ( const char *text, unsigned int textLen, float size, BitmapFont *bitmapFont );

    //
    // Drawing primitives

    void    Rect                ( float x, float y, float w, float h, Colour const &col, float lineWidth=1.0f );
    void    RectFill            ( float x, float y, float w, float h, Colour const &col );    
    void    RectFill            ( float x, float y, float w, float h, Colour const &colTL, Colour const &colTR, Colour const &colBR, Colour const &colBL );    
    void    RectFill            ( float x, float y, float w, float h, Colour const &col1, Colour const &col2, bool horizontal );    
    
    void    Circle              ( float x, float y, float radius, int numPoints, Colour const &col, float lineWidth=1.0f );        
    void    CircleFill          ( float x, float y, float radius, int numPoints, Colour const &col );
    void    TriangleFill        ( float x1, float y1, float x2, float y2, float x3, float y3, Colour const &col );
    void    Line                ( float x1, float y1, float x2, float y2, Colour const &col, float lineWidth=1.0f );
    
    void    BeginLines          ( Colour const &col, float lineWidth );    
    void    Line                ( float x, float y );
    void    EndLines            ();

    // Line strip rendering for continuous lines (replaces GL_LINE_STRIP)
    void    BeginLineStrip2D    ( Colour const &col, float lineWidth );
    void    LineStripVertex2D   ( float x, float y );
    void    EndLineStrip2D      ();

    // VBO caching system for high-performance rendering (replaces display lists)
    void    BeginCachedLineStrip( const char* cacheKey, Colour const &col, float lineWidth );
    void    CachedLineStripVertex( float x, float y );
    void    EndCachedLineStrip  ();
    void    RenderCachedLineStrip( const char* cacheKey );
    void    InvalidateCachedVBO ( const char* cacheKey );

    // Mega-VBO batching system for maximum performance (single draw calls)
    void    BeginMegaVBO        ( const char* megaVBOKey, Colour const &col, float lineWidth );
    void    AddLineStripToMegaVBO( float* vertices, int vertexCount );
    void    EndMegaVBO          ();
    void    RenderMegaVBO       ( const char* megaVBOKey );

    void    SetClip             ( int x, int y, int w, int h );      
    void    ResetClip           ();

    void    Blit                ( Image *src, float x, float y, Colour const &col);
    void    Blit                ( Image *src, float x, float y, float w, float h, Colour const &col);
    void    Blit                ( Image *src, float x, float y, float w, float h, Colour const &col, float angle);
    void    BlitChar            ( unsigned int textureID, float x, float y, float w, float h, 
                                  float texX, float texY, float texW, float texH, Colour const &col);
    
    // PHASE 6.1.1: Selective batching functions for safe performance optimization
    void    BlitBatched         ( Image *src, float x, float y, Colour const &col);
    void    BlitBatched         ( Image *src, float x, float y, float w, float h, Colour const &col);
    void    BlitBatched         ( Image *src, float x, float y, float w, float h, Colour const &col, float angle);

protected:
    char *ScreenshotsDirectory();
};


extern Renderer *g_renderer;

#endif
