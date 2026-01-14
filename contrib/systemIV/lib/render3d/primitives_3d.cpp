#include "systemiv.h"

#include <stdarg.h>
#include <math.h>

#include "lib/resource/image.h"
#include "lib/render/colour.h"
#include "lib/math/vector2.h"

#include "lib/render3d/renderer_3d.h"

extern Renderer3D *g_renderer3d;
extern Renderer *g_renderer;

// ============================================================================
// PRIMITIVE RENDERING FUNCTIONS
// ============================================================================

void Renderer3D::Line3D( float x1, float y1, float z1, float x2, float y2, float z2, Colour const &col, float lineWidth, bool immediateFlush )
{
	FlushLine3DIfFull( 2 );

	if ( m_lineVertexCount3D > 0 && m_currentLineWidth3D != lineWidth )
	{
		FlushLine3D();
	}

#ifndef TARGET_EMSCRIPTEN
	g_renderer->SetLineWidth( lineWidth );
#endif

	m_currentLineWidth3D = lineWidth;

	float r = col.GetRFloat(), g = col.GetGFloat(), b = col.GetBFloat(), a = col.GetAFloat();

	m_lineVertices3D[m_lineVertexCount3D++] = { x1, y1, z1, r, g, b, a };
	m_lineVertices3D[m_lineVertexCount3D++] = { x2, y2, z2, r, g, b, a };

#if IMMEDIATE_MODE_3D
	FlushLine3D();
#else
	if ( immediateFlush )
	{
		FlushLine3D();
	}
#endif
}


void Renderer3D::Circle3D( float x, float y, float z, float radius, int numPoints, Colour const &col, float lineWidth, bool immediateFlush )
{
	FlushCircles3DIfFull( numPoints * 2 );

	if ( m_circleVertexCount3D > 0 && m_currentCircleWidth3D != lineWidth )
	{
		FlushCircles3D();
	}

#ifndef TARGET_EMSCRIPTEN
	g_renderer->SetLineWidth( lineWidth );
#endif

	m_currentCircleWidth3D = lineWidth;

	float r = col.GetRFloat(), g = col.GetGFloat(), b = col.GetBFloat(), a = col.GetAFloat();

	for ( int i = 0; i < numPoints; ++i )
	{
		float angle1 = 2.0f * M_PI * (float)i / (float)numPoints;
		float angle2 = 2.0f * M_PI * (float)( i + 1 ) / (float)numPoints;

		float x1 = x + cosf( angle1 ) * radius;
		float y1 = y + sinf( angle1 ) * radius;
		float x2 = x + cosf( angle2 ) * radius;
		float y2 = y + sinf( angle2 ) * radius;

		m_circleVertices3D[m_circleVertexCount3D++] = { x1, y1, z, r, g, b, a };
		m_circleVertices3D[m_circleVertexCount3D++] = { x2, y2, z, r, g, b, a };
	}

#if IMMEDIATE_MODE_3D
	FlushCircles3D();
#else
	if ( immediateFlush )
	{
		FlushCircles3D();
	}
#endif
}


void Renderer3D::Circle3D( const Vector3<float> &pos, const Vector3<float> &tangent1, const Vector3<float> &tangent2, float radius, int numPoints, Colour const &col, float lineWidth, bool immediateFlush )
{
	FlushCircles3DIfFull( numPoints * 2 );

	if ( m_circleVertexCount3D > 0 && m_currentCircleWidth3D != lineWidth )
	{
		FlushCircles3D();
	}

#ifndef TARGET_EMSCRIPTEN
	g_renderer->SetLineWidth( lineWidth );
#endif

	m_currentCircleWidth3D = lineWidth;

	float r = col.GetRFloat(), g = col.GetGFloat(), b = col.GetBFloat(), a = col.GetAFloat();

	for ( int i = 0; i < numPoints; ++i )
	{
		float angle1 = 2.0f * M_PI * (float)i / (float)numPoints;
		float angle2 = 2.0f * M_PI * (float)( i + 1 ) / (float)numPoints;

		float cos1 = cosf( angle1 );
		float sin1 = sinf( angle1 );
		float cos2 = cosf( angle2 );
		float sin2 = sinf( angle2 );

		Vector3<float> offset1 = tangent1 * ( cos1 * radius ) + tangent2 * ( sin1 * radius );
		Vector3<float> offset2 = tangent1 * ( cos2 * radius ) + tangent2 * ( sin2 * radius );

		Vector3<float> p1 = pos + offset1;
		Vector3<float> p2 = pos + offset2;

		m_circleVertices3D[m_circleVertexCount3D++] = { p1.x, p1.y, p1.z, r, g, b, a };
		m_circleVertices3D[m_circleVertexCount3D++] = { p2.x, p2.y, p2.z, r, g, b, a };
	}

#if IMMEDIATE_MODE_3D
	FlushCircles3D();
#else
	if ( immediateFlush )
	{
		FlushCircles3D();
	}
#endif
}


void Renderer3D::CircleFill3D( float x, float y, float z, float radius, int numPoints, Colour const &col, bool immediateFlush )
{
	FlushCircleFills3DIfFull( numPoints * 3 );

	float r = col.GetRFloat(), g = col.GetGFloat(), b = col.GetBFloat(), a = col.GetAFloat();

	for ( int i = 0; i < numPoints; ++i )
	{
		float angle1 = 2.0f * M_PI * (float)i / (float)numPoints;
		float angle2 = 2.0f * M_PI * (float)( i + 1 ) / (float)numPoints;

		float x1 = x + cosf( angle1 ) * radius;
		float y1 = y + sinf( angle1 ) * radius;
		float x2 = x + cosf( angle2 ) * radius;
		float y2 = y + sinf( angle2 ) * radius;

		// triangle: center, point i, point i+1
		m_circleFillVertices3D[m_circleFillVertexCount3D++] = { x, y, z, r, g, b, a };
		m_circleFillVertices3D[m_circleFillVertexCount3D++] = { x1, y1, z, r, g, b, a };
		m_circleFillVertices3D[m_circleFillVertexCount3D++] = { x2, y2, z, r, g, b, a };
	}

#if IMMEDIATE_MODE_3D
	FlushCircleFills3D();
#else
	if ( immediateFlush )
	{
		FlushCircleFills3D();
	}
#endif
}


void Renderer3D::CircleFill3D( const Vector3<float> &pos, const Vector3<float> &tangent1, const Vector3<float> &tangent2, float radius, int numPoints, Colour const &col, bool immediateFlush )
{
	FlushCircleFills3DIfFull( numPoints * 3 );

	float r = col.GetRFloat(), g = col.GetGFloat(), b = col.GetBFloat(), a = col.GetAFloat();

	for ( int i = 0; i < numPoints; ++i )
	{
		float angle1 = 2.0f * M_PI * (float)i / (float)numPoints;
		float angle2 = 2.0f * M_PI * (float)( i + 1 ) / (float)numPoints;

		float cos1 = cosf( angle1 );
		float sin1 = sinf( angle1 );
		float cos2 = cosf( angle2 );
		float sin2 = sinf( angle2 );

		Vector3<float> offset1 = tangent1 * ( cos1 * radius ) + tangent2 * ( sin1 * radius );
		Vector3<float> offset2 = tangent1 * ( cos2 * radius ) + tangent2 * ( sin2 * radius );

		Vector3<float> p1 = pos + offset1;
		Vector3<float> p2 = pos + offset2;

		// triangle: center, point i, point i+1
		m_circleFillVertices3D[m_circleFillVertexCount3D++] = { pos.x, pos.y, pos.z, r, g, b, a };
		m_circleFillVertices3D[m_circleFillVertexCount3D++] = { p1.x, p1.y, p1.z, r, g, b, a };
		m_circleFillVertices3D[m_circleFillVertexCount3D++] = { p2.x, p2.y, p2.z, r, g, b, a };
	}

#if IMMEDIATE_MODE_3D
	FlushCircleFills3D();
#else
	if ( immediateFlush )
	{
		FlushCircleFills3D();
	}
#endif
}


void Renderer3D::Rect3D( float x, float y, float z, float w, float h, Colour const &col, float lineWidth, bool immediateFlush )
{
	FlushRects3DIfFull( 8 );

	if ( m_rectVertexCount3D > 0 && m_currentRectWidth3D != lineWidth )
	{
		FlushRects3D();
	}

#ifndef TARGET_EMSCRIPTEN
	g_renderer->SetLineWidth( lineWidth );
#endif

	m_currentRectWidth3D = lineWidth;

	float r = col.GetRFloat(), g = col.GetGFloat(), b = col.GetBFloat(), a = col.GetAFloat();

	// top line
	m_rectVertices3D[m_rectVertexCount3D++] = { x, y, z, r, g, b, a };
	m_rectVertices3D[m_rectVertexCount3D++] = { x + w, y, z, r, g, b, a };

	// right line
	m_rectVertices3D[m_rectVertexCount3D++] = { x + w, y, z, r, g, b, a };
	m_rectVertices3D[m_rectVertexCount3D++] = { x + w, y + h, z, r, g, b, a };

	// bottom line
	m_rectVertices3D[m_rectVertexCount3D++] = { x + w, y + h, z, r, g, b, a };
	m_rectVertices3D[m_rectVertexCount3D++] = { x, y + h, z, r, g, b, a };

	// left line
	m_rectVertices3D[m_rectVertexCount3D++] = { x, y + h, z, r, g, b, a };
	m_rectVertices3D[m_rectVertexCount3D++] = { x, y, z, r, g, b, a };

#if IMMEDIATE_MODE_3D
	FlushRects3D();
#else
	if ( immediateFlush )
	{
		FlushRects3D();
	}
#endif
}


void Renderer3D::Rect3D( const Vector3<float> &pos, const Vector3<float> &tangent1, const Vector3<float> &tangent2, float w, float h, Colour const &col, float lineWidth, bool immediateFlush )
{
	FlushRects3DIfFull( 8 );

	if ( m_rectVertexCount3D > 0 && m_currentRectWidth3D != lineWidth )
	{
		FlushRects3D();
	}

#ifndef TARGET_EMSCRIPTEN
	g_renderer->SetLineWidth( lineWidth );
#endif

	m_currentRectWidth3D = lineWidth;

	float r = col.GetRFloat(), g = col.GetGFloat(), b = col.GetBFloat(), a = col.GetAFloat();

	float halfW = w * 0.5f;
	float halfH = h * 0.5f;

	Vector3<float> topLeft = pos - tangent1 * halfW + tangent2 * halfH;
	Vector3<float> topRight = pos + tangent1 * halfW + tangent2 * halfH;
	Vector3<float> bottomRight = pos + tangent1 * halfW - tangent2 * halfH;
	Vector3<float> bottomLeft = pos - tangent1 * halfW - tangent2 * halfH;

	// top line
	m_rectVertices3D[m_rectVertexCount3D++] = { topLeft.x, topLeft.y, topLeft.z, r, g, b, a };
	m_rectVertices3D[m_rectVertexCount3D++] = { topRight.x, topRight.y, topRight.z, r, g, b, a };

	// right line
	m_rectVertices3D[m_rectVertexCount3D++] = { topRight.x, topRight.y, topRight.z, r, g, b, a };
	m_rectVertices3D[m_rectVertexCount3D++] = { bottomRight.x, bottomRight.y, bottomRight.z, r, g, b, a };

	// bottom line
	m_rectVertices3D[m_rectVertexCount3D++] = { bottomRight.x, bottomRight.y, bottomRight.z, r, g, b, a };
	m_rectVertices3D[m_rectVertexCount3D++] = { bottomLeft.x, bottomLeft.y, bottomLeft.z, r, g, b, a };

	// left line
	m_rectVertices3D[m_rectVertexCount3D++] = { bottomLeft.x, bottomLeft.y, bottomLeft.z, r, g, b, a };
	m_rectVertices3D[m_rectVertexCount3D++] = { topLeft.x, topLeft.y, topLeft.z, r, g, b, a };

#if IMMEDIATE_MODE_3D
	FlushRects3D();
#else
	if ( immediateFlush )
	{
		FlushRects3D();
	}
#endif
}


void Renderer3D::RectFill3D( float x, float y, float z, float w, float h, Colour const &col, bool immediateFlush )
{
	FlushRectFills3DIfFull( 6 );

	float r = col.GetRFloat(), g = col.GetGFloat(), b = col.GetBFloat(), a = col.GetAFloat();

	// first triangle: TL, TR, BR
	m_rectFillVertices3D[m_rectFillVertexCount3D++] = { x, y, z, r, g, b, a };
	m_rectFillVertices3D[m_rectFillVertexCount3D++] = { x + w, y, z, r, g, b, a };
	m_rectFillVertices3D[m_rectFillVertexCount3D++] = { x + w, y + h, z, r, g, b, a };

	// second triangle: TL, BR, BL
	m_rectFillVertices3D[m_rectFillVertexCount3D++] = { x, y, z, r, g, b, a };
	m_rectFillVertices3D[m_rectFillVertexCount3D++] = { x + w, y + h, z, r, g, b, a };
	m_rectFillVertices3D[m_rectFillVertexCount3D++] = { x, y + h, z, r, g, b, a };

#if IMMEDIATE_MODE_3D
	FlushRectFills3D();
#else
	if ( immediateFlush )
	{
		FlushRectFills3D();
	}
#endif
}


void Renderer3D::RectFill3D( const Vector3<float> &pos, const Vector3<float> &tangent1, const Vector3<float> &tangent2, float w, float h, Colour const &col, bool immediateFlush )
{
	FlushRectFills3DIfFull( 6 );

	float r = col.GetRFloat(), g = col.GetGFloat(), b = col.GetBFloat(), a = col.GetAFloat();

	float halfW = w * 0.5f;
	float halfH = h * 0.5f;

	Vector3<float> topLeft = pos - tangent1 * halfW + tangent2 * halfH;
	Vector3<float> topRight = pos + tangent1 * halfW + tangent2 * halfH;
	Vector3<float> bottomRight = pos + tangent1 * halfW - tangent2 * halfH;
	Vector3<float> bottomLeft = pos - tangent1 * halfW - tangent2 * halfH;

	// first triangle: TL, TR, BR
	m_rectFillVertices3D[m_rectFillVertexCount3D++] = { topLeft.x, topLeft.y, topLeft.z, r, g, b, a };
	m_rectFillVertices3D[m_rectFillVertexCount3D++] = { topRight.x, topRight.y, topRight.z, r, g, b, a };
	m_rectFillVertices3D[m_rectFillVertexCount3D++] = { bottomRight.x, bottomRight.y, bottomRight.z, r, g, b, a };

	// second triangle: TL, BR, BL
	m_rectFillVertices3D[m_rectFillVertexCount3D++] = { topLeft.x, topLeft.y, topLeft.z, r, g, b, a };
	m_rectFillVertices3D[m_rectFillVertexCount3D++] = { bottomRight.x, bottomRight.y, bottomRight.z, r, g, b, a };
	m_rectFillVertices3D[m_rectFillVertexCount3D++] = { bottomLeft.x, bottomLeft.y, bottomLeft.z, r, g, b, a };

#if IMMEDIATE_MODE_3D
	FlushRectFills3D();
#else
	if ( immediateFlush )
	{
		FlushRectFills3D();
	}
#endif
}


void Renderer3D::TriangleFill3D( float x1, float y1, float z1, float x2, float y2, float z2, float x3, float y3, float z3, Colour const &col, bool immediateFlush )
{
	FlushTriangleFills3DIfFull( 3 );

	float r = col.GetRFloat(), g = col.GetGFloat(), b = col.GetBFloat(), a = col.GetAFloat();

	m_triangleFillVertices3D[m_triangleFillVertexCount3D++] = { x1, y1, z1, r, g, b, a };
	m_triangleFillVertices3D[m_triangleFillVertexCount3D++] = { x2, y2, z2, r, g, b, a };
	m_triangleFillVertices3D[m_triangleFillVertexCount3D++] = { x3, y3, z3, r, g, b, a };

#if IMMEDIATE_MODE_3D
	FlushTriangleFills3D();
#else
	if ( immediateFlush )
	{
		FlushTriangleFills3D();
	}
#endif
}


void Renderer3D::TriangleFill3D( const Vector3<float> &pos, const Vector3<float> &tangent1, const Vector3<float> &tangent2, const Vector2 &v1Offset, const Vector2 &v2Offset, const Vector2 &v3Offset, Colour const &col, bool immediateFlush )
{
	FlushTriangleFills3DIfFull( 3 );

	float r = col.GetRFloat(), g = col.GetGFloat(), b = col.GetBFloat(), a = col.GetAFloat();

	Vector3<float> v1 = pos + tangent1 * v1Offset.x + tangent2 * v1Offset.y;
	Vector3<float> v2 = pos + tangent1 * v2Offset.x + tangent2 * v2Offset.y;
	Vector3<float> v3 = pos + tangent1 * v3Offset.x + tangent2 * v3Offset.y;

	m_triangleFillVertices3D[m_triangleFillVertexCount3D++] = { v1.x, v1.y, v1.z, r, g, b, a };
	m_triangleFillVertices3D[m_triangleFillVertexCount3D++] = { v2.x, v2.y, v2.z, r, g, b, a };
	m_triangleFillVertices3D[m_triangleFillVertexCount3D++] = { v3.x, v3.y, v3.z, r, g, b, a };

#if IMMEDIATE_MODE_3D
	FlushTriangleFills3D();
#else
	if ( immediateFlush )
	{
		FlushTriangleFills3D();
	}
#endif
}