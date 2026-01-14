#include "systemiv.h"

#include "lib/debug/debug_utils.h"
#include "lib/string_utils.h"
#include "lib/render/renderer.h"
#include "lib/render3d/renderer_3d.h"
#include "lib/render3d/megavbo/megavbo_3d.h"

void MegaVBO3D::BeginTexturedMegaVBO3D( const char *megaVBOKey, unsigned int textureID )
{
	Cached3DVBO *cachedVBO = m_cached3DVBOs.GetData( megaVBOKey, nullptr );
	if ( cachedVBO && cachedVBO->isValid )
	{
		return;
	}

	if ( m_currentMegaVBO3DTexturedKey )
	{
		delete[] m_currentMegaVBO3DTexturedKey;
	}

	m_currentMegaVBO3DTexturedKey = newStr( megaVBOKey );

	m_megaVBO3DTexturedActive = true;
	m_currentMegaVBO3DTextureID = textureID;

	m_megaTexturedVertex3DCount = 0;
	m_megaTexturedIndex3DCount = 0;
}


void MegaVBO3D::AddTexturedQuadsToMegaVBO3D( const Vertex3DTextured *vertices,
											 int vertexCount, int quadCount )
{
	if ( !m_megaVBO3DTexturedActive || vertexCount < 4 )
		return;

	int indicesNeeded = quadCount * 6;
	if ( m_megaTexturedVertex3DCount + vertexCount > m_maxMegaTexturedVertices3D ||
		 m_megaTexturedIndex3DCount + indicesNeeded > m_maxMegaTexturedIndices3D )
	{
		AppDebugOut( "Warning: 3D Textured MegaVBO overflow: Vertices: %d/%d, Indices: %d/%d\n",
					 m_megaTexturedVertex3DCount + vertexCount, m_maxMegaTexturedVertices3D,
					 m_megaTexturedIndex3DCount + indicesNeeded, m_maxMegaTexturedIndices3D );
		return;
	}

	//
	// Store starting vertex index for this set of quads

	unsigned int startIndex = m_megaTexturedVertex3DCount;

	//
	// Add vertices

	for ( int i = 0; i < vertexCount; i++ )
	{
		m_megaTexturedVertices3D[m_megaTexturedVertex3DCount] = vertices[i];
		m_megaTexturedVertex3DCount++;
	}

	//
	// Add indices for triangles (6 indices per quad = 2 triangles)

	for ( int q = 0; q < quadCount; q++ )
	{
		int baseIdx = startIndex + ( q * 4 );

		// First triangle
		m_megaTexturedIndices3D[m_megaTexturedIndex3DCount++] = baseIdx + 0;
		m_megaTexturedIndices3D[m_megaTexturedIndex3DCount++] = baseIdx + 1;
		m_megaTexturedIndices3D[m_megaTexturedIndex3DCount++] = baseIdx + 2;

		// Second triangle
		m_megaTexturedIndices3D[m_megaTexturedIndex3DCount++] = baseIdx + 0;
		m_megaTexturedIndices3D[m_megaTexturedIndex3DCount++] = baseIdx + 2;
		m_megaTexturedIndices3D[m_megaTexturedIndex3DCount++] = baseIdx + 3;
	}
}


void MegaVBO3D::AddTexturedTrianglesToMegaVBO3DWithIndices( const Vertex3DTextured *vertices,
															int vertexCount,
															const unsigned int *indices,
															int indexCount )
{
	if ( !m_megaVBO3DTexturedActive || vertexCount < 3 || indexCount < 3 )
		return;

	if ( m_megaTexturedVertex3DCount + vertexCount > m_maxMegaTexturedVertices3D ||
		 m_megaTexturedIndex3DCount + indexCount > m_maxMegaTexturedIndices3D )
	{
		AppDebugOut( "Warning: 3D Textured Triangle MegaVBO overflow: Vertices: %d/%d, Indices: %d/%d\n",
					 m_megaTexturedVertex3DCount + vertexCount, m_maxMegaTexturedVertices3D,
					 m_megaTexturedIndex3DCount + indexCount, m_maxMegaTexturedIndices3D );
		return;
	}

	unsigned int startIndex = m_megaTexturedVertex3DCount;

	for ( int i = 0; i < vertexCount; i++ )
	{
		m_megaTexturedVertices3D[m_megaTexturedVertex3DCount] = vertices[i];
		m_megaTexturedVertex3DCount++;
	}

	for ( int i = 0; i < indexCount; i++ )
	{
		m_megaTexturedIndices3D[m_megaTexturedIndex3DCount++] = startIndex + indices[i];
	}
}


void MegaVBO3D::EndTexturedMegaVBO3D()
{
	if ( !m_megaVBO3DTexturedActive || !m_currentMegaVBO3DTexturedKey )
		return;

	if ( m_megaTexturedVertex3DCount < 4 )
	{
		m_megaVBO3DTexturedActive = false;
		return;
	}

	//
	// Create or get cached Mega-VBO

	Cached3DVBO *cachedVBO = m_cached3DVBOs.GetData( m_currentMegaVBO3DTexturedKey, nullptr );
	if ( !cachedVBO )
	{
		cachedVBO = new Cached3DVBO();
		cachedVBO->VBO = 0;
		cachedVBO->VAO = 0;
		cachedVBO->IBO = 0;
		cachedVBO->vertexCount = 0;
		cachedVBO->indexCount = 0;
		cachedVBO->vertexType = VBO_VERTEX_3D_TEXTURED;
		cachedVBO->isValid = false;
		m_cached3DVBOs.PutData( m_currentMegaVBO3DTexturedKey, cachedVBO );
	}

	bool isNewVBO = ( cachedVBO->VBO == 0 );
	if ( isNewVBO )
	{
		cachedVBO->VAO = g_renderer3d->CreateMegaVBOVertexArray3D();
		cachedVBO->VBO = g_renderer3d->CreateMegaVBOVertexBuffer3D( m_megaTexturedVertex3DCount * sizeof( Vertex3DTextured ), BUFFER_USAGE_STATIC_DRAW );
		cachedVBO->IBO = g_renderer3d->CreateMegaVBOIndexBuffer3D( m_megaTexturedIndex3DCount * sizeof( unsigned int ), BUFFER_USAGE_STATIC_DRAW );

		g_renderer3d->SetupMegaVBOVertexAttributes3DTextured( cachedVBO->VAO, cachedVBO->VBO, cachedVBO->IBO );
	}
	else
	{
		g_renderer->SetVertexArray( cachedVBO->VAO );
		g_renderer->SetArrayBuffer( cachedVBO->VBO );
	}

	g_renderer->SetVertexArray( cachedVBO->VAO );
	g_renderer3d->UploadMegaVBOVertexData3DTextured( cachedVBO->VBO, m_megaTexturedVertices3D, m_megaTexturedVertex3DCount, BUFFER_USAGE_STATIC_DRAW );
	g_renderer3d->UploadMegaVBOIndexData3D( cachedVBO->IBO, m_megaTexturedIndices3D, m_megaTexturedIndex3DCount, BUFFER_USAGE_STATIC_DRAW );

	cachedVBO->vertexCount = m_megaTexturedVertex3DCount;
	cachedVBO->indexCount = m_megaTexturedIndex3DCount;
	cachedVBO->vertexType = VBO_VERTEX_3D_TEXTURED;
	cachedVBO->isValid = true;

	cachedVBO->color.m_r = ( m_currentMegaVBO3DTextureID >> 24 ) & 0xFF;
	cachedVBO->color.m_g = ( m_currentMegaVBO3DTextureID >> 16 ) & 0xFF;
	cachedVBO->color.m_b = ( m_currentMegaVBO3DTextureID >> 8 ) & 0xFF;
	cachedVBO->color.m_a = m_currentMegaVBO3DTextureID & 0xFF;

	m_megaTexturedVertex3DCount = 0;
	m_megaTexturedIndex3DCount = 0;
	m_megaVBO3DTexturedActive = false;
}


void MegaVBO3D::RenderTexturedMegaVBO3D( const char *megaVBOKey )
{
	Cached3DVBO *cachedVBO = m_cached3DVBOs.GetData( megaVBOKey, nullptr );
	if ( !cachedVBO || !cachedVBO->isValid )
	{
		return;
	}

	g_renderer->StartFlushTiming( "MegaVBO_Textured_3D" );

	//
	// Extract texture ID from color storage

	unsigned int textureID = ( (unsigned int)cachedVBO->color.m_r << 24 ) |
							 ( (unsigned int)cachedVBO->color.m_g << 16 ) |
							 ( (unsigned int)cachedVBO->color.m_b << 8 ) |
							 ( (unsigned int)cachedVBO->color.m_a );

	g_renderer->SetShaderProgram( g_renderer3d->m_shader3DTexturedProgram );

	if ( textureID != 0 )
	{
		g_renderer->SetActiveTexture( 0 );
		g_renderer->SetBoundTexture( textureID );
	}

	g_renderer3d->SetTextured3DShaderUniforms();

	g_renderer->SetVertexArray( cachedVBO->VAO );
	g_renderer3d->DrawMegaVBOIndexed3D( PRIMITIVE_TRIANGLES, cachedVBO->indexCount );

	g_renderer->EndFlushTiming( "MegaVBO_Textured_3D" );
	g_renderer3d->IncrementDrawCall3D( "quad_vbo" );
}


bool MegaVBO3D::IsTexturedMegaVBO3DValid( const char *megaVBOKey )
{
	Cached3DVBO *cachedVBO = m_cached3DVBOs.GetData( megaVBOKey, nullptr );
	return ( cachedVBO && cachedVBO->isValid );
}


void MegaVBO3D::SetTexturedMegaVBO3DBufferSizes( int vertexCount, int indexCount,
												 const char *cacheKey )
{
	int newMaxVertices = (int)( vertexCount * 1.1f );
	int newMaxIndices = (int)( indexCount * 1.1f );

	if ( cacheKey )
	{
		InvalidateCached3DVBO( cacheKey );
	}

	if ( m_megaTexturedVertices3D )
	{
		delete[] m_megaTexturedVertices3D;
		m_megaTexturedVertices3D = NULL;
	}
	if ( m_megaTexturedIndices3D )
	{
		delete[] m_megaTexturedIndices3D;
		m_megaTexturedIndices3D = NULL;
	}

	m_maxMegaTexturedVertices3D = newMaxVertices;
	m_maxMegaTexturedIndices3D = newMaxIndices;
	m_megaTexturedVertices3D = new Vertex3DTextured[m_maxMegaTexturedVertices3D];
	m_megaTexturedIndices3D = new unsigned int[m_maxMegaTexturedIndices3D];
}