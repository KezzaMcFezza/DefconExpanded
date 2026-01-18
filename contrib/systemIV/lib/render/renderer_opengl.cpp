#include "systemiv.h"

#ifdef RENDERER_OPENGL
#include <string.h>
#include <time.h>

#include "lib/debug/debug_utils.h"
#include "lib/hi_res_time.h"
#include "lib/resource/image.h"
#include "lib/resource/sprite_atlas.h"
#include "lib/resource/bitmap.h"
#include "lib/resource/bitmapfont.h"
#include "lib/resource/resource.h"
#include "lib/language_table.h"
#include "lib/gucci/window_manager.h"
#include "lib/string_utils.h"
#include "lib/filesys/filesys_utils.h"
#include "lib/render/colour.h"
#include "lib/resource/bitmap.h"
#include "lib/render2d/renderer_2d.h"
#include "lib/render2d/renderer_2d_opengl.h"
#include "lib/render3d/renderer_3d.h"
#include "lib/render3d/renderer_3d_opengl.h"
#include "lib/render2d/megavbo/megavbo_2d.h"
#include "lib/render3d/megavbo/megavbo_3d.h"

#include "renderer.h"
#include "renderer_opengl.h"

// ============================================================================
// CONSTRUCTOR & DESTRUCTOR
// ============================================================================

RendererOpenGL::RendererOpenGL()
	: m_currentViewportX( -1 ),
	  m_currentViewportY( -1 ),
	  m_currentViewportWidth( -1 ),
	  m_currentViewportHeight( -1 ),
	  m_currentScissorX( -1 ),
	  m_currentScissorY( -1 ),
	  m_currentScissorWidth( -1 ),
	  m_currentScissorHeight( -1 ),
	  m_currentActiveTexture( GL_TEXTURE0 ),
	  m_currentLineWidth( -1.0f ),
	  m_currentShaderProgram( 0 ),
	  m_currentVAO( 0 ),
	  m_currentArrayBuffer( 0 ),
	  m_currentElementBuffer( 0 ),
	  m_currentTextureMagFilter( -1 ),
	  m_currentTextureMinFilter( -1 ),
	  m_currentBoundTexture( 0 ),
	  m_scissorTestEnabled( false ),
	  m_cullFaceEnabled( false ),
	  m_cullFaceMode( GL_BACK ),
	  m_blendEnabled( false ),
	  m_depthTestEnabled( false ),
	  m_depthMaskEnabled( false ),
	  m_colorMaskR( true ),
	  m_colorMaskG( true ),
	  m_colorMaskB( true ),
	  m_colorMaskA( true ),
	  m_currentBlendSrcFactor( -1 ),
	  m_currentBlendDstFactor( -1 ),
	  m_msaaFBO( 0 ),
	  m_msaaColorRBO( 0 ),
	  m_msaaDepthRBO( 0 )
{

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
		m_flushTimings[i].queryPoolSize = 0;
		m_flushTimings[i].nextQueryIndex = 0;

		for ( int q = 0; q < FlushTiming::MAX_QUERIES_PER_TYPE; ++q )
		{
			m_flushTimings[i].queryPool[q] = 0;
		}
	}

	m_blendMode = BlendModeNormal;
	m_blendSrcFactor = BLEND_ONE;
	m_blendDstFactor = BLEND_ZERO;
	m_textureSwitches = 0;
	m_prevTextureSwitches = 0;
	m_flushTimingCount = 0;
	m_currentFlushStartTime = 0.0;
	m_msaaEnabled = false;
	m_msaaSamples = 0;
	m_msaaWidth = 0;
	m_msaaHeight = 0;

	g_renderer2d = new Renderer2DOpenGL();
	g_megavbo2d = new MegaVBO2D();
	g_renderer3d = new Renderer3DOpenGL();
	g_megavbo3d = new MegaVBO3D();
}


RendererOpenGL::~RendererOpenGL()
{
	DestroyMSAAFramebuffer();

	for ( int i = 0; i < m_flushTimingCount; i++ )
	{
		for ( int q = 0; q < m_flushTimings[i].queryPoolSize; ++q )
		{
			if ( m_flushTimings[i].queryPool[q] != 0 )
			{
				glDeleteQueries( 1, &m_flushTimings[i].queryPool[q] );
			}
		}
	}
}

// ============================================================================
// OPENGL STATE MANAGEMENT
// ============================================================================

void RendererOpenGL::SetViewport( int x, int y, int width, int height )
{
	if ( m_currentViewportX != x || m_currentViewportY != y ||
		 m_currentViewportWidth != width || m_currentViewportHeight != height )
	{
		glViewport( x, y, width, height );
		m_currentViewportX = x;
		m_currentViewportY = y;
		m_currentViewportWidth = width;
		m_currentViewportHeight = height;
	}
}


void RendererOpenGL::SetActiveTexture( unsigned int textureUnit )
{
	GLenum glTexture;
	if ( textureUnit >= 0x84C0 )
	{
		glTexture = (GLenum)textureUnit;
	}
	else
	{
		glTexture = GL_TEXTURE0 + textureUnit;
	}

	if ( m_currentActiveTexture != glTexture )
	{
		glActiveTexture( glTexture );
		m_currentActiveTexture = glTexture;
	}
}


void RendererOpenGL::SetShaderProgram( unsigned int program )
{
	if ( m_currentShaderProgram != program )
	{
		glUseProgram( program );
		m_currentShaderProgram = program;
	}
}


void RendererOpenGL::SetVertexArray( unsigned int vao )
{
	if ( m_currentVAO != vao )
	{
		glBindVertexArray( vao );
		m_currentVAO = vao;
	}
}


void RendererOpenGL::SetArrayBuffer( unsigned int buffer )
{
	if ( m_currentArrayBuffer != buffer )
	{
		glBindBuffer( GL_ARRAY_BUFFER, buffer );
		m_currentArrayBuffer = buffer;
	}
}


void RendererOpenGL::SetElementBuffer( unsigned int buffer )
{
	if ( m_currentElementBuffer != buffer )
	{
		glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, buffer );
		m_currentElementBuffer = buffer;
	}
}


void RendererOpenGL::SetLineWidth( float width )
{
	if ( m_currentLineWidth != width )
	{
		glLineWidth( width );
		m_currentLineWidth = width;
	}
}


float RendererOpenGL::GetLineWidth() const
{
	return m_currentLineWidth;
}


void RendererOpenGL::SetBoundTexture( unsigned int texture )
{
	if ( m_currentBoundTexture != texture )
	{
		glBindTexture( GL_TEXTURE_2D, texture );
		m_currentBoundTexture = texture;
		m_textureSwitches++;
	}
}


void RendererOpenGL::SetScissorTest( bool enabled )
{
	if ( m_scissorTestEnabled != enabled )
	{
		if ( enabled )
		{
			glEnable( GL_SCISSOR_TEST );
		}
		else
		{
			glDisable( GL_SCISSOR_TEST );
		}
		m_scissorTestEnabled = enabled;
	}
}


void RendererOpenGL::SetScissor( int x, int y, int width, int height )
{
	if ( m_currentScissorX != x || m_currentScissorY != y ||
		 m_currentScissorWidth != width || m_currentScissorHeight != height )
	{
		glScissor( x, y, width, height );
		m_currentScissorX = x;
		m_currentScissorY = y;
		m_currentScissorWidth = width;
		m_currentScissorHeight = height;
	}
}


void RendererOpenGL::SetTextureParameter( unsigned int pname, int param )
{

	GLenum glPname;
	if ( pname == TEXTURE_MAG_FILTER || pname == GL_TEXTURE_MAG_FILTER )
	{
		glPname = GL_TEXTURE_MAG_FILTER;
	}
	else if ( pname == TEXTURE_MIN_FILTER || pname == GL_TEXTURE_MIN_FILTER )
	{
		glPname = GL_TEXTURE_MIN_FILTER;
	}
	else if ( pname == TEXTURE_WRAP_S || pname == GL_TEXTURE_WRAP_S )
	{
		glPname = GL_TEXTURE_WRAP_S;
	}
	else if ( pname == TEXTURE_WRAP_T || pname == GL_TEXTURE_WRAP_T )
	{
		glPname = GL_TEXTURE_WRAP_T;
	}
	else
	{
		glPname = (GLenum)pname; // Assume its a GL constant
	}

	GLint glParam = param;
	if ( ( pname == TEXTURE_MAG_FILTER || pname == GL_TEXTURE_MAG_FILTER ||
		   pname == TEXTURE_MIN_FILTER || pname == GL_TEXTURE_MIN_FILTER ) &&
		 param < 0x2600 )
	{
		switch ( param )
		{
			case TEXTURE_FILTER_NEAREST:
				glParam = GL_NEAREST;
				break;
			case TEXTURE_FILTER_LINEAR:
				glParam = GL_LINEAR;
				break;
			case TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
				glParam = GL_NEAREST_MIPMAP_NEAREST;
				break;
			case TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
				glParam = GL_LINEAR_MIPMAP_NEAREST;
				break;
			case TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
				glParam = GL_NEAREST_MIPMAP_LINEAR;
				break;
			case TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
				glParam = GL_LINEAR_MIPMAP_LINEAR;
				break;
			default:
				glParam = param;
		}
	}

	if ( glPname == GL_TEXTURE_MAG_FILTER )
	{
		if ( m_currentTextureMagFilter != param )
		{
			glTexParameteri( GL_TEXTURE_2D, glPname, glParam );
			m_currentTextureMagFilter = param;
		}
	}
	else if ( glPname == GL_TEXTURE_MIN_FILTER )
	{
		if ( m_currentTextureMinFilter != param )
		{
			glTexParameteri( GL_TEXTURE_2D, glPname, glParam );
			m_currentTextureMinFilter = param;
		}
	}
	else
	{
		glTexParameteri( GL_TEXTURE_2D, glPname, glParam );
	}
}


// ============================================================================
// BLEND MODES & DEPTH
// ============================================================================

void RendererOpenGL::SetBlendMode( int _blendMode )
{
	//
	// flush any existing sprites if the blend mode changes

	if ( m_blendMode != _blendMode && g_renderer2d->GetCurrentStaticSpriteVertexCount() > 0 )
	{
		g_renderer2d->EndStaticSpriteBatch();
	}

	m_blendMode = _blendMode;

	switch ( _blendMode )
	{
		case BlendModeDisabled:
			if ( m_blendEnabled )
			{
				glDisable( GL_BLEND );
				m_blendEnabled = false;
			}
			break;
		case BlendModeNormal:
			SetBlendFunc( BLEND_SRC_ALPHA, BLEND_ONE_MINUS_SRC_ALPHA );
			if ( !m_blendEnabled )
			{
				glEnable( GL_BLEND );
				m_blendEnabled = true;
			}
			break;
		case BlendModeAdditive:
			SetBlendFunc( BLEND_SRC_ALPHA, BLEND_ONE );
			if ( !m_blendEnabled )
			{
				glEnable( GL_BLEND );
				m_blendEnabled = true;
			}
			break;
		case BlendModeSubtractive:
			SetBlendFunc( BLEND_SRC_ALPHA, BLEND_ONE_MINUS_SRC_COLOR );
			if ( !m_blendEnabled )
			{
				glEnable( GL_BLEND );
				m_blendEnabled = true;
			}
			break;
	}
}


void RendererOpenGL::SetBlendFunc( int srcFactor, int dstFactor )
{
	m_blendSrcFactor = srcFactor;
	m_blendDstFactor = dstFactor;

	//
	// Convert abstract blend factors to OpenGL constants

	GLenum glSrcFactor = GL_ONE;
	GLenum glDstFactor = GL_ZERO;

	switch ( srcFactor )
	{
		case BLEND_ZERO:
			glSrcFactor = GL_ZERO;
			break;
		case BLEND_ONE:
			glSrcFactor = GL_ONE;
			break;
		case BLEND_SRC_ALPHA:
			glSrcFactor = GL_SRC_ALPHA;
			break;
		case BLEND_ONE_MINUS_SRC_ALPHA:
			glSrcFactor = GL_ONE_MINUS_SRC_ALPHA;
			break;
		case BLEND_ONE_MINUS_SRC_COLOR:
			glSrcFactor = GL_ONE_MINUS_SRC_COLOR;
			break;
		default:
			glSrcFactor = (GLenum)srcFactor;
			break;
	}

	switch ( dstFactor )
	{
		case BLEND_ZERO:
			glDstFactor = GL_ZERO;
			break;
		case BLEND_ONE:
			glDstFactor = GL_ONE;
			break;
		case BLEND_SRC_ALPHA:
			glDstFactor = GL_SRC_ALPHA;
			break;
		case BLEND_ONE_MINUS_SRC_ALPHA:
			glDstFactor = GL_ONE_MINUS_SRC_ALPHA;
			break;
		case BLEND_ONE_MINUS_SRC_COLOR:
			glDstFactor = GL_ONE_MINUS_SRC_COLOR;
			break;
		default:
			glDstFactor = (GLenum)dstFactor;
			break;
	}

	//
	// Only call glBlendFunc if the parameters have actually changed

	if ( m_currentBlendSrcFactor != srcFactor || m_currentBlendDstFactor != dstFactor )
	{
		glBlendFunc( glSrcFactor, glDstFactor );
		m_currentBlendSrcFactor = srcFactor;
		m_currentBlendDstFactor = dstFactor;
	}
}


void RendererOpenGL::SetDepthBuffer( bool _enabled, bool _clearNow )
{
	if ( _enabled )
	{
		if ( !m_depthTestEnabled )
		{
			glEnable( GL_DEPTH_TEST );
			m_depthTestEnabled = true;
		}
		if ( !m_depthMaskEnabled )
		{
			glDepthMask( true );
			m_depthMaskEnabled = true;
		}
	}
	else
	{
		if ( m_depthTestEnabled )
		{
			glDisable( GL_DEPTH_TEST );
			m_depthTestEnabled = false;
		}
		if ( m_depthMaskEnabled )
		{
			glDepthMask( false );
			m_depthMaskEnabled = false;
		}
	}

	if ( _clearNow )
	{
		glClear( GL_DEPTH_BUFFER_BIT );
	}
}


void RendererOpenGL::SetDepthMask( bool enabled )
{
	if ( m_depthMaskEnabled != enabled )
	{
		glDepthMask( enabled ? GL_TRUE : GL_FALSE );
		m_depthMaskEnabled = enabled;
	}
}


void RendererOpenGL::SetCullFace( bool enabled, int mode )
{
	GLenum glMode = ( mode == CULL_FACE_BACK ) ? GL_BACK : ( mode == CULL_FACE_FRONT ) ? GL_FRONT
																					   : GL_FRONT_AND_BACK;

	if ( enabled )
	{
		if ( !m_cullFaceEnabled )
		{
			glEnable( GL_CULL_FACE );
			m_cullFaceEnabled = true;
		}
		if ( m_cullFaceMode != glMode )
		{
			glCullFace( glMode );
			m_cullFaceMode = glMode;
		}
	}
	else
	{
		if ( m_cullFaceEnabled )
		{
			glDisable( GL_CULL_FACE );
			m_cullFaceEnabled = false;
		}
	}
}


void RendererOpenGL::SetColorMask( bool r, bool g, bool b, bool a )
{
	if ( m_colorMaskR != r || m_colorMaskG != g || m_colorMaskB != b || m_colorMaskA != a )
	{
		glColorMask( r ? GL_TRUE : GL_FALSE,
					 g ? GL_TRUE : GL_FALSE,
					 b ? GL_TRUE : GL_FALSE,
					 a ? GL_TRUE : GL_FALSE );

		m_colorMaskR = r;
		m_colorMaskG = g;
		m_colorMaskB = b;
		m_colorMaskA = a;
	}
}


void RendererOpenGL::SetClearColor( float r, float g, float b, float a )
{
	m_clearColorR = r;
	m_clearColorG = g;
	m_clearColorB = b;
	m_clearColorA = a;

	glClearColor( r, g, b, a );
}


// ============================================================================
// SHADER SETUP
// ============================================================================

unsigned int RendererOpenGL::CreateShader( const char *vertexSource, const char *fragmentSource )
{
	//
	// Create vertex shader

	unsigned int vertexShader = glCreateShader( GL_VERTEX_SHADER );
	glShaderSource( vertexShader, 1, &vertexSource, NULL );
	glCompileShader( vertexShader );

	int success;
	char infoLog[512];
	glGetShaderiv( vertexShader, GL_COMPILE_STATUS, &success );
	if ( !success )
	{
		glGetShaderInfoLog( vertexShader, 512, NULL, infoLog );
		AppDebugOut( "Vertex shader compilation failed: %s\n", infoLog );
	}

	//
	// Create fragment shader

	unsigned int fragmentShader = glCreateShader( GL_FRAGMENT_SHADER );
	glShaderSource( fragmentShader, 1, &fragmentSource, NULL );
	glCompileShader( fragmentShader );

	glGetShaderiv( fragmentShader, GL_COMPILE_STATUS, &success );
	if ( !success )
	{
		glGetShaderInfoLog( fragmentShader, 512, NULL, infoLog );
		AppDebugOut( "Fragment shader compilation failed: %s\n", infoLog );
	}

	//
	// Create shader program

	unsigned int shaderProgram = glCreateProgram();
	glAttachShader( shaderProgram, vertexShader );
	glAttachShader( shaderProgram, fragmentShader );

#ifdef TARGET_OS_MACOSX
	glBindAttribLocation( shaderProgram, 0, "aPos" );
	glBindAttribLocation( shaderProgram, 1, "aColor" );
	glBindAttribLocation( shaderProgram, 2, "aTexCoord" );
#endif

	glLinkProgram( shaderProgram );

	glGetProgramiv( shaderProgram, GL_LINK_STATUS, &success );
	if ( !success )
	{
		glGetProgramInfoLog( shaderProgram, 512, NULL, infoLog );
		AppDebugOut( "Shader program linking failed: %s\n", infoLog );
	}

	glDeleteShader( vertexShader );
	glDeleteShader( fragmentShader );

	return shaderProgram;
}


// ============================================================================
// FRAME COORDINATION
// ============================================================================

void RendererOpenGL::BeginFrame()
{
	g_renderer2d->BeginFrame2D();
	g_renderer3d->BeginFrame3D();

	ResetFlushTimings();
	m_textureSwitches = 0;
}


void RendererOpenGL::EndFrame()
{
	g_renderer2d->EndFrame2D();
	g_renderer3d->EndFrame3D();

	UpdateGpuTimings();

	m_prevTextureSwitches = m_textureSwitches;
}


// ============================================================================
// WINDOW MANAGEMENT
// ============================================================================

void RendererOpenGL::HandleWindowResize()
{
	int screenW = g_windowManager->DrawableWidth();
	int screenH = g_windowManager->DrawableHeight();

	ResizeMSAAFramebuffer( screenW, screenH );

	if ( g_renderer2d )
	{
		g_renderer2d->Reset2DViewport();
	}
}


// ============================================================================
// GPU TIMING
// ============================================================================

void RendererOpenGL::StartFlushTiming( const char *name )
{
	m_currentFlushStartTime = GetHighResTime();

	//
	// Find timing entry

	for ( int i = 0; i < m_flushTimingCount; i++ )
	{
		if ( strcmp( m_flushTimings[i].name, name ) == 0 )
		{
			FlushTiming *timing = &m_flushTimings[i];

			//
			// Get next query from pool

			int queryIndex = timing->nextQueryIndex;

			if ( timing->queryPoolSize < FlushTiming::MAX_QUERIES_PER_TYPE )
			{
				//
				// Create new query object

#ifndef TARGET_EMSCRIPTEN
				glGenQueries( 1, &timing->queryPool[timing->queryPoolSize] );
#endif
				queryIndex = timing->queryPoolSize;
				timing->queryPoolSize++;
			}
			else
			{
				//
				// Wrap around in pool

				queryIndex = timing->nextQueryIndex % FlushTiming::MAX_QUERIES_PER_TYPE;
			}

			//
			// Start GPU timing query

#ifndef TARGET_EMSCRIPTEN
			glBeginQuery( GL_TIME_ELAPSED, timing->queryPool[queryIndex] );
#endif
			timing->queryObject = timing->queryPool[queryIndex];
			timing->queryPending = true;
			timing->nextQueryIndex = ( queryIndex + 1 ) % FlushTiming::MAX_QUERIES_PER_TYPE;
			return;
		}
	}

	//
	// Create new timing entry

	if ( m_flushTimingCount < MAX_FLUSH_TYPES )
	{
		FlushTiming *timing = &m_flushTimings[m_flushTimingCount];
		timing->name = name;
		timing->totalTime = 0.0;
		timing->totalGpuTime = 0.0;
		timing->callCount = 0;
		timing->queryPending = false;
		timing->queryPoolSize = 0;
		timing->nextQueryIndex = 0;

#ifndef TARGET_EMSCRIPTEN
		glGenQueries( 1, &timing->queryPool[0] );
		glBeginQuery( GL_TIME_ELAPSED, timing->queryPool[0] );
#endif
		timing->queryObject = timing->queryPool[0];
		timing->queryPending = true;
		timing->queryPoolSize = 1;
		timing->nextQueryIndex = 1;
		m_flushTimingCount++;
	}
}


void RendererOpenGL::EndFlushTiming( const char *name )
{
	double cpuTime = GetHighResTime() - m_currentFlushStartTime;

	for ( int i = 0; i < m_flushTimingCount; i++ )
	{
		if ( strcmp( m_flushTimings[i].name, name ) == 0 )
		{
			if ( m_flushTimings[i].queryPending )
			{
#ifndef TARGET_EMSCRIPTEN
				glEndQuery( GL_TIME_ELAPSED );
#endif
				m_flushTimings[i].queryPending = false;
			}

			//
			// Update CPU timing

			m_flushTimings[i].totalTime += cpuTime;
			m_flushTimings[i].callCount++;
			return;
		}
	}
}


void RendererOpenGL::UpdateGpuTimings()
{
	//
	// Call this once per frame to collect GPU timing results from all queries in pool

	for ( int i = 0; i < m_flushTimingCount; i++ )
	{
		FlushTiming *timing = &m_flushTimings[i];

		//
		// Check all queries in the pool

		for ( int q = 0; q < timing->queryPoolSize; q++ )
		{
			int available = 0;
#ifndef TARGET_EMSCRIPTEN
			glGetQueryObjectiv( timing->queryPool[q], GL_QUERY_RESULT_AVAILABLE, &available );
#endif
			if ( available )
			{
				uint64_t gpuTime = 0;
#ifndef TARGET_EMSCRIPTEN
				glGetQueryObjectui64v( timing->queryPool[q], GL_QUERY_RESULT, &gpuTime );
#endif
				//
				// Convert nanoseconds to milliseconds

				timing->totalGpuTime += gpuTime / 1000000.0;
			}
		}
	}
}


void RendererOpenGL::ResetFlushTimings()
{
	for ( int i = 0; i < m_flushTimingCount; i++ )
	{
		m_flushTimings[i].totalTime = 0.0;
		m_flushTimings[i].totalGpuTime = 0.0;
		m_flushTimings[i].callCount = 0;
	}
}


// ============================================================================
// SCENE MANAGEMENT
// ============================================================================

void RendererOpenGL::Setup2DRenderState()
{
	SetBoundTexture( 0 );
	SetBlendMode( BlendModeNormal );

	g_renderer3d->InvalidateMatrices3D();
	g_renderer3d->InvalidateFog3D();

	g_renderer2d->Reset2DViewport();

	SetDepthBuffer( false, false );
	SetCullFace( false, CULL_FACE_BACK );
}


void RendererOpenGL::Setup3DRenderState()
{
	SetBoundTexture( 0 );
	SetBlendMode( BlendModeNormal );

	Renderer2DOpenGL *renderer2dOpenGL = dynamic_cast<Renderer2DOpenGL *>( g_renderer2d );
	if ( renderer2dOpenGL )
	{
		renderer2dOpenGL->InvalidateStateCache();
	}

	SetDepthBuffer( true, true );
	SetCullFace( true, CULL_FACE_BACK );
}


void RendererOpenGL::CleanupRenderState()
{
	// Empty for now
}


// ============================================================================
// RENDER PASS SYSTEM
// ============================================================================

void RendererOpenGL::Begin2DRendering()
{
	if ( m_currentPass != RENDER_PASS_NONE )
	{
		return;
	}

	m_currentPass = RENDER_PASS_2D;
	Setup2DRenderState();
}


void RendererOpenGL::End2DRendering()
{
	if ( m_currentPass != RENDER_PASS_2D )
	{
		return;
	}

	g_renderer2d->FlushAllBatches();
	CleanupRenderState();
	m_currentPass = RENDER_PASS_NONE;
}


void RendererOpenGL::Begin3DRendering()
{
	if ( m_currentPass != RENDER_PASS_NONE )
	{
		return;
	}

	m_currentPass = RENDER_PASS_3D;
	Setup3DRenderState();
}


void RendererOpenGL::End3DRendering()
{
	if ( m_currentPass != RENDER_PASS_3D )
	{
		return;
	}

	g_renderer3d->FlushAllBatches3D();
	CleanupRenderState();
	m_currentPass = RENDER_PASS_NONE;
}


void RendererOpenGL::ClearScreen( bool _colour, bool _depth )
{
	if ( _colour )
		glClear( GL_COLOR_BUFFER_BIT );
	if ( _depth )
		glClear( GL_DEPTH_BUFFER_BIT );
}


// ============================================================================
// MSAA FRAMEBUFFER IMPLEMENTATION
// ============================================================================

void RendererOpenGL::InitializeMSAAFramebuffer( int width, int height, int samples )
{
	m_msaaWidth = width;
	m_msaaHeight = height;
	m_msaaSamples = samples;

	if ( samples <= 0 )
	{
		m_msaaEnabled = false;
		m_msaaFBO = 0;
		m_msaaColorRBO = 0;
		m_msaaDepthRBO = 0;
		return;
	}

	//
	// Query the maximum supported samples

	GLint maxSamples = 0;
	glGetIntegerv( GL_MAX_SAMPLES, &maxSamples );

	//
	// Generate and bind the MSAA framebuffer

	glGenFramebuffers( 1, &m_msaaFBO );
	glBindFramebuffer( GL_FRAMEBUFFER, m_msaaFBO );

	//
	// Create the multisampled color renderbuffer

	glGenRenderbuffers( 1, &m_msaaColorRBO );
	glBindRenderbuffer( GL_RENDERBUFFER, m_msaaColorRBO );
	glRenderbufferStorageMultisample( GL_RENDERBUFFER, m_msaaSamples,
									  GL_RGBA8, width, height );
	glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
							   GL_RENDERBUFFER, m_msaaColorRBO );

	//
	// Create the multisampled depth renderbuffer

	glGenRenderbuffers( 1, &m_msaaDepthRBO );
	glBindRenderbuffer( GL_RENDERBUFFER, m_msaaDepthRBO );
	glRenderbufferStorageMultisample( GL_RENDERBUFFER, m_msaaSamples,
									  GL_DEPTH_COMPONENT24, width, height );
	glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
							   GL_RENDERBUFFER, m_msaaDepthRBO );

	//
	// Check the framebuffer for completeness

	GLenum status = glCheckFramebufferStatus( GL_FRAMEBUFFER );
	if ( status != GL_FRAMEBUFFER_COMPLETE )
	{
		AppDebugOut( "MSAA creation failed with status: 0x%x\n", status );

		if ( m_msaaColorRBO )
			glDeleteRenderbuffers( 1, &m_msaaColorRBO );
		if ( m_msaaDepthRBO )
			glDeleteRenderbuffers( 1, &m_msaaDepthRBO );
		if ( m_msaaFBO )
			glDeleteFramebuffers( 1, &m_msaaFBO );

		m_msaaFBO = 0;
		m_msaaColorRBO = 0;
		m_msaaDepthRBO = 0;
		m_msaaEnabled = false;
		m_msaaSamples = 0;
	}
	else
	{
		m_msaaEnabled = true;
		AppDebugOut( "MSAA applied successfully\n" );
	}

	//
	// Unbind the framebuffer

	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	glBindRenderbuffer( GL_RENDERBUFFER, 0 );
}


void RendererOpenGL::ResizeMSAAFramebuffer( int width, int height )
{
	if ( !m_msaaEnabled || m_msaaSamples <= 0 )
	{
		return;
	}

	if ( width == m_msaaWidth && height == m_msaaHeight )
	{
		return;
	}

	//
	// Destroy and reincarnate the FBO with new dimensions

	DestroyMSAAFramebuffer();
	InitializeMSAAFramebuffer( width, height, m_msaaSamples );
}


void RendererOpenGL::DestroyMSAAFramebuffer()
{

	if ( m_msaaColorRBO )
	{
		glDeleteRenderbuffers( 1, &m_msaaColorRBO );
		m_msaaColorRBO = 0;
	}

	if ( m_msaaDepthRBO )
	{
		glDeleteRenderbuffers( 1, &m_msaaDepthRBO );
		m_msaaDepthRBO = 0;
	}

	if ( m_msaaFBO )
	{
		glDeleteFramebuffers( 1, &m_msaaFBO );
		m_msaaFBO = 0;
	}

	m_msaaEnabled = false;
}


void RendererOpenGL::BeginMSAARendering()
{
	if ( m_msaaEnabled && m_msaaFBO != 0 )
	{
		glBindFramebuffer( GL_FRAMEBUFFER, m_msaaFBO );
	}
}


void RendererOpenGL::EndMSAARendering()
{
	if ( m_msaaEnabled && m_msaaFBO != 0 )
	{
		glBindFramebuffer( GL_READ_FRAMEBUFFER, m_msaaFBO );
		glBindFramebuffer( GL_DRAW_FRAMEBUFFER, 0 );

		glBlitFramebuffer( 0, 0, m_msaaWidth, m_msaaHeight,
						   0, 0, m_msaaWidth, m_msaaHeight,
						   GL_COLOR_BUFFER_BIT, GL_NEAREST );

		glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	}
}


// ============================================================================
// SCREENSHOTS
// ============================================================================

void RendererOpenGL::SaveScreenshot()
{
	float timeNow = GetHighResTime();
	char *screenshotsDir = ScreenshotsDirectory();

	int screenW = g_windowManager->DrawableWidth();
	int screenH = g_windowManager->DrawableHeight();

	unsigned char *screenBuffer = new unsigned char[screenW * screenH * 3];
	glReadPixels( 0, 0, screenW, screenH, GL_RGB, GL_UNSIGNED_BYTE, screenBuffer );

	Bitmap bitmap( screenW, screenH );
	bitmap.Clear( Black );

	for ( int y = 0; y < screenH; ++y )
	{
		unsigned const char *line = &screenBuffer[y * screenW * 3];
		for ( int x = 0; x < screenW; ++x )
		{
			unsigned const char *p = &line[x * 3];
			bitmap.PutPixel( x, y, Colour( p[0], p[1], p[2] ) );
		}
	}

	int screenshotIndex = 1;
	while ( true )
	{
		time_t theTimeT = time( NULL );
		tm *theTime = localtime( &theTimeT );
		char filename[2048];

		snprintf( filename, sizeof( filename ), "%s/screenshot %02d-%02d-%02d %02d.bmp",
				  screenshotsDir, 1900 + theTime->tm_year, theTime->tm_mon + 1, theTime->tm_mday, screenshotIndex );

		if ( !DoesFileExist( filename ) )
		{
			bitmap.SaveBmp( filename );
			break;
		}
		++screenshotIndex;
	}

	delete[] screenBuffer;
	delete[] screenshotsDir;

	float timeTaken = GetHighResTime() - timeNow;
	AppDebugOut( "Screenshot %dms\n", int( timeTaken * 1000 ) );
}


// ============================================================================
// ABSTRACTION HELPERS
// ============================================================================

GLenum RendererOpenGL::ConvertTextureAddressMode( int mode )
{
	switch ( mode )
	{
		case TEXTURE_ADDRESS_CLAMP:
			return GL_CLAMP_TO_EDGE;
		case TEXTURE_ADDRESS_REPEAT:
			return GL_REPEAT;
		case TEXTURE_ADDRESS_MIRROR:
			return GL_MIRRORED_REPEAT;
		case TEXTURE_ADDRESS_MIRROR_ONCE:
// GL_MIRROR_CLAMP_TO_EDGE requires OpenGL 4.4+, fallback to GL_MIRRORED_REPEAT
#ifdef GL_MIRROR_CLAMP_TO_EDGE
			return GL_MIRROR_CLAMP_TO_EDGE;
#else
			return GL_MIRRORED_REPEAT;
#endif
		case TEXTURE_ADDRESS_BORDER:
// GL_CLAMP_TO_BORDER is not available in OpenGL ES
// Fall back to GL_CLAMP_TO_EDGE which clamps to edge pixels
#ifdef GL_CLAMP_TO_BORDER
			return GL_CLAMP_TO_BORDER;
#else
			return GL_CLAMP_TO_EDGE;
#endif
		default:
			return GL_CLAMP_TO_EDGE;
	}
}


GLenum RendererOpenGL::ConvertDepthComparisonFunc( int func )
{
	switch ( func )
	{
		case DEPTH_COMPARISON_NEVER:
			return GL_NEVER;
		case DEPTH_COMPARISON_LESS:
			return GL_LESS;
		case DEPTH_COMPARISON_EQUAL:
			return GL_EQUAL;
		case DEPTH_COMPARISON_LESS_EQUAL:
			return GL_LEQUAL;
		case DEPTH_COMPARISON_GREATER:
			return GL_GREATER;
		case DEPTH_COMPARISON_NOT_EQUAL:
			return GL_NOTEQUAL;
		case DEPTH_COMPARISON_GREATER_EQUAL:
			return GL_GEQUAL;
		case DEPTH_COMPARISON_ALWAYS:
			return GL_ALWAYS;
		default:
			return GL_LESS;
	}
}


// ============================================================================
// TEXTURE MANAGEMENT
// ============================================================================

unsigned int RendererOpenGL::CreateTexture( int width, int height, const Colour *pixels, int mipmapLevel )
{
	GLuint texId;

	glGenTextures( 1, &texId );
	glBindTexture( GL_TEXTURE_2D, texId );

	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, ConvertTextureAddressMode( TEXTURE_ADDRESS_CLAMP ) );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, ConvertTextureAddressMode( TEXTURE_ADDRESS_CLAMP ) );

	TextureFilterMode filterMode = GetDefaultTextureFilterMode();
	GLint magFilter = ( filterMode == TEXTURE_FILTER_MODE_NEAREST ) ? GL_NEAREST : GL_LINEAR;
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter );

	if ( mipmapLevel > 0 )
	{
		GLint minFilter = ( filterMode == TEXTURE_FILTER_MODE_NEAREST )
							  ? GL_NEAREST_MIPMAP_LINEAR
							  : GL_LINEAR_MIPMAP_LINEAR;
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, mipmapLevel );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels );
		glGenerateMipmap( GL_TEXTURE_2D );
	}
	else
	{
		GLint minFilter = ( filterMode == TEXTURE_FILTER_MODE_NEAREST ) ? GL_NEAREST : GL_LINEAR;
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0 );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels );
	}

	return (unsigned int)texId;
}


void RendererOpenGL::DeleteTexture( unsigned int textureID )
{
	if ( textureID > 0 )
	{
		GLuint glTexId = (GLuint)textureID;
		glDeleteTextures( 1, &glTexId );
	}
}

#endif // RENDERER_OPENGL
