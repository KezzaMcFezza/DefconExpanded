#include "systemiv.h"

#include <stdarg.h>

#include "lib/resource/bitmapfont.h"
#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/resource/bitmap.h"
#include "lib/render3d/renderer_3d.h"
#include "lib/render/colour.h"
#include "lib/math/vector3.h"

#include "lib/render/renderer.h"
#include "lib/render2d/renderer_2d.h"

void Renderer3D::BlitChar3D( unsigned int textureID, const Vector3<float> &position, float width, float height,
							 float texX, float texY, float texW, float texH, Colour const &col,
							 BillboardMode3D mode, bool immediateFlush )
{

	//
	// Find the font batch for this texture ID, or find an empty one

	int targetBatch = -1;

	//
	// Check if we already have a batch for this texture

	for ( int i = 0; i < Renderer::MAX_FONT_ATLASES; i++ )
	{
		if ( m_fontBatches3D[i].textureID == textureID && m_fontBatches3D[i].vertexCount > 0 )
		{
			targetBatch = i;
			break;
		}
	}

	//
	// If not found, find the first empty batch

	if ( targetBatch == -1 )
	{
		for ( int i = 0; i < Renderer::MAX_FONT_ATLASES; i++ )
		{
			if ( m_fontBatches3D[i].vertexCount == 0 )
			{
				targetBatch = i;
				m_fontBatches3D[i].textureID = textureID;
				break;
			}
		}
	}

	//
	// If no empty batch found, use the current one

	if ( targetBatch == -1 )
	{
		targetBatch = m_currentFontBatchIndex3D;
	}

	FontBatch3D *batch = &m_fontBatches3D[targetBatch];

	if ( batch->vertexCount + 6 > MAX_TEXT_VERTICES_3D )
	{
		m_textVertices3D = batch->vertices;
		m_textVertexCount3D = batch->vertexCount;
		m_currentTextTexture3D = batch->textureID;
		FlushTextBuffer3D( false );
		batch->vertexCount = 0;
	}

	float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;

	//
	// BitmapFont inverts Y texture coordinates
	// Flip V coordinates to compensate

	float u1 = texX, v1 = texY + texH, u2 = texX + texW, v2 = texY;

	Vertex3DTextured vertices[6];

	if ( mode == BILLBOARD_SURFACE_ALIGNED )
	{
		CreateSurfaceAlignedBillboard( position, width, height, vertices, u1, v1, u2, v2, r, g, b, a );
	}
	else if ( mode == BILLBOARD_CAMERA_FACING )
	{
		CreateCameraFacingBillboard( position, width, height, vertices, u1, v1, u2, v2, r, g, b, a );
	}
	else
	{

		//
		// Use exact coordinates (flat quad in XY plane)

		float halfWidth = width * 0.5f;
		float halfHeight = height * 0.5f;

		vertices[0] = Vertex3DTextured( position.x - halfWidth, position.y + halfHeight, position.z, 0.0f, 0.0f, 1.0f, r, g, b, a, u1, v2 );
		vertices[1] = Vertex3DTextured( position.x + halfWidth, position.y + halfHeight, position.z, 0.0f, 0.0f, 1.0f, r, g, b, a, u2, v2 );
		vertices[2] = Vertex3DTextured( position.x + halfWidth, position.y - halfHeight, position.z, 0.0f, 0.0f, 1.0f, r, g, b, a, u2, v1 );
		vertices[3] = vertices[0];
		vertices[4] = vertices[2];
		vertices[5] = Vertex3DTextured( position.x - halfWidth, position.y - halfHeight, position.z, 0.0f, 0.0f, 1.0f, r, g, b, a, u1, v1 );
	}

	//
	// Add vertices to batch

	for ( int i = 0; i < 6; i++ )
	{
		batch->vertices[batch->vertexCount++] = vertices[i];
	}

	m_currentFontBatchIndex3D = targetBatch;
	m_textVertices3D = batch->vertices;
	m_textVertexCount3D = batch->vertexCount;
	m_currentTextTexture3D = batch->textureID;

	if ( m_immediateModeEnabled3D || immediateFlush )
	{
		FlushTextBuffer3D( true );
		batch->vertexCount = 0;
	}
}


// ============================================================================
// TEXT RENDERING FUNCTIONS
// ============================================================================

void Renderer3D::Text3D( float x, float y, float z, Colour const &col, float size, const char *text, ... )
{
	char buf[1024];
	va_list ap;
	va_start( ap, text );
	vsnprintf( buf, sizeof( buf ), text, ap );
	buf[sizeof( buf ) - 1] = '\0';
	TextSimple3D( x, y, z, col, size, buf );
}


void Renderer3D::TextCentre3D( float x, float y, float z, Colour const &col, float size, const char *text, ... )
{
	char buf[1024];
	va_list ap;
	va_start( ap, text );
	vsnprintf( buf, sizeof( buf ), text, ap );
	buf[sizeof( buf ) - 1] = '\0';
	float textWidth = TextWidth3D( buf, size );
	TextSimple3D( x - textWidth / 2.0f, y, z, col, size, buf );
}


void Renderer3D::TextRight3D( float x, float y, float z, Colour const &col, float size, const char *text, ... )
{
	char buf[1024];
	va_list ap;
	va_start( ap, text );
	vsnprintf( buf, sizeof( buf ), text, ap );
	buf[sizeof( buf ) - 1] = '\0';
	float textWidth = TextWidth3D( buf, size );
	TextSimple3D( x - textWidth, y, z, col, size, buf );
}


void Renderer3D::TextSimple3D( float x, float y, float z, Colour const &col, float size, const char *text,
							   BillboardMode3D mode, bool immediateFlush )
{
	BitmapFont *font = g_resource->GetBitmapFont( g_renderer->GetCurrentFontFilename() );
	if ( font )
	{
		font->SetSpacing( g_renderer->GetFontSpacing( g_renderer->GetCurrentFontName() ) );
	}
	else
	{
		font = g_resource->GetBitmapFont( g_renderer->GetDefaultFontFilename() );
		if ( font )
		{
			font->SetSpacing( g_renderer->GetFontSpacing( g_renderer->GetDefaultFontName() ) );
		}
	}

	if ( !font )
		return;

	font->SetHoriztonalFlip( g_renderer->GetHorizFlip() );
	font->SetFixedWidth( g_renderer->GetFixedWidth() );

	unsigned numChars = (unsigned)strlen( text );

	FlushTextBuffer3DIfFull( numChars );

	float currentX = 0.0f;
	Vector3<float> basePosition( x, y, z );

	for ( unsigned int i = 0; i < numChars; ++i )
	{
		unsigned char thisChar = text[i];

		float texX = font->GetTexCoordX( thisChar );
		float texY = font->GetTexCoordY( thisChar );
		float texW = font->GetTexWidth( thisChar );
		float texH = TEX_HEIGHT;

		float horiSize = size * texW / texH;

		float finalTexY = texY;
		float finalTexH = texH;
		if ( font->m_horizontalFlip )
		{
			finalTexY += texH;
			finalTexH = -texH;
		}

		Vector3<float> charPosition;

		if ( mode == BILLBOARD_SURFACE_ALIGNED )
		{

			Vector3<float> east, north;
			Renderer3D::CalculateSphericalTangents( basePosition, east, north );

			charPosition = basePosition + east * ( currentX + horiSize * 0.5f );

			//
			// Project back onto sphere surface to maintain constant radius

			float radius = basePosition.Mag();
			if ( radius > 0.001f )
			{
				charPosition.Normalise();
				charPosition = charPosition * radius;
			}
		}
		else
		{

			charPosition = basePosition;
			charPosition.x += currentX + horiSize * 0.5f;
		}

		BlitChar3D( font->m_textureID, charPosition, horiSize, size,
					texX, finalTexY, texW, finalTexH, col, mode, false );

		currentX += horiSize;
		currentX += size * font->GetSpacing();

		if ( font->m_fixedWidth )
		{
			currentX -= size * 0.55f;
		}
	}

	if ( immediateFlush )
	{
		for ( int i = 0; i < Renderer::MAX_FONT_ATLASES; i++ )
		{
			if ( m_fontBatches3D[i].vertexCount > 0 )
			{
				m_textVertices3D = m_fontBatches3D[i].vertices;
				m_textVertexCount3D = m_fontBatches3D[i].vertexCount;
				m_currentTextTexture3D = m_fontBatches3D[i].textureID;
				FlushTextBuffer3D( immediateFlush );
				m_fontBatches3D[i].vertexCount = 0;
			}
		}
	}
}


void Renderer3D::TextCentreSimple3D( float x, float y, float z, Colour const &col, float size, const char *text,
									 BillboardMode3D mode, bool immediateFlush )
{
	float textWidth = TextWidth3D( text, size );

	if ( mode == BILLBOARD_SURFACE_ALIGNED )
	{

		//
		// For surface-aligned text on spherical surfaces,
		// offset along the surface tangent to center the text

		Vector3<float> position( x, y, z );
		float radius = position.Mag();

		Vector3<float> east, north;
		CalculateSphericalTangents( position, east, north );

		Vector3<float> centeredPos = position - east * ( textWidth * 0.5f );

		//
		// Project back onto sphere surface

		if ( radius > 0.001f )
		{
			centeredPos.Normalise();
			centeredPos = centeredPos * radius;
		}

		TextSimple3D( centeredPos.x, centeredPos.y, centeredPos.z, col, size, text, mode, immediateFlush );
	}
	else
	{

		TextSimple3D( x - textWidth / 2.0f, y, z, col, size, text, mode, immediateFlush );
	}
}


void Renderer3D::TextRightSimple3D( float x, float y, float z, Colour const &col, float size, const char *text,
									BillboardMode3D mode, bool immediateFlush )
{
	float textWidth = TextWidth3D( text, size );

	if ( mode == BILLBOARD_SURFACE_ALIGNED )
	{

		Vector3<float> position( x, y, z );
		float radius = position.Mag();

		Vector3<float> east, north;
		CalculateSphericalTangents( position, east, north );

		Vector3<float> rightAlignedPos = position - east * textWidth;

		if ( radius > 0.001f )
		{
			rightAlignedPos.Normalise();
			rightAlignedPos = rightAlignedPos * radius;
		}

		TextSimple3D( rightAlignedPos.x, rightAlignedPos.y, rightAlignedPos.z, col, size, text, mode, immediateFlush );
	}
	else
	{

		TextSimple3D( x - textWidth, y, z, col, size, text, mode, immediateFlush );
	}
}


float Renderer3D::TextWidth3D( const char *text, float size )
{
	BitmapFont *font = g_resource->GetBitmapFont( g_renderer->GetCurrentFontFilename() );
	if ( font )
	{
		font->SetSpacing( g_renderer->GetFontSpacing( g_renderer->GetCurrentFontName() ) );
	}
	else
	{
		font = g_resource->GetBitmapFont( g_renderer->GetDefaultFontFilename() );
		if ( font )
		{
			font->SetSpacing( g_renderer->GetFontSpacing( g_renderer->GetDefaultFontName() ) );
		}
	}

	if ( !font )
		return -1;

	font->SetHoriztonalFlip( g_renderer->GetHorizFlip() );
	font->SetFixedWidth( g_renderer->GetFixedWidth() );
	return font->GetTextWidth( text, size );
}


float Renderer3D::TextWidth3D( const char *text, unsigned int textLen, float size, BitmapFont *bitmapFont )
{
	if ( !bitmapFont )
		return -1;
	return bitmapFont->GetTextWidth( text, textLen, size );
}


void Renderer3D::FlushTextBuffer3DIfFull( int charactersNeeded )
{
	int verticesNeeded = charactersNeeded * 6;
	if ( m_textVertexCount3D + verticesNeeded > MAX_TEXT_VERTICES_3D )
	{
		FlushTextBuffer3D( false );
	}
}
