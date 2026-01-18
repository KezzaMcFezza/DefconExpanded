#include "systemiv.h"

#include "lib/debug/debug_utils.h"
#include "lib/string_utils.h"
#include "lib/render/renderer.h"
#include "lib/render3d/renderer_3d.h"
#include "lib/render3d/megavbo/megavbo_3d.h"
#include "lib/math/matrix4f.h"

// ============================================================================
// INSTANCING SYSTEM
// ============================================================================

void MegaVBO3D::BeginInstancedMegaVBO( const char *batchKey, const char *meshKey )
{
	if ( !batchKey || !meshKey )
	{
		return;
	}

	//
	// Only skip building if we are not currently building

	if ( !m_instancedMegaVBOActive )
	{
		CachedInstanceBatch *batch = m_cachedInstanceBatches.GetData( batchKey, nullptr );
		if ( batch && batch->isValid )
		{
			return;
		}
	}

	//
	// Verify the mesh exists

	if ( !IsAny3DVBOValid( meshKey ) )
	{
		// Continue anyway, mesh might be created later
	}

	//
	// Clean up previous keys if still active

	if ( m_currentInstancedBatchKey )
	{
		delete[] m_currentInstancedBatchKey;
	}
	if ( m_currentInstancedMeshKey )
	{
		delete[] m_currentInstancedMeshKey;
	}

	m_currentInstancedBatchKey = newStr( batchKey );
	m_currentInstancedMeshKey = newStr( meshKey );
	m_instancedMegaVBOActive = true;
	m_instanceCount = 0;
}


bool MegaVBO3D::AddInstance( const Matrix4f &matrix, const Colour &color )
{
	if ( !m_instancedMegaVBOActive )
	{
		return false;
	}

	if ( m_instanceCount >= m_maxInstances )
	{
		return false;
	}

	m_instanceMatrices[m_instanceCount] = matrix;
	m_instanceColors[m_instanceCount] = color;
	m_instanceCount++;

	return true;
}


bool MegaVBO3D::AddInstanceIfNotFull( const Matrix4f &matrix, const Colour &color )
{
	if ( !m_instancedMegaVBOActive )
	{
		return false;
	}

	if ( m_instanceCount >= m_maxInstances )
	{
		return false;
	}

	m_instanceMatrices[m_instanceCount] = matrix;
	m_instanceColors[m_instanceCount] = color;
	m_instanceCount++;

	return true;
}


void MegaVBO3D::EndInstancedMegaVBO()
{
	if ( !m_instancedMegaVBOActive || !m_currentInstancedBatchKey )
	{
		return;
	}

	if ( m_instanceCount == 0 )
	{
		m_instancedMegaVBOActive = false;
		return;
	}

	//
	// Create or get cached instance batch

	CachedInstanceBatch *batch = m_cachedInstanceBatches.GetData( m_currentInstancedBatchKey, nullptr );

	if ( !batch )
	{

		//
		// Create new batch

		batch = new CachedInstanceBatch();
		batch->meshKey = newStr( m_currentInstancedMeshKey );
		batch->maxInstances = m_maxInstances;
		batch->matrices = new Matrix4f[m_maxInstances];
		batch->colors = new Colour[m_maxInstances];
		batch->instanceCount = 0;
		batch->isValid = false;

		m_cachedInstanceBatches.PutData( m_currentInstancedBatchKey, batch );
	}
	else
	{


		//
		// If mesh changed, update it

		if ( strcmp( batch->meshKey, m_currentInstancedMeshKey ) != 0 )
		{
			delete[] batch->meshKey;
			batch->meshKey = newStr( m_currentInstancedMeshKey );
		}

		//
		// Resize if needed

		if ( batch->maxInstances < m_instanceCount )
		{
			delete[] batch->matrices;
			delete[] batch->colors;

			batch->maxInstances = m_instanceCount;
			batch->matrices = new Matrix4f[batch->maxInstances];
			batch->colors = new Colour[batch->maxInstances];
		}
	}

	for ( int i = 0; i < m_instanceCount; i++ )
	{
		batch->matrices[i] = m_instanceMatrices[i];
		batch->colors[i] = m_instanceColors[i];
	}

	batch->instanceCount = m_instanceCount;
	batch->isValid = true;

	m_instanceCount = 0;
	m_instancedMegaVBOActive = false;
}


void MegaVBO3D::RenderInstancedMegaVBO3D( const char *batchKey )
{
	if ( !batchKey )
	{
		return;
	}

	CachedInstanceBatch *batch = m_cachedInstanceBatches.GetData( batchKey, nullptr );
	if ( !batch || !batch->isValid )
	{
		return;
	}

	if ( !IsAny3DVBOValid( batch->meshKey ) )
	{
		return;
	}

	if ( batch->instanceCount <= 0 )
	{
		return;
	}

	Cached3DVBO *cachedVBO = m_cached3DVBOs.GetData( batch->meshKey, nullptr );
	if ( !cachedVBO || !cachedVBO->isValid )
	{
		return;
	}

	g_renderer->StartFlushTiming( "MegaVBO_Instanced_3D" );

	g_renderer->SetShaderProgram( g_renderer3d->m_shader3DModelProgram );
	g_renderer3d->Set3DModelShaderUniformsInstanced( batch->matrices, batch->colors, batch->instanceCount );

	//
	// For textured VBOs, we need to remap vertex attributes for instanced rendering.
	// location 0 = position, location 1 = color (from location 2)

	if ( cachedVBO->vertexType == VBO_VERTEX_3D_TEXTURED )
	{
		g_renderer3d->SetupMegaVBOInstancedVertexAttributes3DTextured( cachedVBO->VAO, cachedVBO->VBO, cachedVBO->IBO );
	}
	else
	{
		//
		// For non-textured VBOs, use standard setup

		g_renderer->SetVertexArray( cachedVBO->VAO );
		g_renderer->SetArrayBuffer( cachedVBO->VBO );
		g_renderer3d->SetupMegaVBOVertexAttributes3D( cachedVBO->VAO, cachedVBO->VBO, cachedVBO->IBO );
	}

	g_renderer->SetVertexArray( cachedVBO->VAO );
	g_renderer3d->DrawMegaVBOIndexedInstanced3D( PRIMITIVE_TRIANGLES, cachedVBO->indexCount, batch->instanceCount );

	g_renderer->EndFlushTiming( "MegaVBO_Instanced_3D" );
	g_renderer3d->IncrementDrawCall3D( DRAW_CALL_TRIANGLE_VBO );
}


bool MegaVBO3D::IsInstancedMegaVBOValid( const char *batchKey )
{
	if ( !batchKey )
	{
		return false;
	}

	CachedInstanceBatch *batch = m_cachedInstanceBatches.GetData( batchKey, nullptr );
	return ( batch && batch->isValid );
}


void MegaVBO3D::InvalidateInstancedMegaVBO( const char *batchKey )
{
	if ( !batchKey )
	{
		return;
	}

	CachedInstanceBatch *batch = m_cachedInstanceBatches.GetData( batchKey, nullptr );
	if ( batch )
	{

		if ( batch->meshKey )
		{
			delete[] batch->meshKey;
		}
		if ( batch->matrices )
		{
			delete[] batch->matrices;
		}
		if ( batch->colors )
		{
			delete[] batch->colors;
		}

		delete batch;
		m_cachedInstanceBatches.RemoveData( batchKey );
	}
}


void MegaVBO3D::InvalidateAllInstanceBatches()
{
	for ( auto it = m_cachedInstanceBatches.begin(); it != m_cachedInstanceBatches.end(); ++it )
	{
		InvalidateInstancedMegaVBO( it.get_key().c_str() );
	}
}


void MegaVBO3D::InvalidateInstanceBatchPrefix( const char *batchKeyPrefix )
{
	if ( !batchKeyPrefix )
		return;

	size_t prefixLen = strlen( batchKeyPrefix );

	for ( auto it = m_cachedInstanceBatches.begin(); it != m_cachedInstanceBatches.end(); ++it )
	{
		const std::string &key = it.get_key();
		if ( strncmp( key.c_str(), batchKeyPrefix, prefixLen ) == 0 )
		{
			InvalidateInstancedMegaVBO( key.c_str() );
		}
	}
}


int MegaVBO3D::GetCachedInstanceBatchCount()
{
	int count = 0;

	for ( auto it = m_cachedInstanceBatches.begin(); it != m_cachedInstanceBatches.end(); ++it )
	{
		CachedInstanceBatch *batch = *it;
		if ( batch && batch->isValid )
		{
			count++;
		}
	}

	return count;
}