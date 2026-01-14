#include "systemiv.h"

#include <stdarg.h>

#include "lib/preferences.h"

#include "lib/render3d/renderer_3d.h"
#include "lib/render/renderer.h"
#include "lib/render2d/renderer_2d.h"

extern Renderer3D *g_renderer3d;

// ============================================================================
// BEGIN SCENES
// ============================================================================

void Renderer3D::BeginLineBatch3D()
{
	m_lineVertexCount3D = 0;
	m_currentLineWidth3D = 1.0f;
}


void Renderer3D::BeginStaticSpriteBatch3D()
{
	m_staticSpriteVertexCount3D = 0;
	m_currentStaticSpriteTexture3D = 0;
}


void Renderer3D::BeginRotatingSpriteBatch3D()
{
	m_rotatingSpriteVertexCount3D = 0;
	m_currentRotatingSpriteTexture3D = 0;
}


void Renderer3D::BeginTextBatch3D()
{
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


void Renderer3D::BeginCircleBatch3D()
{
	m_circleVertexCount3D = 0;
	m_currentCircleWidth3D = 1.0f;
}


void Renderer3D::BeginCircleFillBatch3D()
{
	m_circleFillVertexCount3D = 0;
}


void Renderer3D::BeginRectBatch3D()
{
	m_rectVertexCount3D = 0;
	m_currentRectWidth3D = 1.0f;
}


void Renderer3D::BeginRectFillBatch3D()
{
	m_rectFillVertexCount3D = 0;
}


void Renderer3D::BeginTriangleFillBatch3D()
{
	m_triangleFillVertexCount3D = 0;
}


// ============================================================================
// END SCENES
// ============================================================================

void Renderer3D::EndLineBatch3D()
{
	if ( m_lineVertexCount3D > 0 )
	{
		FlushLine3D();
	}
}


void Renderer3D::EndStaticSpriteBatch3D()
{
	if ( m_staticSpriteVertexCount3D > 0 )
	{
		FlushStaticSprites3D();
	}
}


void Renderer3D::EndRotatingSpriteBatch3D()
{
	if ( m_rotatingSpriteVertexCount3D > 0 )
	{
		FlushRotatingSprite3D();
	}
}


void Renderer3D::EndTextBatch3D()
{
	int activeBatches = 0;

	for ( int i = 0; i < Renderer::MAX_FONT_ATLASES; i++ )
	{
		if ( m_fontBatches3D[i].vertexCount > 0 )
		{
			activeBatches++;

			m_textVertices3D = m_fontBatches3D[i].vertices;
			m_textVertexCount3D = m_fontBatches3D[i].vertexCount;
			m_currentTextTexture3D = m_fontBatches3D[i].textureID;

			FlushTextBuffer3D();

			m_fontBatches3D[i].vertexCount = 0;
		}
	}

	if ( activeBatches > m_activeFontBatches3D )
	{
		m_activeFontBatches3D = activeBatches;
	}

	m_currentFontBatchIndex3D = 0;
	m_textVertices3D = m_fontBatches3D[0].vertices;
	m_textVertexCount3D = 0;
	m_currentTextTexture3D = 0;
}


void Renderer3D::EndCircleBatch3D()
{
	if ( m_circleVertexCount3D > 0 )
	{
		FlushCircles3D();
	}
}


void Renderer3D::EndCircleFillBatch3D()
{
	if ( m_circleFillVertexCount3D > 0 )
	{
		FlushCircleFills3D();
	}
}


void Renderer3D::EndRectBatch3D()
{
	if ( m_rectVertexCount3D > 0 )
	{
		FlushRects3D();
	}
}


void Renderer3D::EndRectFillBatch3D()
{
	if ( m_rectFillVertexCount3D > 0 )
	{
		FlushRectFills3D();
	}
}


void Renderer3D::EndTriangleFillBatch3D()
{
	if ( m_triangleFillVertexCount3D > 0 )
	{
		FlushTriangleFills3D();
	}
}


void Renderer3D::FlushAllBatches3D()
{
	EndTextBatch3D();
	EndLineBatch3D();
	EndRectBatch3D();
	EndRectFillBatch3D();
	EndRotatingSpriteBatch3D();
	EndStaticSpriteBatch3D();
	EndCircleBatch3D();
	EndCircleFillBatch3D();
	EndTriangleFillBatch3D();
}


// ============================================================================
// FLUSH IF FULL
// ============================================================================

void Renderer3D::FlushLine3DIfFull( int segmentsNeeded )
{
	if ( m_lineVertexCount3D + segmentsNeeded > MAX_LINE_VERTICES_3D )
	{
		FlushLine3D();
	}
}


void Renderer3D::FlushStaticSprites3DIfFull( int verticesNeeded )
{
	if ( m_staticSpriteVertexCount3D + verticesNeeded > MAX_STATIC_SPRITE_VERTICES_3D )
	{
		FlushStaticSprites3D();
	}
}


void Renderer3D::FlushRotatingSprite3DIfFull( int verticesNeeded )
{
	if ( m_rotatingSpriteVertexCount3D + verticesNeeded > MAX_ROTATING_SPRITE_VERTICES_3D )
	{
		FlushRotatingSprite3D();
	}
}


void Renderer3D::FlushCircles3DIfFull( int verticesNeeded )
{
	if ( m_circleVertexCount3D + verticesNeeded > MAX_CIRCLE_VERTICES_3D )
	{
		FlushCircles3D();
	}
}


void Renderer3D::FlushCircleFills3DIfFull( int verticesNeeded )
{
	if ( m_circleFillVertexCount3D + verticesNeeded > MAX_CIRCLE_FILL_VERTICES_3D )
	{
		FlushCircleFills3D();
	}
}


void Renderer3D::FlushRects3DIfFull( int verticesNeeded )
{
	if ( m_rectVertexCount3D + verticesNeeded > MAX_RECT_VERTICES_3D )
	{
		FlushRects3D();
	}
}


void Renderer3D::FlushRectFills3DIfFull( int verticesNeeded )
{
	if ( m_rectFillVertexCount3D + verticesNeeded > MAX_RECT_FILL_VERTICES_3D )
	{
		FlushRectFills3D();
	}
}


void Renderer3D::FlushTriangleFills3DIfFull( int verticesNeeded )
{
	if ( m_triangleFillVertexCount3D + verticesNeeded > MAX_TRIANGLE_FILL_VERTICES_3D )
	{
		FlushTriangleFills3D();
	}
}