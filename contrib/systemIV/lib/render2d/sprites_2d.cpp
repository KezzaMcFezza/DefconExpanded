#include "systemiv.h"

#include <stdarg.h>

#include "lib/resource/bitmapfont.h"
#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/resource/bitmap.h"
#include "lib/resource/sprite_atlas.h"
#include "lib/math/vector3.h"
#include "lib/render3d/renderer_3d.h"
#include "lib/render/colour.h"

#include "lib/render/renderer.h"
#include "lib/render2d/renderer_2d.h"

void Renderer2D::StaticSprite( Image *src, float x, float y, float w, float h, Colour const &col, bool immediateFlush )
{
	FlushStaticSpritesIfFull( 6 );

	unsigned int effectiveTextureID = g_renderer->GetEffectiveTextureID( src );

	if ( m_staticSpriteVertexCount > 0 &&
		 m_currentStaticSpriteTexture != effectiveTextureID &&
		 m_currentStaticSpriteTexture != 0 )
	{
		FlushStaticSprites( false );
	}

	m_currentStaticSpriteTexture = effectiveTextureID;

	float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
	float u1, v1, u2, v2;

	g_renderer->GetImageUVCoords( src, u1, v1, u2, v2 );

	// First triangle: TL, TR, BR
	m_staticSpriteVertices[m_staticSpriteVertexCount++] = { x, y, r, g, b, a, u1, v2 };
	m_staticSpriteVertices[m_staticSpriteVertexCount++] = { x + w, y, r, g, b, a, u2, v2 };
	m_staticSpriteVertices[m_staticSpriteVertexCount++] = { x + w, y + h, r, g, b, a, u2, v1 };

	// Second triangle: TL, BR, BL
	m_staticSpriteVertices[m_staticSpriteVertexCount++] = { x, y, r, g, b, a, u1, v2 };
	m_staticSpriteVertices[m_staticSpriteVertexCount++] = { x + w, y + h, r, g, b, a, u2, v1 };
	m_staticSpriteVertices[m_staticSpriteVertexCount++] = { x, y + h, r, g, b, a, u1, v1 };

	if ( m_immediateModeEnabled || immediateFlush )
	{
		FlushStaticSprites( true );
	}
}


void Renderer2D::StaticSprite( Image *src, float x, float y, Colour const &col, bool immediateFlush )
{
	float w = src->Width();
	float h = src->Height();
	StaticSprite( src, x, y, w, h, col, immediateFlush );
}


void Renderer2D::RotatingSprite( Image *src, float x, float y, float w, float h, Colour const &col, float angle, bool immediateFlush )
{
	FlushRotatingSpritesIfFull( 6 );

	unsigned int effectiveTextureID = g_renderer->GetEffectiveTextureID( src );

	if ( m_rotatingSpriteVertexCount > 0 &&
		 m_currentRotatingSpriteTexture != effectiveTextureID &&
		 m_currentRotatingSpriteTexture != 0 )
	{
		FlushRotatingSprite( false );
	}

	m_currentRotatingSpriteTexture = effectiveTextureID;

	float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
	float u1, v1, u2, v2;

	g_renderer->GetImageUVCoords( src, u1, v1, u2, v2 );

	// Calculate rotated vertices
	Vector3<float> vert1( -w, +h, 0 );
	Vector3<float> vert2( +w, +h, 0 );
	Vector3<float> vert3( +w, -h, 0 );
	Vector3<float> vert4( -w, -h, 0 );

	vert1.RotateAroundZ( angle );
	vert2.RotateAroundZ( angle );
	vert3.RotateAroundZ( angle );
	vert4.RotateAroundZ( angle );

	vert1 += Vector3<float>( x, y, 0 );
	vert2 += Vector3<float>( x, y, 0 );
	vert3 += Vector3<float>( x, y, 0 );
	vert4 += Vector3<float>( x, y, 0 );


	// First triangle: vert1, vert2, vert3
	m_rotatingSpriteVertices[m_rotatingSpriteVertexCount++] = { vert1.x, vert1.y, r, g, b, a, u1, v2 };
	m_rotatingSpriteVertices[m_rotatingSpriteVertexCount++] = { vert2.x, vert2.y, r, g, b, a, u2, v2 };
	m_rotatingSpriteVertices[m_rotatingSpriteVertexCount++] = { vert3.x, vert3.y, r, g, b, a, u2, v1 };

	// Second triangle: vert1, vert3, vert4
	m_rotatingSpriteVertices[m_rotatingSpriteVertexCount++] = { vert1.x, vert1.y, r, g, b, a, u1, v2 };
	m_rotatingSpriteVertices[m_rotatingSpriteVertexCount++] = { vert3.x, vert3.y, r, g, b, a, u2, v1 };
	m_rotatingSpriteVertices[m_rotatingSpriteVertexCount++] = { vert4.x, vert4.y, r, g, b, a, u1, v1 };

	if ( m_immediateModeEnabled || immediateFlush )
	{
		FlushRotatingSprite( true );
	}
}