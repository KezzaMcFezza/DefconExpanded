#include "systemiv.h"

#include "model.h"
#include "model_material.h"
#include "lib/string_utils.h"

Model::Model()
	: m_boundsMin( 0, 0, 0 ),
	  m_boundsMax( 0, 0, 0 ),
	  m_loaded( false ),
	  m_cacheKey( NULL )
{
	m_filename[0] = '\0';
}


Model::Model( const char *filename )
	: m_boundsMin( 0, 0, 0 ),
	  m_boundsMax( 0, 0, 0 ),
	  m_loaded( false ),
	  m_cacheKey( NULL )
{
	if ( filename )
	{
		strncpy( m_filename, filename, sizeof( m_filename ) - 1 );
		m_filename[sizeof( m_filename ) - 1] = '\0';
	}
	else
	{
		m_filename[0] = '\0';
	}
}


Model::~Model()
{
	m_meshes.clear();

	for ( size_t i = 0; i < m_materials.size(); i++ )
	{
		delete m_materials[i];
	}
	m_materials.clear();

	if ( m_cacheKey )
	{
		delete[] m_cacheKey;
		m_cacheKey = NULL;
	}
}


bool Model::IsLoaded() const
{
	return m_loaded;
}


int Model::GetMeshCount() const
{
	return (int)m_meshes.size();
}


const ModelMesh *Model::GetMesh( int index ) const
{
	if ( index >= 0 && index < (int)m_meshes.size() )
	{
		return &m_meshes[index];
	}
	return NULL;
}


int Model::GetMaterialCount() const
{
	return (int)m_materials.size();
}


ModelMaterial *Model::GetMaterial( int index )
{
	if ( index >= 0 && index < (int)m_materials.size() )
	{
		return m_materials[index];
	}
	return ModelMaterial::GetDefaultMaterial();
}


void Model::GetBounds( Vector3<float> &outMin, Vector3<float> &outMax ) const
{
	outMin = m_boundsMin;
	outMax = m_boundsMax;
}


float Model::GetBoundsRadius() const
{
	Vector3<float> size = m_boundsMax - m_boundsMin;
	return size.Mag() * 0.5f;
}


const char *Model::GetCacheKey() const
{
	return m_cacheKey ? m_cacheKey : "";
}


int Model::GetVBOCount() const
{
	return (int)m_vboCacheKeys.size();
}


const char *Model::GetVBOCacheKey( int index ) const
{
	if ( index >= 0 && index < (int)m_vboCacheKeys.size() )
	{
		return m_vboCacheKeys[index].c_str();
	}
	return NULL;
}


void Model::SetCacheKey( const char *key )
{
	if ( m_cacheKey )
	{
		delete[] m_cacheKey;
		m_cacheKey = NULL;
	}
	if ( key )
	{
		m_cacheKey = newStr( key );
	}
}


void Model::AddVBOCacheKey( const char *key )
{
	if ( key )
	{
		m_vboCacheKeys.push_back( std::string( key ) );
	}
}


void Model::ClearVBOCacheKeys()
{
	m_vboCacheKeys.clear();
}


void Model::CalculateBounds()
{
	if ( m_meshes.empty() )
	{
		return;
	}

	bool first = true;

	for ( size_t i = 0; i < m_meshes.size(); i++ )
	{
		const ModelMesh &mesh = m_meshes[i];

		for ( size_t j = 0; j < mesh.positions.size(); j += 3 )
		{
			Vector3<float> pos( mesh.positions[j], mesh.positions[j + 1], mesh.positions[j + 2] );

			if ( first )
			{
				m_boundsMin = pos;
				m_boundsMax = pos;
				first = false;
			}
			else
			{
				m_boundsMin.x = std::min( m_boundsMin.x, pos.x );
				m_boundsMin.y = std::min( m_boundsMin.y, pos.y );
				m_boundsMin.z = std::min( m_boundsMin.z, pos.z );

				m_boundsMax.x = std::max( m_boundsMax.x, pos.x );
				m_boundsMax.y = std::max( m_boundsMax.y, pos.y );
				m_boundsMax.z = std::max( m_boundsMax.z, pos.z );
			}
		}
	}
}
