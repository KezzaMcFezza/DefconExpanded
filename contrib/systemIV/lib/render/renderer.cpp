#include "systemiv.h"

#include "lib/debug/debug_utils.h"
#include "lib/resource/image.h"
#include "lib/resource/sprite_atlas.h"
#include "lib/resource/bitmapfont.h"
#include "lib/resource/resource.h"
#include "lib/language_table.h"
#include "lib/string_utils.h"
#include "lib/render2d/renderer_2d.h"
#include "lib/render3d/renderer_3d.h"
#include "lib/render2d/megavbo/megavbo_2d.h"
#include "lib/render3d/megavbo/megavbo_3d.h"
#include "lib/render/antialiasing/anti_aliasing.h"

#include "renderer.h"
#include "renderer_opengl.h"
#include "renderer_d3d11.h"

Renderer *g_renderer = NULL;

// ============================================================================
// CONSTRUCTOR & DESTRUCTOR
// ============================================================================

Renderer::Renderer()
	: m_defaultFontName( NULL ),
	  m_defaultFontFilename( NULL ),
	  m_defaultFontLanguageSpecific( false ),
	  m_currentFontName( NULL ),
	  m_currentFontFilename( NULL ),
	  m_currentFontLanguageSpecific( false ),
	  m_horizFlip( false ),
	  m_fixedWidth( false ),
	  m_negative( false ),
	  m_textureSwitches( 0 ),
	  m_prevTextureSwitches( 0 ),
	  m_flushTimingCount( 0 ),
	  m_currentFlushStartTime( 0.0 ),
	  m_antiAliasing( nullptr ),
	  m_currentPass( RENDER_PASS_NONE ),
	  m_clearColorR( 0.0f ),
	  m_clearColorG( 0.0f ),
	  m_clearColorB( 0.0f ),
	  m_clearColorA( 1.0f ),
	  m_blendMode( BlendModeNormal ),
	  m_blendSrcFactor( 0 ),
	  m_blendDstFactor( 0 )
{
	//
	// Set global pointer immediately so subrenderers can use g_renderer
	// app.cpp will also set it, but this ensures its available during construction

	g_renderer = this;

	//
	// Initialize flush timings

	for ( int i = 0; i < MAX_FLUSH_TYPES; i++ )
	{
		m_flushTimings[i].name = NULL;
		m_flushTimings[i].totalTime = 0.0;
		m_flushTimings[i].totalGpuTime = 0.0;
		m_flushTimings[i].callCount = 0;
		m_flushTimings[i].queryObject = 0;
		m_flushTimings[i].queryPending = false;
	}
}


Renderer::~Renderer()
{
	if ( m_currentFontName )
		delete[] m_currentFontName;
	if ( m_currentFontFilename )
		delete[] m_currentFontFilename;
	if ( m_defaultFontName )
		delete[] m_defaultFontName;
	if ( m_defaultFontFilename )
		delete[] m_defaultFontFilename;

	if ( g_megavbo3d )
	{
		delete g_megavbo3d;
		g_megavbo3d = NULL;
	}

	if ( g_renderer3d )
	{
		delete g_renderer3d;
		g_renderer3d = NULL;
	}

	if ( g_megavbo2d )
	{
		delete g_megavbo2d;
		g_megavbo2d = NULL;
	}

	if ( g_renderer2d )
	{
		delete g_renderer2d;
		g_renderer2d = NULL;
	}

	if ( m_antiAliasing )
	{
		m_antiAliasing->Destroy();
		delete m_antiAliasing;
		m_antiAliasing = nullptr;
	}
}


// ============================================================================
// FACTORY METHOD
// ============================================================================

Renderer *Renderer::Create()
{
	RendererType type = GetRendererType();

	switch ( type )
	{
#ifdef RENDERER_OPENGL
		case RENDERER_TYPE_OPENGL:
			return new RendererOpenGL();
#endif
#ifdef RENDERER_DIRECTX11
		case RENDERER_TYPE_DIRECTX11:
			return new RendererD3D11();
#endif
		default:
			AppReleaseAssert( false, "Unknown renderer type or no renderer backend compiled" );
			return nullptr;
	}
}


// ============================================================================
// ATLAS COORDINATES
// ============================================================================

void Renderer::GetImageUVCoords( Image *image, float &u1, float &v1, float &u2, float &v2 )
{
	AtlasImage *atlasImage = dynamic_cast<AtlasImage *>( image );
	if ( atlasImage )
	{

		//
		// Use atlas coordinates

		const AtlasCoord *coord = atlasImage->GetAtlasCoord();
		if ( coord )
		{
			u1 = coord->u1;
			v1 = coord->v1;
			u2 = coord->u2;
			v2 = coord->v2;
		}
		else
		{
			return;
		}
	}
	else
	{
		//
		// Regular image, use full texture with edge padding

		float onePixelW = 1.0f / (float)image->Width();
		float onePixelH = 1.0f / (float)image->Height();
		u1 = onePixelW;
		v1 = onePixelH;
		u2 = 1.0f - onePixelW;
		v2 = 1.0f - onePixelH;
	}

	//
	// Apply UV adjustments if set
	//
	// Positive values shrink  (reduce UV range)
	// Negative values stretch (increase UV range)

	if ( image->m_uvAdjustX != 0.0f )
	{
		float shrinkX = ( u2 - u1 ) * image->m_uvAdjustX;
		u1 += shrinkX;
		u2 -= shrinkX;
	}

	if ( image->m_uvAdjustY != 0.0f )
	{
		float shrinkY = ( v2 - v1 ) * image->m_uvAdjustY;
		v1 += shrinkY;
		v2 -= shrinkY;
	}
}


unsigned int Renderer::GetEffectiveTextureID( Image *image )
{
	AtlasImage *atlasImage = dynamic_cast<AtlasImage *>( image );
	if ( atlasImage )
	{
		return atlasImage->GetAtlasTextureID();
	}
	return image->m_textureID;
}


const Renderer::FlushTiming *Renderer::GetFlushTimings( int &count ) const
{
	count = m_flushTimingCount;
	return m_flushTimings;
}


BitmapFont *Renderer::GetBitmapFont()
{
	BitmapFont *font = g_resource->GetBitmapFont( m_currentFontFilename );
	if ( font )
	{
		font->SetSpacing( GetFontSpacing( m_currentFontName ) );
	}
	else
	{
		font = g_resource->GetBitmapFont( m_defaultFontFilename );
		if ( font )
		{
			font->SetSpacing( GetFontSpacing( m_defaultFontName ) );
		}
	}

	if ( !font )
		return NULL;

	font->SetHoriztonalFlip( m_horizFlip );
	font->SetFixedWidth( m_fixedWidth );
	return font;
}


char *Renderer::ScreenshotsDirectory()
{
#ifdef TARGET_OS_MACOSX
	if ( getenv( "HOME" ) )
		return ConcatPaths( getenv( "HOME" ), "Pictures", NULL );
#endif
	return newStr( "." );
}


// ============================================================================
// FONT MANAGEMENT
// ============================================================================

static const char *GetFontFilename( const char *_fontName, const char *_langName, bool *_fontLanguageSpecific )
{
	static char result[512];

	if ( _langName || g_languageTable->m_lang )
	{
		*_fontLanguageSpecific = true;
		if ( _langName )
		{
			snprintf( result, sizeof( result ), "fonts/%s_%s.bmp", _fontName, _langName );
		}
		else
		{
			snprintf( result, sizeof( result ), "fonts/%s_%s.bmp", _fontName, g_languageTable->m_lang->m_name );
		}
		result[sizeof( result ) - 1] = '\x0';

		if ( !g_resource->TestBitmapFont( result ) )
		{
			*_fontLanguageSpecific = false;
			snprintf( result, sizeof( result ), "fonts/%s.bmp", _fontName );
			result[sizeof( result ) - 1] = '\x0';
		}
	}
	else
	{
		*_fontLanguageSpecific = false;
		snprintf( result, sizeof( result ), "fonts/%s.bmp", _fontName );
		result[sizeof( result ) - 1] = '\x0';
	}

	return result;
}


void Renderer::SetDefaultFont( const char *font, const char *_langName )
{
	if ( m_currentFontName )
	{
		delete[] m_currentFontName;
		m_currentFontName = NULL;
	}

	if ( m_currentFontFilename )
	{
		delete[] m_currentFontFilename;
		m_currentFontFilename = NULL;
	}

	m_currentFontLanguageSpecific = false;

	if ( m_defaultFontName )
		delete[] m_defaultFontName;
	if ( m_defaultFontFilename )
		delete[] m_defaultFontFilename;

	const char *fullFilename = GetFontFilename( font, _langName, &m_defaultFontLanguageSpecific );
	m_defaultFontName = newStr( font );
	m_defaultFontFilename = newStr( fullFilename );
}


void Renderer::SetFontSpacing( const char *font, float _spacing )
{
	float *p = m_fontSpacings.GetPointer( font );
	if ( p )
	{
		*p = _spacing;
	}
	else
	{
		m_fontSpacings.PutData( font, _spacing );
	}
}


float Renderer::GetFontSpacing( const char *font )
{
	return m_fontSpacings.GetData( font, BitmapFont::DEFAULT_SPACING );
}


void Renderer::SetFont( const char *font, bool horizFlip, bool negative, bool fixedWidth, const char *_langName )
{
	if ( font || !_langName )
	{
		if ( m_currentFontName )
		{
			delete[] m_currentFontName;
			m_currentFontName = NULL;
		}
	}

	if ( m_currentFontFilename )
	{
		delete[] m_currentFontFilename;
		m_currentFontFilename = NULL;
	}
	m_currentFontLanguageSpecific = false;

	if ( font )
	{
		m_currentFontName = newStr( font );
		const char *fontFilename = GetFontFilename( font, _langName, &m_currentFontLanguageSpecific );
		m_currentFontFilename = newStr( fontFilename );
	}
	else if ( _langName )
	{
		if ( m_currentFontName )
		{
			const char *fontFilename = GetFontFilename( m_currentFontName, _langName, &m_currentFontLanguageSpecific );
			m_currentFontFilename = newStr( fontFilename );
		}
		else if ( m_defaultFontName )
		{
			m_currentFontName = newStr( m_defaultFontName );
			const char *fontFilename = GetFontFilename( m_defaultFontName, _langName, &m_currentFontLanguageSpecific );
			m_currentFontFilename = newStr( fontFilename );
		}
	}

	m_horizFlip = horizFlip;
	m_fixedWidth = fixedWidth;
	m_negative = negative;

	if ( m_negative )
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
	if ( font )
	{
		return m_currentFontLanguageSpecific;
	}
	else
	{
		font = g_resource->GetBitmapFont( m_defaultFontFilename );
		if ( font )
		{
			return m_defaultFontLanguageSpecific;
		}
	}
	return false;
}


bool Renderer::IsAntiAliasingEnabled() const
{
	if ( m_antiAliasing )
	{
		return m_antiAliasing->IsEnabled();
	}
	return false;
}


int Renderer::GetAntiAliasingSamples() const
{
	if ( m_antiAliasing )
	{
		return m_antiAliasing->GetSamples();
	}
	return 0;
}
