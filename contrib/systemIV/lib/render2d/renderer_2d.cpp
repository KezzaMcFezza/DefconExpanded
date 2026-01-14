#include "systemiv.h"

#include <time.h>
#include <stdarg.h>
#include <algorithm>

#include "lib/gucci/window_manager.h"
#include "lib/resource/bitmapfont.h"
#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/resource/bitmap.h"
#include "lib/resource/sprite_atlas.h"
#include "lib/hi_res_time.h"
#include "lib/debug/debug_utils.h"
#include "lib/preferences.h"
#include "lib/string_utils.h"
#include "lib/filesys/filesys_utils.h"
#include "lib/language_table.h"
#include "lib/render3d/renderer_3d.h"
#include "lib/math/matrix4f.h"
#include "lib/render/colour.h"
#include "shaders/vertex.glsl.h"
#include "shaders/color_fragment.glsl.h"
#include "shaders/texture_fragment.glsl.h"

#include "lib/render/renderer.h"
#include "renderer_2d.h"
#include "megavbo/megavbo_2d.h"

Renderer2D *g_renderer2d = NULL;

// ============================================================================
// CONSTRUCTOR & DESTRUCTOR
// ============================================================================

Renderer2D::Renderer2D()
	: m_alpha( 1.0f ),
	  m_colourDepth( 32 ),
	  m_mouseMode( 0 ),
	  m_shaderProgram( 0 ),
	  m_colorShaderProgram( 0 ),
	  m_textureShaderProgram( 0 ),
	  m_lineShaderProgram( 0 ),
	  m_VAO( 0 ),
	  m_VBO( 0 ),
	  m_textVAO( 0 ),
	  m_textVBO( 0 ),
	  m_spriteVAO( 0 ),
	  m_spriteVBO( 0 ),
	  m_rotatingSpriteVAO( 0 ),
	  m_rotatingSpriteVBO( 0 ),
	  m_lineVAO( 0 ),
	  m_lineVBO( 0 ),
	  m_circleVAO( 0 ),
	  m_circleVBO( 0 ),
	  m_circleFillVAO( 0 ),
	  m_circleFillVBO( 0 ),
	  m_rectVAO( 0 ),
	  m_rectVBO( 0 ),
	  m_rectFillVAO( 0 ),
	  m_rectFillVBO( 0 ),
	  m_triangleFillVAO( 0 ),
	  m_triangleFillVBO( 0 ),
	  m_immediateVAO( 0 ),
	  m_immediateVBO( 0 ),
	  m_bufferNeedsUpload( true ),
	  m_projMatrixLocation( -1 ),
	  m_modelViewMatrixLocation( -1 ),
	  m_colorLocation( -1 ),
	  m_textureLocation( -1 ),
	  m_triangleVertexCount( 0 ),
	  m_lineVertexCount( 0 ),
	  m_currentFontBatchIndex( 0 ),
	  m_textVertices( NULL ),
	  m_textVertexCount( 0 ),
	  m_currentTextTexture( 0 ),
	  m_staticSpriteVertexCount( 0 ),
	  m_currentStaticSpriteTexture( 0 ),
	  m_rotatingSpriteVertexCount( 0 ),
	  m_currentRotatingSpriteTexture( 0 ),
	  m_circleVertexCount( 0 ),
	  m_circleFillVertexCount( 0 ),
	  m_rectVertexCount( 0 ),
	  m_rectFillVertexCount( 0 ),
	  m_triangleFillVertexCount( 0 ),
	  m_lineConversionBuffer( NULL ),
	  m_lineConversionBufferSize( 0 ),
	  m_currentLineColor( 0, 0, 0, 0 ),
	  m_currentLineWidth( 1.0f ),
	  m_lineStripActive( false ),
	  m_lineStripColor( 0, 0, 0, 0 ),
	  m_lineStripWidth( 1.0f ),
	  m_currentCircleWidth( 1.0f ),
	  m_currentRectWidth( 1.0f ),
	  m_activeBuffer( BUFFER_IMMEDIATE ),
	  m_batchingTextures( true )
{
	m_currentFontBatchIndex = 0;
	m_textVertices = m_fontBatches[0].vertices;
	m_textVertexCount = 0;
	m_currentTextTexture = 0;
	m_lineVertexCount = 0;
	m_staticSpriteVertexCount = 0;
	m_currentStaticSpriteTexture = 0;
	m_rotatingSpriteVertexCount = 0;
	m_currentRotatingSpriteTexture = 0;
	m_circleVertexCount = 0;
	m_circleFillVertexCount = 0;
	m_rectVertexCount = 0;
	m_rectFillVertexCount = 0;
	m_triangleFillVertexCount = 0;
	m_drawCallsPerFrame = 0;
	m_immediateTriangleCalls = 0;
	m_immediateLineCalls = 0;
	m_textCalls = 0;
	m_lineCalls = 0;
	m_staticSpriteCalls = 0;
	m_rotatingSpriteCalls = 0;
	m_circleCalls = 0;
	m_circleFillCalls = 0;
	m_rectCalls = 0;
	m_rectFillCalls = 0;
	m_triangleFillCalls = 0;
	m_lineVBOCalls = 0;
	m_quadVBOCalls = 0;
	m_triangleVBOCalls = 0;
	m_prevDrawCallsPerFrame = 0;
	m_prevImmediateTriangleCalls = 0;
	m_prevImmediateLineCalls = 0;
	m_prevTextCalls = 0;
	m_prevLineCalls = 0;
	m_prevStaticSpriteCalls = 0;
	m_prevRotatingSpriteCalls = 0;
	m_prevCircleCalls = 0;
	m_prevCircleFillCalls = 0;
	m_prevRectCalls = 0;
	m_prevRectFillCalls = 0;
	m_prevTriangleFillCalls = 0;
	m_prevLineVBOCalls = 0;
	m_prevQuadVBOCalls = 0;
	m_prevTriangleVBOCalls = 0;
	m_activeFontBatches = 0;
	m_prevActiveFontBatches = 0;

	//
	// Initialize font-aware batching system

	for ( int i = 0; i < Renderer::MAX_FONT_ATLASES; i++ )
	{
		m_fontBatches[i].vertexCount = 0;
		m_fontBatches[i].textureID = 0;
	}

	//
	// Allocate line conversion buffer

	m_lineConversionBufferSize = std::max( MAX_VERTICES * 2, MAX_LINE_VERTICES );
	m_lineConversionBuffer = new Vertex2D[m_lineConversionBufferSize];
}


Renderer2D::~Renderer2D()
{
	Shutdown();
}


// ============================================================================
// VIEWPORT & SCENE MANAGEMENT
// ============================================================================

void Renderer2D::Set2DViewport( float l, float r, float b, float t, int x, int y, int w, int h )
{
	m_modelViewMatrix.LoadIdentity();
	m_projectionMatrix.Ortho( l - 0.325f, r - 0.325f, b - 0.325f, t - 0.325f, -1.0f, 1.0f );

	//
	// Calculate scale factors between logical and physical resolution

	float scaleX = (float)g_windowManager->DrawableWidth() / (float)g_windowManager->WindowW();
	float scaleY = (float)g_windowManager->DrawableHeight() / (float)g_windowManager->WindowH();

	//
	// Apply scaling to convert logical coordinates to physical viewport coordinates

	int physicalX = (int)( x * scaleX );
	int physicalY = (int)( ( g_windowManager->WindowH() - h - y ) * scaleY );
	int physicalW = (int)( w * scaleX );
	int physicalH = (int)( h * scaleY );

	g_renderer->SetViewport( physicalX, physicalY, physicalW, physicalH );
}


void Renderer2D::Reset2DViewport()
{
	Set2DViewport( 0, g_windowManager->WindowW(), g_windowManager->WindowH(), 0,
				   0, 0, g_windowManager->WindowW(), g_windowManager->WindowH() );
	ResetClip();
}


void Renderer2D::BeginFrame2D()
{
	ResetFrameCounters();
}


void Renderer2D::EndFrame2D()
{
}


void Renderer2D::Shutdown()
{
	if ( m_lineConversionBuffer )
	{
		delete[] m_lineConversionBuffer;
		m_lineConversionBuffer = NULL;
	}
}


void Renderer2D::SetClip( int x, int y, int w, int h )
{

	//
	// flush all eclipse and text batches before changing scissor state
	// create a seperate draw call for each rendering operation with
	// scissor testing enabled. as you cannot have multiple scissor regions
	// active in the same draw call. other options like shader clipping was
	// explored but due to being ass at programming i just caused lots of
	// rendering artifacts.
	//
	// we increase draw calls by around 10 percent with this method :(

	EndRectFillBatch();
	EndTriangleFillBatch();
	EndStaticSpriteBatch();
	EndLineBatch();
	EndRectBatch();
	EndTextBatch();

	//
	// calculate scale factors between logical and physical resolution

	float scaleX = (float)g_windowManager->DrawableWidth() / (float)g_windowManager->WindowW();
	float scaleY = (float)g_windowManager->DrawableHeight() / (float)g_windowManager->WindowH();

	int sx = int( x * scaleX );
	int sy = int( ( g_windowManager->WindowH() - h - y ) * scaleY );
	int sw = int( w * scaleX );
	int sh = int( h * scaleY );

	g_renderer->SetScissor( sx, sy, sw, sh );
	g_renderer->SetScissorTest( true );
}


void Renderer2D::ResetClip()
{

	EndRectFillBatch();
	EndTriangleFillBatch();
	EndStaticSpriteBatch();
	EndLineBatch();
	EndRectBatch();
	EndTextBatch();

	g_renderer->SetScissorTest( false );
}


// ============================================================================
// DEBUG MENU FUNCTIONS
// ============================================================================


void Renderer2D::ResetFrameCounters()
{

	//
	// store previous frames data

	m_prevDrawCallsPerFrame = m_drawCallsPerFrame;
	m_prevImmediateTriangleCalls = m_immediateTriangleCalls;
	m_prevImmediateLineCalls = m_immediateLineCalls;
	m_prevTextCalls = m_textCalls;
	m_prevLineCalls = m_lineCalls;
	m_prevStaticSpriteCalls = m_staticSpriteCalls;
	m_prevRotatingSpriteCalls = m_rotatingSpriteCalls;
	m_prevCircleCalls = m_circleCalls;
	m_prevCircleFillCalls = m_circleFillCalls;
	m_prevRectCalls = m_rectCalls;
	m_prevRectFillCalls = m_rectFillCalls;
	m_prevTriangleFillCalls = m_triangleFillCalls;
	m_prevLineVBOCalls = m_lineVBOCalls;
	m_prevQuadVBOCalls = m_quadVBOCalls;
	m_prevTriangleVBOCalls = m_triangleVBOCalls;
	m_prevActiveFontBatches = m_activeFontBatches;

	//
	// reset current frame counters

	m_drawCallsPerFrame = 0;
	m_immediateTriangleCalls = 0;
	m_immediateLineCalls = 0;
	m_textCalls = 0;
	m_lineCalls = 0;
	m_staticSpriteCalls = 0;
	m_rotatingSpriteCalls = 0;
	m_circleCalls = 0;
	m_circleFillCalls = 0;
	m_rectCalls = 0;
	m_rectFillCalls = 0;
	m_triangleFillCalls = 0;
	m_lineVBOCalls = 0;
	m_quadVBOCalls = 0;
	m_triangleVBOCalls = 0;
	m_activeFontBatches = 0;
}

//
// increment draw calls for the 2d renderer

void Renderer2D::IncrementDrawCall( const char *bufferType )
{
	m_drawCallsPerFrame++;

	constexpr auto hash = []( const char *str ) constexpr
	{
		uint32_t hash = 5381;
		while ( *str )
		{
			hash = ( ( hash << 5 ) + hash ) + *str++;
		}
		return hash;
	};

	switch ( hash( bufferType ) )
	{
		case hash( "immediate_triangles" ):
			m_immediateTriangleCalls++;
			break;
		case hash( "immediate_lines" ):
			m_immediateLineCalls++;
			break;
		case hash( "text" ):
			m_textCalls++;
			break;
		case hash( "lines" ):
			m_lineCalls++;
			break;
		case hash( "static_sprites" ):
			m_staticSpriteCalls++;
			break;
		case hash( "rotating_sprites" ):
			m_rotatingSpriteCalls++;
			break;
		case hash( "circles" ):
			m_circleCalls++;
			break;
		case hash( "circle_fills" ):
			m_circleFillCalls++;
			break;
		case hash( "rects" ):
			m_rectCalls++;
			break;
		case hash( "rect_fills" ):
			m_rectFillCalls++;
			break;
		case hash( "triangle_fills" ):
			m_triangleFillCalls++;
			break;
		case hash( "line_vbo" ):
			m_lineVBOCalls++;
			break;
		case hash( "quad_vbo" ):
			m_quadVBOCalls++;
			break;
		case hash( "triangle_vbo" ):
			m_triangleVBOCalls++;
			break;
	}
}
