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
#include "renderer_3d.h"
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
    m_triangleVertexCount(0),
    m_lineVertexCount(0),
    m_lineStripActive(false),
    m_cachedLineStripActive(false),
    m_currentCacheKey(NULL),
    m_megaVBOActive(false),
    m_currentMegaVBOKey(NULL),
    m_megaVertices(NULL),
    m_megaVertexCount(0),
    m_frameCounter(0),
    m_lastFlushTime(GetHighResTime()),  // Initialize with current time
    // PERFORMANCE FIX: Initialize new specialized buffer systems
    m_uiTriangleVertexCount(0),
    m_uiLineVertexCount(0),
    m_textVertexCount(0),
    m_currentTextTexture(0),
    m_spriteVertexCount(0),
    m_currentSpriteTexture(0),
    // Unit rendering specialized buffer initialization
    m_unitTrailVertexCount(0),
    m_unitMainVertexCount(0),
    m_currentUnitMainTexture(0),
    m_unitHighlightVertexCount(0),
    m_currentUnitHighlightTexture(0),
    m_unitStateVertexCount(0),
    m_currentUnitStateTexture(0),
    m_unitCounterVertexCount(0),
    m_currentUnitCounterTexture(0),
    m_unitNukeVertexCount(0),
    m_currentUnitNukeTexture(0),
    // Effect rendering specialized buffer initialization
    m_effectsLineVertexCount(0),
    m_effectsSpriteVertexCount(0),
    m_currentEffectsSpriteTexture(0),
    m_allowImmedateFlush(true)  // Keep immediate flushing enabled for now
{
    // Initialize modern OpenGL components
    InitializeShaders();
    SetupVertexArrays();
    
    // Allocate mega vertex buffer
    m_megaVertices = new Vertex2D[MAX_MEGA_VERTICES];
    
    // Initialize 3D renderer
    g_renderer3d = new Renderer3D(this);

    // PERFORMANCE TRACKING: Initialize draw call counters
    // Initialize both current and previous frame counters to 0
    m_drawCallsPerFrame = 0;
    m_legacyTriangleCalls = 0;
    m_legacyLineCalls = 0;
    m_uiTriangleCalls = 0;
    m_uiLineCalls = 0;
    m_textCalls = 0;
    m_spriteCalls = 0;
    // Unit rendering performance counters
    m_unitTrailCalls = 0;
    m_unitMainSpriteCalls = 0;
    m_unitHighlightCalls = 0;
    m_unitStateIconCalls = 0;
    m_unitCounterCalls = 0;
    m_unitNukeIconCalls = 0;
    // Effect rendering performance counters
    m_effectsLineCalls = 0;
    m_effectsSpriteCalls = 0;
    // Previous frame data
    m_prevDrawCallsPerFrame = 0;
    m_prevLegacyTriangleCalls = 0;
    m_prevLegacyLineCalls = 0;
    m_prevUiTriangleCalls = 0;
    m_prevUiLineCalls = 0;
    m_prevTextCalls = 0;
    m_prevSpriteCalls = 0;
    // Unit rendering previous frame data
    m_prevUnitTrailCalls = 0;
    m_prevUnitMainSpriteCalls = 0;
    m_prevUnitHighlightCalls = 0;
    m_prevUnitStateIconCalls = 0;
    m_prevUnitCounterCalls = 0;
    m_prevUnitNukeIconCalls = 0;
    // Effect rendering previous frame data
    m_prevEffectsLineCalls = 0;
    m_prevEffectsSpriteCalls = 0;
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
    if (m_lineVertexCount + 8 > MAX_VERTICES) {
        FlushLines();
    }
    
    // Convert color to float
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    // Create 4 lines to form rectangle outline
    // Top line: (x, y) to (x + w, y)
    m_lineVertices[m_lineVertexCount].x = x; m_lineVertices[m_lineVertexCount].y = y;
    m_lineVertices[m_lineVertexCount].r = r; m_lineVertices[m_lineVertexCount].g = g; m_lineVertices[m_lineVertexCount].b = b; m_lineVertices[m_lineVertexCount].a = a;
    m_lineVertices[m_lineVertexCount].u = 0.0f; m_lineVertices[m_lineVertexCount].v = 0.0f;
    m_lineVertexCount++;
    
    m_lineVertices[m_lineVertexCount].x = x + w; m_lineVertices[m_lineVertexCount].y = y;
    m_lineVertices[m_lineVertexCount].r = r; m_lineVertices[m_lineVertexCount].g = g; m_lineVertices[m_lineVertexCount].b = b; m_lineVertices[m_lineVertexCount].a = a;
    m_lineVertices[m_lineVertexCount].u = 0.0f; m_lineVertices[m_lineVertexCount].v = 0.0f;
    m_lineVertexCount++;
    
    // Right line: (x + w, y) to (x + w, y + h)
    m_lineVertices[m_lineVertexCount].x = x + w; m_lineVertices[m_lineVertexCount].y = y;
    m_lineVertices[m_lineVertexCount].r = r; m_lineVertices[m_lineVertexCount].g = g; m_lineVertices[m_lineVertexCount].b = b; m_lineVertices[m_lineVertexCount].a = a;
    m_lineVertices[m_lineVertexCount].u = 0.0f; m_lineVertices[m_lineVertexCount].v = 0.0f;
    m_lineVertexCount++;
    
    m_lineVertices[m_lineVertexCount].x = x + w; m_lineVertices[m_lineVertexCount].y = y + h;
    m_lineVertices[m_lineVertexCount].r = r; m_lineVertices[m_lineVertexCount].g = g; m_lineVertices[m_lineVertexCount].b = b; m_lineVertices[m_lineVertexCount].a = a;
    m_lineVertices[m_lineVertexCount].u = 0.0f; m_lineVertices[m_lineVertexCount].v = 0.0f;
    m_lineVertexCount++;
    
    // Bottom line: (x + w, y + h) to (x, y + h)
    m_lineVertices[m_lineVertexCount].x = x + w; m_lineVertices[m_lineVertexCount].y = y + h;
    m_lineVertices[m_lineVertexCount].r = r; m_lineVertices[m_lineVertexCount].g = g; m_lineVertices[m_lineVertexCount].b = b; m_lineVertices[m_lineVertexCount].a = a;
    m_lineVertices[m_lineVertexCount].u = 0.0f; m_lineVertices[m_lineVertexCount].v = 0.0f;
    m_lineVertexCount++;
    
    m_lineVertices[m_lineVertexCount].x = x; m_lineVertices[m_lineVertexCount].y = y + h;
    m_lineVertices[m_lineVertexCount].r = r; m_lineVertices[m_lineVertexCount].g = g; m_lineVertices[m_lineVertexCount].b = b; m_lineVertices[m_lineVertexCount].a = a;
    m_lineVertices[m_lineVertexCount].u = 0.0f; m_lineVertices[m_lineVertexCount].v = 0.0f;
    m_lineVertexCount++;
    
    // Left line: (x, y + h) to (x, y)
    m_lineVertices[m_lineVertexCount].x = x; m_lineVertices[m_lineVertexCount].y = y + h;
    m_lineVertices[m_lineVertexCount].r = r; m_lineVertices[m_lineVertexCount].g = g; m_lineVertices[m_lineVertexCount].b = b; m_lineVertices[m_lineVertexCount].a = a;
    m_lineVertices[m_lineVertexCount].u = 0.0f; m_lineVertices[m_lineVertexCount].v = 0.0f;
    m_lineVertexCount++;
    
    m_lineVertices[m_lineVertexCount].x = x; m_lineVertices[m_lineVertexCount].y = y;
    m_lineVertices[m_lineVertexCount].r = r; m_lineVertices[m_lineVertexCount].g = g; m_lineVertices[m_lineVertexCount].b = b; m_lineVertices[m_lineVertexCount].a = a;
    m_lineVertices[m_lineVertexCount].u = 0.0f; m_lineVertices[m_lineVertexCount].v = 0.0f;
    m_lineVertexCount++;
    
    // Note: lineWidth handling would need glLineWidth equivalent in modern OpenGL
    // For now, we'll ignore lineWidth as it's deprecated in core profile
    
    // FIXED: Immediate flush with separated buffers for performance + correctness
    FlushLines();
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
    if (m_triangleVertexCount + 6 > MAX_VERTICES) {
        FlushTriangles(false);
    }
    
    // Convert colors to floats
    float rTL = colTL.m_r / 255.0f, gTL = colTL.m_g / 255.0f, bTL = colTL.m_b / 255.0f, aTL = colTL.m_a / 255.0f;
    float rTR = colTR.m_r / 255.0f, gTR = colTR.m_g / 255.0f, bTR = colTR.m_b / 255.0f, aTR = colTR.m_a / 255.0f;
    float rBR = colBR.m_r / 255.0f, gBR = colBR.m_g / 255.0f, bBR = colBR.m_b / 255.0f, aBR = colBR.m_a / 255.0f;
    float rBL = colBL.m_r / 255.0f, gBL = colBL.m_g / 255.0f, bBL = colBL.m_b / 255.0f, aBL = colBL.m_a / 255.0f;
    
    // First triangle: TL, TR, BR
    m_triangleVertices[m_triangleVertexCount].x = x;      m_triangleVertices[m_triangleVertexCount].y = y;
    m_triangleVertices[m_triangleVertexCount].r = rTL;    m_triangleVertices[m_triangleVertexCount].g = gTL;
    m_triangleVertices[m_triangleVertexCount].b = bTL;    m_triangleVertices[m_triangleVertexCount].a = aTL;
    m_triangleVertices[m_triangleVertexCount].u = 0.0f;   m_triangleVertices[m_triangleVertexCount].v = 0.0f;
    m_triangleVertexCount++;
    
    m_triangleVertices[m_triangleVertexCount].x = x + w;  m_triangleVertices[m_triangleVertexCount].y = y;
    m_triangleVertices[m_triangleVertexCount].r = rTR;    m_triangleVertices[m_triangleVertexCount].g = gTR;
    m_triangleVertices[m_triangleVertexCount].b = bTR;    m_triangleVertices[m_triangleVertexCount].a = aTR;
    m_triangleVertices[m_triangleVertexCount].u = 1.0f;   m_triangleVertices[m_triangleVertexCount].v = 0.0f;
    m_triangleVertexCount++;
    
    m_triangleVertices[m_triangleVertexCount].x = x + w;  m_triangleVertices[m_triangleVertexCount].y = y + h;
    m_triangleVertices[m_triangleVertexCount].r = rBR;    m_triangleVertices[m_triangleVertexCount].g = gBR;
    m_triangleVertices[m_triangleVertexCount].b = bBR;    m_triangleVertices[m_triangleVertexCount].a = aBR;
    m_triangleVertices[m_triangleVertexCount].u = 1.0f;   m_triangleVertices[m_triangleVertexCount].v = 1.0f;
    m_triangleVertexCount++;
    
    // Second triangle: TL, BR, BL
    m_triangleVertices[m_triangleVertexCount].x = x;      m_triangleVertices[m_triangleVertexCount].y = y;
    m_triangleVertices[m_triangleVertexCount].r = rTL;    m_triangleVertices[m_triangleVertexCount].g = gTL;
    m_triangleVertices[m_triangleVertexCount].b = bTL;    m_triangleVertices[m_triangleVertexCount].a = aTL;
    m_triangleVertices[m_triangleVertexCount].u = 0.0f;   m_triangleVertices[m_triangleVertexCount].v = 0.0f;
    m_triangleVertexCount++;
    
    m_triangleVertices[m_triangleVertexCount].x = x + w;  m_triangleVertices[m_triangleVertexCount].y = y + h;
    m_triangleVertices[m_triangleVertexCount].r = rBR;    m_triangleVertices[m_triangleVertexCount].g = gBR;
    m_triangleVertices[m_triangleVertexCount].b = bBR;    m_triangleVertices[m_triangleVertexCount].a = aBR;
    m_triangleVertices[m_triangleVertexCount].u = 1.0f;   m_triangleVertices[m_triangleVertexCount].v = 1.0f;
    m_triangleVertexCount++;
    
    m_triangleVertices[m_triangleVertexCount].x = x;      m_triangleVertices[m_triangleVertexCount].y = y + h;
    m_triangleVertices[m_triangleVertexCount].r = rBL;    m_triangleVertices[m_triangleVertexCount].g = gBL;
    m_triangleVertices[m_triangleVertexCount].b = bBL;    m_triangleVertices[m_triangleVertexCount].a = aBL;
    m_triangleVertices[m_triangleVertexCount].u = 0.0f;   m_triangleVertices[m_triangleVertexCount].v = 1.0f;
    m_triangleVertexCount++;
    
    FlushTriangles(false);
}


void Renderer::Line ( float x1, float y1, float x2, float y2, Colour const &col, float lineWidth )
{
    // Convert to modern OpenGL using vertex buffer
    if (m_lineVertexCount + 2 > MAX_VERTICES) {
        FlushLines();
    }
    
    // Convert color to float
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    // Add line vertices
    m_lineVertices[m_lineVertexCount].x = x1; m_lineVertices[m_lineVertexCount].y = y1;
    m_lineVertices[m_lineVertexCount].r = r; m_lineVertices[m_lineVertexCount].g = g; m_lineVertices[m_lineVertexCount].b = b; m_lineVertices[m_lineVertexCount].a = a;
    m_lineVertices[m_lineVertexCount].u = 0.0f; m_lineVertices[m_lineVertexCount].v = 0.0f;
    m_lineVertexCount++;
    
    m_lineVertices[m_lineVertexCount].x = x2; m_lineVertices[m_lineVertexCount].y = y2;
    m_lineVertices[m_lineVertexCount].r = r; m_lineVertices[m_lineVertexCount].g = g; m_lineVertices[m_lineVertexCount].b = b; m_lineVertices[m_lineVertexCount].a = a;
    m_lineVertices[m_lineVertexCount].u = 0.0f; m_lineVertices[m_lineVertexCount].v = 0.0f;
    m_lineVertexCount++;
    
    // Note: lineWidth handling would need geometry shader or thick line implementation
    // For now, we'll ignore lineWidth as it's deprecated in core profile
    
    // FIXED: Immediate flush with separated buffers for performance + correctness
    FlushLines();
}

void Renderer::Circle( float x, float y, float radius, int numPoints, Colour const &col, float lineWidth )
{
    // Convert to modern OpenGL using vertex buffer
    // We need numPoints * 2 vertices for GL_LINES (each line segment needs 2 vertices)
    if (m_lineVertexCount + numPoints * 2 > MAX_VERTICES) {
        FlushLines();
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
        m_lineVertices[m_lineVertexCount].x = x1; m_lineVertices[m_lineVertexCount].y = y1;
        m_lineVertices[m_lineVertexCount].r = r; m_lineVertices[m_lineVertexCount].g = g; m_lineVertices[m_lineVertexCount].b = b; m_lineVertices[m_lineVertexCount].a = a;
        m_lineVertices[m_lineVertexCount].u = 0.0f; m_lineVertices[m_lineVertexCount].v = 0.0f;
        m_lineVertexCount++;
        
        m_lineVertices[m_lineVertexCount].x = x2; m_lineVertices[m_lineVertexCount].y = y2;
        m_lineVertices[m_lineVertexCount].r = r; m_lineVertices[m_lineVertexCount].g = g; m_lineVertices[m_lineVertexCount].b = b; m_lineVertices[m_lineVertexCount].a = a;
        m_lineVertices[m_lineVertexCount].u = 0.0f; m_lineVertices[m_lineVertexCount].v = 0.0f;
        m_lineVertexCount++;
    }
    
    // Note: lineWidth handling would need geometry shader or thick line implementation
    // For now, we'll ignore lineWidth as it's deprecated in core profile
    
    // FIXED: Immediate flush with separated buffers for performance + correctness
    FlushLines();
}

void Renderer::CircleFill ( float x, float y, float radius, int numPoints, Colour const &col )
{
    // Convert to modern OpenGL using vertex buffer
    // We need numPoints * 3 vertices for triangles (triangle fan converted to individual triangles)
    if (m_triangleVertexCount + numPoints * 3 > MAX_VERTICES) {
        FlushTriangles(false);
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
        m_triangleVertices[m_triangleVertexCount].x = x; m_triangleVertices[m_triangleVertexCount].y = y;
        m_triangleVertices[m_triangleVertexCount].r = r; m_triangleVertices[m_triangleVertexCount].g = g; m_triangleVertices[m_triangleVertexCount].b = b; m_triangleVertices[m_triangleVertexCount].a = a;
        m_triangleVertices[m_triangleVertexCount].u = 0.5f; m_triangleVertices[m_triangleVertexCount].v = 0.5f;
        m_triangleVertexCount++;
        
        // Point i
        m_triangleVertices[m_triangleVertexCount].x = x1; m_triangleVertices[m_triangleVertexCount].y = y1;
        m_triangleVertices[m_triangleVertexCount].r = r; m_triangleVertices[m_triangleVertexCount].g = g; m_triangleVertices[m_triangleVertexCount].b = b; m_triangleVertices[m_triangleVertexCount].a = a;
        m_triangleVertices[m_triangleVertexCount].u = 0.0f; m_triangleVertices[m_triangleVertexCount].v = 0.0f;
        m_triangleVertexCount++;
        
        // Point i+1
        m_triangleVertices[m_triangleVertexCount].x = x2; m_triangleVertices[m_triangleVertexCount].y = y2;
        m_triangleVertices[m_triangleVertexCount].r = r; m_triangleVertices[m_triangleVertexCount].g = g; m_triangleVertices[m_triangleVertexCount].b = b; m_triangleVertices[m_triangleVertexCount].a = a;
        m_triangleVertices[m_triangleVertexCount].u = 1.0f; m_triangleVertices[m_triangleVertexCount].v = 0.0f;
        m_triangleVertexCount++;
    }
    
    // FIXED: Immediate flush with separated buffers for performance + correctness
    FlushTriangles(false);
}

void Renderer::TriangleFill( float x1, float y1, float x2, float y2, float x3, float y3, Colour const &col )
{
    // Convert to modern OpenGL using vertex buffer  
    if (m_triangleVertexCount + 3 > MAX_VERTICES) {
        FlushTriangles(false);
    }
    
    // Convert color to float
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    // Create one triangle with the three vertices
    m_triangleVertices[m_triangleVertexCount].x = x1; m_triangleVertices[m_triangleVertexCount].y = y1;
    m_triangleVertices[m_triangleVertexCount].r = r;  m_triangleVertices[m_triangleVertexCount].g = g;
    m_triangleVertices[m_triangleVertexCount].b = b;  m_triangleVertices[m_triangleVertexCount].a = a;
    m_triangleVertices[m_triangleVertexCount].u = 0.0f; m_triangleVertices[m_triangleVertexCount].v = 0.0f;
    m_triangleVertexCount++;
    
    m_triangleVertices[m_triangleVertexCount].x = x2; m_triangleVertices[m_triangleVertexCount].y = y2;
    m_triangleVertices[m_triangleVertexCount].r = r;  m_triangleVertices[m_triangleVertexCount].g = g;
    m_triangleVertices[m_triangleVertexCount].b = b;  m_triangleVertices[m_triangleVertexCount].a = a;
    m_triangleVertices[m_triangleVertexCount].u = 0.0f; m_triangleVertices[m_triangleVertexCount].v = 0.0f;
    m_triangleVertexCount++;
    
    m_triangleVertices[m_triangleVertexCount].x = x3; m_triangleVertices[m_triangleVertexCount].y = y3;
    m_triangleVertices[m_triangleVertexCount].r = r;  m_triangleVertices[m_triangleVertexCount].g = g;
    m_triangleVertices[m_triangleVertexCount].b = b;  m_triangleVertices[m_triangleVertexCount].a = a;
    m_triangleVertices[m_triangleVertexCount].u = 0.0f; m_triangleVertices[m_triangleVertexCount].v = 0.0f;
    m_triangleVertexCount++;
    
    // FIXED: Immediate flush with separated buffers for performance + correctness
    FlushTriangles(false);
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
    if (m_lineVertexCount + 1 > MAX_VERTICES) {
        // If we're running out of space, we need to handle this carefully
        // For now, just flush what we have (this breaks the line strip but prevents overflow)
        FlushLines();
    }
    
    // Convert color to float
    float r = m_currentLineColor.m_r / 255.0f, g = m_currentLineColor.m_g / 255.0f;
    float b = m_currentLineColor.m_b / 255.0f, a = m_currentLineColor.m_a / 255.0f;
    
    // Add vertex to our line buffer
    m_lineVertices[m_lineVertexCount].x = x; m_lineVertices[m_lineVertexCount].y = y;
    m_lineVertices[m_lineVertexCount].r = r; m_lineVertices[m_lineVertexCount].g = g;
    m_lineVertices[m_lineVertexCount].b = b; m_lineVertices[m_lineVertexCount].a = a;
    m_lineVertices[m_lineVertexCount].u = 0.0f; m_lineVertices[m_lineVertexCount].v = 0.0f;
    m_lineVertexCount++;
}

void Renderer::EndLines()
{
    // Convert the accumulated vertices to GL_LINES format
    // We need to convert from line strip to individual line segments
    if (m_lineVertexCount < 2) {
        // Not enough vertices for lines, just clear
        m_lineVertexCount = 0;
        return;
    }
    
    // Create a temporary buffer for line segments
    // We need (m_lineVertexCount - 1) * 2 vertices for GL_LINES from a line strip
    int tempLineVertexCount = (m_lineVertexCount - 1) * 2;
    Vertex2D* tempLineVertices = new Vertex2D[tempLineVertexCount];
    
    int lineIndex = 0;
    for (int i = 0; i < m_lineVertexCount - 1; i++) {
        // Line from vertex i to vertex i+1
        tempLineVertices[lineIndex++] = m_lineVertices[i];
        tempLineVertices[lineIndex++] = m_lineVertices[i + 1];
    }
    
    // Clear our current vertex buffer and add the line segments
    m_lineVertexCount = 0;
    
    // Add all line segments to our vertex buffer
    for (int i = 0; i < tempLineVertexCount; i++) {
        if (m_lineVertexCount >= MAX_VERTICES) {
            FlushLines();
        }
        m_lineVertices[m_lineVertexCount++] = tempLineVertices[i];
    }
    
    // Clean up temporary buffer
    delete[] tempLineVertices;
    
    // Flush the lines
    FlushLines();
}

// Line strip rendering functions for continuous lines (replaces GL_LINE_STRIP)
void Renderer::BeginLineStrip2D(Colour const &col, float lineWidth)
{
    // Store line strip state
    m_lineStripActive = true;
    m_lineStripColor = col;
    m_lineStripWidth = lineWidth;
    
    // Clear vertex buffer for new line strip
    m_lineVertexCount = 0;
}

void Renderer::LineStripVertex2D(float x, float y)
{
    if (!m_lineStripActive) return;
    
    // Check if we need to flush due to buffer overflow
    if (m_lineVertexCount >= MAX_VERTICES) {
        // Convert current vertices to line segments and flush
        EndLineStrip2D();
        BeginLineStrip2D(m_lineStripColor, m_lineStripWidth);
    }
    
    // Convert color to float
    float r = m_lineStripColor.m_r / 255.0f, g = m_lineStripColor.m_g / 255.0f;
    float b = m_lineStripColor.m_b / 255.0f, a = m_lineStripColor.m_a / 255.0f;
    
    // Add vertex to our line strip buffer
    m_lineVertices[m_lineVertexCount].x = x; m_lineVertices[m_lineVertexCount].y = y;
    m_lineVertices[m_lineVertexCount].r = r; m_lineVertices[m_lineVertexCount].g = g;
    m_lineVertices[m_lineVertexCount].b = b; m_lineVertices[m_lineVertexCount].a = a;
    m_lineVertices[m_lineVertexCount].u = 0.0f; m_lineVertices[m_lineVertexCount].v = 0.0f;
    m_lineVertexCount++;
}

void Renderer::EndLineStrip2D()
{
    if (!m_lineStripActive) return;
    
    // Convert the accumulated vertices to GL_LINES format
    if (m_lineVertexCount < 2) {
        // Not enough vertices for lines, just clear
        m_lineVertexCount = 0;
        m_lineStripActive = false;
        return;
    }
    
    // Create a temporary buffer for line segments
    // We need (m_lineVertexCount - 1) * 2 vertices for GL_LINES from a line strip
    int tempVertexCount = (m_lineVertexCount - 1) * 2;
    Vertex2D* tempVertices = new Vertex2D[tempVertexCount];
    
    int lineIndex = 0;
    for (int i = 0; i < m_lineVertexCount - 1; i++) {
        // Line from vertex i to vertex i+1
        tempVertices[lineIndex++] = m_lineVertices[i];
        tempVertices[lineIndex++] = m_lineVertices[i + 1];
    }
    
    // Clear our current vertex buffer and add the line segments
    m_lineVertexCount = 0;
    
    // Add all line segments to our vertex buffer
    for (int i = 0; i < tempVertexCount; i++) {
        if (m_lineVertexCount >= MAX_VERTICES) {
            FlushLines();
        }
        m_lineVertices[m_lineVertexCount++] = tempVertices[i];
    }
    
    // Clean up temporary buffer
    delete[] tempVertices;
    
    // Flush the lines
    FlushLines();
    
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
    m_lineVertexCount = 0;
}

void Renderer::CachedLineStripVertex(float x, float y)
{
    if (!m_cachedLineStripActive) return;
    
    // Check if we need to flush due to buffer overflow
    if (m_lineVertexCount >= MAX_VERTICES) {
        // For cached VBOs, we need to handle this differently
        // For now, just ignore extra vertices (this shouldn't happen for coastlines)
        AppDebugOut("Warning: Cached line strip exceeded vertex buffer size\n");
        return;
    }
    
    // Convert color to float
    float r = m_cachedLineStripColor.m_r / 255.0f, g = m_cachedLineStripColor.m_g / 255.0f;
    float b = m_cachedLineStripColor.m_b / 255.0f, a = m_cachedLineStripColor.m_a / 255.0f;
    
    // Add vertex to our cached line strip buffer
    m_lineVertices[m_lineVertexCount].x = x; m_lineVertices[m_lineVertexCount].y = y;
    m_lineVertices[m_lineVertexCount].r = r; m_lineVertices[m_lineVertexCount].g = g;
    m_lineVertices[m_lineVertexCount].b = b; m_lineVertices[m_lineVertexCount].a = a;
    m_lineVertices[m_lineVertexCount].u = 0.0f; m_lineVertices[m_lineVertexCount].v = 0.0f;
    m_lineVertexCount++;
}

void Renderer::EndCachedLineStrip()
{
    if (!m_cachedLineStripActive || !m_currentCacheKey) return;
    
    // Convert the accumulated vertices to GL_LINES format and store in VBO
    if (m_lineVertexCount < 2) {
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
    int lineVertexCount = (m_lineVertexCount - 1) * 2;
    Vertex2D* lineVertices = new Vertex2D[lineVertexCount];
    
    int lineIndex = 0;
    for (int i = 0; i < m_lineVertexCount - 1; i++) {
        // Line from vertex i to vertex i+1
        lineVertices[lineIndex++] = m_lineVertices[i];
        lineVertices[lineIndex++] = m_lineVertices[i + 1];
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
    m_lineVertexCount = 0;
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
    if (m_triangleVertexCount + 6 > MAX_VERTICES) {
        FlushTriangles(true);
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
    
    m_triangleVertices[m_triangleVertexCount].x = x;      m_triangleVertices[m_triangleVertexCount].y = y;
    m_triangleVertices[m_triangleVertexCount].r = r;      m_triangleVertices[m_triangleVertexCount].g = g;
    m_triangleVertices[m_triangleVertexCount].b = b;      m_triangleVertices[m_triangleVertexCount].a = a;
    m_triangleVertices[m_triangleVertexCount].u = onePixelW; m_triangleVertices[m_triangleVertexCount].v = 1.0f - onePixelH;
    m_triangleVertexCount++;
    
    m_triangleVertices[m_triangleVertexCount].x = x + w;  m_triangleVertices[m_triangleVertexCount].y = y;
    m_triangleVertices[m_triangleVertexCount].r = r;      m_triangleVertices[m_triangleVertexCount].g = g;
    m_triangleVertices[m_triangleVertexCount].b = b;      m_triangleVertices[m_triangleVertexCount].a = a;
    m_triangleVertices[m_triangleVertexCount].u = 1.0f - onePixelW; m_triangleVertices[m_triangleVertexCount].v = 1.0f - onePixelH;
    m_triangleVertexCount++;
    
    m_triangleVertices[m_triangleVertexCount].x = x + w;  m_triangleVertices[m_triangleVertexCount].y = y + h;
    m_triangleVertices[m_triangleVertexCount].r = r;      m_triangleVertices[m_triangleVertexCount].g = g;
    m_triangleVertices[m_triangleVertexCount].b = b;      m_triangleVertices[m_triangleVertexCount].a = a;
    m_triangleVertices[m_triangleVertexCount].u = 1.0f - onePixelW; m_triangleVertices[m_triangleVertexCount].v = onePixelH;
    m_triangleVertexCount++;
    
    // Second triangle: TL, BR, BL
    m_triangleVertices[m_triangleVertexCount].x = x;      m_triangleVertices[m_triangleVertexCount].y = y;
    m_triangleVertices[m_triangleVertexCount].r = r;      m_triangleVertices[m_triangleVertexCount].g = g;
    m_triangleVertices[m_triangleVertexCount].b = b;      m_triangleVertices[m_triangleVertexCount].a = a;
    m_triangleVertices[m_triangleVertexCount].u = onePixelW; m_triangleVertices[m_triangleVertexCount].v = 1.0f - onePixelH;
    m_triangleVertexCount++;
    
    m_triangleVertices[m_triangleVertexCount].x = x + w;  m_triangleVertices[m_triangleVertexCount].y = y + h;
    m_triangleVertices[m_triangleVertexCount].r = r;      m_triangleVertices[m_triangleVertexCount].g = g;
    m_triangleVertices[m_triangleVertexCount].b = b;      m_triangleVertices[m_triangleVertexCount].a = a;
    m_triangleVertices[m_triangleVertexCount].u = 1.0f - onePixelW; m_triangleVertices[m_triangleVertexCount].v = onePixelH;
    m_triangleVertexCount++;
    
    m_triangleVertices[m_triangleVertexCount].x = x;      m_triangleVertices[m_triangleVertexCount].y = y + h;
    m_triangleVertices[m_triangleVertexCount].r = r;      m_triangleVertices[m_triangleVertexCount].g = g;
    m_triangleVertices[m_triangleVertexCount].b = b;      m_triangleVertices[m_triangleVertexCount].a = a;
    m_triangleVertices[m_triangleVertexCount].u = onePixelW; m_triangleVertices[m_triangleVertexCount].v = onePixelH;
    m_triangleVertexCount++;
    
    FlushTriangles(true);
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
    if (m_triangleVertexCount + 6 > MAX_VERTICES) {
        FlushTriangles(true);
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
    m_triangleVertices[m_triangleVertexCount].x = vert1.x; m_triangleVertices[m_triangleVertexCount].y = vert1.y;
    m_triangleVertices[m_triangleVertexCount].r = r; m_triangleVertices[m_triangleVertexCount].g = g; m_triangleVertices[m_triangleVertexCount].b = b; m_triangleVertices[m_triangleVertexCount].a = a;
    m_triangleVertices[m_triangleVertexCount].u = onePixelW; m_triangleVertices[m_triangleVertexCount].v = 1.0f - onePixelH;
    m_triangleVertexCount++;
    
    m_triangleVertices[m_triangleVertexCount].x = vert2.x; m_triangleVertices[m_triangleVertexCount].y = vert2.y;
    m_triangleVertices[m_triangleVertexCount].r = r; m_triangleVertices[m_triangleVertexCount].g = g; m_triangleVertices[m_triangleVertexCount].b = b; m_triangleVertices[m_triangleVertexCount].a = a;
    m_triangleVertices[m_triangleVertexCount].u = 1.0f - onePixelW; m_triangleVertices[m_triangleVertexCount].v = 1.0f - onePixelH;
    m_triangleVertexCount++;
    
    m_triangleVertices[m_triangleVertexCount].x = vert3.x; m_triangleVertices[m_triangleVertexCount].y = vert3.y;
    m_triangleVertices[m_triangleVertexCount].r = r; m_triangleVertices[m_triangleVertexCount].g = g; m_triangleVertices[m_triangleVertexCount].b = b; m_triangleVertices[m_triangleVertexCount].a = a;
    m_triangleVertices[m_triangleVertexCount].u = 1.0f - onePixelW; m_triangleVertices[m_triangleVertexCount].v = onePixelH;
    m_triangleVertexCount++;
    
    // Second triangle: vert1, vert3, vert4
    m_triangleVertices[m_triangleVertexCount].x = vert1.x; m_triangleVertices[m_triangleVertexCount].y = vert1.y;
    m_triangleVertices[m_triangleVertexCount].r = r; m_triangleVertices[m_triangleVertexCount].g = g; m_triangleVertices[m_triangleVertexCount].b = b; m_triangleVertices[m_triangleVertexCount].a = a;
    m_triangleVertices[m_triangleVertexCount].u = onePixelW; m_triangleVertices[m_triangleVertexCount].v = 1.0f - onePixelH;
    m_triangleVertexCount++;
    
    m_triangleVertices[m_triangleVertexCount].x = vert3.x; m_triangleVertices[m_triangleVertexCount].y = vert3.y;
    m_triangleVertices[m_triangleVertexCount].r = r; m_triangleVertices[m_triangleVertexCount].g = g; m_triangleVertices[m_triangleVertexCount].b = b; m_triangleVertices[m_triangleVertexCount].a = a;
    m_triangleVertices[m_triangleVertexCount].u = 1.0f - onePixelW; m_triangleVertices[m_triangleVertexCount].v = onePixelH;
    m_triangleVertexCount++;
    
    m_triangleVertices[m_triangleVertexCount].x = vert4.x; m_triangleVertices[m_triangleVertexCount].y = vert4.y;
    m_triangleVertices[m_triangleVertexCount].r = r; m_triangleVertices[m_triangleVertexCount].g = g; m_triangleVertices[m_triangleVertexCount].b = b; m_triangleVertices[m_triangleVertexCount].a = a;
    m_triangleVertices[m_triangleVertexCount].u = onePixelW; m_triangleVertices[m_triangleVertexCount].v = onePixelH;
    m_triangleVertexCount++;
    
    // PHASE 6.1 ROLLBACK: Restore immediate flush to fix render order issues
    FlushTriangles(true);
}

void Renderer::BlitChar( unsigned int textureID, float x, float y, float w, float h, 
                          float texX, float texY, float texW, float texH, Colour const &col)
{
    // Convert to modern OpenGL using vertex buffer  
    if (m_triangleVertexCount + 6 > MAX_VERTICES) {
        FlushTriangles(true);
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
    m_triangleVertices[m_triangleVertexCount].x = x;      m_triangleVertices[m_triangleVertexCount].y = y;
    m_triangleVertices[m_triangleVertexCount].r = r;      m_triangleVertices[m_triangleVertexCount].g = g;
    m_triangleVertices[m_triangleVertexCount].b = b;      m_triangleVertices[m_triangleVertexCount].a = a;
    m_triangleVertices[m_triangleVertexCount].u = u1;     m_triangleVertices[m_triangleVertexCount].v = v2;
    m_triangleVertexCount++;
    
    m_triangleVertices[m_triangleVertexCount].x = x + w;  m_triangleVertices[m_triangleVertexCount].y = y;
    m_triangleVertices[m_triangleVertexCount].r = r;      m_triangleVertices[m_triangleVertexCount].g = g;
    m_triangleVertices[m_triangleVertexCount].b = b;      m_triangleVertices[m_triangleVertexCount].a = a;
    m_triangleVertices[m_triangleVertexCount].u = u2;     m_triangleVertices[m_triangleVertexCount].v = v2;
    m_triangleVertexCount++;
    
    m_triangleVertices[m_triangleVertexCount].x = x + w;  m_triangleVertices[m_triangleVertexCount].y = y + h;
    m_triangleVertices[m_triangleVertexCount].r = r;      m_triangleVertices[m_triangleVertexCount].g = g;
    m_triangleVertices[m_triangleVertexCount].b = b;      m_triangleVertices[m_triangleVertexCount].a = a;
    m_triangleVertices[m_triangleVertexCount].u = u2;     m_triangleVertices[m_triangleVertexCount].v = v1;
    m_triangleVertexCount++;
    
    // Second triangle: TL, BR, BL
    m_triangleVertices[m_triangleVertexCount].x = x;      m_triangleVertices[m_triangleVertexCount].y = y;
    m_triangleVertices[m_triangleVertexCount].r = r;      m_triangleVertices[m_triangleVertexCount].g = g;
    m_triangleVertices[m_triangleVertexCount].b = b;      m_triangleVertices[m_triangleVertexCount].a = a;
    m_triangleVertices[m_triangleVertexCount].u = u1;     m_triangleVertices[m_triangleVertexCount].v = v2;
    m_triangleVertexCount++;
    
    m_triangleVertices[m_triangleVertexCount].x = x + w;  m_triangleVertices[m_triangleVertexCount].y = y + h;
    m_triangleVertices[m_triangleVertexCount].r = r;      m_triangleVertices[m_triangleVertexCount].g = g;
    m_triangleVertices[m_triangleVertexCount].b = b;      m_triangleVertices[m_triangleVertexCount].a = a;
    m_triangleVertices[m_triangleVertexCount].u = u2;     m_triangleVertices[m_triangleVertexCount].v = v1;
    m_triangleVertexCount++;
    
    m_triangleVertices[m_triangleVertexCount].x = x;      m_triangleVertices[m_triangleVertexCount].y = y + h;
    m_triangleVertices[m_triangleVertexCount].r = r;      m_triangleVertices[m_triangleVertexCount].g = g;
    m_triangleVertices[m_triangleVertexCount].b = b;      m_triangleVertices[m_triangleVertexCount].a = a;
    m_triangleVertices[m_triangleVertexCount].u = u1;     m_triangleVertices[m_triangleVertexCount].v = v1;
    m_triangleVertexCount++;
    
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
    if (m_triangleVertexCount + 6 > MAX_VERTICES) {
        FlushTriangles(true);
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
    m_triangleVertices[m_triangleVertexCount].x = x;      m_triangleVertices[m_triangleVertexCount].y = y;
    m_triangleVertices[m_triangleVertexCount].r = r;      m_triangleVertices[m_triangleVertexCount].g = g;
    m_triangleVertices[m_triangleVertexCount].b = b;      m_triangleVertices[m_triangleVertexCount].a = a;
    m_triangleVertices[m_triangleVertexCount].u = onePixelW; m_triangleVertices[m_triangleVertexCount].v = 1.0f - onePixelH;
    m_triangleVertexCount++;
    
    m_triangleVertices[m_triangleVertexCount].x = x + w;  m_triangleVertices[m_triangleVertexCount].y = y;
    m_triangleVertices[m_triangleVertexCount].r = r;      m_triangleVertices[m_triangleVertexCount].g = g;
    m_triangleVertices[m_triangleVertexCount].b = b;      m_triangleVertices[m_triangleVertexCount].a = a;
    m_triangleVertices[m_triangleVertexCount].u = 1.0f - onePixelW; m_triangleVertices[m_triangleVertexCount].v = 1.0f - onePixelH;
    m_triangleVertexCount++;
    
    m_triangleVertices[m_triangleVertexCount].x = x + w;  m_triangleVertices[m_triangleVertexCount].y = y + h;
    m_triangleVertices[m_triangleVertexCount].r = r;      m_triangleVertices[m_triangleVertexCount].g = g;
    m_triangleVertices[m_triangleVertexCount].b = b;      m_triangleVertices[m_triangleVertexCount].a = a;
    m_triangleVertices[m_triangleVertexCount].u = 1.0f - onePixelW; m_triangleVertices[m_triangleVertexCount].v = onePixelH;
    m_triangleVertexCount++;
    
    // Second triangle: TL, BR, BL
    m_triangleVertices[m_triangleVertexCount].x = x;      m_triangleVertices[m_triangleVertexCount].y = y;
    m_triangleVertices[m_triangleVertexCount].r = r;      m_triangleVertices[m_triangleVertexCount].g = g;
    m_triangleVertices[m_triangleVertexCount].b = b;      m_triangleVertices[m_triangleVertexCount].a = a;
    m_triangleVertices[m_triangleVertexCount].u = onePixelW; m_triangleVertices[m_triangleVertexCount].v = 1.0f - onePixelH;
    m_triangleVertexCount++;
    
    m_triangleVertices[m_triangleVertexCount].x = x + w;  m_triangleVertices[m_triangleVertexCount].y = y + h;
    m_triangleVertices[m_triangleVertexCount].r = r;      m_triangleVertices[m_triangleVertexCount].g = g;
    m_triangleVertices[m_triangleVertexCount].b = b;      m_triangleVertices[m_triangleVertexCount].a = a;
    m_triangleVertices[m_triangleVertexCount].u = 1.0f - onePixelW; m_triangleVertices[m_triangleVertexCount].v = onePixelH;
    m_triangleVertexCount++;
    
    m_triangleVertices[m_triangleVertexCount].x = x;      m_triangleVertices[m_triangleVertexCount].y = y + h;
    m_triangleVertices[m_triangleVertexCount].r = r;      m_triangleVertices[m_triangleVertexCount].g = g;
    m_triangleVertices[m_triangleVertexCount].b = b;      m_triangleVertices[m_triangleVertexCount].a = a;
    m_triangleVertices[m_triangleVertexCount].u = onePixelW; m_triangleVertices[m_triangleVertexCount].v = onePixelH;
    m_triangleVertexCount++;
    
    // PHASE 6.1.1 OPTIMIZATION: No immediate flush - let batching system handle it
    // Objects with same texture will now be batched together for safe performance gain
}

void Renderer::BlitBatched( Image *src, float x, float y, float w, float h, Colour const &col, float angle)
{    
    // Convert to modern OpenGL using vertex buffer
    if (m_triangleVertexCount + 6 > MAX_VERTICES) {
        FlushTriangles(true);
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
    m_triangleVertices[m_triangleVertexCount].x = vert1.x; m_triangleVertices[m_triangleVertexCount].y = vert1.y;
    m_triangleVertices[m_triangleVertexCount].r = r; m_triangleVertices[m_triangleVertexCount].g = g; m_triangleVertices[m_triangleVertexCount].b = b; m_triangleVertices[m_triangleVertexCount].a = a;
    m_triangleVertices[m_triangleVertexCount].u = onePixelW; m_triangleVertices[m_triangleVertexCount].v = 1.0f - onePixelH;
    m_triangleVertexCount++;
    
    m_triangleVertices[m_triangleVertexCount].x = vert2.x; m_triangleVertices[m_triangleVertexCount].y = vert2.y;
    m_triangleVertices[m_triangleVertexCount].r = r; m_triangleVertices[m_triangleVertexCount].g = g; m_triangleVertices[m_triangleVertexCount].b = b; m_triangleVertices[m_triangleVertexCount].a = a;
    m_triangleVertices[m_triangleVertexCount].u = 1.0f - onePixelW; m_triangleVertices[m_triangleVertexCount].v = 1.0f - onePixelH;
    m_triangleVertexCount++;
    
    m_triangleVertices[m_triangleVertexCount].x = vert3.x; m_triangleVertices[m_triangleVertexCount].y = vert3.y;
    m_triangleVertices[m_triangleVertexCount].r = r; m_triangleVertices[m_triangleVertexCount].g = g; m_triangleVertices[m_triangleVertexCount].b = b; m_triangleVertices[m_triangleVertexCount].a = a;
    m_triangleVertices[m_triangleVertexCount].u = 1.0f - onePixelW; m_triangleVertices[m_triangleVertexCount].v = onePixelH;
    m_triangleVertexCount++;
    
    // Second triangle: vert1, vert3, vert4
    m_triangleVertices[m_triangleVertexCount].x = vert1.x; m_triangleVertices[m_triangleVertexCount].y = vert1.y;
    m_triangleVertices[m_triangleVertexCount].r = r; m_triangleVertices[m_triangleVertexCount].g = g; m_triangleVertices[m_triangleVertexCount].b = b; m_triangleVertices[m_triangleVertexCount].a = a;
    m_triangleVertices[m_triangleVertexCount].u = onePixelW; m_triangleVertices[m_triangleVertexCount].v = 1.0f - onePixelH;
    m_triangleVertexCount++;
    
    m_triangleVertices[m_triangleVertexCount].x = vert3.x; m_triangleVertices[m_triangleVertexCount].y = vert3.y;
    m_triangleVertices[m_triangleVertexCount].r = r; m_triangleVertices[m_triangleVertexCount].g = g; m_triangleVertices[m_triangleVertexCount].b = b; m_triangleVertices[m_triangleVertexCount].a = a;
    m_triangleVertices[m_triangleVertexCount].u = 1.0f - onePixelW; m_triangleVertices[m_triangleVertexCount].v = onePixelH;
    m_triangleVertexCount++;
    
    m_triangleVertices[m_triangleVertexCount].x = vert4.x; m_triangleVertices[m_triangleVertexCount].y = vert4.y;
    m_triangleVertices[m_triangleVertexCount].r = r; m_triangleVertices[m_triangleVertexCount].g = g; m_triangleVertices[m_triangleVertexCount].b = b; m_triangleVertices[m_triangleVertexCount].a = a;
    m_triangleVertices[m_triangleVertexCount].u = onePixelW; m_triangleVertices[m_triangleVertexCount].v = onePixelH;
    m_triangleVertexCount++;
    
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
        FlushTriangles(useTexture);
        return;
    }
    
    // If texture is changing and we have vertices to render, flush first
    if (m_triangleVertexCount > 0 && m_currentBoundTexture != newTextureID) {
        FlushTriangles(useTexture);
    }
    
    // Update current texture state
    m_currentBoundTexture = newTextureID;
}

void Renderer::FlushVertices(unsigned int primitiveType, bool useTexture) {
    if (primitiveType == GL_TRIANGLES) {
        FlushTriangles(useTexture);
    } else if (primitiveType == GL_LINES) {
        FlushLines();
    }
}

void Renderer::FlushTriangles(bool useTexture) {
    if (m_triangleVertexCount == 0) return;
    
    // PERFORMANCE TRACKING: Count legacy triangle draw call
    IncrementDrawCall("legacy_triangles");
    
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
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_triangleVertexCount * sizeof(Vertex2D), m_triangleVertices);
    
    // Draw
    glDrawArrays(GL_TRIANGLES, 0, m_triangleVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    // Reset vertex count
    m_triangleVertexCount = 0;
}

void Renderer::FlushLines() {
    if (m_lineVertexCount == 0) return;
    
    // PERFORMANCE TRACKING: Count legacy line draw call
    IncrementDrawCall("legacy_lines");
    
    // Lines always use color shader (no textures)
    glUseProgram(m_colorShaderProgram);
    
    // Set uniforms
    int projLoc = glGetUniformLocation(m_colorShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_colorShaderProgram, "uModelView");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    
    // Upload vertex data
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_lineVertexCount * sizeof(Vertex2D), m_lineVertices);
    
    // Draw
    glDrawArrays(GL_LINES, 0, m_lineVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    // Reset vertex count
    m_lineVertexCount = 0;
}

void Renderer::FlushAllBuffers() {
    FlushTriangles(false);  // Flush color triangles
    FlushLines();           // Flush lines
}

// Helper function for time-based flushing experiment
bool Renderer::ShouldFlushThisFrame() {
    // Always flush if either buffer is getting full (safety)
    if (m_triangleVertexCount >= MAX_VERTICES - 100 || m_lineVertexCount >= MAX_VERTICES - 100) {
        return true;
    }
    
    // ULTIMATE TEST: Only flush once per second using high-res time!
    float currentTime = GetHighResTime();
    if (currentTime - m_lastFlushTime >= 1.0f) {
        m_lastFlushTime = currentTime;
        return true;
    }
    
    return false;
}

// PERFORMANCE FIX: Specialized flush methods for new buffer systems

void Renderer::FlushUITriangles() {
    if (m_uiTriangleVertexCount == 0) return;
    
    // PERFORMANCE TRACKING: Count UI triangle draw call
    IncrementDrawCall("ui_triangles");
    
    // UI triangles always use color shader (no textures)
    glUseProgram(m_colorShaderProgram);
    
    // Set uniforms
    int projLoc = glGetUniformLocation(m_colorShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_colorShaderProgram, "uModelView");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    
    // Upload UI triangle vertex data
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_uiTriangleVertexCount * sizeof(Vertex2D), m_uiTriangleVertices);
    
    // Draw
    glDrawArrays(GL_TRIANGLES, 0, m_uiTriangleVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    // Reset vertex count
    m_uiTriangleVertexCount = 0;
}

void Renderer::FlushUILines() {
    if (m_uiLineVertexCount == 0) return;
    
    // PERFORMANCE TRACKING: Count UI line draw call
    IncrementDrawCall("ui_lines");
    
    // UI lines always use color shader (no textures)
    glUseProgram(m_colorShaderProgram);
    
    // Set uniforms
    int projLoc = glGetUniformLocation(m_colorShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_colorShaderProgram, "uModelView");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    
    // Upload UI line vertex data
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_uiLineVertexCount * sizeof(Vertex2D), m_uiLineVertices);
    
    // Draw
    glDrawArrays(GL_LINES, 0, m_uiLineVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    // Reset vertex count
    m_uiLineVertexCount = 0;
}

void Renderer::FlushTextBuffer() {
    if (m_textVertexCount == 0) return;
    
    // PERFORMANCE TRACKING: Count text draw call
    IncrementDrawCall("text");
    
    // Text always uses texture shader
    glUseProgram(m_textureShaderProgram);
    
    // Set uniforms
    int projLoc = glGetUniformLocation(m_textureShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_textureShaderProgram, "uModelView");
    int texLoc = glGetUniformLocation(m_textureShaderProgram, "ourTexture");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    glUniform1i(texLoc, 0); // Use texture unit 0
    
    // Upload text vertex data
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_textVertexCount * sizeof(Vertex2D), m_textVertices);
    
    // Draw
    glDrawArrays(GL_TRIANGLES, 0, m_textVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Renderer::FlushSpriteBuffer() {
    if (m_spriteVertexCount == 0) return;
    
    // PERFORMANCE TRACKING: Count sprite draw call
    IncrementDrawCall("sprites");
    
    // Sprites always use texture shader
    glUseProgram(m_textureShaderProgram);
    
    // Set uniforms
    int projLoc = glGetUniformLocation(m_textureShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_textureShaderProgram, "uModelView");
    int texLoc = glGetUniformLocation(m_textureShaderProgram, "ourTexture");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    glUniform1i(texLoc, 0); // Use texture unit 0
    
    // Upload sprite vertex data
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_spriteVertexCount * sizeof(Vertex2D), m_spriteVertices);
    
    // Draw
    glDrawArrays(GL_TRIANGLES, 0, m_spriteVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    // Reset vertex count
    m_spriteVertexCount = 0;
}

void Renderer::FlushSpriteContext() {
    FlushSpriteBuffer();
}

// Batch management functions for testing

void Renderer::BeginUIBatch() {
    // Disable immediate flushing for UI operations
    m_allowImmedateFlush = false;
}

void Renderer::EndUIBatch() {
    // Flush UI buffers and re-enable immediate flushing
    FlushUIContext();
    m_allowImmedateFlush = true;
}

void Renderer::BeginTextBatch() {
    // Disable immediate flushing for text operations  
    m_allowImmedateFlush = false;
}

void Renderer::EndTextBatch() {
    // Flush text buffer and re-enable immediate flushing
    FlushTextContext();
    m_allowImmedateFlush = true;
}

void Renderer::BeginSpriteBatch() {
    // Disable immediate flushing for sprite operations
    m_allowImmedateFlush = false;
}

void Renderer::EndSpriteBatch() {
    // Flush sprite buffer and re-enable immediate flushing
    FlushSpriteContext();
    m_allowImmedateFlush = true;
}

// PERFORMANCE TRACKING: Draw call counter implementations

void Renderer::ResetFrameCounters() {
    // Store previous frame's data for display
    m_prevDrawCallsPerFrame = m_drawCallsPerFrame;
    m_prevLegacyTriangleCalls = m_legacyTriangleCalls;
    m_prevLegacyLineCalls = m_legacyLineCalls;
    m_prevUiTriangleCalls = m_uiTriangleCalls;
    m_prevUiLineCalls = m_uiLineCalls;
    m_prevTextCalls = m_textCalls;
    m_prevSpriteCalls = m_spriteCalls;
    // Unit rendering previous frame data
    m_prevUnitTrailCalls = m_unitTrailCalls;
    m_prevUnitMainSpriteCalls = m_unitMainSpriteCalls;
    m_prevUnitHighlightCalls = m_unitHighlightCalls;
    m_prevUnitStateIconCalls = m_unitStateIconCalls;
    m_prevUnitCounterCalls = m_unitCounterCalls;
    m_prevUnitNukeIconCalls = m_unitNukeIconCalls;
    // Effect rendering previous frame data
    m_prevEffectsLineCalls = m_effectsLineCalls;
    m_prevEffectsSpriteCalls = m_effectsSpriteCalls;
    
    // Reset current frame counters
    m_drawCallsPerFrame = 0;
    m_legacyTriangleCalls = 0;
    m_legacyLineCalls = 0;
    m_uiTriangleCalls = 0;
    m_uiLineCalls = 0;
    m_textCalls = 0;
    m_spriteCalls = 0;
    // Unit rendering current frame counters
    m_unitTrailCalls = 0;
    m_unitMainSpriteCalls = 0;
    m_unitHighlightCalls = 0;
    m_unitStateIconCalls = 0;
    m_unitCounterCalls = 0;
    m_unitNukeIconCalls = 0;
    // Effect rendering current frame counters
    m_effectsLineCalls = 0;
    m_effectsSpriteCalls = 0;
}

void Renderer::IncrementDrawCall(const char* bufferType) {
    m_drawCallsPerFrame++;
    
    // Legacy buffer types
    if (strcmp(bufferType, "legacy_triangles") == 0) {
        m_legacyTriangleCalls++;
    } else if (strcmp(bufferType, "legacy_lines") == 0) {
        m_legacyLineCalls++;
    // Core buffer types
    } else if (strcmp(bufferType, "ui_triangles") == 0) {
        m_uiTriangleCalls++;
    } else if (strcmp(bufferType, "ui_lines") == 0) {
        m_uiLineCalls++;
    } else if (strcmp(bufferType, "text") == 0) {
        m_textCalls++;
    } else if (strcmp(bufferType, "sprites") == 0) {
        m_spriteCalls++;
    // Unit rendering buffer types
    } else if (strcmp(bufferType, "unit_trails") == 0) {
        m_unitTrailCalls++;
    } else if (strcmp(bufferType, "unit_main_sprites") == 0) {
        m_unitMainSpriteCalls++;
    } else if (strcmp(bufferType, "unit_highlights") == 0) {
        m_unitHighlightCalls++;
    } else if (strcmp(bufferType, "unit_state_icons") == 0) {
        m_unitStateIconCalls++;
    } else if (strcmp(bufferType, "unit_counters") == 0) {
        m_unitCounterCalls++;
    } else if (strcmp(bufferType, "unit_nuke_icons") == 0) {
        m_unitNukeIconCalls++;
    // Effect rendering buffer types
    } else if (strcmp(bufferType, "effects_lines") == 0) {
        m_effectsLineCalls++;
    } else if (strcmp(bufferType, "effects_sprites") == 0) {
        m_effectsSpriteCalls++;
    }
}

void Renderer::BeginFrame() {
    ResetFrameCounters();
    // Reset any frame-specific state here
}

void Renderer::FlushAllSpecializedBuffers() {
    // Core specialized buffers
    FlushUITriangles();
    FlushUILines();
    FlushTextBuffer();
    FlushSpriteBuffer();
    
    // Unit rendering specialized buffers
    FlushUnitTrails();
    FlushUnitMainSprites();
    FlushUnitHighlights();
    FlushUnitStateIcons();
    FlushUnitCounters();
    FlushUnitNukeIcons();
    
    // Effect rendering specialized buffers
    FlushEffectsLines();
    FlushEffectsSprites();
}

void Renderer::EndFrame() {
    // Flush all remaining buffers
    FlushAllSpecializedBuffers();
}

// UNIT RENDERING SPECIALIZED FLUSH METHODS

void Renderer::FlushUnitTrails() {
    if (m_unitTrailVertexCount == 0) return;
    
    // PERFORMANCE TRACKING: Count unit trail draw call
    IncrementDrawCall("unit_trails");
    
    // Unit trails always use color shader (no textures)
    glUseProgram(m_colorShaderProgram);
    
    // Set uniforms
    int projLoc = glGetUniformLocation(m_colorShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_colorShaderProgram, "uModelView");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    
    // Upload unit trail vertex data
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_unitTrailVertexCount * sizeof(Vertex2D), m_unitTrailVertices);
    
    // Draw
    glDrawArrays(GL_LINES, 0, m_unitTrailVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    // Reset vertex count
    m_unitTrailVertexCount = 0;
}

void Renderer::FlushUnitMainSprites() {
    if (m_unitMainVertexCount == 0) return;
    
    // PERFORMANCE TRACKING: Count unit main sprite draw call
    IncrementDrawCall("unit_main_sprites");
    
    // Unit main sprites always use texture shader
    glUseProgram(m_textureShaderProgram);
    
    // BUGFIX: Texture binding is now handled in UnitMainSprite() for immediate consistency
    // The texture should already be bound, but we ensure it's still bound here
    if (m_currentUnitMainTexture != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_currentUnitMainTexture);
    }
    
    // Set uniforms
    int projLoc = glGetUniformLocation(m_textureShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_textureShaderProgram, "uModelView");
    int texLoc = glGetUniformLocation(m_textureShaderProgram, "ourTexture");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    glUniform1i(texLoc, 0); // Use texture unit 0
    
    // Upload unit main sprite vertex data
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_unitMainVertexCount * sizeof(Vertex2D), m_unitMainVertices);
    
    // Draw
    glDrawArrays(GL_TRIANGLES, 0, m_unitMainVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    // Reset vertex count
    m_unitMainVertexCount = 0;
}

void Renderer::FlushUnitHighlights() {
    if (m_unitHighlightVertexCount == 0) return;
    
    // PERFORMANCE TRACKING: Count unit highlight draw call
    IncrementDrawCall("unit_highlights");
    
    // Unit highlights always use texture shader (blur textures)
    glUseProgram(m_textureShaderProgram);
    
    // Set uniforms
    int projLoc = glGetUniformLocation(m_textureShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_textureShaderProgram, "uModelView");
    int texLoc = glGetUniformLocation(m_textureShaderProgram, "ourTexture");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    glUniform1i(texLoc, 0); // Use texture unit 0
    
    // Upload unit highlight vertex data
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_unitHighlightVertexCount * sizeof(Vertex2D), m_unitHighlightVertices);
    
    // Draw
    glDrawArrays(GL_TRIANGLES, 0, m_unitHighlightVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    // Reset vertex count
    m_unitHighlightVertexCount = 0;
}

void Renderer::FlushUnitStateIcons() {
    if (m_unitStateVertexCount == 0) return;
    
    // PERFORMANCE TRACKING: Count unit state icon draw call
    IncrementDrawCall("unit_state_icons");
    
    // Unit state icons always use texture shader
    glUseProgram(m_textureShaderProgram);
    
    // Set uniforms
    int projLoc = glGetUniformLocation(m_textureShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_textureShaderProgram, "uModelView");
    int texLoc = glGetUniformLocation(m_textureShaderProgram, "ourTexture");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    glUniform1i(texLoc, 0); // Use texture unit 0
    
    // Upload unit state icon vertex data
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_unitStateVertexCount * sizeof(Vertex2D), m_unitStateVertices);
    
    // Draw
    glDrawArrays(GL_TRIANGLES, 0, m_unitStateVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    // Reset vertex count
    m_unitStateVertexCount = 0;
}

void Renderer::FlushUnitCounters() {
    if (m_unitCounterVertexCount == 0) return;
    
    // PERFORMANCE TRACKING: Count unit counter draw call
    IncrementDrawCall("unit_counters");
    
    // Unit counters always use texture shader (font texture)
    glUseProgram(m_textureShaderProgram);
    
    // Set uniforms
    int projLoc = glGetUniformLocation(m_textureShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_textureShaderProgram, "uModelView");
    int texLoc = glGetUniformLocation(m_textureShaderProgram, "ourTexture");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    glUniform1i(texLoc, 0); // Use texture unit 0
    
    // Upload unit counter vertex data
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_unitCounterVertexCount * sizeof(Vertex2D), m_unitCounterVertices);
    
    // Draw
    glDrawArrays(GL_TRIANGLES, 0, m_unitCounterVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    // Reset vertex count
    m_unitCounterVertexCount = 0;
}

void Renderer::FlushUnitNukeIcons() {
    if (m_unitNukeVertexCount == 0) return;
    
    // PERFORMANCE TRACKING: Count unit nuke icon draw call
    IncrementDrawCall("unit_nuke_icons");
    
    // Unit nuke icons always use texture shader (smallnuke.bmp)
    glUseProgram(m_textureShaderProgram);
    
    // Set uniforms
    int projLoc = glGetUniformLocation(m_textureShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_textureShaderProgram, "uModelView");
    int texLoc = glGetUniformLocation(m_textureShaderProgram, "ourTexture");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    glUniform1i(texLoc, 0); // Use texture unit 0
    
    // Upload unit nuke icon vertex data
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_unitNukeVertexCount * sizeof(Vertex2D), m_unitNukeVertices);
    
    // Draw
    glDrawArrays(GL_TRIANGLES, 0, m_unitNukeVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    // Reset vertex count
    m_unitNukeVertexCount = 0;
}

// EFFECTS RENDERING SPECIALIZED FLUSH METHODS

void Renderer::FlushEffectsLines() {
    if (m_effectsLineVertexCount == 0) return;
    
    // PERFORMANCE TRACKING: Count effects line draw call
    IncrementDrawCall("effects_lines");
    
    // Effects lines always use color shader (no textures)
    glUseProgram(m_colorShaderProgram);
    
    // Set uniforms
    int projLoc = glGetUniformLocation(m_colorShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_colorShaderProgram, "uModelView");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    
    // Upload effects line vertex data
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_effectsLineVertexCount * sizeof(Vertex2D), m_effectsLineVertices);
    
    // Draw
    glDrawArrays(GL_LINES, 0, m_effectsLineVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    // Reset vertex count
    m_effectsLineVertexCount = 0;
}

void Renderer::FlushEffectsSprites() {
    if (m_effectsSpriteVertexCount == 0) return;
    
    // PERFORMANCE TRACKING: Count effects sprite draw call
    IncrementDrawCall("effects_sprites");
    
    // Effects sprites always use texture shader
    glUseProgram(m_textureShaderProgram);
    
    // BUGFIX: Texture binding is now handled in EffectsSprite() for immediate consistency
    // The texture should already be bound, but we ensure it's still bound here
    if (m_currentEffectsSpriteTexture != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_currentEffectsSpriteTexture);
    }
    
    // Set uniforms
    int projLoc = glGetUniformLocation(m_textureShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_textureShaderProgram, "uModelView");
    int texLoc = glGetUniformLocation(m_textureShaderProgram, "ourTexture");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    glUniform1i(texLoc, 0); // Use texture unit 0
    
    // Upload effects sprite vertex data
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_effectsSpriteVertexCount * sizeof(Vertex2D), m_effectsSpriteVertices);
    
    // Draw
    glDrawArrays(GL_TRIANGLES, 0, m_effectsSpriteVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    // Reset vertex count
    m_effectsSpriteVertexCount = 0;
}

// COMPLETE BUFFER SYSTEM FLUSH METHODS

void Renderer::FlushAllUnitBuffers() {
    FlushUnitTrails();
    FlushUnitMainSprites();
    FlushUnitHighlights();
    FlushUnitStateIcons();
    FlushUnitCounters();
    FlushUnitNukeIcons();
}

void Renderer::FlushAllEffectBuffers() {
    FlushEffectsLines();
    FlushEffectsSprites();
}

// NEW: Unit trail optimized rendering methods (batched trail system)

void Renderer::UnitTrailLine(float x1, float y1, float x2, float y2, Colour const &col) {
    // Check if we need to flush due to buffer overflow
    FlushUnitTrailsIfFull(2); // Need 2 vertices for one line segment
    
    // Convert color to float
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    // Add line segment to unit trail buffer
    m_unitTrailVertices[m_unitTrailVertexCount].x = x1; 
    m_unitTrailVertices[m_unitTrailVertexCount].y = y1;
    m_unitTrailVertices[m_unitTrailVertexCount].r = r; 
    m_unitTrailVertices[m_unitTrailVertexCount].g = g; 
    m_unitTrailVertices[m_unitTrailVertexCount].b = b; 
    m_unitTrailVertices[m_unitTrailVertexCount].a = a;
    m_unitTrailVertices[m_unitTrailVertexCount].u = 0.0f; 
    m_unitTrailVertices[m_unitTrailVertexCount].v = 0.0f;
    m_unitTrailVertexCount++;
    
    m_unitTrailVertices[m_unitTrailVertexCount].x = x2; 
    m_unitTrailVertices[m_unitTrailVertexCount].y = y2;
    m_unitTrailVertices[m_unitTrailVertexCount].r = r; 
    m_unitTrailVertices[m_unitTrailVertexCount].g = g; 
    m_unitTrailVertices[m_unitTrailVertexCount].b = b; 
    m_unitTrailVertices[m_unitTrailVertexCount].a = a;
    m_unitTrailVertices[m_unitTrailVertexCount].u = 0.0f; 
    m_unitTrailVertices[m_unitTrailVertexCount].v = 0.0f;
    m_unitTrailVertexCount++;
}

void Renderer::BeginUnitTrailBatch() {
    // Disable immediate flushing for trail operations
    // Trail segments will be batched until EndUnitTrailBatch()
    // Note: We don't reset the buffer here in case it has existing segments
}

void Renderer::EndUnitTrailBatch() {
    // Flush all accumulated trail segments in one draw call
    FlushUnitTrails();
}

void Renderer::FlushUnitTrailsIfFull(int segmentsNeeded) {
    // Check if adding the requested segments would overflow the buffer
    if (m_unitTrailVertexCount + segmentsNeeded > MAX_UNIT_TRAIL_VERTICES) {
        FlushUnitTrails();
    }
}

// NEW: Effects line optimized rendering methods (batched effects system)

void Renderer::EffectsLine(float x1, float y1, float x2, float y2, Colour const &col) {
    // Check if we need to flush due to buffer overflow
    FlushEffectsLinesIfFull(2); // Need 2 vertices for one line segment
    
    // Convert color to float
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    // Add line segment to effects line buffer
    m_effectsLineVertices[m_effectsLineVertexCount].x = x1; 
    m_effectsLineVertices[m_effectsLineVertexCount].y = y1;
    m_effectsLineVertices[m_effectsLineVertexCount].r = r; 
    m_effectsLineVertices[m_effectsLineVertexCount].g = g; 
    m_effectsLineVertices[m_effectsLineVertexCount].b = b; 
    m_effectsLineVertices[m_effectsLineVertexCount].a = a;
    m_effectsLineVertices[m_effectsLineVertexCount].u = 0.0f; 
    m_effectsLineVertices[m_effectsLineVertexCount].v = 0.0f;
    m_effectsLineVertexCount++;
    
    m_effectsLineVertices[m_effectsLineVertexCount].x = x2; 
    m_effectsLineVertices[m_effectsLineVertexCount].y = y2;
    m_effectsLineVertices[m_effectsLineVertexCount].r = r; 
    m_effectsLineVertices[m_effectsLineVertexCount].g = g; 
    m_effectsLineVertices[m_effectsLineVertexCount].b = b; 
    m_effectsLineVertices[m_effectsLineVertexCount].a = a;
    m_effectsLineVertices[m_effectsLineVertexCount].u = 0.0f; 
    m_effectsLineVertices[m_effectsLineVertexCount].v = 0.0f;
    m_effectsLineVertexCount++;
    
    // BATCHING FIX: Remove immediate flush to allow proper batching
    // Effects lines will now be batched until EndEffectsLineBatch() is called
    // This mirrors the working unit trail system approach
}

void Renderer::BeginEffectsLineBatch() {
    // Disable immediate flushing for effects line operations
    // Effects line segments will be batched until EndEffectsLineBatch()
    // Note: We don't reset the buffer here in case it has existing segments
}

void Renderer::EndEffectsLineBatch() {
    // Flush all accumulated effects line segments in one draw call
    FlushEffectsLines();
}

void Renderer::FlushEffectsLinesIfFull(int segmentsNeeded) {
    // Check if adding the requested segments would overflow the buffer
    if (m_effectsLineVertexCount + segmentsNeeded > MAX_EFFECTS_LINE_VERTICES) {
        FlushEffectsLines();
    }
}

// NEW: Unit main sprite optimized rendering methods (batched sprite system)

void Renderer::UnitMainSprite(Image *src, float x, float y, float w, float h, Colour const &col) {
    // Check if we need to flush due to buffer overflow (6 vertices per sprite)
    FlushUnitMainSpritesIfFull(6);
    
    // TEXTURE-BASED BATCHING: Smart flush only when texture changes
    // This automatically batches all units with the same texture together
    if (m_unitMainVertexCount > 0 && m_currentUnitMainTexture != src->m_textureID) {
        FlushUnitMainSprites(); // Flush previous texture group
    }
    
    // Bind new texture for this batch
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, src->m_textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    if (src->m_mipmapping) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    
    // Update current texture state for batching
    m_currentUnitMainTexture = src->m_textureID;
    
    // Convert color to float
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    // Calculate texture coordinates
    float onePixelW = 1.0f / (float) src->Width();
    float onePixelH = 1.0f / (float) src->Height();
    
    // First triangle: TL, TR, BR
    m_unitMainVertices[m_unitMainVertexCount].x = x;      
    m_unitMainVertices[m_unitMainVertexCount].y = y;
    m_unitMainVertices[m_unitMainVertexCount].r = r;      
    m_unitMainVertices[m_unitMainVertexCount].g = g;
    m_unitMainVertices[m_unitMainVertexCount].b = b;      
    m_unitMainVertices[m_unitMainVertexCount].a = a;
    m_unitMainVertices[m_unitMainVertexCount].u = onePixelW; 
    m_unitMainVertices[m_unitMainVertexCount].v = 1.0f - onePixelH;
    m_unitMainVertexCount++;
    
    m_unitMainVertices[m_unitMainVertexCount].x = x + w;  
    m_unitMainVertices[m_unitMainVertexCount].y = y;
    m_unitMainVertices[m_unitMainVertexCount].r = r;      
    m_unitMainVertices[m_unitMainVertexCount].g = g;
    m_unitMainVertices[m_unitMainVertexCount].b = b;      
    m_unitMainVertices[m_unitMainVertexCount].a = a;
    m_unitMainVertices[m_unitMainVertexCount].u = 1.0f - onePixelW; 
    m_unitMainVertices[m_unitMainVertexCount].v = 1.0f - onePixelH;
    m_unitMainVertexCount++;
    
    m_unitMainVertices[m_unitMainVertexCount].x = x + w;  
    m_unitMainVertices[m_unitMainVertexCount].y = y + h;
    m_unitMainVertices[m_unitMainVertexCount].r = r;      
    m_unitMainVertices[m_unitMainVertexCount].g = g;
    m_unitMainVertices[m_unitMainVertexCount].b = b;      
    m_unitMainVertices[m_unitMainVertexCount].a = a;
    m_unitMainVertices[m_unitMainVertexCount].u = 1.0f - onePixelW; 
    m_unitMainVertices[m_unitMainVertexCount].v = onePixelH;
    m_unitMainVertexCount++;
    
    // Second triangle: TL, BR, BL
    m_unitMainVertices[m_unitMainVertexCount].x = x;      
    m_unitMainVertices[m_unitMainVertexCount].y = y;
    m_unitMainVertices[m_unitMainVertexCount].r = r;      
    m_unitMainVertices[m_unitMainVertexCount].g = g;
    m_unitMainVertices[m_unitMainVertexCount].b = b;      
    m_unitMainVertices[m_unitMainVertexCount].a = a;
    m_unitMainVertices[m_unitMainVertexCount].u = onePixelW; 
    m_unitMainVertices[m_unitMainVertexCount].v = 1.0f - onePixelH;
    m_unitMainVertexCount++;
    
    m_unitMainVertices[m_unitMainVertexCount].x = x + w;  
    m_unitMainVertices[m_unitMainVertexCount].y = y + h;
    m_unitMainVertices[m_unitMainVertexCount].r = r;      
    m_unitMainVertices[m_unitMainVertexCount].g = g;
    m_unitMainVertices[m_unitMainVertexCount].b = b;      
    m_unitMainVertices[m_unitMainVertexCount].a = a;
    m_unitMainVertices[m_unitMainVertexCount].u = 1.0f - onePixelW; 
    m_unitMainVertices[m_unitMainVertexCount].v = onePixelH;
    m_unitMainVertexCount++;
    
    m_unitMainVertices[m_unitMainVertexCount].x = x;      
    m_unitMainVertices[m_unitMainVertexCount].y = y + h;
    m_unitMainVertices[m_unitMainVertexCount].r = r;      
    m_unitMainVertices[m_unitMainVertexCount].g = g;
    m_unitMainVertices[m_unitMainVertexCount].b = b;      
    m_unitMainVertices[m_unitMainVertexCount].a = a;
    m_unitMainVertices[m_unitMainVertexCount].u = onePixelW; 
    m_unitMainVertices[m_unitMainVertexCount].v = onePixelH;
    m_unitMainVertexCount++;
    
    // BATCHING FIX: Remove immediate flush to enable texture-based batching
    // Units with same texture will now be batched together for massive performance gain
    // FlushUnitMainSprites(); // REMOVED - now batched by texture automatically
}

void Renderer::FlushUnitMainSpritesIfFull(int verticesNeeded) {
    // Check if adding the requested vertices would overflow the buffer
    if (m_unitMainVertexCount + verticesNeeded > MAX_UNIT_MAIN_VERTICES) {
        FlushUnitMainSprites();
    }
}

void Renderer::FlushUnitRotatingIfFull(int verticesNeeded) {
    // Check if adding the requested vertices would overflow the rotating buffer
    if (m_unitRotatingVertexCount + verticesNeeded > MAX_UNIT_ROTATING_VERTICES) {
        FlushUnitRotating();
    }
}

// TEXTURE-BASED BATCHING: Unit main sprite batch management
void Renderer::BeginUnitMainBatch() {
    // Clear any existing vertices from previous batch
    m_unitMainVertexCount = 0;
    m_currentUnitMainTexture = 0;
    
    // Unit main sprites will now be accumulated by texture until EndUnitMainBatch()
    // This enables automatic texture-based batching for massive performance gains
}

void Renderer::EndUnitMainBatch() {
    // Flush any remaining unit main sprites
    if (m_unitMainVertexCount > 0) {
        FlushUnitMainSprites();
    }
}

// ROTATING BATCHING SYSTEM: Separate buffer for rotating sprites (aircraft/nukes)
void Renderer::BeginUnitRotatingBatch() {
    // Clear any existing vertices from previous batch
    m_unitRotatingVertexCount = 0;
    m_currentUnitRotatingTexture = 0;
    
    // Rotating sprites will now be accumulated by texture until EndUnitRotatingBatch()
    // This enables automatic texture-based batching for massive performance gains
}

void Renderer::EndUnitRotatingBatch() {
    // Flush any remaining rotating sprites
    if (m_unitRotatingVertexCount > 0) {
        FlushUnitRotating();
    }
}

// NEW: Unit state icon optimized rendering methods (batched state icon system)

void Renderer::UnitStateIcon(Image *stateSrc, float x, float y, float w, float h, Colour const &col) {
    // Check if we need to flush due to buffer overflow (6 vertices per sprite)
    FlushUnitStateIconsIfFull(6);
    
    // TEXTURE-BASED BATCHING: Smart flush only when texture changes
    // This automatically batches all state icons with the same texture together
    if (m_unitStateVertexCount > 0 && m_currentUnitStateTexture != stateSrc->m_textureID) {
        FlushUnitStateIcons(); // Flush previous texture group
    }
    
    // Bind new texture for this batch
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, stateSrc->m_textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    if (stateSrc->m_mipmapping) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    
    // Update current texture state for batching
    m_currentUnitStateTexture = stateSrc->m_textureID;
    
    // Convert color to float
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    // Calculate texture coordinates
    float onePixelW = 1.0f / (float) stateSrc->Width();
    float onePixelH = 1.0f / (float) stateSrc->Height();
    
    // First triangle: TL, TR, BR
    m_unitStateVertices[m_unitStateVertexCount].x = x;      
    m_unitStateVertices[m_unitStateVertexCount].y = y;
    m_unitStateVertices[m_unitStateVertexCount].r = r;
    m_unitStateVertices[m_unitStateVertexCount].g = g;
    m_unitStateVertices[m_unitStateVertexCount].b = b;      
    m_unitStateVertices[m_unitStateVertexCount].a = a;
    m_unitStateVertices[m_unitStateVertexCount].u = onePixelW; 
    m_unitStateVertices[m_unitStateVertexCount].v = 1.0f - onePixelH;
    m_unitStateVertexCount++;
    
    m_unitStateVertices[m_unitStateVertexCount].x = x + w;  
    m_unitStateVertices[m_unitStateVertexCount].y = y;
    m_unitStateVertices[m_unitStateVertexCount].r = r;      
    m_unitStateVertices[m_unitStateVertexCount].g = g;
    m_unitStateVertices[m_unitStateVertexCount].b = b;      
    m_unitStateVertices[m_unitStateVertexCount].a = a;
    m_unitStateVertices[m_unitStateVertexCount].u = 1.0f - onePixelW; 
    m_unitStateVertices[m_unitStateVertexCount].v = 1.0f - onePixelH;
    m_unitStateVertexCount++;
    
    m_unitStateVertices[m_unitStateVertexCount].x = x + w;  
    m_unitStateVertices[m_unitStateVertexCount].y = y + h;
    m_unitStateVertices[m_unitStateVertexCount].r = r;      
    m_unitStateVertices[m_unitStateVertexCount].g = g;
    m_unitStateVertices[m_unitStateVertexCount].b = b;      
    m_unitStateVertices[m_unitStateVertexCount].a = a;
    m_unitStateVertices[m_unitStateVertexCount].u = 1.0f - onePixelW; 
    m_unitStateVertices[m_unitStateVertexCount].v = onePixelH;
    m_unitStateVertexCount++;
    
    // Second triangle: TL, BR, BL
    m_unitStateVertices[m_unitStateVertexCount].x = x;      
    m_unitStateVertices[m_unitStateVertexCount].y = y;
    m_unitStateVertices[m_unitStateVertexCount].r = r;      
    m_unitStateVertices[m_unitStateVertexCount].g = g;
    m_unitStateVertices[m_unitStateVertexCount].b = b;      
    m_unitStateVertices[m_unitStateVertexCount].a = a;
    m_unitStateVertices[m_unitStateVertexCount].u = onePixelW; 
    m_unitStateVertices[m_unitStateVertexCount].v = 1.0f - onePixelH;
    m_unitStateVertexCount++;
    
    m_unitStateVertices[m_unitStateVertexCount].x = x + w;  
    m_unitStateVertices[m_unitStateVertexCount].y = y + h;
    m_unitStateVertices[m_unitStateVertexCount].r = r;      
    m_unitStateVertices[m_unitStateVertexCount].g = g;
    m_unitStateVertices[m_unitStateVertexCount].b = b;      
    m_unitStateVertices[m_unitStateVertexCount].a = a;
    m_unitStateVertices[m_unitStateVertexCount].u = 1.0f - onePixelW; 
    m_unitStateVertices[m_unitStateVertexCount].v = onePixelH;
    m_unitStateVertexCount++;
    
    m_unitStateVertices[m_unitStateVertexCount].x = x;      
    m_unitStateVertices[m_unitStateVertexCount].y = y + h;
    m_unitStateVertices[m_unitStateVertexCount].r = r;      
    m_unitStateVertices[m_unitStateVertexCount].g = g;
    m_unitStateVertices[m_unitStateVertexCount].b = b;      
    m_unitStateVertices[m_unitStateVertexCount].a = a;
    m_unitStateVertices[m_unitStateVertexCount].u = onePixelW; 
    m_unitStateVertices[m_unitStateVertexCount].v = onePixelH;
    m_unitStateVertexCount++;
    
    // BATCHING FIX: Remove immediate flush to enable texture-based batching
    // State icons with same texture will now be batched together for massive performance gain
}

void Renderer::FlushUnitStateIconsIfFull(int verticesNeeded) {
    // Check if adding the requested vertices would overflow the buffer
    if (m_unitStateVertexCount + verticesNeeded > MAX_UNIT_STATE_VERTICES) {
        FlushUnitStateIcons();
    }
}

// TEXTURE-BASED BATCHING: Unit state icon batch management
void Renderer::BeginUnitStateBatch() {
    // Clear any existing vertices from previous batch
    m_unitStateVertexCount = 0;
    m_currentUnitStateTexture = 0;
    
    // Unit state icons will now be accumulated by texture until EndUnitStateBatch()
    // This enables automatic texture-based batching for massive performance gains
}

void Renderer::EndUnitStateBatch() {
    // Flush any remaining unit state icons
    if (m_unitStateVertexCount > 0) {
        FlushUnitStateIcons();
    }
}

// NEW: Unit nuke icon optimized rendering methods (batched nuke icon system)

void Renderer::UnitNukeIcon(float x, float y, float w, float h, Colour const &col) {
    // Check if we need to flush due to buffer overflow (6 vertices per sprite)
    FlushUnitNukeIconsIfFull(6);
    
    // Nuke icons use a fixed texture (smallnuke.bmp)
    // Load the default nuke icon texture
    Image *nukeBmp = g_resource->GetImage("graphics/smallnuke.bmp");
    if (!nukeBmp) return; // No nuke texture available
    
    // TEXTURE-BASED BATCHING: Smart flush only when texture changes
    if (m_unitNukeVertexCount > 0 && m_currentUnitNukeTexture != nukeBmp->m_textureID) {
        FlushUnitNukeIcons(); // Flush previous texture group
    }
    
    // Bind nuke texture for this batch
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, nukeBmp->m_textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    if (nukeBmp->m_mipmapping) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    
    // Update current texture state for batching
    m_currentUnitNukeTexture = nukeBmp->m_textureID;
    
    // Convert color to float
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    // Calculate texture coordinates
    float onePixelW = 1.0f / (float) nukeBmp->Width();
    float onePixelH = 1.0f / (float) nukeBmp->Height();
    
    // First triangle: TL, TR, BR
    m_unitNukeVertices[m_unitNukeVertexCount].x = x;      
    m_unitNukeVertices[m_unitNukeVertexCount].y = y;
    m_unitNukeVertices[m_unitNukeVertexCount].r = r;
    m_unitNukeVertices[m_unitNukeVertexCount].g = g;
    m_unitNukeVertices[m_unitNukeVertexCount].b = b;      
    m_unitNukeVertices[m_unitNukeVertexCount].a = a;
    m_unitNukeVertices[m_unitNukeVertexCount].u = onePixelW; 
    m_unitNukeVertices[m_unitNukeVertexCount].v = 1.0f - onePixelH;
    m_unitNukeVertexCount++;
    
    m_unitNukeVertices[m_unitNukeVertexCount].x = x + w;  
    m_unitNukeVertices[m_unitNukeVertexCount].y = y;
    m_unitNukeVertices[m_unitNukeVertexCount].r = r;      
    m_unitNukeVertices[m_unitNukeVertexCount].g = g;
    m_unitNukeVertices[m_unitNukeVertexCount].b = b;      
    m_unitNukeVertices[m_unitNukeVertexCount].a = a;
    m_unitNukeVertices[m_unitNukeVertexCount].u = 1.0f - onePixelW; 
    m_unitNukeVertices[m_unitNukeVertexCount].v = 1.0f - onePixelH;
    m_unitNukeVertexCount++;
    
    m_unitNukeVertices[m_unitNukeVertexCount].x = x + w;  
    m_unitNukeVertices[m_unitNukeVertexCount].y = y + h;
    m_unitNukeVertices[m_unitNukeVertexCount].r = r;      
    m_unitNukeVertices[m_unitNukeVertexCount].g = g;
    m_unitNukeVertices[m_unitNukeVertexCount].b = b;      
    m_unitNukeVertices[m_unitNukeVertexCount].a = a;
    m_unitNukeVertices[m_unitNukeVertexCount].u = 1.0f - onePixelW; 
    m_unitNukeVertices[m_unitNukeVertexCount].v = onePixelH;
    m_unitNukeVertexCount++;
    
    // Second triangle: TL, BR, BL
    m_unitNukeVertices[m_unitNukeVertexCount].x = x;      
    m_unitNukeVertices[m_unitNukeVertexCount].y = y;
    m_unitNukeVertices[m_unitNukeVertexCount].r = r;      
    m_unitNukeVertices[m_unitNukeVertexCount].g = g;
    m_unitNukeVertices[m_unitNukeVertexCount].b = b;      
    m_unitNukeVertices[m_unitNukeVertexCount].a = a;
    m_unitNukeVertices[m_unitNukeVertexCount].u = onePixelW; 
    m_unitNukeVertices[m_unitNukeVertexCount].v = 1.0f - onePixelH;
    m_unitNukeVertexCount++;
    
    m_unitNukeVertices[m_unitNukeVertexCount].x = x + w;  
    m_unitNukeVertices[m_unitNukeVertexCount].y = y + h;
    m_unitNukeVertices[m_unitNukeVertexCount].r = r;      
    m_unitNukeVertices[m_unitNukeVertexCount].g = g;
    m_unitNukeVertices[m_unitNukeVertexCount].b = b;      
    m_unitNukeVertices[m_unitNukeVertexCount].a = a;
    m_unitNukeVertices[m_unitNukeVertexCount].u = 1.0f - onePixelW; 
    m_unitNukeVertices[m_unitNukeVertexCount].v = onePixelH;
    m_unitNukeVertexCount++;
    
    m_unitNukeVertices[m_unitNukeVertexCount].x = x;      
    m_unitNukeVertices[m_unitNukeVertexCount].y = y + h;
    m_unitNukeVertices[m_unitNukeVertexCount].r = r;      
    m_unitNukeVertices[m_unitNukeVertexCount].g = g;
    m_unitNukeVertices[m_unitNukeVertexCount].b = b;      
    m_unitNukeVertices[m_unitNukeVertexCount].a = a;
    m_unitNukeVertices[m_unitNukeVertexCount].u = onePixelW; 
    m_unitNukeVertices[m_unitNukeVertexCount].v = onePixelH;
    m_unitNukeVertexCount++;
    
    // BATCHING FIX: Remove immediate flush to enable texture-based batching
    // Nuke icons will now be batched together for massive performance gain
}

void Renderer::FlushUnitNukeIconsIfFull(int verticesNeeded) {
    // Check if adding the requested vertices would overflow the buffer
    if (m_unitNukeVertexCount + verticesNeeded > MAX_UNIT_NUKE_VERTICES) {
        FlushUnitNukeIcons();
    }
}

// TEXTURE-BASED BATCHING: Unit nuke icon batch management
void Renderer::BeginUnitNukeBatch() {
    // Clear any existing vertices from previous batch
    m_unitNukeVertexCount = 0;
    m_currentUnitNukeTexture = 0;
    
    // Unit nuke icons will now be accumulated by texture until EndUnitNukeBatch()
    // This enables automatic texture-based batching for massive performance gains
}

void Renderer::EndUnitNukeBatch() {
    // Flush any remaining unit nuke icons
    if (m_unitNukeVertexCount > 0) {
        FlushUnitNukeIcons();
    }
}

// ROTATING RENDERING: Dedicated method for rotating sprites (aircraft/nukes) with rotation (separate buffer)
void Renderer::UnitRotating(Image *src, float x, float y, float w, float h, Colour const &col, float angle) {
    // Check if we need to flush due to buffer overflow (6 vertices per sprite)
    FlushUnitRotatingIfFull(6);
    
    // TEXTURE-BASED BATCHING: Smart flush only when texture changes
    // This automatically batches all rotating sprites with the same texture together
    if (m_unitRotatingVertexCount > 0 && m_currentUnitRotatingTexture != src->m_textureID) {
        FlushUnitRotating(); // Flush previous texture group
    }
    
    // Bind new texture for this batch
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, src->m_textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    if (src->m_mipmapping) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    
    // Update current texture state for batching
    m_currentUnitRotatingTexture = src->m_textureID;
    
    // ROTATION MATH: Calculate rotated vertices (same as original Blit with angle)
    Vector3<float> vert1( -w, +h, 0 );
    Vector3<float> vert2( +w, +h, 0 );
    Vector3<float> vert3( +w, -h, 0 );
    Vector3<float> vert4( -w, -h, 0 );

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
    
    // Calculate texture coordinates
    float onePixelW = 1.0f / (float) src->Width();
    float onePixelH = 1.0f / (float) src->Height();
    
    // ROTATED QUAD: Create two triangles for the rotated sprite
    // First triangle: vert1, vert2, vert3
    m_unitRotatingVertices[m_unitRotatingVertexCount].x = vert1.x;      
    m_unitRotatingVertices[m_unitRotatingVertexCount].y = vert1.y;
    m_unitRotatingVertices[m_unitRotatingVertexCount].r = r;      
    m_unitRotatingVertices[m_unitRotatingVertexCount].g = g;
    m_unitRotatingVertices[m_unitRotatingVertexCount].b = b;      
    m_unitRotatingVertices[m_unitRotatingVertexCount].a = a;
    m_unitRotatingVertices[m_unitRotatingVertexCount].u = onePixelW; 
    m_unitRotatingVertices[m_unitRotatingVertexCount].v = 1.0f - onePixelH;
    m_unitRotatingVertexCount++;
    
    m_unitRotatingVertices[m_unitRotatingVertexCount].x = vert2.x;      
    m_unitRotatingVertices[m_unitRotatingVertexCount].y = vert2.y;
    m_unitRotatingVertices[m_unitRotatingVertexCount].r = r;      
    m_unitRotatingVertices[m_unitRotatingVertexCount].g = g;
    m_unitRotatingVertices[m_unitRotatingVertexCount].b = b;      
    m_unitRotatingVertices[m_unitRotatingVertexCount].a = a;
    m_unitRotatingVertices[m_unitRotatingVertexCount].u = 1.0f - onePixelW; 
    m_unitRotatingVertices[m_unitRotatingVertexCount].v = 1.0f - onePixelH;
    m_unitRotatingVertexCount++;
    
    m_unitRotatingVertices[m_unitRotatingVertexCount].x = vert3.x;      
    m_unitRotatingVertices[m_unitRotatingVertexCount].y = vert3.y;
    m_unitRotatingVertices[m_unitRotatingVertexCount].r = r;      
    m_unitRotatingVertices[m_unitRotatingVertexCount].g = g;
    m_unitRotatingVertices[m_unitRotatingVertexCount].b = b;      
    m_unitRotatingVertices[m_unitRotatingVertexCount].a = a;
    m_unitRotatingVertices[m_unitRotatingVertexCount].u = 1.0f - onePixelW; 
    m_unitRotatingVertices[m_unitRotatingVertexCount].v = onePixelH;
    m_unitRotatingVertexCount++;
    
    // Second triangle: vert1, vert3, vert4
    m_unitRotatingVertices[m_unitRotatingVertexCount].x = vert1.x;      
    m_unitRotatingVertices[m_unitRotatingVertexCount].y = vert1.y;
    m_unitRotatingVertices[m_unitRotatingVertexCount].r = r;      
    m_unitRotatingVertices[m_unitRotatingVertexCount].g = g;
    m_unitRotatingVertices[m_unitRotatingVertexCount].b = b;      
    m_unitRotatingVertices[m_unitRotatingVertexCount].a = a;
    m_unitRotatingVertices[m_unitRotatingVertexCount].u = onePixelW; 
    m_unitRotatingVertices[m_unitRotatingVertexCount].v = 1.0f - onePixelH;
    m_unitRotatingVertexCount++;
    
    m_unitRotatingVertices[m_unitRotatingVertexCount].x = vert3.x;      
    m_unitRotatingVertices[m_unitRotatingVertexCount].y = vert3.y;
    m_unitRotatingVertices[m_unitRotatingVertexCount].r = r;      
    m_unitRotatingVertices[m_unitRotatingVertexCount].g = g;
    m_unitRotatingVertices[m_unitRotatingVertexCount].b = b;      
    m_unitRotatingVertices[m_unitRotatingVertexCount].a = a;
    m_unitRotatingVertices[m_unitRotatingVertexCount].u = 1.0f - onePixelW; 
    m_unitRotatingVertices[m_unitRotatingVertexCount].v = onePixelH;
    m_unitRotatingVertexCount++;
    
    m_unitRotatingVertices[m_unitRotatingVertexCount].x = vert4.x;      
    m_unitRotatingVertices[m_unitRotatingVertexCount].y = vert4.y;
    m_unitRotatingVertices[m_unitRotatingVertexCount].r = r;      
    m_unitRotatingVertices[m_unitRotatingVertexCount].g = g;
    m_unitRotatingVertices[m_unitRotatingVertexCount].b = b;      
    m_unitRotatingVertices[m_unitRotatingVertexCount].a = a;
    m_unitRotatingVertices[m_unitRotatingVertexCount].u = onePixelW; 
    m_unitRotatingVertices[m_unitRotatingVertexCount].v = onePixelH;
    m_unitRotatingVertexCount++;
    
    // BATCHING FIX: Remove immediate flush to enable texture-based batching
    // Rotating sprites with same texture will now be batched together in separate buffer
}

// NEW: Rotated unit nuke icon rendering (for bomber nuke symbols that need rotation)
void Renderer::UnitNukeIcon(float x, float y, float w, float h, Colour const &col, float angle) {
    // Check if we need to flush due to buffer overflow (6 vertices per sprite)
    FlushUnitNukeIconsIfFull(6);
    
    // Nuke icons use a fixed texture (smallnuke.bmp)
    // Load the default nuke icon texture
    Image *nukeBmp = g_resource->GetImage("graphics/smallnuke.bmp");
    if (!nukeBmp) return; // No nuke texture available
    
    // TEXTURE-BASED BATCHING: Smart flush only when texture changes
    if (m_unitNukeVertexCount > 0 && m_currentUnitNukeTexture != nukeBmp->m_textureID) {
        FlushUnitNukeIcons(); // Flush previous texture group
    }
    
    // Bind nuke texture for this batch
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, nukeBmp->m_textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    if (nukeBmp->m_mipmapping) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    
    // Update current texture state for batching
    m_currentUnitNukeTexture = nukeBmp->m_textureID;
    
    // ROTATION MATH: Calculate rotated vertices (same as original Blit with angle)
    Vector3<float> vert1( -w, +h, 0 );
    Vector3<float> vert2( +w, +h, 0 );
    Vector3<float> vert3( +w, -h, 0 );
    Vector3<float> vert4( -w, -h, 0 );

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
    
    // Calculate texture coordinates
    float onePixelW = 1.0f / (float) nukeBmp->Width();
    float onePixelH = 1.0f / (float) nukeBmp->Height();
    
    // ROTATED QUAD: Create two triangles for the rotated nuke symbol
    // First triangle: vert1, vert2, vert3
    m_unitNukeVertices[m_unitNukeVertexCount].x = vert1.x;      
    m_unitNukeVertices[m_unitNukeVertexCount].y = vert1.y;
    m_unitNukeVertices[m_unitNukeVertexCount].r = r;      
    m_unitNukeVertices[m_unitNukeVertexCount].g = g;
    m_unitNukeVertices[m_unitNukeVertexCount].b = b;      
    m_unitNukeVertices[m_unitNukeVertexCount].a = a;
    m_unitNukeVertices[m_unitNukeVertexCount].u = onePixelW; 
    m_unitNukeVertices[m_unitNukeVertexCount].v = 1.0f - onePixelH;
    m_unitNukeVertexCount++;
    
    m_unitNukeVertices[m_unitNukeVertexCount].x = vert2.x;      
    m_unitNukeVertices[m_unitNukeVertexCount].y = vert2.y;
    m_unitNukeVertices[m_unitNukeVertexCount].r = r;      
    m_unitNukeVertices[m_unitNukeVertexCount].g = g;
    m_unitNukeVertices[m_unitNukeVertexCount].b = b;      
    m_unitNukeVertices[m_unitNukeVertexCount].a = a;
    m_unitNukeVertices[m_unitNukeVertexCount].u = 1.0f - onePixelW; 
    m_unitNukeVertices[m_unitNukeVertexCount].v = 1.0f - onePixelH;
    m_unitNukeVertexCount++;
    
    m_unitNukeVertices[m_unitNukeVertexCount].x = vert3.x;      
    m_unitNukeVertices[m_unitNukeVertexCount].y = vert3.y;
    m_unitNukeVertices[m_unitNukeVertexCount].r = r;      
    m_unitNukeVertices[m_unitNukeVertexCount].g = g;
    m_unitNukeVertices[m_unitNukeVertexCount].b = b;      
    m_unitNukeVertices[m_unitNukeVertexCount].a = a;
    m_unitNukeVertices[m_unitNukeVertexCount].u = 1.0f - onePixelW; 
    m_unitNukeVertices[m_unitNukeVertexCount].v = onePixelH;
    m_unitNukeVertexCount++;
    
    // Second triangle: vert1, vert3, vert4
    m_unitNukeVertices[m_unitNukeVertexCount].x = vert1.x;      
    m_unitNukeVertices[m_unitNukeVertexCount].y = vert1.y;
    m_unitNukeVertices[m_unitNukeVertexCount].r = r;      
    m_unitNukeVertices[m_unitNukeVertexCount].g = g;
    m_unitNukeVertices[m_unitNukeVertexCount].b = b;      
    m_unitNukeVertices[m_unitNukeVertexCount].a = a;
    m_unitNukeVertices[m_unitNukeVertexCount].u = onePixelW; 
    m_unitNukeVertices[m_unitNukeVertexCount].v = 1.0f - onePixelH;
    m_unitNukeVertexCount++;
    
    m_unitNukeVertices[m_unitNukeVertexCount].x = vert3.x;      
    m_unitNukeVertices[m_unitNukeVertexCount].y = vert3.y;
    m_unitNukeVertices[m_unitNukeVertexCount].r = r;      
    m_unitNukeVertices[m_unitNukeVertexCount].g = g;
    m_unitNukeVertices[m_unitNukeVertexCount].b = b;      
    m_unitNukeVertices[m_unitNukeVertexCount].a = a;
    m_unitNukeVertices[m_unitNukeVertexCount].u = 1.0f - onePixelW; 
    m_unitNukeVertices[m_unitNukeVertexCount].v = onePixelH;
    m_unitNukeVertexCount++;
    
    m_unitNukeVertices[m_unitNukeVertexCount].x = vert4.x;      
    m_unitNukeVertices[m_unitNukeVertexCount].y = vert4.y;
    m_unitNukeVertices[m_unitNukeVertexCount].r = r;      
    m_unitNukeVertices[m_unitNukeVertexCount].g = g;
    m_unitNukeVertices[m_unitNukeVertexCount].b = b;      
    m_unitNukeVertices[m_unitNukeVertexCount].a = a;
    m_unitNukeVertices[m_unitNukeVertexCount].u = onePixelW; 
    m_unitNukeVertices[m_unitNukeVertexCount].v = onePixelH;
    m_unitNukeVertexCount++;
    
    // BATCHING FIX: Remove immediate flush to enable texture-based batching
    // Rotated nuke symbols with same texture will now be batched together
}

void Renderer::FlushUnitRotating() {
    if (m_unitRotatingVertexCount == 0) return;
    
    // PERFORMANCE TRACKING: Count rotating sprite draw call
    IncrementDrawCall("unit_rotating");
    
    // Rotating sprites always use texture shader
    glUseProgram(m_textureShaderProgram);
    
    // BUGFIX: Texture binding is now handled in UnitRotating() for immediate consistency
    // The texture should already be bound, but we ensure it's still bound here
    if (m_currentUnitRotatingTexture != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_currentUnitRotatingTexture);
    }
    
    // Set uniforms
    int projLoc = glGetUniformLocation(m_textureShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_textureShaderProgram, "uModelView");
    int texLoc = glGetUniformLocation(m_textureShaderProgram, "ourTexture");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    glUniform1i(texLoc, 0); // Use texture unit 0
    
    // Upload rotating sprite vertex data
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_unitRotatingVertexCount * sizeof(Vertex2D), m_unitRotatingVertices);
    
    // Draw
    glDrawArrays(GL_TRIANGLES, 0, m_unitRotatingVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    // Reset vertex count
    m_unitRotatingVertexCount = 0;
}

// EFFECTS SPRITE BATCHING SYSTEM: For explosions, particles, and other effects

void Renderer::BeginEffectsSpriteBatch() {
    // Clear any existing vertices from previous batch
    m_effectsSpriteVertexCount = 0;
    m_currentEffectsSpriteTexture = 0;
    
    // Effects sprites will now be accumulated by texture until EndEffectsSpriteBatch()
    // This enables automatic texture-based batching for massive performance gains
}

void Renderer::EndEffectsSpriteBatch() {
    // Flush any remaining effects sprites
    if (m_effectsSpriteVertexCount > 0) {
        FlushEffectsSprites();
    }
}

void Renderer::EffectsSprite(Image *src, float x, float y, float w, float h, Colour const &col) {
    // Check if we need to flush due to buffer overflow (6 vertices per sprite)
    if (m_effectsSpriteVertexCount + 6 > MAX_EFFECTS_SPRITE_VERTICES) {
        FlushEffectsSprites();
    }
    
    // TEXTURE-BASED BATCHING: Smart flush only when texture changes
    // This automatically batches all effects sprites with the same texture together
    if (m_effectsSpriteVertexCount > 0 && m_currentEffectsSpriteTexture != src->m_textureID) {
        FlushEffectsSprites(); // Flush previous texture group
    }
    
    // Bind new texture for this batch
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, src->m_textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    if (src->m_mipmapping) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    
    // Update current texture state for batching
    m_currentEffectsSpriteTexture = src->m_textureID;
    
    // Convert color to float
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    // Calculate texture coordinates
    float onePixelW = 1.0f / (float) src->Width();
    float onePixelH = 1.0f / (float) src->Height();
    
    // First triangle: TL, TR, BR
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].x = x;      
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].y = y;
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].r = r;
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].g = g;
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].b = b;      
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].a = a;
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].u = onePixelW; 
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].v = 1.0f - onePixelH;
    m_effectsSpriteVertexCount++;
    
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].x = x + w;  
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].y = y;
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].r = r;      
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].g = g;
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].b = b;      
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].a = a;
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].u = 1.0f - onePixelW; 
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].v = 1.0f - onePixelH;
    m_effectsSpriteVertexCount++;
    
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].x = x + w;  
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].y = y + h;
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].r = r;      
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].g = g;
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].b = b;      
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].a = a;
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].u = 1.0f - onePixelW; 
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].v = onePixelH;
    m_effectsSpriteVertexCount++;
    
    // Second triangle: TL, BR, BL
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].x = x;      
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].y = y;
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].r = r;      
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].g = g;
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].b = b;      
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].a = a;
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].u = onePixelW; 
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].v = 1.0f - onePixelH;
    m_effectsSpriteVertexCount++;
    
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].x = x + w;  
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].y = y + h;
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].r = r;      
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].g = g;
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].b = b;      
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].a = a;
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].u = 1.0f - onePixelW; 
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].v = onePixelH;
    m_effectsSpriteVertexCount++;
    
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].x = x;      
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].y = y + h;
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].r = r;      
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].g = g;
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].b = b;      
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].a = a;
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].u = onePixelW; 
    m_effectsSpriteVertices[m_effectsSpriteVertexCount].v = onePixelH;
    m_effectsSpriteVertexCount++;
    
    // BATCHING FIX: Remove immediate flush to enable texture-based batching
    // Effects sprites with same texture will now be batched together for massive performance gain
}

