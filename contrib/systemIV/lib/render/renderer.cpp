#include "lib/universal_include.h"

#include <time.h>
#include <stdarg.h>

#include "lib/eclipse/eclipse.h"
#include "lib/gucci/window_manager.h"
#include "lib/gucci/input.h"
#include "lib/resource/bitmapfont.h"
#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/resource/bitmap.h"
#include "lib/math/vector3.h"
#include "lib/math/math_utils.h"
#include "lib/hi_res_time.h"
#include "lib/debug_utils.h"
#include "lib/preferences.h"
#include "lib/string_utils.h"
#include "lib/filesys/filesys_utils.h"
#include "lib/language_table.h"


#include "renderer.h"
#include "renderer3d.h"
#include "colour.h"

Renderer *g_renderer = NULL;

// Matrix4f implementation
Matrix4f::Matrix4f() {
    LoadIdentity();
}

void Matrix4f::LoadIdentity() {
    for (int i = 0; i < 16; i++) {
        m[i] = 0.0f;
    }
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

void Matrix4f::Ortho(float left, float right, float bottom, float top, float nearZ, float farZ) {
    LoadIdentity();
    
    float tx = -(right + left) / (right - left);
    float ty = -(top + bottom) / (top - bottom);
    float tz = -(farZ + nearZ) / (farZ - nearZ);
    
    m[0] = 2.0f / (right - left);
    m[5] = 2.0f / (top - bottom);
    m[10] = -2.0f / (farZ - nearZ);
    m[12] = tx;
    m[13] = ty;
    m[14] = tz;
}

void Matrix4f::Multiply(const Matrix4f& other) {
    Matrix4f result;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            result.m[i * 4 + j] = 0.0f;
            for (int k = 0; k < 4; k++) {
                result.m[i * 4 + j] += m[i * 4 + k] * other.m[k * 4 + j];
            }
        }
    }
    *this = result;
}

static const char *GetFontFilename( const char *_fontName, const char *_langName, bool *_fontLanguageSpecific )
{
	static char result[512];

	if( _langName || g_languageTable->m_lang )
	{
		*_fontLanguageSpecific = true;
		if( _langName )
		{
			snprintf( result, sizeof(result), "fonts/%s_%s.bmp", _fontName, _langName );
		}
		else
		{
			snprintf( result, sizeof(result), "fonts/%s_%s.bmp", _fontName, g_languageTable->m_lang->m_name );
		}
		result[ sizeof(result) - 1 ] = '\x0';

		if( !g_resource->TestBitmapFont( result ) )
		{
			*_fontLanguageSpecific = false;
			snprintf( result, sizeof(result), "fonts/%s.bmp", _fontName );
			result[ sizeof(result) - 1 ] = '\x0';
		}
	}
	else
	{
		*_fontLanguageSpecific = false;
		snprintf( result, sizeof(result), "fonts/%s.bmp", _fontName );
		result[ sizeof(result) - 1 ] = '\x0';
	}

	return result;
}


Renderer::Renderer()
:   m_alpha(1.0f),
    m_currentFontName(NULL),
    m_currentFontFilename(NULL),
    m_currentFontLanguageSpecific(false),
    m_defaultFontName(NULL),
    m_defaultFontFilename(NULL),
    m_defaultFontLanguageSpecific(false),
    m_horizFlip(false),
    m_fixedWidth(false),
    m_negative(false),
    m_blendSrcFactor(GL_ONE),
    m_blendDstFactor(GL_ZERO),
    m_currentBoundTexture(0),
    m_batchingTextures(true),
    m_shaderProgram(0),
    m_colorShaderProgram(0),
    m_textureShaderProgram(0),
    m_VAO(0),
    m_VBO(0),
    m_vertexCount(0),
    m_lineStripActive(false),
    m_cachedLineStripActive(false),
    m_currentCacheKey(NULL),
    m_megaVBOActive(false),
    m_currentMegaVBOKey(NULL),
    m_megaVertices(NULL),
    m_megaVertexCount(0)
{
    // Initialize modern OpenGL components
    InitializeShaders();
    SetupVertexArrays();
    
    // Allocate mega vertex buffer
    m_megaVertices = new Vertex2D[MAX_MEGA_VERTICES];
    
    // Initialize 3D renderer
    g_renderer3d = new Renderer3D(this);
}


Renderer::~Renderer()
{
	if( m_currentFontName )
	{
		delete [] m_currentFontName;
	}
	if( m_currentFontFilename )
	{
		delete [] m_currentFontFilename;
	}
	if( m_defaultFontName )
	{
		delete [] m_defaultFontName;
	}
	if( m_defaultFontFilename )
	{
		delete [] m_defaultFontFilename;
	}
	
	// Clean up OpenGL 3.3 resources
	if (m_colorShaderProgram) {
		glDeleteProgram(m_colorShaderProgram);
	}
	if (m_textureShaderProgram) {
		glDeleteProgram(m_textureShaderProgram);
	}
	if (m_VAO) {
		glDeleteVertexArrays(1, &m_VAO);
	}
	if (m_VBO) {
		glDeleteBuffers(1, &m_VBO);
	}
	
	// Clean up mega vertex buffer
	if (m_megaVertices) {
		delete[] m_megaVertices;
		m_megaVertices = NULL;
	}
	
	// Clean up 3D renderer
	if (g_renderer3d) {
		delete g_renderer3d;
		g_renderer3d = NULL;
	}
}


void Renderer::Set2DViewport ( float l, float r, float b, float t,
                               int x, int y, int w, int h )
{
    // Replace deprecated matrix stack operations with modern matrix calculations
    m_modelViewMatrix.LoadIdentity();
    m_projectionMatrix.Ortho(l - 0.325f, r - 0.325f, b - 0.325f, t - 0.325f, -1.0f, 1.0f);
    
    float scaleX = g_windowManager->GetHighDPIScaleX();
    float scaleY = g_windowManager->GetHighDPIScaleY();
    
    glViewport( scaleX * x,
                scaleY * (g_windowManager->WindowH() - h - y),
                scaleX * w,
                scaleY * h );
}

void Renderer::BeginScene()
{
    // PERFORMANCE OPTIMIZATION: Reset texture batching state for new frame
    m_currentBoundTexture = 0;
    
    SetBlendFunc ( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
    glEnable    ( GL_BLEND );

#ifdef TARGET_OS_MACOSX
	// Note by Chris
	// This line smoothing code actually crashes some Mac OSX machines
	// Typically Radeon gfx with Mac osx 10.7
	// So let's not even bother
	return;
#endif
		
    // OpenGL 3.3 Core Profile: Modern anti-aliasing is handled via MSAA (Multisample Anti-Aliasing)
    // which is configured at context creation time, not per-frame
}


void Renderer::ClearScreen( bool _colour, bool _depth )
{
    if( _colour ) glClear( GL_COLOR_BUFFER_BIT );
    if( _depth ) glClear( GL_DEPTH_BUFFER_BIT );
}


void Renderer::Reset2DViewport()
{
    Set2DViewport( 0, 
                   g_windowManager->WindowW(), 
                   g_windowManager->WindowH(), 
                   0, 0, 0, 
                   g_windowManager->WindowW(), 
                   g_windowManager->WindowH() );
}


void Renderer::Shutdown()
{
}


void Renderer::SetBlendMode( int _blendMode )
{
    m_blendMode = _blendMode;
    
    switch( _blendMode )
    {
        case BlendModeDisabled:
            glDisable   ( GL_BLEND );
            break;

        case BlendModeNormal:
            SetBlendFunc ( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
            glEnable    ( GL_BLEND );
            break;

        case BlendModeAdditive:
            SetBlendFunc ( GL_SRC_ALPHA, GL_ONE );
            glEnable    ( GL_BLEND );
            break;

        case BlendModeSubtractive:
            SetBlendFunc ( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_COLOR );
            glEnable    ( GL_BLEND );
            break;
    }
}


void Renderer::SetBlendFunc(int srcFactor, int dstFactor )
{
	m_blendSrcFactor = srcFactor;
	m_blendDstFactor = dstFactor;

	glBlendFunc( srcFactor, dstFactor );
}


void Renderer::SetDepthBuffer( bool _enabled, bool _clearNow )
{
    if( _enabled )
    {
        glEnable ( GL_DEPTH_TEST );
        glDepthMask( true );
    }
    else
    {
        glDisable( GL_DEPTH_TEST );
        glDepthMask( false );
    }
 
    if( _clearNow )     
    {
        glClear( GL_DEPTH_BUFFER_BIT );
    }
}


void Renderer::SetDefaultFont( const char *font, const char *_langName )
{
    if( m_currentFontName )
    {
        delete [] m_currentFontName;
        m_currentFontName = NULL;
    }
    if( m_currentFontFilename )
    {
        delete [] m_currentFontFilename;
        m_currentFontFilename = NULL;
    }
    m_currentFontLanguageSpecific = false;

    if( m_defaultFontName )
    {
        delete [] m_defaultFontName;
    }
    if( m_defaultFontFilename )
    {
        delete [] m_defaultFontFilename;
    }

    const char *fullFilename = GetFontFilename( font, _langName, &m_defaultFontLanguageSpecific );

    m_defaultFontName = newStr( font );
    m_defaultFontFilename = newStr( fullFilename );
}


void Renderer::SetFontSpacing( const char *font, float _spacing )
{
	BTree<float> *fontSpacing = m_fontSpacings.LookupTree( font );
	if( fontSpacing )
	{
		fontSpacing->data = _spacing;
	}
	else
	{
		m_fontSpacings.PutData( font, _spacing );
	}
}


float Renderer::GetFontSpacing( const char *font )
{
	BTree<float> *fontSpacing = m_fontSpacings.LookupTree( font );
	if( fontSpacing )
	{
		return fontSpacing->data;
	}
	return BitmapFont::DEFAULT_SPACING;
}


void Renderer::SetFont( const char *font, bool horizFlip, bool negative, bool fixedWidth, const char *_langName )
{
    if( font || !_langName )
    {
        if( m_currentFontName )
        {
            delete [] m_currentFontName;
            m_currentFontName = NULL;
        }
    }
    if( m_currentFontFilename )
    {
        delete [] m_currentFontFilename;
        m_currentFontFilename = NULL;
    }
    m_currentFontLanguageSpecific = false;

    if( font )
    {
        m_currentFontName = newStr( font );
        const char *fontFilename = GetFontFilename( font, _langName, &m_currentFontLanguageSpecific );
        m_currentFontFilename = newStr( fontFilename );
    }
    else if( _langName )
    {
        if( m_currentFontName )
        {
            const char *fontFilename = GetFontFilename( m_currentFontName, _langName, &m_currentFontLanguageSpecific );
            m_currentFontFilename = newStr( fontFilename );
        }
        else if( m_defaultFontName )
        {
            m_currentFontName = newStr( m_defaultFontName );
            const char *fontFilename = GetFontFilename( m_defaultFontName, _langName, &m_currentFontLanguageSpecific );
            m_currentFontFilename = newStr( fontFilename );
        }
    }

    m_horizFlip = horizFlip;
    m_fixedWidth = fixedWidth;
    m_negative = negative;

    if( m_negative )
    {
        SetBlendMode( BlendModeSubtractive );
    }
    else
    {
        SetBlendMode( BlendModeNormal );
    }
}

void Renderer::SetFont( const char *font, const char *_langName )
{
	SetFont( font, m_horizFlip, m_negative, m_fixedWidth, _langName );
}


bool Renderer::IsFontLanguageSpecific()
{
    BitmapFont *font = g_resource->GetBitmapFont( m_currentFontFilename );
    if( font )
    {
        return m_currentFontLanguageSpecific;
    }
    else
    {
        font = g_resource->GetBitmapFont( m_defaultFontFilename );
        if( font )
        {
            return m_defaultFontLanguageSpecific;
        }
    }
    return false;
}


BitmapFont *Renderer::GetBitmapFont()
{
    BitmapFont *font = g_resource->GetBitmapFont( m_currentFontFilename );
    if( font )
    {
        font->SetSpacing( GetFontSpacing( m_currentFontName ) );
    }
    else
    {
        font = g_resource->GetBitmapFont( m_defaultFontFilename );
        if( font )
        {
            font->SetSpacing( GetFontSpacing( m_defaultFontName ) );
        }
    }

    if( !font ) return NULL;

    font->SetHoriztonalFlip( m_horizFlip );
    font->SetFixedWidth( m_fixedWidth );

    return font;
}


void Renderer::Text( float x, float y, Colour const &col, float size, const char *text, ... )
{   
    char buf[1024];
    va_list ap;
    va_start (ap, text);
    vsprintf(buf, text, ap);

    TextSimple( x, y, col, size, buf );
}


void Renderer::TextCentre( float x, float y, Colour const &col, float size, const char *text, ... )
{
    char buf[1024];
    va_list ap;
    va_start (ap, text);
    vsprintf(buf, text, ap);

    float actualX = x - TextWidth( buf, size )/2.0f;
    TextSimple( actualX, y, col, size, buf );
}


void Renderer::TextRight( float x, float y, Colour const &col, float size, const char *text, ... )
{
    char buf[1024];
    va_list ap;
    va_start (ap, text);
    vsprintf(buf, text, ap);

    float actualX = x - TextWidth( buf, size );
    TextSimple( actualX, y, col, size, buf );
}


void Renderer::TextCentreSimple( float x, float y, Colour const &col, float size, const char *text )
{
    float actualX = x - TextWidth( text, size )/2.0f;
    TextSimple( actualX, y, col, size, text );
}


void Renderer::TextRightSimple( float x, float y, Colour const &col, float size, const char *text )
{
    float actualX = x - TextWidth( text, size );
    TextSimple( actualX, y, col, size, text );
}


void Renderer::TextSimple( float x, float y, Colour const &col, float size, const char *text )
{
    BitmapFont *font = g_resource->GetBitmapFont( m_currentFontFilename );
    if( font )
    {
        font->SetSpacing( GetFontSpacing( m_currentFontName ) );
    }
    else
    {
        font = g_resource->GetBitmapFont( m_defaultFontFilename );
        if( font )
        {
            font->SetSpacing( GetFontSpacing( m_defaultFontName ) );
        }
    }

    if( font )
    {    
        font->SetHoriztonalFlip( m_horizFlip );
        font->SetFixedWidth( m_fixedWidth );
                
        if( m_blendMode != BlendModeSubtractive )
        {			
			int blendSrc = m_blendSrcFactor, blendDst = m_blendDstFactor;

			SetBlendFunc( GL_SRC_ALPHA, GL_ONE );

	        font->DrawText2DSimple( x, y, size, text, col );

			SetBlendFunc(blendSrc, blendDst);
        }
		else
		{
			font->DrawText2DSimple( x, y, size, text, col );
		}
    }
}

float Renderer::TextWidth ( const char *text, float size )
{
    BitmapFont *font = g_resource->GetBitmapFont( m_currentFontFilename );
    if( font )
    {
        font->SetSpacing( GetFontSpacing( m_currentFontName ) );
    }
    else
    {
        font = g_resource->GetBitmapFont( m_defaultFontFilename );
        if( font )
        {
            font->SetSpacing( GetFontSpacing( m_defaultFontName ) );
        }
    }

    if( !font ) return -1;

    font->SetHoriztonalFlip( m_horizFlip );
    font->SetFixedWidth( m_fixedWidth );
    return font->GetTextWidth( text, size );
}


float Renderer::TextWidth ( const char *text, unsigned int textLen, float size, BitmapFont *bitmapFont )
{
    if( !bitmapFont ) return -1;

    return bitmapFont->GetTextWidth( text, textLen, size );
}


void Renderer::Rect ( float x, float y, float w, float h, Colour const &col, float lineWidth )
{
    // Convert to modern OpenGL using vertex buffer
    if (m_vertexCount + 8 > MAX_VERTICES) {
        FlushVertices(GL_LINES, false);
    }
    
    // Convert color to float
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    // Create 4 lines to form rectangle outline
    // Top line: (x, y) to (x + w, y)
    m_vertices[m_vertexCount].x = x; m_vertices[m_vertexCount].y = y;
    m_vertices[m_vertexCount].r = r; m_vertices[m_vertexCount].g = g; m_vertices[m_vertexCount].b = b; m_vertices[m_vertexCount].a = a;
    m_vertices[m_vertexCount].u = 0.0f; m_vertices[m_vertexCount].v = 0.0f;
    m_vertexCount++;
    
    m_vertices[m_vertexCount].x = x + w; m_vertices[m_vertexCount].y = y;
    m_vertices[m_vertexCount].r = r; m_vertices[m_vertexCount].g = g; m_vertices[m_vertexCount].b = b; m_vertices[m_vertexCount].a = a;
    m_vertices[m_vertexCount].u = 0.0f; m_vertices[m_vertexCount].v = 0.0f;
    m_vertexCount++;
    
    // Right line: (x + w, y) to (x + w, y + h)
    m_vertices[m_vertexCount].x = x + w; m_vertices[m_vertexCount].y = y;
    m_vertices[m_vertexCount].r = r; m_vertices[m_vertexCount].g = g; m_vertices[m_vertexCount].b = b; m_vertices[m_vertexCount].a = a;
    m_vertices[m_vertexCount].u = 0.0f; m_vertices[m_vertexCount].v = 0.0f;
    m_vertexCount++;
    
    m_vertices[m_vertexCount].x = x + w; m_vertices[m_vertexCount].y = y + h;
    m_vertices[m_vertexCount].r = r; m_vertices[m_vertexCount].g = g; m_vertices[m_vertexCount].b = b; m_vertices[m_vertexCount].a = a;
    m_vertices[m_vertexCount].u = 0.0f; m_vertices[m_vertexCount].v = 0.0f;
    m_vertexCount++;
    
    // Bottom line: (x + w, y + h) to (x, y + h)
    m_vertices[m_vertexCount].x = x + w; m_vertices[m_vertexCount].y = y + h;
    m_vertices[m_vertexCount].r = r; m_vertices[m_vertexCount].g = g; m_vertices[m_vertexCount].b = b; m_vertices[m_vertexCount].a = a;
    m_vertices[m_vertexCount].u = 0.0f; m_vertices[m_vertexCount].v = 0.0f;
    m_vertexCount++;
    
    m_vertices[m_vertexCount].x = x; m_vertices[m_vertexCount].y = y + h;
    m_vertices[m_vertexCount].r = r; m_vertices[m_vertexCount].g = g; m_vertices[m_vertexCount].b = b; m_vertices[m_vertexCount].a = a;
    m_vertices[m_vertexCount].u = 0.0f; m_vertices[m_vertexCount].v = 0.0f;
    m_vertexCount++;
    
    // Left line: (x, y + h) to (x, y)
    m_vertices[m_vertexCount].x = x; m_vertices[m_vertexCount].y = y + h;
    m_vertices[m_vertexCount].r = r; m_vertices[m_vertexCount].g = g; m_vertices[m_vertexCount].b = b; m_vertices[m_vertexCount].a = a;
    m_vertices[m_vertexCount].u = 0.0f; m_vertices[m_vertexCount].v = 0.0f;
    m_vertexCount++;
    
    m_vertices[m_vertexCount].x = x; m_vertices[m_vertexCount].y = y;
    m_vertices[m_vertexCount].r = r; m_vertices[m_vertexCount].g = g; m_vertices[m_vertexCount].b = b; m_vertices[m_vertexCount].a = a;
    m_vertices[m_vertexCount].u = 0.0f; m_vertices[m_vertexCount].v = 0.0f;
    m_vertexCount++;
    
    // Note: lineWidth handling would need glLineWidth equivalent in modern OpenGL
    // For now, we'll ignore lineWidth as it's deprecated in core profile
    
    // Flush immediately to maintain immediate-mode behavior
    FlushVertices(GL_LINES, false);
}


void Renderer::RectFill ( float x, float y, float w, float h, Colour const &col )
{
    RectFill( x, y, w, h, col, col, col, col );
}


void Renderer::RectFill( float x, float y, float w, float h, Colour const &col1, Colour const &col2, bool horizontal )
{
    if( horizontal )
    {
        RectFill( x, y, w, h, col1, col1, col2, col2 );
    }
    else
    {
        RectFill( x, y, w, h, col1, col2, col2, col1 );
    }
}


void Renderer::RectFill( float x, float y, float w, float h, Colour const &colTL, Colour const &colTR, Colour const &colBR, Colour const &colBL )
{
    // Convert to modern OpenGL using vertex buffer
    if (m_vertexCount + 6 > MAX_VERTICES) {
        FlushVertices(GL_TRIANGLES, false);
    }
    
    // Convert colors to floats
    float rTL = colTL.m_r / 255.0f, gTL = colTL.m_g / 255.0f, bTL = colTL.m_b / 255.0f, aTL = colTL.m_a / 255.0f;
    float rTR = colTR.m_r / 255.0f, gTR = colTR.m_g / 255.0f, bTR = colTR.m_b / 255.0f, aTR = colTR.m_a / 255.0f;
    float rBR = colBR.m_r / 255.0f, gBR = colBR.m_g / 255.0f, bBR = colBR.m_b / 255.0f, aBR = colBR.m_a / 255.0f;
    float rBL = colBL.m_r / 255.0f, gBL = colBL.m_g / 255.0f, bBL = colBL.m_b / 255.0f, aBL = colBL.m_a / 255.0f;
    
    // First triangle: TL, TR, BR
    m_vertices[m_vertexCount].x = x;      m_vertices[m_vertexCount].y = y;
    m_vertices[m_vertexCount].r = rTL;    m_vertices[m_vertexCount].g = gTL;
    m_vertices[m_vertexCount].b = bTL;    m_vertices[m_vertexCount].a = aTL;
    m_vertices[m_vertexCount].u = 0.0f;   m_vertices[m_vertexCount].v = 0.0f;
    m_vertexCount++;
    
    m_vertices[m_vertexCount].x = x + w;  m_vertices[m_vertexCount].y = y;
    m_vertices[m_vertexCount].r = rTR;    m_vertices[m_vertexCount].g = gTR;
    m_vertices[m_vertexCount].b = bTR;    m_vertices[m_vertexCount].a = aTR;
    m_vertices[m_vertexCount].u = 1.0f;   m_vertices[m_vertexCount].v = 0.0f;
    m_vertexCount++;
    
    m_vertices[m_vertexCount].x = x + w;  m_vertices[m_vertexCount].y = y + h;
    m_vertices[m_vertexCount].r = rBR;    m_vertices[m_vertexCount].g = gBR;
    m_vertices[m_vertexCount].b = bBR;    m_vertices[m_vertexCount].a = aBR;
    m_vertices[m_vertexCount].u = 1.0f;   m_vertices[m_vertexCount].v = 1.0f;
    m_vertexCount++;
    
    // Second triangle: TL, BR, BL
    m_vertices[m_vertexCount].x = x;      m_vertices[m_vertexCount].y = y;
    m_vertices[m_vertexCount].r = rTL;    m_vertices[m_vertexCount].g = gTL;
    m_vertices[m_vertexCount].b = bTL;    m_vertices[m_vertexCount].a = aTL;
    m_vertices[m_vertexCount].u = 0.0f;   m_vertices[m_vertexCount].v = 0.0f;
    m_vertexCount++;
    
    m_vertices[m_vertexCount].x = x + w;  m_vertices[m_vertexCount].y = y + h;
    m_vertices[m_vertexCount].r = rBR;    m_vertices[m_vertexCount].g = gBR;
    m_vertices[m_vertexCount].b = bBR;    m_vertices[m_vertexCount].a = aBR;
    m_vertices[m_vertexCount].u = 1.0f;   m_vertices[m_vertexCount].v = 1.0f;
    m_vertexCount++;
    
    m_vertices[m_vertexCount].x = x;      m_vertices[m_vertexCount].y = y + h;
    m_vertices[m_vertexCount].r = rBL;    m_vertices[m_vertexCount].g = gBL;
    m_vertices[m_vertexCount].b = bBL;    m_vertices[m_vertexCount].a = aBL;
    m_vertices[m_vertexCount].u = 0.0f;   m_vertices[m_vertexCount].v = 1.0f;
    m_vertexCount++;
    
    // Flush immediately to maintain immediate-mode behavior for now
    FlushVertices(GL_TRIANGLES, false);
}


void Renderer::Line ( float x1, float y1, float x2, float y2, Colour const &col, float lineWidth )
{
    // Convert to modern OpenGL using vertex buffer
    if (m_vertexCount + 2 > MAX_VERTICES) {
        FlushVertices(GL_LINES, false);
    }
    
    // Convert color to float
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    // Add line vertices
    m_vertices[m_vertexCount].x = x1; m_vertices[m_vertexCount].y = y1;
    m_vertices[m_vertexCount].r = r; m_vertices[m_vertexCount].g = g; m_vertices[m_vertexCount].b = b; m_vertices[m_vertexCount].a = a;
    m_vertices[m_vertexCount].u = 0.0f; m_vertices[m_vertexCount].v = 0.0f;
    m_vertexCount++;
    
    m_vertices[m_vertexCount].x = x2; m_vertices[m_vertexCount].y = y2;
    m_vertices[m_vertexCount].r = r; m_vertices[m_vertexCount].g = g; m_vertices[m_vertexCount].b = b; m_vertices[m_vertexCount].a = a;
    m_vertices[m_vertexCount].u = 0.0f; m_vertices[m_vertexCount].v = 0.0f;
    m_vertexCount++;
    
    // Note: lineWidth handling would need geometry shader or thick line implementation
    // For now, we'll ignore lineWidth as it's deprecated in core profile
    
    // Flush immediately to maintain immediate-mode behavior
    FlushVertices(GL_LINES, false);
}

void Renderer::Circle( float x, float y, float radius, int numPoints, Colour const &col, float lineWidth )
{
    // Convert to modern OpenGL using vertex buffer
    // We need numPoints * 2 vertices for GL_LINES (each line segment needs 2 vertices)
    if (m_vertexCount + numPoints * 2 > MAX_VERTICES) {
        FlushVertices(GL_LINES, false);
    }
    
    // Convert color to float
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    // Generate circle as line segments
    for( int i = 0; i < numPoints; ++i )
    {
        float angle1 = 2.0f * M_PI * (float) i / (float) numPoints;
        float angle2 = 2.0f * M_PI * (float) (i + 1) / (float) numPoints;
        
        float x1 = x + cosf(angle1) * radius;
        float y1 = y + sinf(angle1) * radius;
        float x2 = x + cosf(angle2) * radius;
        float y2 = y + sinf(angle2) * radius;
        
        // Add line segment from point i to point i+1
        m_vertices[m_vertexCount].x = x1; m_vertices[m_vertexCount].y = y1;
        m_vertices[m_vertexCount].r = r; m_vertices[m_vertexCount].g = g; m_vertices[m_vertexCount].b = b; m_vertices[m_vertexCount].a = a;
        m_vertices[m_vertexCount].u = 0.0f; m_vertices[m_vertexCount].v = 0.0f;
        m_vertexCount++;
        
        m_vertices[m_vertexCount].x = x2; m_vertices[m_vertexCount].y = y2;
        m_vertices[m_vertexCount].r = r; m_vertices[m_vertexCount].g = g; m_vertices[m_vertexCount].b = b; m_vertices[m_vertexCount].a = a;
        m_vertices[m_vertexCount].u = 0.0f; m_vertices[m_vertexCount].v = 0.0f;
        m_vertexCount++;
    }
    
    // Note: lineWidth handling would need geometry shader or thick line implementation
    // For now, we'll ignore lineWidth as it's deprecated in core profile
    
    // Flush immediately to maintain immediate-mode behavior
    FlushVertices(GL_LINES, false);
}

void Renderer::CircleFill ( float x, float y, float radius, int numPoints, Colour const &col )
{
    // Convert to modern OpenGL using vertex buffer
    // We need numPoints * 3 vertices for triangles (triangle fan converted to individual triangles)
    if (m_vertexCount + numPoints * 3 > MAX_VERTICES) {
        FlushVertices(GL_TRIANGLES, false);
    }
    
    // Convert color to float
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    // Generate circle as triangles (converting triangle fan to individual triangles)
    for( int i = 0; i < numPoints; ++i )
    {
        float angle1 = 2.0f * M_PI * (float) i / (float) numPoints;
        float angle2 = 2.0f * M_PI * (float) (i + 1) / (float) numPoints;
        
        float x1 = x + cosf(angle1) * radius;
        float y1 = y + sinf(angle1) * radius;
        float x2 = x + cosf(angle2) * radius;
        float y2 = y + sinf(angle2) * radius;
        
        // Triangle: center, point i, point i+1
        // Center vertex
        m_vertices[m_vertexCount].x = x; m_vertices[m_vertexCount].y = y;
        m_vertices[m_vertexCount].r = r; m_vertices[m_vertexCount].g = g; m_vertices[m_vertexCount].b = b; m_vertices[m_vertexCount].a = a;
        m_vertices[m_vertexCount].u = 0.5f; m_vertices[m_vertexCount].v = 0.5f;
        m_vertexCount++;
        
        // Point i
        m_vertices[m_vertexCount].x = x1; m_vertices[m_vertexCount].y = y1;
        m_vertices[m_vertexCount].r = r; m_vertices[m_vertexCount].g = g; m_vertices[m_vertexCount].b = b; m_vertices[m_vertexCount].a = a;
        m_vertices[m_vertexCount].u = 0.0f; m_vertices[m_vertexCount].v = 0.0f;
        m_vertexCount++;
        
        // Point i+1
        m_vertices[m_vertexCount].x = x2; m_vertices[m_vertexCount].y = y2;
        m_vertices[m_vertexCount].r = r; m_vertices[m_vertexCount].g = g; m_vertices[m_vertexCount].b = b; m_vertices[m_vertexCount].a = a;
        m_vertices[m_vertexCount].u = 1.0f; m_vertices[m_vertexCount].v = 0.0f;
        m_vertexCount++;
    }
    
    // Flush immediately to maintain immediate-mode behavior
    FlushVertices(GL_TRIANGLES, false);
}

void Renderer::TriangleFill( float x1, float y1, float x2, float y2, float x3, float y3, Colour const &col )
{
    // Convert to modern OpenGL using vertex buffer  
    if (m_vertexCount + 3 > MAX_VERTICES) {
        FlushVertices(GL_TRIANGLES, false);
    }
    
    // Convert color to float
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    // Create one triangle with the three vertices
    m_vertices[m_vertexCount].x = x1; m_vertices[m_vertexCount].y = y1;
    m_vertices[m_vertexCount].r = r;  m_vertices[m_vertexCount].g = g;
    m_vertices[m_vertexCount].b = b;  m_vertices[m_vertexCount].a = a;
    m_vertices[m_vertexCount].u = 0.0f; m_vertices[m_vertexCount].v = 0.0f;
    m_vertexCount++;
    
    m_vertices[m_vertexCount].x = x2; m_vertices[m_vertexCount].y = y2;
    m_vertices[m_vertexCount].r = r;  m_vertices[m_vertexCount].g = g;
    m_vertices[m_vertexCount].b = b;  m_vertices[m_vertexCount].a = a;
    m_vertices[m_vertexCount].u = 0.0f; m_vertices[m_vertexCount].v = 0.0f;
    m_vertexCount++;
    
    m_vertices[m_vertexCount].x = x3; m_vertices[m_vertexCount].y = y3;
    m_vertices[m_vertexCount].r = r;  m_vertices[m_vertexCount].g = g;
    m_vertices[m_vertexCount].b = b;  m_vertices[m_vertexCount].a = a;
    m_vertices[m_vertexCount].u = 0.0f; m_vertices[m_vertexCount].v = 0.0f;
    m_vertexCount++;
    
    // Flush immediately to maintain immediate-mode behavior
    FlushVertices(GL_TRIANGLES, false);
}

void Renderer::BeginLines ( Colour const &col, float lineWidth )
{
    // Store the current line color for the upcoming line vertices
    // We'll store it in a member variable since the old system set it once for all lines
    m_currentLineColor = col;
    
    // Note: lineWidth handling would need geometry shader or thick line implementation
    // For now, we'll ignore lineWidth as it's deprecated in core profile
    
    // We don't start GL_LINE_LOOP here anymore - just prepare for accumulating vertices
}

void Renderer::Line( float x, float y )
{
    // Add a single vertex to the current line strip
    // We'll connect these with lines when EndLines() is called
    if (m_vertexCount + 1 > MAX_VERTICES) {
        // If we're running out of space, we need to handle this carefully
        // For now, just flush what we have (this breaks the line strip but prevents overflow)
        FlushVertices(GL_LINES, false);
    }
    
    // Convert color to float
    float r = m_currentLineColor.m_r / 255.0f, g = m_currentLineColor.m_g / 255.0f;
    float b = m_currentLineColor.m_b / 255.0f, a = m_currentLineColor.m_a / 255.0f;
    
    // Add vertex to our line buffer
    m_vertices[m_vertexCount].x = x; m_vertices[m_vertexCount].y = y;
    m_vertices[m_vertexCount].r = r; m_vertices[m_vertexCount].g = g;
    m_vertices[m_vertexCount].b = b; m_vertices[m_vertexCount].a = a;
    m_vertices[m_vertexCount].u = 0.0f; m_vertices[m_vertexCount].v = 0.0f;
    m_vertexCount++;
}

void Renderer::EndLines()
{
    // Convert the accumulated vertices to GL_LINES format
    // We need to convert from line strip to individual line segments
    if (m_vertexCount < 2) {
        // Not enough vertices for lines, just clear
        m_vertexCount = 0;
        return;
    }
    
    // Create a temporary buffer for line segments
    // We need (m_vertexCount - 1) * 2 vertices for GL_LINES from a line strip
    int lineVertexCount = (m_vertexCount - 1) * 2;
    Vertex2D* lineVertices = new Vertex2D[lineVertexCount];
    
    int lineIndex = 0;
    for (int i = 0; i < m_vertexCount - 1; i++) {
        // Line from vertex i to vertex i+1
        lineVertices[lineIndex++] = m_vertices[i];
        lineVertices[lineIndex++] = m_vertices[i + 1];
    }
    
    // Clear our current vertex buffer and add the line segments
    m_vertexCount = 0;
    
    // Add all line segments to our vertex buffer
    for (int i = 0; i < lineVertexCount; i++) {
        if (m_vertexCount >= MAX_VERTICES) {
            FlushVertices(GL_LINES, false);
        }
        m_vertices[m_vertexCount++] = lineVertices[i];
    }
    
    // Clean up temporary buffer
    delete[] lineVertices;
    
    // Flush the lines
    FlushVertices(GL_LINES, false);
}

// Line strip rendering functions for continuous lines (replaces GL_LINE_STRIP)
void Renderer::BeginLineStrip2D(Colour const &col, float lineWidth)
{
    // Store line strip state
    m_lineStripActive = true;
    m_lineStripColor = col;
    m_lineStripWidth = lineWidth;
    
    // Clear vertex buffer for new line strip
    m_vertexCount = 0;
}

void Renderer::LineStripVertex2D(float x, float y)
{
    if (!m_lineStripActive) return;
    
    // Check if we need to flush due to buffer overflow
    if (m_vertexCount >= MAX_VERTICES) {
        // Convert current vertices to line segments and flush
        EndLineStrip2D();
        BeginLineStrip2D(m_lineStripColor, m_lineStripWidth);
    }
    
    // Convert color to float
    float r = m_lineStripColor.m_r / 255.0f, g = m_lineStripColor.m_g / 255.0f;
    float b = m_lineStripColor.m_b / 255.0f, a = m_lineStripColor.m_a / 255.0f;
    
    // Add vertex to our line strip buffer
    m_vertices[m_vertexCount].x = x; m_vertices[m_vertexCount].y = y;
    m_vertices[m_vertexCount].r = r; m_vertices[m_vertexCount].g = g;
    m_vertices[m_vertexCount].b = b; m_vertices[m_vertexCount].a = a;
    m_vertices[m_vertexCount].u = 0.0f; m_vertices[m_vertexCount].v = 0.0f;
    m_vertexCount++;
}

void Renderer::EndLineStrip2D()
{
    if (!m_lineStripActive) return;
    
    // Convert the accumulated vertices to GL_LINES format
    if (m_vertexCount < 2) {
        // Not enough vertices for lines, just clear
        m_vertexCount = 0;
        m_lineStripActive = false;
        return;
    }
    
    // Create a temporary buffer for line segments
    // We need (m_vertexCount - 1) * 2 vertices for GL_LINES from a line strip
    int lineVertexCount = (m_vertexCount - 1) * 2;
    Vertex2D* lineVertices = new Vertex2D[lineVertexCount];
    
    int lineIndex = 0;
    for (int i = 0; i < m_vertexCount - 1; i++) {
        // Line from vertex i to vertex i+1
        lineVertices[lineIndex++] = m_vertices[i];
        lineVertices[lineIndex++] = m_vertices[i + 1];
    }
    
    // Clear our current vertex buffer and add the line segments
    m_vertexCount = 0;
    
    // Add all line segments to our vertex buffer
    for (int i = 0; i < lineVertexCount; i++) {
        if (m_vertexCount >= MAX_VERTICES) {
            FlushVertices(GL_LINES, false);
        }
        m_vertices[m_vertexCount++] = lineVertices[i];
    }
    
    // Clean up temporary buffer
    delete[] lineVertices;
    
    // Flush the lines
    FlushVertices(GL_LINES, false);
    
    // Reset line strip state
    m_lineStripActive = false;
}

// VBO caching system for high-performance rendering (replaces display lists)
void Renderer::BeginCachedLineStrip(const char* cacheKey, Colour const &col, float lineWidth)
{
    // Check if this VBO already exists and is valid
    BTree<CachedVBO*>* tree = m_cachedVBOs.LookupTree(cacheKey);
    if (tree && tree->data && tree->data->isValid) {
        // VBO already exists, no need to rebuild
        return;
    }
    
    // Store cache key for building
    if (m_currentCacheKey) {
        delete[] m_currentCacheKey;
    }
    
    if (m_currentMegaVBOKey) {
        delete[] m_currentMegaVBOKey;
    }
    
    if (m_megaVertices) {
        delete[] m_megaVertices;
    }
    m_currentCacheKey = newStr(cacheKey);
    
    // Store cached line strip state
    m_cachedLineStripActive = true;
    m_cachedLineStripColor = col;
    m_cachedLineStripWidth = lineWidth;
    
    // Clear vertex buffer for new cached line strip
    m_vertexCount = 0;
}

void Renderer::CachedLineStripVertex(float x, float y)
{
    if (!m_cachedLineStripActive) return;
    
    // Check if we need to flush due to buffer overflow
    if (m_vertexCount >= MAX_VERTICES) {
        // For cached VBOs, we need to handle this differently
        // For now, just ignore extra vertices (this shouldn't happen for coastlines)
        AppDebugOut("Warning: Cached line strip exceeded vertex buffer size\n");
        return;
    }
    
    // Convert color to float
    float r = m_cachedLineStripColor.m_r / 255.0f, g = m_cachedLineStripColor.m_g / 255.0f;
    float b = m_cachedLineStripColor.m_b / 255.0f, a = m_cachedLineStripColor.m_a / 255.0f;
    
    // Add vertex to our cached line strip buffer
    m_vertices[m_vertexCount].x = x; m_vertices[m_vertexCount].y = y;
    m_vertices[m_vertexCount].r = r; m_vertices[m_vertexCount].g = g;
    m_vertices[m_vertexCount].b = b; m_vertices[m_vertexCount].a = a;
    m_vertices[m_vertexCount].u = 0.0f; m_vertices[m_vertexCount].v = 0.0f;
    m_vertexCount++;
}

void Renderer::EndCachedLineStrip()
{
    if (!m_cachedLineStripActive || !m_currentCacheKey) return;
    
    // Convert the accumulated vertices to GL_LINES format and store in VBO
    if (m_vertexCount < 2) {
        // Not enough vertices for lines
        m_cachedLineStripActive = false;
        return;
    }
    
    // Create or get cached VBO
    BTree<CachedVBO*>* tree = m_cachedVBOs.LookupTree(m_currentCacheKey);
    CachedVBO* cachedVBO = NULL;
    if (!tree || !tree->data) {
        cachedVBO = new CachedVBO();
        cachedVBO->VBO = 0;
        cachedVBO->VAO = 0;
        cachedVBO->vertexCount = 0;
        cachedVBO->isValid = false;
        m_cachedVBOs.PutData(m_currentCacheKey, cachedVBO);
    } else {
        cachedVBO = tree->data;
    }
    
    // Create line segments from line strip
    int lineVertexCount = (m_vertexCount - 1) * 2;
    Vertex2D* lineVertices = new Vertex2D[lineVertexCount];
    
    int lineIndex = 0;
    for (int i = 0; i < m_vertexCount - 1; i++) {
        // Line from vertex i to vertex i+1
        lineVertices[lineIndex++] = m_vertices[i];
        lineVertices[lineIndex++] = m_vertices[i + 1];
    }
    
    // Create VBO if it doesn't exist
    if (cachedVBO->VBO == 0) {
        glGenVertexArrays(1, &cachedVBO->VAO);
        glGenBuffers(1, &cachedVBO->VBO);
    }
    
    // Upload data to VBO
    glBindVertexArray(cachedVBO->VAO);
    glBindBuffer(GL_ARRAY_BUFFER, cachedVBO->VBO);
    glBufferData(GL_ARRAY_BUFFER, lineVertexCount * sizeof(Vertex2D), lineVertices, GL_STATIC_DRAW);
    
    // Set up vertex attributes
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    
    // Store VBO info
    cachedVBO->vertexCount = lineVertexCount;
    cachedVBO->color = m_cachedLineStripColor;
    cachedVBO->lineWidth = m_cachedLineStripWidth;
    cachedVBO->isValid = true;
    
    // Clean up
    delete[] lineVertices;
    m_vertexCount = 0;
    m_cachedLineStripActive = false;
}

void Renderer::RenderCachedLineStrip(const char* cacheKey)
{
    BTree<CachedVBO*>* tree = m_cachedVBOs.LookupTree(cacheKey);
    if (!tree || !tree->data || !tree->data->isValid) {
        return; // VBO doesn't exist or is invalid
    }
    CachedVBO* cachedVBO = tree->data;
    
    // Use color shader
    glUseProgram(m_colorShaderProgram);
    
    // Set uniforms
    int projLoc = glGetUniformLocation(m_colorShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_colorShaderProgram, "uModelView");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    
    // Render the cached VBO
    glBindVertexArray(cachedVBO->VAO);
    glDrawArrays(GL_LINES, 0, cachedVBO->vertexCount);
    glBindVertexArray(0);
    glUseProgram(0);
}

void Renderer::InvalidateCachedVBO(const char* cacheKey)
{
    BTree<CachedVBO*>* tree = m_cachedVBOs.LookupTree(cacheKey);
    if (tree && tree->data) {
        CachedVBO* cachedVBO = tree->data;
        cachedVBO->isValid = false;
        if (cachedVBO->VBO != 0) {
            glDeleteBuffers(1, &cachedVBO->VBO);
            cachedVBO->VBO = 0;
        }
        if (cachedVBO->VAO != 0) {
            glDeleteVertexArrays(1, &cachedVBO->VAO);
            cachedVBO->VAO = 0;
        }
    }
}

// Mega-VBO batching system for maximum performance (single draw calls)
void Renderer::BeginMegaVBO(const char* megaVBOKey, Colour const &col, float lineWidth)
{
    // Check if this mega-VBO already exists and is valid
    BTree<CachedVBO*>* tree = m_cachedVBOs.LookupTree(megaVBOKey);
    if (tree && tree->data && tree->data->isValid) {
        // Mega-VBO already exists, no need to rebuild
        return;
    }
    
    // Store mega-VBO key for building
    if (m_currentMegaVBOKey) {
        delete[] m_currentMegaVBOKey;
    }
    m_currentMegaVBOKey = newStr(megaVBOKey);
    
    // Store mega-VBO state
    m_megaVBOActive = true;
    m_megaVBOColor = col;
    m_megaVBOWidth = lineWidth;
    
    // Clear mega vertex buffer
    m_megaVertexCount = 0;
}

void Renderer::AddLineStripToMegaVBO(float* vertices, int vertexCount)
{
    if (!m_megaVBOActive || vertexCount < 2) return;
    
    // Convert color to float
    float r = m_megaVBOColor.m_r / 255.0f, g = m_megaVBOColor.m_g / 255.0f;
    float b = m_megaVBOColor.m_b / 255.0f, a = m_megaVBOColor.m_a / 255.0f;
    
    // Convert line strip to line segments and add to mega buffer
    for (int i = 0; i < vertexCount - 1; i++) {
        if (m_megaVertexCount + 2 >= MAX_MEGA_VERTICES) {
            AppDebugOut("Warning: Mega-VBO exceeded maximum vertex count\n");
            return;
        }
        
        // Line from vertex i to vertex i+1
        // First vertex of line segment
        m_megaVertices[m_megaVertexCount].x = vertices[i * 2];
        m_megaVertices[m_megaVertexCount].y = vertices[i * 2 + 1];
        m_megaVertices[m_megaVertexCount].r = r;
        m_megaVertices[m_megaVertexCount].g = g;
        m_megaVertices[m_megaVertexCount].b = b;
        m_megaVertices[m_megaVertexCount].a = a;
        m_megaVertices[m_megaVertexCount].u = 0.0f;
        m_megaVertices[m_megaVertexCount].v = 0.0f;
        m_megaVertexCount++;
        
        // Second vertex of line segment
        m_megaVertices[m_megaVertexCount].x = vertices[(i + 1) * 2];
        m_megaVertices[m_megaVertexCount].y = vertices[(i + 1) * 2 + 1];
        m_megaVertices[m_megaVertexCount].r = r;
        m_megaVertices[m_megaVertexCount].g = g;
        m_megaVertices[m_megaVertexCount].b = b;
        m_megaVertices[m_megaVertexCount].a = a;
        m_megaVertices[m_megaVertexCount].u = 0.0f;
        m_megaVertices[m_megaVertexCount].v = 0.0f;
        m_megaVertexCount++;
    }
}

void Renderer::EndMegaVBO()
{
    if (!m_megaVBOActive || !m_currentMegaVBOKey) return;
    
    if (m_megaVertexCount < 2) {
        // Not enough vertices for lines
        m_megaVBOActive = false;
        return;
    }
    
    // Create or get cached mega-VBO
    BTree<CachedVBO*>* tree = m_cachedVBOs.LookupTree(m_currentMegaVBOKey);
    CachedVBO* cachedVBO = NULL;
    if (!tree || !tree->data) {
        cachedVBO = new CachedVBO();
        cachedVBO->VBO = 0;
        cachedVBO->VAO = 0;
        cachedVBO->vertexCount = 0;
        cachedVBO->isValid = false;
        m_cachedVBOs.PutData(m_currentMegaVBOKey, cachedVBO);
    } else {
        cachedVBO = tree->data;
    }
    
    // Create VBO if it doesn't exist
    if (cachedVBO->VBO == 0) {
        glGenVertexArrays(1, &cachedVBO->VAO);
        glGenBuffers(1, &cachedVBO->VBO);
    }
    
    // Upload mega data to VBO
    glBindVertexArray(cachedVBO->VAO);
    glBindBuffer(GL_ARRAY_BUFFER, cachedVBO->VBO);
    glBufferData(GL_ARRAY_BUFFER, m_megaVertexCount * sizeof(Vertex2D), m_megaVertices, GL_STATIC_DRAW);
    
    // Set up vertex attributes
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    
    // Store mega-VBO info
    cachedVBO->vertexCount = m_megaVertexCount;
    cachedVBO->color = m_megaVBOColor;
    cachedVBO->lineWidth = m_megaVBOWidth;
    cachedVBO->isValid = true;
    
#ifdef EMSCRIPTEN_DEBUG
    AppDebugOut("Created mega-VBO '%s' with %d vertices\n", m_currentMegaVBOKey, m_megaVertexCount);
#endif
    // Reset state
    m_megaVertexCount = 0;
    m_megaVBOActive = false;
}

void Renderer::RenderMegaVBO(const char* megaVBOKey)
{
    BTree<CachedVBO*>* tree = m_cachedVBOs.LookupTree(megaVBOKey);
    if (!tree || !tree->data || !tree->data->isValid) {
        return; // Mega-VBO doesn't exist or is invalid
    }
    
    CachedVBO* cachedVBO = tree->data;
    
    // Use color shader
    glUseProgram(m_colorShaderProgram);
    
    // Set uniforms
    int projLoc = glGetUniformLocation(m_colorShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_colorShaderProgram, "uModelView");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    
    // Render the mega-VBO with single draw call
    glBindVertexArray(cachedVBO->VAO);
    glDrawArrays(GL_LINES, 0, cachedVBO->vertexCount);
    glBindVertexArray(0);
    glUseProgram(0);
}

void Renderer::SetClip( int x, int y, int w, int h )
{
    float scaleX = g_windowManager->GetHighDPIScaleX();
    float scaleY = g_windowManager->GetHighDPIScaleY();

    glScissor( scaleX * x,
               scaleY * (g_windowManager->WindowH() - h - y),
               scaleX * w,
               scaleY * h );
    glEnable( GL_SCISSOR_TEST );
}

void Renderer::ResetClip()
{
    glDisable( GL_SCISSOR_TEST );
}

void Renderer::Blit( Image *src, float x, float y, float w, float h, Colour const &col )
{    
    if (m_vertexCount + 6 > MAX_VERTICES) {
        FlushVertices(GL_TRIANGLES, true);
    }
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, src->m_textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    if (src->m_mipmapping) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    
    float onePixelW = 1.0f / (float) src->Width();
    float onePixelH = 1.0f / (float) src->Height();
    
    m_vertices[m_vertexCount].x = x;      m_vertices[m_vertexCount].y = y;
    m_vertices[m_vertexCount].r = r;      m_vertices[m_vertexCount].g = g;
    m_vertices[m_vertexCount].b = b;      m_vertices[m_vertexCount].a = a;
    m_vertices[m_vertexCount].u = onePixelW; m_vertices[m_vertexCount].v = 1.0f - onePixelH;
    m_vertexCount++;
    
    m_vertices[m_vertexCount].x = x + w;  m_vertices[m_vertexCount].y = y;
    m_vertices[m_vertexCount].r = r;      m_vertices[m_vertexCount].g = g;
    m_vertices[m_vertexCount].b = b;      m_vertices[m_vertexCount].a = a;
    m_vertices[m_vertexCount].u = 1.0f - onePixelW; m_vertices[m_vertexCount].v = 1.0f - onePixelH;
    m_vertexCount++;
    
    m_vertices[m_vertexCount].x = x + w;  m_vertices[m_vertexCount].y = y + h;
    m_vertices[m_vertexCount].r = r;      m_vertices[m_vertexCount].g = g;
    m_vertices[m_vertexCount].b = b;      m_vertices[m_vertexCount].a = a;
    m_vertices[m_vertexCount].u = 1.0f - onePixelW; m_vertices[m_vertexCount].v = onePixelH;
    m_vertexCount++;
    
    // Second triangle: TL, BR, BL
    m_vertices[m_vertexCount].x = x;      m_vertices[m_vertexCount].y = y;
    m_vertices[m_vertexCount].r = r;      m_vertices[m_vertexCount].g = g;
    m_vertices[m_vertexCount].b = b;      m_vertices[m_vertexCount].a = a;
    m_vertices[m_vertexCount].u = onePixelW; m_vertices[m_vertexCount].v = 1.0f - onePixelH;
    m_vertexCount++;
    
    m_vertices[m_vertexCount].x = x + w;  m_vertices[m_vertexCount].y = y + h;
    m_vertices[m_vertexCount].r = r;      m_vertices[m_vertexCount].g = g;
    m_vertices[m_vertexCount].b = b;      m_vertices[m_vertexCount].a = a;
    m_vertices[m_vertexCount].u = 1.0f - onePixelW; m_vertices[m_vertexCount].v = onePixelH;
    m_vertexCount++;
    
    m_vertices[m_vertexCount].x = x;      m_vertices[m_vertexCount].y = y + h;
    m_vertices[m_vertexCount].r = r;      m_vertices[m_vertexCount].g = g;
    m_vertices[m_vertexCount].b = b;      m_vertices[m_vertexCount].a = a;
    m_vertices[m_vertexCount].u = onePixelW; m_vertices[m_vertexCount].v = onePixelH;
    m_vertexCount++;
    
    FlushVertices(GL_TRIANGLES, true);
}

void Renderer::Blit( Image *image, float x, float y, Colour const &col )
{
    float w = image->Width();
    float h = image->Height();
    
    Blit( image, x, y, w, h, col );
}


void Renderer::Blit ( Image *src, float x, float y, float w, float h, Colour const &col, float angle)
{    
    // Convert to modern OpenGL using vertex buffer
    if (m_vertexCount + 6 > MAX_VERTICES) {
        FlushVertices(GL_TRIANGLES, true);
    }
    
    // Calculate rotated vertices (preserving original math)
    Vector3<float> vert1( -w, +h,0 );
    Vector3<float> vert2( +w, +h,0 );
    Vector3<float> vert3( +w, -h,0 );
    Vector3<float> vert4( -w, -h,0 );

    vert1.RotateAroundZ(angle);
    vert2.RotateAroundZ(angle);
    vert3.RotateAroundZ(angle);
    vert4.RotateAroundZ(angle);

    vert1 += Vector3<float>( x, y, 0 );
    vert2 += Vector3<float>( x, y, 0 );
    vert3 += Vector3<float>( x, y, 0 );
    vert4 += Vector3<float>( x, y, 0 );

    // Convert color to float
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    // Bind texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, src->m_textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    if (src->m_mipmapping) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    
    float onePixelW = 1.0f / (float) src->Width();
    float onePixelH = 1.0f / (float) src->Height();
    
    // Create two triangles for the rotated quad
    // First triangle: vert1, vert2, vert3
    m_vertices[m_vertexCount].x = vert1.x; m_vertices[m_vertexCount].y = vert1.y;
    m_vertices[m_vertexCount].r = r; m_vertices[m_vertexCount].g = g; m_vertices[m_vertexCount].b = b; m_vertices[m_vertexCount].a = a;
    m_vertices[m_vertexCount].u = onePixelW; m_vertices[m_vertexCount].v = 1.0f - onePixelH;
    m_vertexCount++;
    
    m_vertices[m_vertexCount].x = vert2.x; m_vertices[m_vertexCount].y = vert2.y;
    m_vertices[m_vertexCount].r = r; m_vertices[m_vertexCount].g = g; m_vertices[m_vertexCount].b = b; m_vertices[m_vertexCount].a = a;
    m_vertices[m_vertexCount].u = 1.0f - onePixelW; m_vertices[m_vertexCount].v = 1.0f - onePixelH;
    m_vertexCount++;
    
    m_vertices[m_vertexCount].x = vert3.x; m_vertices[m_vertexCount].y = vert3.y;
    m_vertices[m_vertexCount].r = r; m_vertices[m_vertexCount].g = g; m_vertices[m_vertexCount].b = b; m_vertices[m_vertexCount].a = a;
    m_vertices[m_vertexCount].u = 1.0f - onePixelW; m_vertices[m_vertexCount].v = onePixelH;
    m_vertexCount++;
    
    // Second triangle: vert1, vert3, vert4
    m_vertices[m_vertexCount].x = vert1.x; m_vertices[m_vertexCount].y = vert1.y;
    m_vertices[m_vertexCount].r = r; m_vertices[m_vertexCount].g = g; m_vertices[m_vertexCount].b = b; m_vertices[m_vertexCount].a = a;
    m_vertices[m_vertexCount].u = onePixelW; m_vertices[m_vertexCount].v = 1.0f - onePixelH;
    m_vertexCount++;
    
    m_vertices[m_vertexCount].x = vert3.x; m_vertices[m_vertexCount].y = vert3.y;
    m_vertices[m_vertexCount].r = r; m_vertices[m_vertexCount].g = g; m_vertices[m_vertexCount].b = b; m_vertices[m_vertexCount].a = a;
    m_vertices[m_vertexCount].u = 1.0f - onePixelW; m_vertices[m_vertexCount].v = onePixelH;
    m_vertexCount++;
    
    m_vertices[m_vertexCount].x = vert4.x; m_vertices[m_vertexCount].y = vert4.y;
    m_vertices[m_vertexCount].r = r; m_vertices[m_vertexCount].g = g; m_vertices[m_vertexCount].b = b; m_vertices[m_vertexCount].a = a;
    m_vertices[m_vertexCount].u = onePixelW; m_vertices[m_vertexCount].v = onePixelH;
    m_vertexCount++;
    
    // PHASE 6.1 ROLLBACK: Restore immediate flush to fix render order issues
    FlushVertices(GL_TRIANGLES, true);
}

void Renderer::BlitChar( unsigned int textureID, float x, float y, float w, float h, 
                          float texX, float texY, float texW, float texH, Colour const &col)
{
    // Convert to modern OpenGL using vertex buffer  
    if (m_vertexCount + 6 > MAX_VERTICES) {
        FlushVertices(GL_TRIANGLES, true);
    }
    
    // Convert color to float
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    // PERFORMANCE OPTIMIZATION: Smart flush only on texture change
    FlushIfTextureChanged(textureID, true);
    
    // Bind texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    
    // Calculate texture coordinates for the character sub-region
    float u1 = texX;
    float v1 = texY;
    float u2 = texX + texW;
    float v2 = texY + texH;
    
    // Create two triangles for the quad
    // First triangle: TL, TR, BR
    m_vertices[m_vertexCount].x = x;      m_vertices[m_vertexCount].y = y;
    m_vertices[m_vertexCount].r = r;      m_vertices[m_vertexCount].g = g;
    m_vertices[m_vertexCount].b = b;      m_vertices[m_vertexCount].a = a;
    m_vertices[m_vertexCount].u = u1;     m_vertices[m_vertexCount].v = v2;
    m_vertexCount++;
    
    m_vertices[m_vertexCount].x = x + w;  m_vertices[m_vertexCount].y = y;
    m_vertices[m_vertexCount].r = r;      m_vertices[m_vertexCount].g = g;
    m_vertices[m_vertexCount].b = b;      m_vertices[m_vertexCount].a = a;
    m_vertices[m_vertexCount].u = u2;     m_vertices[m_vertexCount].v = v2;
    m_vertexCount++;
    
    m_vertices[m_vertexCount].x = x + w;  m_vertices[m_vertexCount].y = y + h;
    m_vertices[m_vertexCount].r = r;      m_vertices[m_vertexCount].g = g;
    m_vertices[m_vertexCount].b = b;      m_vertices[m_vertexCount].a = a;
    m_vertices[m_vertexCount].u = u2;     m_vertices[m_vertexCount].v = v1;
    m_vertexCount++;
    
    // Second triangle: TL, BR, BL
    m_vertices[m_vertexCount].x = x;      m_vertices[m_vertexCount].y = y;
    m_vertices[m_vertexCount].r = r;      m_vertices[m_vertexCount].g = g;
    m_vertices[m_vertexCount].b = b;      m_vertices[m_vertexCount].a = a;
    m_vertices[m_vertexCount].u = u1;     m_vertices[m_vertexCount].v = v2;
    m_vertexCount++;
    
    m_vertices[m_vertexCount].x = x + w;  m_vertices[m_vertexCount].y = y + h;
    m_vertices[m_vertexCount].r = r;      m_vertices[m_vertexCount].g = g;
    m_vertices[m_vertexCount].b = b;      m_vertices[m_vertexCount].a = a;
    m_vertices[m_vertexCount].u = u2;     m_vertices[m_vertexCount].v = v1;
    m_vertexCount++;
    
    m_vertices[m_vertexCount].x = x;      m_vertices[m_vertexCount].y = y + h;
    m_vertices[m_vertexCount].r = r;      m_vertices[m_vertexCount].g = g;
    m_vertices[m_vertexCount].b = b;      m_vertices[m_vertexCount].a = a;
    m_vertices[m_vertexCount].u = u1;     m_vertices[m_vertexCount].v = v1;
    m_vertexCount++;
    
    // PERFORMANCE OPTIMIZATION: No immediate flush - let batching system handle it
    // Characters with same texture will now be batched together for massive performance gain
}

// PHASE 6.1.1: Selective batching functions for safe performance optimization
// These functions use smart batching for objects that can be safely batched by texture

void Renderer::BlitBatched( Image *image, float x, float y, Colour const &col )
{
    float w = image->Width();
    float h = image->Height();
    
    BlitBatched( image, x, y, w, h, col );
}

void Renderer::BlitBatched( Image *src, float x, float y, float w, float h, Colour const &col )
{    
    // Convert to modern OpenGL using vertex buffer
    if (m_vertexCount + 6 > MAX_VERTICES) {
        FlushVertices(GL_TRIANGLES, true);
    }
    
    // Convert color to float
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    // PHASE 6.1.1 OPTIMIZATION: Smart flush only on texture change (like font system)
    FlushIfTextureChanged(src->m_textureID, true);
    
    // Bind texture (we'll handle this in FlushVertices)
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, src->m_textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    if (src->m_mipmapping) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    
    float onePixelW = 1.0f / (float) src->Width();
    float onePixelH = 1.0f / (float) src->Height();
    
    // Create two triangles for the quad
    // First triangle: TL, TR, BR
    m_vertices[m_vertexCount].x = x;      m_vertices[m_vertexCount].y = y;
    m_vertices[m_vertexCount].r = r;      m_vertices[m_vertexCount].g = g;
    m_vertices[m_vertexCount].b = b;      m_vertices[m_vertexCount].a = a;
    m_vertices[m_vertexCount].u = onePixelW; m_vertices[m_vertexCount].v = 1.0f - onePixelH;
    m_vertexCount++;
    
    m_vertices[m_vertexCount].x = x + w;  m_vertices[m_vertexCount].y = y;
    m_vertices[m_vertexCount].r = r;      m_vertices[m_vertexCount].g = g;
    m_vertices[m_vertexCount].b = b;      m_vertices[m_vertexCount].a = a;
    m_vertices[m_vertexCount].u = 1.0f - onePixelW; m_vertices[m_vertexCount].v = 1.0f - onePixelH;
    m_vertexCount++;
    
    m_vertices[m_vertexCount].x = x + w;  m_vertices[m_vertexCount].y = y + h;
    m_vertices[m_vertexCount].r = r;      m_vertices[m_vertexCount].g = g;
    m_vertices[m_vertexCount].b = b;      m_vertices[m_vertexCount].a = a;
    m_vertices[m_vertexCount].u = 1.0f - onePixelW; m_vertices[m_vertexCount].v = onePixelH;
    m_vertexCount++;
    
    // Second triangle: TL, BR, BL
    m_vertices[m_vertexCount].x = x;      m_vertices[m_vertexCount].y = y;
    m_vertices[m_vertexCount].r = r;      m_vertices[m_vertexCount].g = g;
    m_vertices[m_vertexCount].b = b;      m_vertices[m_vertexCount].a = a;
    m_vertices[m_vertexCount].u = onePixelW; m_vertices[m_vertexCount].v = 1.0f - onePixelH;
    m_vertexCount++;
    
    m_vertices[m_vertexCount].x = x + w;  m_vertices[m_vertexCount].y = y + h;
    m_vertices[m_vertexCount].r = r;      m_vertices[m_vertexCount].g = g;
    m_vertices[m_vertexCount].b = b;      m_vertices[m_vertexCount].a = a;
    m_vertices[m_vertexCount].u = 1.0f - onePixelW; m_vertices[m_vertexCount].v = onePixelH;
    m_vertexCount++;
    
    m_vertices[m_vertexCount].x = x;      m_vertices[m_vertexCount].y = y + h;
    m_vertices[m_vertexCount].r = r;      m_vertices[m_vertexCount].g = g;
    m_vertices[m_vertexCount].b = b;      m_vertices[m_vertexCount].a = a;
    m_vertices[m_vertexCount].u = onePixelW; m_vertices[m_vertexCount].v = onePixelH;
    m_vertexCount++;
    
    // PHASE 6.1.1 OPTIMIZATION: No immediate flush - let batching system handle it
    // Objects with same texture will now be batched together for safe performance gain
}

void Renderer::BlitBatched( Image *src, float x, float y, float w, float h, Colour const &col, float angle)
{    
    // Convert to modern OpenGL using vertex buffer
    if (m_vertexCount + 6 > MAX_VERTICES) {
        FlushVertices(GL_TRIANGLES, true);
    }
    
    // Calculate rotated vertices (preserving original math)
    Vector3<float> vert1( -w, +h,0 );
    Vector3<float> vert2( +w, +h,0 );
    Vector3<float> vert3( +w, -h,0 );
    Vector3<float> vert4( -w, -h,0 );

    vert1.RotateAroundZ(angle);
    vert2.RotateAroundZ(angle);
    vert3.RotateAroundZ(angle);
    vert4.RotateAroundZ(angle);

    vert1 += Vector3<float>( x, y, 0 );
    vert2 += Vector3<float>( x, y, 0 );
    vert3 += Vector3<float>( x, y, 0 );
    vert4 += Vector3<float>( x, y, 0 );

    // Convert color to float
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    // PHASE 6.1.1 OPTIMIZATION: Smart flush only on texture change (like font system)
    FlushIfTextureChanged(src->m_textureID, true);
    
    // Bind texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, src->m_textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    if (src->m_mipmapping) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    
    float onePixelW = 1.0f / (float) src->Width();
    float onePixelH = 1.0f / (float) src->Height();
    
    // Create two triangles for the rotated quad
    // First triangle: vert1, vert2, vert3
    m_vertices[m_vertexCount].x = vert1.x; m_vertices[m_vertexCount].y = vert1.y;
    m_vertices[m_vertexCount].r = r; m_vertices[m_vertexCount].g = g; m_vertices[m_vertexCount].b = b; m_vertices[m_vertexCount].a = a;
    m_vertices[m_vertexCount].u = onePixelW; m_vertices[m_vertexCount].v = 1.0f - onePixelH;
    m_vertexCount++;
    
    m_vertices[m_vertexCount].x = vert2.x; m_vertices[m_vertexCount].y = vert2.y;
    m_vertices[m_vertexCount].r = r; m_vertices[m_vertexCount].g = g; m_vertices[m_vertexCount].b = b; m_vertices[m_vertexCount].a = a;
    m_vertices[m_vertexCount].u = 1.0f - onePixelW; m_vertices[m_vertexCount].v = 1.0f - onePixelH;
    m_vertexCount++;
    
    m_vertices[m_vertexCount].x = vert3.x; m_vertices[m_vertexCount].y = vert3.y;
    m_vertices[m_vertexCount].r = r; m_vertices[m_vertexCount].g = g; m_vertices[m_vertexCount].b = b; m_vertices[m_vertexCount].a = a;
    m_vertices[m_vertexCount].u = 1.0f - onePixelW; m_vertices[m_vertexCount].v = onePixelH;
    m_vertexCount++;
    
    // Second triangle: vert1, vert3, vert4
    m_vertices[m_vertexCount].x = vert1.x; m_vertices[m_vertexCount].y = vert1.y;
    m_vertices[m_vertexCount].r = r; m_vertices[m_vertexCount].g = g; m_vertices[m_vertexCount].b = b; m_vertices[m_vertexCount].a = a;
    m_vertices[m_vertexCount].u = onePixelW; m_vertices[m_vertexCount].v = 1.0f - onePixelH;
    m_vertexCount++;
    
    m_vertices[m_vertexCount].x = vert3.x; m_vertices[m_vertexCount].y = vert3.y;
    m_vertices[m_vertexCount].r = r; m_vertices[m_vertexCount].g = g; m_vertices[m_vertexCount].b = b; m_vertices[m_vertexCount].a = a;
    m_vertices[m_vertexCount].u = 1.0f - onePixelW; m_vertices[m_vertexCount].v = onePixelH;
    m_vertexCount++;
    
    m_vertices[m_vertexCount].x = vert4.x; m_vertices[m_vertexCount].y = vert4.y;
    m_vertices[m_vertexCount].r = r; m_vertices[m_vertexCount].g = g; m_vertices[m_vertexCount].b = b; m_vertices[m_vertexCount].a = a;
    m_vertices[m_vertexCount].u = onePixelW; m_vertices[m_vertexCount].v = onePixelH;
    m_vertexCount++;
    
    // PHASE 6.1.1 OPTIMIZATION: No immediate flush - let batching system handle it
    // Objects with same texture will now be batched together for safe performance gain
}

void Renderer::SaveScreenshot()
{
    float timeNow = GetHighResTime();
	char *screenshotsDir = ScreenshotsDirectory();

    int screenW = g_windowManager->GetHighDPIScaleX() * g_windowManager->WindowW();
    int screenH = g_windowManager->GetHighDPIScaleY() * g_windowManager->WindowH();
    
    unsigned char *screenBuffer = new unsigned char[screenW * screenH * 3];

    glReadPixels(0, 0, screenW, screenH, GL_RGB, GL_UNSIGNED_BYTE, screenBuffer);


	//
	// Copy tile data into big bitmap

    Bitmap bitmap( screenW, screenH );
    bitmap.Clear(Black);
    
	for (int y = 0; y < screenH; ++y)
	{
		unsigned const char *line = &screenBuffer[y * screenW * 3];
		for (int x = 0; x < screenW; ++x)
		{
			unsigned const char *p = &line[x * 3];
			bitmap.PutPixel(x, y, Colour(p[0], p[1], p[2]));
		}
	}

    //
    // Save the bitmap to a file

    int screenshotIndex = 1;
    while( true )
    {
        time_t theTimeT = time(NULL);
        tm *theTime = localtime(&theTimeT);
        char filename[2048];
		
        snprintf( filename, sizeof(filename), "%s/screenshot %02d-%02d-%02d %02d.bmp", 
								screenshotsDir,
								1900+theTime->tm_year, 
                                theTime->tm_mon+1,
                                theTime->tm_mday,
                                screenshotIndex );
        if( !DoesFileExist(filename) )
        {
            bitmap.SaveBmp( filename );
            break;
        }
        ++screenshotIndex;
    }

    //
    // Clear up

    delete[] screenBuffer;
	delete[] screenshotsDir;

	float timeTaken = GetHighResTime() - timeNow;
    AppDebugOut( "Screenshot %dms\n", int(timeTaken*1000) );

}

char *Renderer::ScreenshotsDirectory()
{
#ifdef TARGET_OS_MACOSX
	if ( getenv("HOME") )
		return ConcatPaths( getenv("HOME"), "Pictures", NULL );
#endif
	return newStr(".");
}

// Modern OpenGL 3.3 Core Profile implementations

unsigned int Renderer::CreateShader(const char* vertexSource, const char* fragmentSource) {
    // Create vertex shader
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSource, NULL);
    glCompileShader(vertexShader);
    
    // Check for vertex shader compile errors
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        AppDebugOut("Vertex shader compilation failed: %s\n", infoLog);
    }
    
    // Create fragment shader
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
    glCompileShader(fragmentShader);
    
    // Check for fragment shader compile errors
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        AppDebugOut("Fragment shader compilation failed: %s\n", infoLog);
    }
    
    // Create shader program
    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    
    // Check for linking errors
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        AppDebugOut("Shader program linking failed: %s\n", infoLog);
    }
    
    // Clean up individual shaders as they're linked into program now
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    return shaderProgram;
}

void Renderer::InitializeShaders() {
    // Create separate shader sources for WebGL vs Desktop
#ifdef TARGET_EMSCRIPTEN
    // WebGL 2.0 (OpenGL ES 3.0) shaders
    const char* vertexShaderSource = R"(#version 300 es
precision highp float;

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec4 aColor;
layout (location = 2) in vec2 aTexCoord;

uniform mat4 uProjection;
uniform mat4 uModelView;

out vec4 vertexColor;
out vec2 texCoord;

void main() {
    gl_Position = uProjection * uModelView * vec4(aPos, 0.0, 1.0);
    vertexColor = aColor;
    texCoord = aTexCoord;
}
)";

    const char* colorFragmentShaderSource = R"(#version 300 es
precision mediump float;

in vec4 vertexColor;
out vec4 FragColor;

void main() {
    FragColor = vertexColor;
}
)";

    const char* textureFragmentShaderSource = R"(#version 300 es
precision mediump float;

in vec4 vertexColor;
in vec2 texCoord;

uniform sampler2D ourTexture;

out vec4 FragColor;

void main() {
    FragColor = texture(ourTexture, texCoord) * vertexColor;
}
)";
#else
    // Desktop OpenGL 3.3 Core shaders
    const char* vertexShaderSource = R"(#version 330 core

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec4 aColor;
layout (location = 2) in vec2 aTexCoord;

uniform mat4 uProjection;
uniform mat4 uModelView;

out vec4 vertexColor;
out vec2 texCoord;

void main() {
    gl_Position = uProjection * uModelView * vec4(aPos, 0.0, 1.0);
    vertexColor = aColor;
    texCoord = aTexCoord;
}
)";

    const char* colorFragmentShaderSource = R"(#version 330 core

in vec4 vertexColor;
out vec4 FragColor;

void main() {
    FragColor = vertexColor;
}
)";

    const char* textureFragmentShaderSource = R"(#version 330 core

in vec4 vertexColor;
in vec2 texCoord;

uniform sampler2D ourTexture;

out vec4 FragColor;

void main() {
    FragColor = texture(ourTexture, texCoord) * vertexColor;
}
)";
#endif
    
    // Create shader programs
    m_colorShaderProgram = CreateShader(vertexShaderSource, colorFragmentShaderSource);
    m_textureShaderProgram = CreateShader(vertexShaderSource, textureFragmentShaderSource);
    
    // Set default to color shader
    m_shaderProgram = m_colorShaderProgram;
}

void Renderer::SetupVertexArrays() {
    // Generate VAO and VBO
    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    
    // Allocate buffer for maximum vertices
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex2D) * MAX_VERTICES, NULL, GL_DYNAMIC_DRAW);
    
    // Position attribute (location 0)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Color attribute (location 1)
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    // Texture coordinate attribute (location 2)
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

// Smart batching helper - flushes only when texture changes
void Renderer::FlushIfTextureChanged(unsigned int newTextureID, bool useTexture) {
    // If batching is disabled, always flush (preserves old behavior)
    if (!m_batchingTextures) {
        FlushVertices(GL_TRIANGLES, useTexture);
        return;
    }
    
    // If texture is changing and we have vertices to render, flush first
    if (m_vertexCount > 0 && m_currentBoundTexture != newTextureID) {
        FlushVertices(GL_TRIANGLES, useTexture);
    }
    
    // Update current texture state
    m_currentBoundTexture = newTextureID;
}

void Renderer::FlushVertices(unsigned int primitiveType, bool useTexture) {
    if (m_vertexCount == 0) return;
    
    // Choose appropriate shader
    unsigned int shaderToUse = useTexture ? m_textureShaderProgram : m_colorShaderProgram;
    glUseProgram(shaderToUse);
    
    // Set uniforms
    int projLoc = glGetUniformLocation(shaderToUse, "uProjection");
    int modelViewLoc = glGetUniformLocation(shaderToUse, "uModelView");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    
    // Set texture uniform for texture shader
    if (useTexture) {
        int texLoc = glGetUniformLocation(shaderToUse, "ourTexture");
        glUniform1i(texLoc, 0); // Use texture unit 0
    }
    
    // Upload vertex data
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_vertexCount * sizeof(Vertex2D), m_vertices);
    
    // Draw
    glDrawArrays(primitiveType, 0, m_vertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    // Reset vertex count
    m_vertexCount = 0;
}
