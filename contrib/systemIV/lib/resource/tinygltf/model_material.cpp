#include "systemiv.h"

#include "model_material.h"
#include "lib/resource/image.h"
#include "lib/render/colour.h"

ModelMaterial *ModelMaterial::s_defaultMaterial = NULL;

ModelMaterial::ModelMaterial()
	: baseColorFactor( 255, 255, 255, 255 ),
	  metallicFactor( 1.0f ),
	  roughnessFactor( 1.0f ),
	  emissiveFactor( 0, 0, 0, 255 ),
	  alphaMode( ALPHA_OPAQUE ),
	  alphaCutoff( 0.5f ),
	  doubleSided( false ),
	  index( -1 )
{
}


ModelMaterial::~ModelMaterial()
{
	// Resource cache system owns image pointers
}


ModelMaterial *ModelMaterial::GetDefaultMaterial()
{
	if ( !s_defaultMaterial )
	{
		s_defaultMaterial = new ModelMaterial();
		s_defaultMaterial->name = "DefaultMaterial";
		s_defaultMaterial->baseColorFactor = Colour( 200, 200, 200, 255 );
		s_defaultMaterial->metallicFactor = 0.0f;
		s_defaultMaterial->roughnessFactor = 0.5f;
	}

	return s_defaultMaterial;
}