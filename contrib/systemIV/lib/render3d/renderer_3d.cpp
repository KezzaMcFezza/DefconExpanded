#include "lib/resource/image.h"
#include "systemiv.h"

#include <math.h>
#include <string.h>

#include "lib/debug/debug_utils.h"
#include "lib/hi_res_time.h"
#include "lib/render/renderer.h"
#include "lib/render2d/renderer_2d.h"
#include "lib/resource/sprite_atlas.h"
#include "lib/resource/image.h"
#include "lib/math/matrix4f.h"
#include "shaders/vertex.glsl.h"
#include "shaders/fragment.glsl.h"
#include "shaders/textured_vertex.glsl.h"
#include "shaders/textured_fragment.glsl.h"
#include "shaders/model_vertex.glsl.h"

#include "renderer_3d.h"

Renderer3D *g_renderer3d = NULL;

// ============================================================================
// CONSTRUCTOR & DESTRUCTOR
// ============================================================================

Renderer3D::Renderer3D()
	: m_shader3DProgram( 0 ),
	  m_shader3DTexturedProgram( 0 ),
	  m_shader3DModelProgram( 0 ),
	  m_spriteVAO3D( 0 ), m_spriteVBO3D( 0 ),
	  m_lineVAO3D( 0 ), m_lineVBO3D( 0 ),
	  m_circleVAO3D( 0 ), m_circleVBO3D( 0 ),
	  m_circleFillVAO3D( 0 ), m_circleFillVBO3D( 0 ),
	  m_rectVAO3D( 0 ), m_rectVBO3D( 0 ),
	  m_rectFillVAO3D( 0 ), m_rectFillVBO3D( 0 ),
	  m_triangleFillVAO3D( 0 ), m_triangleFillVBO3D( 0 ),
	  m_currentShaderProgram3D( 0 ),
	  m_matrices3DNeedUpdate( true ),
	  m_fog3DNeedsUpdate( true ),
	  m_vertex3DCount( 0 ),
	  m_vertex3DTexturedCount( 0 ),
	  m_lineStrip3DActive( false ),
	  m_lineStrip3DWidth( 1.0f ),
	  m_currentLineWidth3D( 1.0f ),
	  m_currentCircleWidth3D( 1.0f ),
	  m_currentRectWidth3D( 1.0f ),
	  m_texturedQuad3DActive( false ),
	  m_currentTexture3D( 0 ),
	  m_lineConversionBuffer3D( NULL ),
	  m_lineConversionBufferSize3D( 0 ),
	  m_lineVertexCount3D( 0 ),
	  m_staticSpriteVertexCount3D( 0 ),
	  m_currentStaticSpriteTexture3D( 0 ),
	  m_rotatingSpriteVertexCount3D( 0 ),
	  m_currentRotatingSpriteTexture3D( 0 ),
	  m_textVertexCount3D( 0 ),
	  m_currentTextTexture3D( 0 ),
	  m_currentFontBatchIndex3D( 0 ),
	  m_textVertices3D( nullptr ),
	  m_circleVertexCount3D( 0 ),
	  m_circleFillVertexCount3D( 0 ),
	  m_rectVertexCount3D( 0 ),
	  m_rectFillVertexCount3D( 0 ),
	  m_triangleFillVertexCount3D( 0 ),
	  m_immediateModeEnabled3D( false )
{
	// Initialize fog parameters
	m_fogEnabled = false;
	m_fogOrientationBased = false;
	m_fogStart = 1.0f;
	m_fogEnd = 10.0f;
	m_fogDensity = 1.0f;
	m_fogColor[0] = 0.0f;  // R
	m_fogColor[1] = 0.0f;  // G
	m_fogColor[2] = 0.0f;  // B
	m_fogColor[3] = 1.0f;  // A
	m_cameraPos[0] = 0.0f; // X
	m_cameraPos[1] = 0.0f; // Y
	m_cameraPos[2] = 0.0f; // Z

	m_drawCallsPerFrame3D = 0;
	m_immediateVertexCalls3D = 0;
	m_immediateTriangleCalls3D = 0;
	m_lineCalls3D = 0;
	m_staticSpriteCalls3D = 0;
	m_rotatingSpriteCalls3D = 0;
	m_textCalls3D = 0;
	m_megaVBOCalls3D = 0;
	m_circleCalls3D = 0;
	m_circleFillCalls3D = 0;
	m_rectCalls3D = 0;
	m_rectFillCalls3D = 0;
	m_triangleFillCalls3D = 0;
	m_lineVBOCalls3D = 0;
	m_quadVBOCalls3D = 0;
	m_triangleVBOCalls3D = 0;
	m_prevDrawCallsPerFrame3D = 0;
	m_prevImmediateLineCalls3D = 0;
	m_prevImmediateTriangleCalls3D = 0;
	m_prevLineCalls3D = 0;
	m_prevStaticSpriteCalls3D = 0;
	m_prevRotatingSpriteCalls3D = 0;
	m_prevTextCalls3D = 0;
	m_prevMegaVBOCalls3D = 0;
	m_prevCircleCalls3D = 0;
	m_prevCircleFillCalls3D = 0;
	m_prevRectCalls3D = 0;
	m_prevRectFillCalls3D = 0;
	m_prevTriangleFillCalls3D = 0;
	m_prevLineVBOCalls3D = 0;
	m_prevQuadVBOCalls3D = 0;
	m_prevTriangleVBOCalls3D = 0;
	m_activeFontBatches3D = 0;
	m_prevActiveFontBatches3D = 0;

	m_lineConversionBufferSize3D = MAX_3D_VERTICES * 2;
	m_lineConversionBuffer3D = new Vertex3D[m_lineConversionBufferSize3D];

	//
	// Initialize font-aware batching system

	for ( int i = 0; i < Renderer::MAX_FONT_ATLASES; i++ )
	{
		m_fontBatches3D[i].vertexCount = 0;
		m_fontBatches3D[i].textureID = 0;
	}

	m_currentFontBatchIndex3D = 0;
	m_textVertices3D = m_fontBatches3D[0].vertices;
	m_textVertexCount3D = 0;
	m_currentTextTexture3D = 0;
}


Renderer3D::~Renderer3D()
{
	Shutdown();
}


void Renderer3D::Shutdown()
{
	if ( m_lineConversionBuffer3D )
	{
		delete[] m_lineConversionBuffer3D;
		m_lineConversionBuffer3D = NULL;
	}
}


void Renderer3D::SetPerspective( float fovy, float aspect, float nearZ, float farZ )
{
	m_projectionMatrix3D.Perspective( fovy, aspect, nearZ, farZ );
	InvalidateMatrices3D();
}


void Renderer3D::SetLookAt( float eyeX, float eyeY, float eyeZ,
							float centerX, float centerY, float centerZ,
							float upX, float upY, float upZ )
{
	m_modelViewMatrix3D.LookAt( eyeX, eyeY, eyeZ, centerX, centerY, centerZ, upX, upY, upZ );
	InvalidateMatrices3D();
}


void Renderer3D::BeginLineStrip3D( Colour const &col, float lineWidth )
{
	m_lineStrip3DActive = true;
	m_lineStrip3DColor = col;
	m_lineStrip3DWidth = lineWidth;
	m_vertex3DCount = 0;

#ifndef TARGET_EMSCRIPTEN
	g_renderer->SetLineWidth( lineWidth );
#endif
}


void Renderer3D::LineStripVertex3D( float x, float y, float z )
{
	if ( !m_lineStrip3DActive )
		return;

	//
	// Check buffer overflow

	if ( m_vertex3DCount >= MAX_3D_VERTICES )
	{
		return;
	}

	//
	// Convert color to float

	float r = m_lineStrip3DColor.GetRFloat();
	float g = m_lineStrip3DColor.GetGFloat();
	float b = m_lineStrip3DColor.GetBFloat();
	float a = m_lineStrip3DColor.GetAFloat();

	m_vertices3D[m_vertex3DCount] = Vertex3D( x, y, z, r, g, b, a );
	m_vertex3DCount++;
}


void Renderer3D::LineStripVertex3D( const Vector3<float> &vertex )
{
	LineStripVertex3D( vertex.x, vertex.y, vertex.z );
}


void Renderer3D::EndLineStrip3D()
{
	if ( !m_lineStrip3DActive || m_vertex3DCount < 2 )
	{
		m_lineStrip3DActive = false;
		m_vertex3DCount = 0;
		return;
	}

	int lineVertexCount = ( m_vertex3DCount - 1 ) * 2;
	Vertex3D *lineVertices = m_lineConversionBuffer3D;

	int lineIndex = 0;
	for ( int i = 0; i < m_vertex3DCount - 1; i++ )
	{

		//
		// Line from vertex i to vertex i+1

		lineVertices[lineIndex++] = m_vertices3D[i];
		lineVertices[lineIndex++] = m_vertices3D[i + 1];
	}

	//
	// Clear current buffer and add line segments

	m_lineVertexCount3D = 0;
	for ( int i = 0; i < lineVertexCount; i++ )
	{
		if ( m_lineVertexCount3D >= MAX_LINE_VERTICES_3D )
			break;
		m_lineVertices3D[m_lineVertexCount3D++] = lineVertices[i];
	}

	m_vertex3DCount = 0;

#ifndef TARGET_EMSCRIPTEN
	g_renderer->SetLineWidth( m_lineStrip3DWidth );
#endif

	FlushLine3D( false );

	m_lineStrip3DActive = false;
}


void Renderer3D::BeginLineLoop3D( Colour const &col, float lineWidth )
{
	BeginLineStrip3D( col, lineWidth );
}


void Renderer3D::LineLoopVertex3D( float x, float y, float z )
{
	LineStripVertex3D( x, y, z );
}


void Renderer3D::LineLoopVertex3D( const Vector3<float> &vertex )
{
	LineStripVertex3D( vertex );
}


void Renderer3D::EndLineLoop3D()
{
	if ( !m_lineStrip3DActive || m_vertex3DCount < 3 )
	{
		m_lineStrip3DActive = false;
		m_vertex3DCount = 0;
		return;
	}

	//
	// For line loop, add a final line from last vertex back to first

	Vertex3D firstVertex = m_vertices3D[0];
	if ( m_vertex3DCount < MAX_3D_VERTICES )
	{
		m_vertices3D[m_vertex3DCount++] = firstVertex;
	}

	EndLineStrip3D();
}


void Renderer3D::SetColor( const Colour &col )
{
	m_lineStrip3DColor = col;
}


void Renderer3D::Clear3DState()
{
	m_vertex3DCount = 0;
	m_lineStrip3DActive = false;
}


void Renderer3D::EnableDistanceFog( float start, float end, float density, float r, float g, float b, float a )
{
	m_fogEnabled = true;
	m_fogOrientationBased = false;
	m_fogStart = start;
	m_fogEnd = end;
	m_fogDensity = density;
	m_fogColor[0] = r;
	m_fogColor[1] = g;
	m_fogColor[2] = b;
	m_fogColor[3] = a;
	InvalidateFog3D();
}


void Renderer3D::EnableOrientationFog( float r, float g, float b, float a, float camX, float camY, float camZ )
{
	m_fogEnabled = true;
	m_fogOrientationBased = true;
	m_fogColor[0] = r;
	m_fogColor[1] = g;
	m_fogColor[2] = b;
	m_fogColor[3] = a;
	m_cameraPos[0] = camX;
	m_cameraPos[1] = camY;
	m_cameraPos[2] = camZ;
	InvalidateFog3D();
}


void Renderer3D::DisableFog()
{
	m_fogEnabled = false;
	InvalidateFog3D();
}


void Renderer3D::SetCameraPosition( float x, float y, float z )
{
	m_cameraPos[0] = x;
	m_cameraPos[1] = y;
	m_cameraPos[2] = z;
	InvalidateFog3D();
}


void Renderer3D::CreateSurfaceAlignedBillboard( const Vector3<float> &position, float width, float height,
												Vertex3DTextured *vertices, float u1, float v1, float u2, float v2,
												float r, float g, float b, float a, float rotation )
{

	//
	// Create billboard that lays flat

	Vector3<float> normal = position;
	normal.Normalise();

	//
	// Create consistent up direction relative surface

	Vector3<float> up = Vector3<float>( 0.0f, 1.0f, 0.0f );

	Vector3<float> tangent1;
	if ( fabsf( normal.y ) > 0.98f )
	{
		tangent1 = Vector3<float>( 1.0f, 0.0f, 0.0f );
	}
	else
	{
		tangent1 = up ^ normal;
		tangent1.Normalise();
	}

	//
	// Create tangent pointing north

	Vector3<float> tangent2 = normal ^ tangent1;

	//
	// Apply rotation if specified

	if ( rotation != 0.0f )
	{
		float c = cosf( rotation );
		float s = sinf( rotation );

		Vector3<float> rotatedTangent1 = tangent1 * c + tangent2 * s;
		Vector3<float> rotatedTangent2 = tangent2 * c - tangent1 * s;
		tangent1 = rotatedTangent1;
		tangent2 = rotatedTangent2;
	}

	float halfWidth = width * 0.5f;
	float halfHeight = height * 0.5f;

	// First triangle: TL, TR, BR
	vertices[0] = Vertex3DTextured(
		position.x - tangent1.x * halfWidth + tangent2.x * halfHeight,
		position.y - tangent1.y * halfWidth + tangent2.y * halfHeight,
		position.z - tangent1.z * halfWidth + tangent2.z * halfHeight,
		normal.x, normal.y, normal.z,
		r, g, b, a, u1, v2 );
	vertices[1] = Vertex3DTextured(
		position.x + tangent1.x * halfWidth + tangent2.x * halfHeight,
		position.y + tangent1.y * halfWidth + tangent2.y * halfHeight,
		position.z + tangent1.z * halfWidth + tangent2.z * halfHeight,
		normal.x, normal.y, normal.z,
		r, g, b, a, u2, v2 );
	vertices[2] = Vertex3DTextured(
		position.x + tangent1.x * halfWidth - tangent2.x * halfHeight,
		position.y + tangent1.y * halfWidth - tangent2.y * halfHeight,
		position.z + tangent1.z * halfWidth - tangent2.z * halfHeight,
		normal.x, normal.y, normal.z,
		r, g, b, a, u2, v1 );

	// Second triangle: TL, BR, BL
	vertices[3] = vertices[0]; // TL
	vertices[4] = vertices[2]; // BR
	vertices[5] = Vertex3DTextured(
		position.x - tangent1.x * halfWidth - tangent2.x * halfHeight,
		position.y - tangent1.y * halfWidth - tangent2.y * halfHeight,
		position.z - tangent1.z * halfWidth - tangent2.z * halfHeight,
		normal.x, normal.y, normal.z,
		r, g, b, a, u1, v1 );
}


void Renderer3D::CreateCameraFacingBillboard( const Vector3<float> &position, float width, float height,
											  Vertex3DTextured *vertices, float u1, float v1, float u2, float v2,
											  float r, float g, float b, float a, float rotation )
{

	//
	// Create billboard that faces the camera

	Vector3<float> cameraPos( m_cameraPos[0], m_cameraPos[1], m_cameraPos[2] );
	Vector3<float> cameraDir = cameraPos - position;
	cameraDir.Normalise();

	//
	// Create billboard facing camera

	Vector3<float> up = Vector3<float>( 0.0f, 1.0f, 0.0f );
	Vector3<float> right = up ^ cameraDir;
	right.Normalise();
	up = cameraDir ^ right;
	up.Normalise();

	if ( rotation != 0.0f )
	{
		float cosRot = cosf( rotation );
		float sinRot = sinf( rotation );
		Vector3<float> rotatedRight = right * cosRot + up * sinRot;
		Vector3<float> rotatedUp = -right * sinRot + up * cosRot;
		right = rotatedRight;
		up = rotatedUp;
	}

	float halfWidth = width * 0.5f;
	float halfHeight = height * 0.5f;

	// First triangle: TL, TR, BR
	vertices[0] = Vertex3DTextured(
		position.x - right.x * halfWidth + up.x * halfHeight,
		position.y - right.y * halfWidth + up.y * halfHeight,
		position.z - right.z * halfWidth + up.z * halfHeight,
		cameraDir.x, cameraDir.y, cameraDir.z,
		r, g, b, a, u1, v2 );
	vertices[1] = Vertex3DTextured(
		position.x + right.x * halfWidth + up.x * halfHeight,
		position.y + right.y * halfWidth + up.y * halfHeight,
		position.z + right.z * halfWidth + up.z * halfHeight,
		cameraDir.x, cameraDir.y, cameraDir.z,
		r, g, b, a, u2, v2 );
	vertices[2] = Vertex3DTextured(
		position.x + right.x * halfWidth - up.x * halfHeight,
		position.y + right.y * halfWidth - up.y * halfHeight,
		position.z + right.z * halfWidth - up.z * halfHeight,
		cameraDir.x, cameraDir.y, cameraDir.z,
		r, g, b, a, u2, v1 );

	// Second triangle: TL, BR, BL
	vertices[3] = vertices[0]; // TL
	vertices[4] = vertices[2]; // BR
	vertices[5] = Vertex3DTextured(
		position.x - right.x * halfWidth - up.x * halfHeight,
		position.y - right.y * halfWidth - up.y * halfHeight,
		position.z - right.z * halfWidth - up.z * halfHeight,
		cameraDir.x, cameraDir.y, cameraDir.z,
		r, g, b, a, u1, v1 );
}


void Renderer3D::CalculateSphericalTangents( const Vector3<float> &position, Vector3<float> &outEast, Vector3<float> &outNorth )
{
	Vector3<float> normal = position;
	normal.Normalise();

	Vector3<float> worldNorth( 0.0f, 1.0f, 0.0f );

	if ( fabsf( normal.y ) > 0.999f )
	{
		outEast = Vector3<float>( 1.0f, 0.0f, 0.0f );
	}
	else
	{
		outEast = worldNorth ^ normal;
		outEast.Normalise();
	}

	outNorth = normal ^ outEast;
	outNorth.Normalise();
}


//
// Increment draw calls for the 3d renderer

void Renderer3D::IncrementDrawCall3D( DrawCallType type )
{
	m_drawCallsPerFrame3D++;

	switch ( type )
	{
		case DRAW_CALL_IMMEDIATE_VERTICES_3D:
			m_immediateVertexCalls3D++;
			break;
		case DRAW_CALL_IMMEDIATE_TRIANGLES:
			m_immediateTriangleCalls3D++;
			break;
		case DRAW_CALL_IMMEDIATE_LINES:
			m_immediateLineCalls3D++;
			break;
		case DRAW_CALL_IMMEDIATE_TEXT:
			m_immediateTextCalls3D++;
			break;
		case DRAW_CALL_IMMEDIATE_STATIC_SPRITES:
			m_immediateStaticSpriteCalls3D++;
			break;
		case DRAW_CALL_IMMEDIATE_ROTATING_SPRITES:
			m_immediateRotatingSpriteCalls3D++;
			break;
		case DRAW_CALL_IMMEDIATE_CIRCLES:
			m_immediateCircleCalls3D++;
			break;
		case DRAW_CALL_IMMEDIATE_CIRCLE_FILLS:
			m_immediateCircleFillCalls3D++;
			break;
		case DRAW_CALL_IMMEDIATE_RECTS:
			m_immediateRectCalls3D++;
			break;
		case DRAW_CALL_IMMEDIATE_RECT_FILLS:
			m_immediateRectFillCalls3D++;
			break;
		case DRAW_CALL_IMMEDIATE_TRIANGLE_FILLS:
			m_immediateTriangleFillCalls3D++;
			break;
		case DRAW_CALL_TEXT:
			m_textCalls3D++;
			break;
		case DRAW_CALL_LINES:
			m_lineCalls3D++;
			break;
		case DRAW_CALL_STATIC_SPRITES:
			m_staticSpriteCalls3D++;
			break;
		case DRAW_CALL_ROTATING_SPRITES:
			m_rotatingSpriteCalls3D++;
			break;
		case DRAW_CALL_CIRCLES:
			m_circleCalls3D++;
			break;
		case DRAW_CALL_CIRCLE_FILLS:
			m_circleFillCalls3D++;
			break;
		case DRAW_CALL_RECTS:
			m_rectCalls3D++;
			break;
		case DRAW_CALL_RECT_FILLS:
			m_rectFillCalls3D++;
			break;
		case DRAW_CALL_TRIANGLE_FILLS:
			m_triangleFillCalls3D++;
			break;
		case DRAW_CALL_LINE_VBO:
			m_lineVBOCalls3D++;
			break;
		case DRAW_CALL_QUAD_VBO:
			m_quadVBOCalls3D++;
			break;
		case DRAW_CALL_TRIANGLE_VBO:
			m_triangleVBOCalls3D++;
			break;
		default:
			break;
	}
}


void Renderer3D::ResetFrameCounters3D()
{
	m_prevDrawCallsPerFrame3D = m_drawCallsPerFrame3D;
	m_prevImmediateVertexCalls3D = m_immediateVertexCalls3D;
	m_prevImmediateTriangleCalls3D = m_immediateTriangleCalls3D;
	m_prevImmediateLineCalls3D = m_immediateLineCalls3D;
	m_prevImmediateTextCalls3D = m_immediateTextCalls3D;
	m_prevImmediateStaticSpriteCalls3D = m_immediateStaticSpriteCalls3D;
	m_prevImmediateRotatingSpriteCalls3D = m_immediateRotatingSpriteCalls3D;
	m_prevImmediateCircleCalls3D = m_immediateCircleCalls3D;
	m_prevImmediateCircleFillCalls3D = m_immediateCircleFillCalls3D;
	m_prevImmediateRectCalls3D = m_immediateRectCalls3D;
	m_prevImmediateRectFillCalls3D = m_immediateRectFillCalls3D;
	m_prevImmediateTriangleFillCalls3D = m_immediateTriangleFillCalls3D;
	m_prevLineCalls3D = m_lineCalls3D;
	m_prevStaticSpriteCalls3D = m_staticSpriteCalls3D;
	m_prevRotatingSpriteCalls3D = m_rotatingSpriteCalls3D;
	m_prevTextCalls3D = m_textCalls3D;
	m_prevMegaVBOCalls3D = m_megaVBOCalls3D;
	m_prevCircleCalls3D = m_circleCalls3D;
	m_prevCircleFillCalls3D = m_circleFillCalls3D;
	m_prevRectCalls3D = m_rectCalls3D;
	m_prevRectFillCalls3D = m_rectFillCalls3D;
	m_prevTriangleFillCalls3D = m_triangleFillCalls3D;
	m_prevLineVBOCalls3D = m_lineVBOCalls3D;
	m_prevQuadVBOCalls3D = m_quadVBOCalls3D;
	m_prevTriangleVBOCalls3D = m_triangleVBOCalls3D;
	m_prevActiveFontBatches3D = m_activeFontBatches3D;

	//
	// Reset current frame counters

	m_drawCallsPerFrame3D = 0;
	m_immediateVertexCalls3D = 0;
	m_immediateTriangleCalls3D = 0;
	m_immediateLineCalls3D = 0;
	m_immediateTextCalls3D = 0;
	m_immediateStaticSpriteCalls3D = 0;
	m_immediateRotatingSpriteCalls3D = 0;
	m_immediateCircleCalls3D = 0;
	m_immediateCircleFillCalls3D = 0;
	m_immediateRectCalls3D = 0;
	m_immediateRectFillCalls3D = 0;
	m_immediateTriangleFillCalls3D = 0;
	m_lineCalls3D = 0;
	m_staticSpriteCalls3D = 0;
	m_rotatingSpriteCalls3D = 0;
	m_textCalls3D = 0;
	m_megaVBOCalls3D = 0;
	m_circleCalls3D = 0;
	m_circleFillCalls3D = 0;
	m_rectCalls3D = 0;
	m_rectFillCalls3D = 0;
	m_triangleFillCalls3D = 0;
	m_lineVBOCalls3D = 0;
	m_quadVBOCalls3D = 0;
	m_triangleVBOCalls3D = 0;
	m_activeFontBatches3D = 0;
}


void Renderer3D::BeginFrame3D()
{
	ResetFrameCounters3D();
}


void Renderer3D::EndFrame3D()
{
}
