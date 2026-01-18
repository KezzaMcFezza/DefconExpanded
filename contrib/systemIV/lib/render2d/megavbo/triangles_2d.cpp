#include "systemiv.h"

#include "lib/debug/debug_utils.h"
#include "lib/string_utils.h"
#include "lib/render/renderer.h"
#include "lib/render2d/renderer_2d.h"
#include "lib/render2d/megavbo/megavbo_2d.h"

void MegaVBO2D::BeginTriangleMegaVBO( const char *megaVBOKey, Colour const &col )
{
	CachedVBO *cachedVBO = m_cachedVBOs.GetData( megaVBOKey, nullptr );
	if ( cachedVBO && cachedVBO->isValid )
	{
		return;
	}

	if ( m_currentMegaVBOTrianglesKey )
	{
		delete[] m_currentMegaVBOTrianglesKey;
	}
	m_currentMegaVBOTrianglesKey = newStr( megaVBOKey );

	m_megaVBOTrianglesActive = true;
	m_megaVBOTrianglesColor = col;

	m_megaTriangleVertexCount = 0;
	m_megaTriangleIndexCount = 0;
}


void MegaVBO2D::AddTrianglesToMegaVBO( const float *vertices, int vertexCount )
{
	if ( !m_megaVBOTrianglesActive || vertexCount < 3 )
		return;

	if ( m_megaTriangleVertexCount + vertexCount > m_maxMegaTriangleVertices ||
		 m_megaTriangleIndexCount + vertexCount > m_maxMegaTriangleIndices )
	{
		AppDebugOut( "Warning: 2D Triangle MegaVBO overflow: Vertices: %d/%d, Indices: %d/%d\n",
					 m_megaTriangleVertexCount + vertexCount, m_maxMegaTriangleVertices,
					 m_megaTriangleIndexCount + vertexCount, m_maxMegaTriangleIndices );
		return;
	}

	//
	// Convert color to float

	float r = m_megaVBOTrianglesColor.m_r / 255.0f, g = m_megaVBOTrianglesColor.m_g / 255.0f;
	float b = m_megaVBOTrianglesColor.m_b / 255.0f, a = m_megaVBOTrianglesColor.m_a / 255.0f;

	//
	// Store starting vertex index for these triangles

	unsigned int startIndex = m_megaTriangleVertexCount;

	//
	// Add vertices

	for ( int i = 0; i < vertexCount; i++ )
	{
		m_megaTriangleVertices[m_megaTriangleVertexCount++] = {
			vertices[i * 2], vertices[i * 2 + 1],
			r, g, b, a,
			0.0f, 0.0f };
	}

	//
	// Add indices (3 per triangle)

	for ( int i = 0; i < vertexCount; i++ )
	{
		m_megaTriangleIndices[m_megaTriangleIndexCount++] = startIndex + i;
	}
}


void MegaVBO2D::EndTriangleMegaVBO()
{
	if ( !m_megaVBOTrianglesActive || !m_currentMegaVBOTrianglesKey )
		return;

	if ( m_megaTriangleVertexCount < 3 )
	{
		m_megaVBOTrianglesActive = false;
		return;
	}

	//
	// Create or get cached Mega-VBO

	CachedVBO *cachedVBO = m_cachedVBOs.GetData( m_currentMegaVBOTrianglesKey, nullptr );
	if ( !cachedVBO )
	{
		cachedVBO = new CachedVBO();
		cachedVBO->VBO = 0;
		cachedVBO->VAO = 0;
		cachedVBO->IBO = 0;
		cachedVBO->vertexCount = 0;
		cachedVBO->indexCount = 0;
		cachedVBO->vertexType = VBO_VERTEX_2D;
		cachedVBO->isValid = false;
		m_cachedVBOs.PutData( m_currentMegaVBOTrianglesKey, cachedVBO );
	}

	bool isNewVBO = ( cachedVBO->VBO == 0 );
	if ( isNewVBO )
	{
		cachedVBO->VAO = g_renderer2d->CreateMegaVBOVertexArray();
		cachedVBO->VBO = g_renderer2d->CreateMegaVBOVertexBuffer( m_megaTriangleVertexCount * sizeof( Vertex2D ), BUFFER_USAGE_STATIC_DRAW );
		cachedVBO->IBO = g_renderer2d->CreateMegaVBOIndexBuffer( m_megaTriangleIndexCount * sizeof( unsigned int ), BUFFER_USAGE_STATIC_DRAW );

		g_renderer2d->SetupMegaVBOVertexAttributes2D( cachedVBO->VAO, cachedVBO->VBO, cachedVBO->IBO );
	}
	else
	{
		g_renderer->SetVertexArray( cachedVBO->VAO );
		g_renderer->SetArrayBuffer( cachedVBO->VBO );
	}

	g_renderer->SetVertexArray( cachedVBO->VAO );
	g_renderer2d->UploadMegaVBOVertexData( cachedVBO->VBO, m_megaTriangleVertices, m_megaTriangleVertexCount, BUFFER_USAGE_STATIC_DRAW );
	g_renderer2d->UploadMegaVBOIndexData( cachedVBO->IBO, m_megaTriangleIndices, m_megaTriangleIndexCount, BUFFER_USAGE_STATIC_DRAW );

	cachedVBO->vertexCount = m_megaTriangleVertexCount;
	cachedVBO->indexCount = m_megaTriangleIndexCount;
	cachedVBO->color = m_megaVBOTrianglesColor;
	cachedVBO->vertexType = VBO_VERTEX_2D;
	cachedVBO->isValid = true;

	m_megaTriangleVertexCount = 0;
	m_megaTriangleIndexCount = 0;
	m_megaVBOTrianglesActive = false;
}


void MegaVBO2D::RenderTriangleMegaVBO( const char *megaVBOKey )
{
	CachedVBO *cachedVBO = m_cachedVBOs.GetData( megaVBOKey, nullptr );
	if ( !cachedVBO || !cachedVBO->isValid )
	{
		return;
	}

	g_renderer->StartFlushTiming( "MegaVBO_Triangles_2D" );

	g_renderer->SetShaderProgram( g_renderer2d->m_colorShaderProgram );
	g_renderer2d->SetColorShaderUniforms();

	g_renderer->SetVertexArray( cachedVBO->VAO );
	g_renderer2d->DrawMegaVBOIndexed( PRIMITIVE_TRIANGLES, cachedVBO->indexCount );

	g_renderer->EndFlushTiming( "MegaVBO_Triangles_2D" );
	g_renderer2d->IncrementDrawCall( DRAW_CALL_TRIANGLE_VBO );
}


bool MegaVBO2D::IsTriangleMegaVBOValid( const char *megaVBOKey )
{
	CachedVBO *cachedVBO = m_cachedVBOs.GetData( megaVBOKey, nullptr );
	return ( cachedVBO && cachedVBO->isValid );
}


void MegaVBO2D::SetTriangleMegaVBOBufferSizes( int vertexCount,
											   int indexCount,
											   const char *cacheKey )
{
	int newMaxVertices = (int)( vertexCount * 1.1f );
	int newMaxIndices = (int)( indexCount * 1.1f );

	if ( cacheKey )
	{
		InvalidateCachedVBO( cacheKey );
	}

	if ( m_megaTriangleVertices )
	{
		delete[] m_megaTriangleVertices;
		m_megaTriangleVertices = NULL;
	}
	if ( m_megaTriangleIndices )
	{
		delete[] m_megaTriangleIndices;
		m_megaTriangleIndices = NULL;
	}

	m_maxMegaTriangleVertices = newMaxVertices;
	m_maxMegaTriangleIndices = newMaxIndices;
	m_megaTriangleVertices = new Vertex2D[m_maxMegaTriangleVertices];
	m_megaTriangleIndices = new unsigned int[m_maxMegaTriangleIndices];
}
