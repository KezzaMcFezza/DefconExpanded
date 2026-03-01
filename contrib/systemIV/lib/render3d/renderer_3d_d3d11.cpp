#include "systemiv.h"

#ifdef RENDERER_DIRECTX11

#include "renderer_3d_d3d11.h"
#include "lib/render/renderer.h"
#include "lib/render/renderer_d3d11.h"
#include "lib/gucci/window_manager.h"
#include "lib/gucci/window_manager_d3d11.h"
#include "shaders/vertex.hlsl.h"
#include "shaders/fragment.hlsl.h"
#include "shaders/textured_vertex.hlsl.h"
#include "shaders/textured_fragment.hlsl.h"
#include "shaders/model_vertex.hlsl.h"
#include "shaders/line_geometry_3d.hlsl.h"

// ============================================================================
// CONSTRUCTOR & DESTRUCTOR
// ============================================================================

Renderer3DD3D11::Renderer3DD3D11()
	: m_device( nullptr ),
	  m_deviceContext( nullptr ),
	  m_vertexShader3D( nullptr ),
	  m_pixelShader3D( nullptr ),
	  m_texturedVertexShader3D( nullptr ),
	  m_texturedPixelShader3D( nullptr ),
	  m_modelVertexShader3D( nullptr ),
	  m_modelPixelShader3D( nullptr ),
	  m_inputLayout3D( nullptr ),
	  m_inputLayoutTextured3D( nullptr ),
	  m_lineGeometryShaderThin3D( nullptr ),
	  m_lineGeometryShaderThick3D( nullptr ),
	  m_transformConstantBuffer( nullptr ),
	  m_fogConstantBuffer( nullptr ),
	  m_modelConstantBuffer( nullptr ),
	  m_lineWidthConstantBuffer3D( nullptr ),
	  m_lineBuffer3D( nullptr ),
	  m_staticSpriteBuffer3D( nullptr ),
	  m_rotatingSpriteBuffer3D( nullptr ),
	  m_textBuffer3D( nullptr ),
	  m_circleBuffer3D( nullptr ),
	  m_circleFillBuffer3D( nullptr ),
	  m_rectBuffer3D( nullptr ),
	  m_rectFillBuffer3D( nullptr ),
	  m_triangleFillBuffer3D( nullptr ),
	  m_currentTextureSRV( nullptr ),
	  m_currentTextureID( 0 ),
	  m_nextVBOId( 1 ),
	  m_currentVAO( 0 ),
	  m_primitiveRestartEnabled( false ),
	  m_primitiveRestartIndex( 0 )
{
	GetDeviceAndContext();
	Initialize3DShaders();
	Cache3DUniformLocations();
	Setup3DVertexArrays();
	Setup3DTexturedVertexArrays();
	CreateConstantBuffers();
}


Renderer3DD3D11::~Renderer3DD3D11()
{
	CleanupBuffers3D();
	ReleaseShaderResources();
	ReleaseDeviceAndContext();
}


// ============================================================================
// CLEANUP
// ============================================================================


void Renderer3DD3D11::CleanupBuffers3D()
{
	if ( m_lineBuffer3D )
	{
		m_lineBuffer3D->Release();
		m_lineBuffer3D = nullptr;
	}
	if ( m_staticSpriteBuffer3D )
	{
		m_staticSpriteBuffer3D->Release();
		m_staticSpriteBuffer3D = nullptr;
	}
	if ( m_rotatingSpriteBuffer3D )
	{
		m_rotatingSpriteBuffer3D->Release();
		m_rotatingSpriteBuffer3D = nullptr;
	}
	if ( m_textBuffer3D )
	{
		m_textBuffer3D->Release();
		m_textBuffer3D = nullptr;
	}
	if ( m_circleBuffer3D )
	{
		m_circleBuffer3D->Release();
		m_circleBuffer3D = nullptr;
	}
	if ( m_circleFillBuffer3D )
	{
		m_circleFillBuffer3D->Release();
		m_circleFillBuffer3D = nullptr;
	}
	if ( m_rectBuffer3D )
	{
		m_rectBuffer3D->Release();
		m_rectBuffer3D = nullptr;
	}
	if ( m_rectFillBuffer3D )
	{
		m_rectFillBuffer3D->Release();
		m_rectFillBuffer3D = nullptr;
	}
	if ( m_triangleFillBuffer3D )
	{
		m_triangleFillBuffer3D->Release();
		m_triangleFillBuffer3D = nullptr;
	}

	for ( auto &pair : m_vboMap )
	{
		if ( pair.second )
		{
			pair.second->Release();
		}
	}
	m_vboMap.clear();

	for ( auto &pair : m_iboMap )
	{
		if ( pair.second )
		{
			pair.second->Release();
		}
	}
	m_iboMap.clear();

	m_vaoMap.clear();
	m_lineStripSegments.clear();
}


void Renderer3DD3D11::ReleaseShaderResources()
{
	if ( m_vertexShader3D )
	{
		m_vertexShader3D->Release();
		m_vertexShader3D = nullptr;
	}
	if ( m_pixelShader3D )
	{
		m_pixelShader3D->Release();
		m_pixelShader3D = nullptr;
	}
	if ( m_texturedVertexShader3D )
	{
		m_texturedVertexShader3D->Release();
		m_texturedVertexShader3D = nullptr;
	}
	if ( m_texturedPixelShader3D )
	{
		m_texturedPixelShader3D->Release();
		m_texturedPixelShader3D = nullptr;
	}
	if ( m_modelVertexShader3D )
	{
		m_modelVertexShader3D->Release();
		m_modelVertexShader3D = nullptr;
	}
	if ( m_modelPixelShader3D )
	{
		m_modelPixelShader3D->Release();
		m_modelPixelShader3D = nullptr;
	}
	if ( m_inputLayout3D )
	{
		m_inputLayout3D->Release();
		m_inputLayout3D = nullptr;
	}
	if ( m_inputLayoutTextured3D )
	{
		m_inputLayoutTextured3D->Release();
		m_inputLayoutTextured3D = nullptr;
	}
	if ( m_lineGeometryShaderThin3D )
	{
		m_lineGeometryShaderThin3D->Release();
		m_lineGeometryShaderThin3D = nullptr;
	}
	if ( m_lineGeometryShaderThick3D )
	{
		m_lineGeometryShaderThick3D->Release();
		m_lineGeometryShaderThick3D = nullptr;
	}
	if ( m_lineWidthConstantBuffer3D )
	{
		m_lineWidthConstantBuffer3D->Release();
		m_lineWidthConstantBuffer3D = nullptr;
	}
	if ( m_transformConstantBuffer )
	{
		m_transformConstantBuffer->Release();
		m_transformConstantBuffer = nullptr;
	}
	if ( m_fogConstantBuffer )
	{
		m_fogConstantBuffer->Release();
		m_fogConstantBuffer = nullptr;
	}
	if ( m_modelConstantBuffer )
	{
		m_modelConstantBuffer->Release();
		m_modelConstantBuffer = nullptr;
	}
}


void Renderer3DD3D11::InvalidateStateCache()
{
	m_currentVAO = 0;
}


void Renderer3DD3D11::ReleaseDeviceAndContext()
{
	if ( m_deviceContext )
	{
		m_deviceContext->Release();
		m_deviceContext = nullptr;
	}
	if ( m_device )
	{
		m_device->Release();
		m_device = nullptr;
	}
}


void Renderer3DD3D11::HandleDeviceReset()
{
	CleanupBuffers3D();
	ReleaseShaderResources();
	ReleaseDeviceAndContext();
	GetDeviceAndContext();
	Initialize3DShaders();
	Cache3DUniformLocations();
	Setup3DVertexArrays();
	Setup3DTexturedVertexArrays();
	CreateConstantBuffers();
	InvalidateStateCache();

	m_currentTextureID = 0;
	m_currentTextureSRV = nullptr;
}


// ============================================================================
// DEVICE & CONTEXT
// ============================================================================

void Renderer3DD3D11::GetDeviceAndContext()
{
	WindowManagerD3D11 *windowManager = dynamic_cast<WindowManagerD3D11 *>( g_windowManager );
	if ( windowManager )
	{
		m_device = windowManager->GetDevice();
		m_deviceContext = windowManager->GetDeviceContext();

		if ( m_device )
			m_device->AddRef();
		if ( m_deviceContext )
			m_deviceContext->AddRef();
	}
}


// ============================================================================
// HELPER METHODS
// ============================================================================

bool Renderer3DD3D11::CheckHR( HRESULT hr, const char *operation )
{
	return RendererD3D11::CheckHRResult( hr, operation );
}


bool Renderer3DD3D11::CheckHR( HRESULT hr, const char *operation, ID3DBlob *errorBlob )
{
	return RendererD3D11::CheckHRResult( hr, operation, errorBlob );
}


// ============================================================================
// SHADER MANAGEMENT
// ============================================================================

void Renderer3DD3D11::Initialize3DShaders()
{
	if ( !m_device )
		return;

	HRESULT hr;
	ID3DBlob *vertexBlob = nullptr;
	ID3DBlob *pixelBlob = nullptr;
	ID3DBlob *errorBlob = nullptr;

	//
	// Compile basic 3D vertex shader

	hr = D3DCompile( VERTEX_3D_SHADER_SOURCE_HLSL, strlen( VERTEX_3D_SHADER_SOURCE_HLSL ),
					 nullptr, nullptr, nullptr, "main", "vs_5_0", 0, 0, &vertexBlob, &errorBlob );

	if ( !CheckHR( hr, "3D vertex shader compilation", errorBlob ) )
	{
		if ( vertexBlob )
			vertexBlob->Release();
		return;
	}

	hr = m_device->CreateVertexShader( vertexBlob->GetBufferPointer(), vertexBlob->GetBufferSize(),
									   nullptr, &m_vertexShader3D );
	if ( !CheckHR( hr, "create 3D vertex shader" ) )
	{
		vertexBlob->Release();
		return;
	}

	//
	// Create input layout for basic 3D vertices (position + color)

	D3D11_INPUT_ELEMENT_DESC layout3D[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	hr = m_device->CreateInputLayout( layout3D, ARRAYSIZE( layout3D ),
									  vertexBlob->GetBufferPointer(), vertexBlob->GetBufferSize(),
									  &m_inputLayout3D );
	if ( !CheckHR( hr, "create 3D input layout" ) )
	{
		vertexBlob->Release();
		return;
	}

	vertexBlob->Release();

	//
	// Compile basic 3D pixel shader

	hr = D3DCompile( FRAGMENT_3D_SHADER_SOURCE_HLSL, strlen( FRAGMENT_3D_SHADER_SOURCE_HLSL ),
					 nullptr, nullptr, nullptr, "main", "ps_5_0", 0, 0, &pixelBlob, &errorBlob );

	if ( !CheckHR( hr, "3D pixel shader compilation", errorBlob ) )
	{
		if ( pixelBlob )
			pixelBlob->Release();
		return;
	}

	hr = m_device->CreatePixelShader( pixelBlob->GetBufferPointer(), pixelBlob->GetBufferSize(),
									  nullptr, &m_pixelShader3D );
	if ( !CheckHR( hr, "create 3D pixel shader" ) )
	{
		pixelBlob->Release();
		return;
	}

	pixelBlob->Release();

	//
	// Compile textured 3D vertex shader

	hr = D3DCompile( TEXTURED_VERTEX_3D_SHADER_SOURCE_HLSL, strlen( TEXTURED_VERTEX_3D_SHADER_SOURCE_HLSL ),
					 nullptr, nullptr, nullptr, "main", "vs_5_0", 0, 0, &vertexBlob, &errorBlob );

	if ( !CheckHR( hr, "textured 3D vertex shader compilation", errorBlob ) )
	{
		if ( vertexBlob )
			vertexBlob->Release();
		return;
	}

	hr = m_device->CreateVertexShader( vertexBlob->GetBufferPointer(), vertexBlob->GetBufferSize(),
									   nullptr, &m_texturedVertexShader3D );
	if ( !CheckHR( hr, "create textured 3D vertex shader" ) )
	{
		vertexBlob->Release();
		return;
	}

	//
	// Create input layout for textured 3D vertices (position + normal + color + texcoord)

	D3D11_INPUT_ELEMENT_DESC layoutTextured3D[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 40, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	hr = m_device->CreateInputLayout( layoutTextured3D, ARRAYSIZE( layoutTextured3D ),
									  vertexBlob->GetBufferPointer(), vertexBlob->GetBufferSize(),
									  &m_inputLayoutTextured3D );
	if ( !CheckHR( hr, "create textured 3D input layout" ) )
	{
		vertexBlob->Release();
		return;
	}

	vertexBlob->Release();

	//
	// Compile textured 3D pixel shader

	hr = D3DCompile( TEXTURED_FRAGMENT_3D_SHADER_SOURCE_HLSL, strlen( TEXTURED_FRAGMENT_3D_SHADER_SOURCE_HLSL ),
					 nullptr, nullptr, nullptr, "main", "ps_5_0", 0, 0, &pixelBlob, &errorBlob );

	if ( !CheckHR( hr, "textured 3D pixel shader compilation", errorBlob ) )
	{
		if ( pixelBlob )
			pixelBlob->Release();
		return;
	}

	hr = m_device->CreatePixelShader( pixelBlob->GetBufferPointer(), pixelBlob->GetBufferSize(),
									  nullptr, &m_texturedPixelShader3D );
	if ( !CheckHR( hr, "create textured 3D pixel shader" ) )
	{
		pixelBlob->Release();
		return;
	}

	pixelBlob->Release();

	//
	// Compile model 3D vertex shader

	hr = D3DCompile( MODEL_VERTEX_3D_SHADER_SOURCE_HLSL, strlen( MODEL_VERTEX_3D_SHADER_SOURCE_HLSL ),
					 nullptr, nullptr, nullptr, "main", "vs_5_0", 0, 0, &vertexBlob, &errorBlob );

	if ( !CheckHR( hr, "model 3D vertex shader compilation", errorBlob ) )
	{
		if ( vertexBlob )
			vertexBlob->Release();
		return;
	}

	hr = m_device->CreateVertexShader( vertexBlob->GetBufferPointer(), vertexBlob->GetBufferSize(),
									   nullptr, &m_modelVertexShader3D );
	if ( !CheckHR( hr, "create model 3D vertex shader" ) )
	{
		vertexBlob->Release();
		return;
	}

	vertexBlob->Release();

	//
	// Compile model 3D pixel shader, reuses fragment shader

	hr = D3DCompile( FRAGMENT_3D_SHADER_SOURCE_HLSL, strlen( FRAGMENT_3D_SHADER_SOURCE_HLSL ),
					 nullptr, nullptr, nullptr, "main", "ps_5_0", 0, 0, &pixelBlob, &errorBlob );

	if ( !CheckHR( hr, "model 3D pixel shader compilation", errorBlob ) )
	{
		if ( pixelBlob )
			pixelBlob->Release();
		return;
	}

	hr = m_device->CreatePixelShader( pixelBlob->GetBufferPointer(), pixelBlob->GetBufferSize(),
									  nullptr, &m_modelPixelShader3D );
	if ( !CheckHR( hr, "create model 3D pixel shader" ) )
	{
		pixelBlob->Release();
		return;
	}

	pixelBlob->Release();

	//
	// Compile thin line geometry shader for 3D

	ID3DBlob *geometryBlob = nullptr;

	hr = D3DCompile( LINE_GEOMETRY_3D_THIN_SHADER_SOURCE_HLSL,
					 strlen( LINE_GEOMETRY_3D_THIN_SHADER_SOURCE_HLSL ),
					 nullptr, nullptr, nullptr, "main", "gs_5_0", 0, 0,
					 &geometryBlob, &errorBlob );

	if ( !CheckHR( hr, "thin 3D line geometry shader compilation", errorBlob ) )
	{
		if ( geometryBlob )
			geometryBlob->Release();
		return;
	}

	hr = m_device->CreateGeometryShader( geometryBlob->GetBufferPointer(),
										 geometryBlob->GetBufferSize(),
										 nullptr, &m_lineGeometryShaderThin3D );

	if ( !CheckHR( hr, "create thin 3D line geometry shader" ) )
	{
		geometryBlob->Release();
		return;
	}

	geometryBlob->Release();

	//
	// Compile thick line geometry shader for 3D

	hr = D3DCompile( LINE_GEOMETRY_3D_THICK_SHADER_SOURCE_HLSL,
					 strlen( LINE_GEOMETRY_3D_THICK_SHADER_SOURCE_HLSL ),
					 nullptr, nullptr, nullptr, "main", "gs_5_0", 0, 0,
					 &geometryBlob, &errorBlob );

	if ( !CheckHR( hr, "thick 3D line geometry shader compilation", errorBlob ) )
	{
		if ( geometryBlob )
			geometryBlob->Release();
		return;
	}

	hr = m_device->CreateGeometryShader( geometryBlob->GetBufferPointer(),
										 geometryBlob->GetBufferSize(),
										 nullptr, &m_lineGeometryShaderThick3D );

	if ( !CheckHR( hr, "create thick 3D line geometry shader" ) )
	{
		geometryBlob->Release();
		return;
	}

	geometryBlob->Release();

#ifdef _DEBUG
	AppDebugOut( "Renderer3DD3D11: 3D shaders initialized\n" );
#endif
}


void Renderer3DD3D11::Cache3DUniformLocations()
{
	// DirectX11 doesnt use uniform locations
	// Uniforms are set via constant buffers
}


// ============================================================================
// CONSTANT BUFFERS
// ============================================================================

void Renderer3DD3D11::CreateConstantBuffers()
{
	if ( !m_device )
		return;

	// Create transform constant buffer
	D3D11_BUFFER_DESC desc = {};
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.ByteWidth = sizeof( TransformBuffer );
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	HRESULT hr = m_device->CreateBuffer( &desc, nullptr, &m_transformConstantBuffer );
	if ( !CheckHR( hr, "create transform constant buffer" ) )
	{
		return;
	}

	//
	// Create fog constant buffer

	desc.ByteWidth = sizeof( FogBuffer );
	hr = m_device->CreateBuffer( &desc, nullptr, &m_fogConstantBuffer );
	if ( !CheckHR( hr, "create fog constant buffer" ) )
	{
		return;
	}

	//
	// Create model constant buffer

	desc.ByteWidth = sizeof( ModelBuffer );
	hr = m_device->CreateBuffer( &desc, nullptr, &m_modelConstantBuffer );
	if ( !CheckHR( hr, "create model constant buffer" ) )
	{
		return;
	}

	//
	// Create line width constant buffer for 3D

	desc.ByteWidth = sizeof( LineWidthBuffer3D );
	hr = m_device->CreateBuffer( &desc, nullptr, &m_lineWidthConstantBuffer3D );
	if ( !CheckHR( hr, "create line width constant buffer 3D" ) )
	{
		return;
	}

#ifdef _DEBUG
	AppDebugOut( "Renderer3DD3D11: Constant buffers created\n" );
#endif
}


void Renderer3DD3D11::UpdateTransformBuffer()
{
	if ( !m_deviceContext || !m_transformConstantBuffer )
		return;

	D3D11_MAPPED_SUBRESOURCE mapped;
	HRESULT hr = m_deviceContext->Map( m_transformConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped );

	if ( SUCCEEDED( hr ) )
	{
		TransformBuffer *buffer = (TransformBuffer *)mapped.pData;

		for ( int i = 0; i < 16; i++ )
		{
			buffer->uProjection[i] = m_projectionMatrix3D.m[i];
			buffer->uModelView[i] = m_modelViewMatrix3D.m[i];
		}

		m_deviceContext->Unmap( m_transformConstantBuffer, 0 );
	}
}


void Renderer3DD3D11::UpdateFogBuffer()
{
	if ( !m_deviceContext || !m_fogConstantBuffer )
		return;

	D3D11_MAPPED_SUBRESOURCE mapped;
	HRESULT hr = m_deviceContext->Map( m_fogConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped );

	if ( SUCCEEDED( hr ) )
	{
		FogBuffer *buffer = (FogBuffer *)mapped.pData;

		buffer->uFogEnabled = m_fogEnabled ? 1 : 0;
		buffer->uFogOrientationBased = m_fogOrientationBased ? 1 : 0;
		buffer->uFogStart = m_fogStart;
		buffer->uFogEnd = m_fogEnd;
		buffer->uFogColor[0] = m_fogColor[0];
		buffer->uFogColor[1] = m_fogColor[1];
		buffer->uFogColor[2] = m_fogColor[2];
		buffer->uFogColor[3] = m_fogColor[3];
		buffer->uCameraPos[0] = m_cameraPos[0];
		buffer->uCameraPos[1] = m_cameraPos[1];
		buffer->uCameraPos[2] = m_cameraPos[2];
		buffer->padding = 0.0f;

		m_deviceContext->Unmap( m_fogConstantBuffer, 0 );
	}
}


void Renderer3DD3D11::UpdateModelBuffer( const Matrix4f &modelMatrix, const Colour &modelColor )
{
	if ( !m_deviceContext || !m_modelConstantBuffer )
		return;

	D3D11_MAPPED_SUBRESOURCE mapped;
	HRESULT hr = m_deviceContext->Map( m_modelConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped );

	if ( SUCCEEDED( hr ) )
	{
		ModelBuffer *buffer = (ModelBuffer *)mapped.pData;

		//
		// Copy model matrix

		for ( int i = 0; i < 16; i++ )
		{
			buffer->uModelMatrix[i] = modelMatrix.m[i];
		}

		//
		// Copy model color

		buffer->uModelColor[0] = modelColor.GetRFloat();
		buffer->uModelColor[1] = modelColor.GetGFloat();
		buffer->uModelColor[2] = modelColor.GetBFloat();
		buffer->uModelColor[3] = modelColor.GetAFloat();

		buffer->uUseInstancing = 0;
		buffer->uInstanceCount = 0;
		buffer->padding1 = 0.0f;
		buffer->padding2 = 0.0f;

		m_deviceContext->Unmap( m_modelConstantBuffer, 0 );
	}
}


void Renderer3DD3D11::UpdateModelBufferInstanced( const Matrix4f *modelMatrices, const Colour *modelColors, int instanceCount )
{
	if ( !m_deviceContext || !m_modelConstantBuffer )
		return;
	if ( instanceCount > MAX_INSTANCES )
		return;

	D3D11_MAPPED_SUBRESOURCE mapped;
	HRESULT hr = m_deviceContext->Map( m_modelConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped );

	if ( SUCCEEDED( hr ) )
	{
		ModelBuffer *buffer = (ModelBuffer *)mapped.pData;

		buffer->uUseInstancing = 1;
		buffer->uInstanceCount = instanceCount;

		//
		// Copy instance data

		for ( int i = 0; i < instanceCount && i < MAX_INSTANCES; i++ )
		{
			for ( int j = 0; j < 16; j++ )
			{
				buffer->uModelMatrices[i * 16 + j] = modelMatrices[i].m[j];
			}

			buffer->uModelColors[i * 4 + 0] = modelColors[i].GetRFloat();
			buffer->uModelColors[i * 4 + 1] = modelColors[i].GetGFloat();
			buffer->uModelColors[i * 4 + 2] = modelColors[i].GetBFloat();
			buffer->uModelColors[i * 4 + 3] = modelColors[i].GetAFloat();
		}

		m_deviceContext->Unmap( m_modelConstantBuffer, 0 );
	}
}


// ============================================================================
// SHADER UNIFORMS
// ============================================================================

void Renderer3DD3D11::Set3DShaderUniforms()
{
	if ( m_matrices3DNeedUpdate || m_currentShaderProgram3D != m_shader3DProgram )
	{
		UpdateTransformBuffer();
	}

	if ( m_fog3DNeedsUpdate || m_currentShaderProgram3D != m_shader3DProgram )
	{
		UpdateFogBuffer();
	}

	if ( m_deviceContext )
	{
		m_deviceContext->VSSetShader( m_vertexShader3D, nullptr, 0 );
		m_deviceContext->PSSetShader( m_pixelShader3D, nullptr, 0 );
		m_deviceContext->IASetInputLayout( m_inputLayout3D );

		ID3D11Buffer *constantBuffers[] = { m_transformConstantBuffer, m_fogConstantBuffer };
		m_deviceContext->VSSetConstantBuffers( 0, 2, constantBuffers );
		m_deviceContext->PSSetConstantBuffers( 0, 2, constantBuffers );

		//
		// Clear geometry shader and its constant buffers

		m_deviceContext->GSSetShader( nullptr, nullptr, 0 );
		ID3D11Buffer *nullBuffer = nullptr;
		m_deviceContext->GSSetConstantBuffers( 0, 1, &nullBuffer );
		m_deviceContext->GSSetConstantBuffers( 1, 1, &nullBuffer );
		m_deviceContext->GSSetConstantBuffers( 2, 1, &nullBuffer );

		//
		// Clear texture binding and invalidate BindTexture cache

		ID3D11ShaderResourceView *nullSRV = nullptr;
		m_deviceContext->PSSetShaderResources( 0, 1, &nullSRV );
		m_currentTextureID = 0;
		m_currentTextureSRV = nullptr;

		//
		// Ensure render states are applied
		// blend, depth, rasterizer

		RendererD3D11 *rendererD3D11 = dynamic_cast<RendererD3D11 *>( g_renderer );
		if ( rendererD3D11 )
		{
			rendererD3D11->UpdateBlendState();
			rendererD3D11->UpdateDepthState();
			rendererD3D11->UpdateRasterizerState();
		}
	}

	m_currentShaderProgram3D = m_shader3DProgram;
	m_matrices3DNeedUpdate = false;
	m_fog3DNeedsUpdate = false;
}


void Renderer3DD3D11::SetLineShaderUniforms3D( float lineWidth )
{
	if ( m_matrices3DNeedUpdate || m_currentShaderProgram3D != m_shader3DProgram )
	{
		UpdateTransformBuffer();
	}

	if ( m_fog3DNeedsUpdate || m_currentShaderProgram3D != m_shader3DProgram )
	{
		UpdateFogBuffer();
	}

	if ( m_deviceContext )
	{
		m_deviceContext->VSSetShader( m_vertexShader3D, nullptr, 0 );
		m_deviceContext->PSSetShader( m_pixelShader3D, nullptr, 0 );
		m_deviceContext->IASetInputLayout( m_inputLayout3D );

		ID3D11Buffer *constantBuffers[] = { m_transformConstantBuffer, m_fogConstantBuffer };
		m_deviceContext->VSSetConstantBuffers( 0, 2, constantBuffers );
		m_deviceContext->PSSetConstantBuffers( 0, 2, constantBuffers );

		//
		// For line width 1, use normal line rendering without geometry shader

		if ( lineWidth == 1.0f )
		{
			//
			// Clear geometry shader for normal lines

			m_deviceContext->GSSetShader( nullptr, nullptr, 0 );
			ID3D11Buffer *nullBuffer = nullptr;
			m_deviceContext->GSSetConstantBuffers( 0, 1, &nullBuffer );
			m_deviceContext->GSSetConstantBuffers( 1, 1, &nullBuffer );
			m_deviceContext->GSSetConstantBuffers( 2, 1, &nullBuffer );
		}
		else
		{
			//
			// Update and bind line width constant buffer

			D3D11_MAPPED_SUBRESOURCE mappedResource;
			HRESULT hr = m_deviceContext->Map( m_lineWidthConstantBuffer3D, 0,
											   D3D11_MAP_WRITE_DISCARD, 0, &mappedResource );

			if ( SUCCEEDED( hr ) )
			{
				LineWidthBuffer3D *lineWidthData = (LineWidthBuffer3D *)mappedResource.pData;
				lineWidthData->lineWidth = lineWidth;
				lineWidthData->viewportWidth = (float)g_windowManager->GetPhysicalWidth();
				lineWidthData->viewportHeight = (float)g_windowManager->GetPhysicalHeight();
				lineWidthData->padding = 0.0f;

				m_deviceContext->Unmap( m_lineWidthConstantBuffer3D, 0 );
			}

			ID3D11GeometryShader *geometryShader = ( lineWidth < 1.8f ) ? m_lineGeometryShaderThin3D : m_lineGeometryShaderThick3D;

			m_deviceContext->GSSetShader( geometryShader, nullptr, 0 );

			//
			// Bind constant buffers to geometry shader
			// b0 = TransformBuffer, b1 = FogBuffer, b2 = LineWidthBuffer

			ID3D11Buffer *gsConstantBuffers[] = { m_transformConstantBuffer, m_fogConstantBuffer, m_lineWidthConstantBuffer3D };
			m_deviceContext->GSSetConstantBuffers( 0, 3, gsConstantBuffers );
		}

		//
		// Clear texture binding and invalidate BindTexture cache
		// so subsequent sprite flushes don't skip PSSetShaderResources

		ID3D11ShaderResourceView *nullSRV = nullptr;
		m_deviceContext->PSSetShaderResources( 0, 1, &nullSRV );
		m_currentTextureID = 0;
		m_currentTextureSRV = nullptr;

		//
		// Ensure render states are applied

		RendererD3D11 *rendererD3D11 = dynamic_cast<RendererD3D11 *>( g_renderer );
		if ( rendererD3D11 )
		{
			rendererD3D11->UpdateBlendState();
			rendererD3D11->UpdateDepthState();
			rendererD3D11->UpdateRasterizerState();
		}
	}

	m_currentShaderProgram3D = m_shader3DProgram;
	m_matrices3DNeedUpdate = false;
	m_fog3DNeedsUpdate = false;
}


void Renderer3DD3D11::SetTextured3DShaderUniforms()
{
	if ( m_matrices3DNeedUpdate || m_currentShaderProgram3D != m_shader3DTexturedProgram )
	{
		UpdateTransformBuffer();
	}

	if ( m_fog3DNeedsUpdate || m_currentShaderProgram3D != m_shader3DTexturedProgram )
	{
		UpdateFogBuffer();
	}

	if ( m_deviceContext )
	{
		m_deviceContext->GSSetShader( nullptr, nullptr, 0 );
		ID3D11Buffer *nullBuffer = nullptr;

		m_deviceContext->GSSetConstantBuffers( 0, 1, &nullBuffer );
		m_deviceContext->GSSetConstantBuffers( 1, 1, &nullBuffer );
		m_deviceContext->GSSetConstantBuffers( 2, 1, &nullBuffer );

		m_deviceContext->VSSetShader( m_texturedVertexShader3D, nullptr, 0 );
		m_deviceContext->PSSetShader( m_texturedPixelShader3D, nullptr, 0 );
		m_deviceContext->IASetInputLayout( m_inputLayoutTextured3D );

		ID3D11Buffer *constantBuffers[] = { m_transformConstantBuffer, m_fogConstantBuffer };
		m_deviceContext->VSSetConstantBuffers( 0, 2, constantBuffers );
		m_deviceContext->PSSetConstantBuffers( 0, 2, constantBuffers );
		
		m_deviceContext->VSSetConstantBuffers( 2, 1, &nullBuffer );
		m_deviceContext->PSSetConstantBuffers( 2, 1, &nullBuffer );

		RendererD3D11 *rendererD3D11 = dynamic_cast<RendererD3D11 *>( g_renderer );
		if ( rendererD3D11 )
		{

			unsigned int boundTexture = rendererD3D11->GetCurrentBoundTexture();
			if ( boundTexture != 0 )
			{
				ID3D11ShaderResourceView *srv = rendererD3D11->GetTextureSRV( boundTexture );
				if ( srv )
				{
					m_deviceContext->PSSetShaderResources( 0, 1, &srv );
				}
			}
			else if ( m_currentTextureSRV != nullptr && m_currentTextureID != 0 )
			{
				m_deviceContext->PSSetShaderResources( 0, 1, &m_currentTextureSRV );
			}
			else
			{
				ID3D11ShaderResourceView *nullSRV = nullptr;
				m_deviceContext->PSSetShaderResources( 0, 1, &nullSRV );
			}

			rendererD3D11->UpdateBlendState();
			rendererD3D11->UpdateDepthState();
			rendererD3D11->UpdateRasterizerState();
		}
	}

	m_currentShaderProgram3D = m_shader3DTexturedProgram;
	m_matrices3DNeedUpdate = false;
	m_fog3DNeedsUpdate = false;
}


void Renderer3DD3D11::SetFogUniforms3D( unsigned int shaderProgram )
{
	UpdateFogBuffer();
}


void Renderer3DD3D11::Set3DModelShaderUniforms( const Matrix4f &modelMatrix, const Colour &modelColor )
{
	if ( m_matrices3DNeedUpdate || m_currentShaderProgram3D != m_shader3DModelProgram )
	{
		UpdateTransformBuffer();
	}

	if ( m_fog3DNeedsUpdate || m_currentShaderProgram3D != m_shader3DModelProgram )
	{
		UpdateFogBuffer();
	}

	UpdateModelBuffer( modelMatrix, modelColor );

	if ( m_deviceContext )
	{
		m_deviceContext->GSSetShader( nullptr, nullptr, 0 );
		ID3D11Buffer *nullBuffer = nullptr;
		m_deviceContext->GSSetConstantBuffers( 0, 1, &nullBuffer );
		m_deviceContext->GSSetConstantBuffers( 1, 1, &nullBuffer );
		m_deviceContext->GSSetConstantBuffers( 2, 1, &nullBuffer );

		m_deviceContext->VSSetShader( m_modelVertexShader3D, nullptr, 0 );
		m_deviceContext->PSSetShader( m_modelPixelShader3D, nullptr, 0 );
		m_deviceContext->IASetInputLayout( m_inputLayout3D );

		ID3D11Buffer *constantBuffers[] = { m_transformConstantBuffer, m_fogConstantBuffer, m_modelConstantBuffer };
		m_deviceContext->VSSetConstantBuffers( 0, 3, constantBuffers );
		m_deviceContext->PSSetConstantBuffers( 0, 3, constantBuffers );

		RendererD3D11 *rendererD3D11 = dynamic_cast<RendererD3D11 *>( g_renderer );
		if ( rendererD3D11 )
		{
			rendererD3D11->UpdateBlendState();
			rendererD3D11->UpdateDepthState();
			rendererD3D11->UpdateRasterizerState();
		}
	}

	m_currentShaderProgram3D = m_shader3DModelProgram;
	m_matrices3DNeedUpdate = false;
	m_fog3DNeedsUpdate = false;
}


void Renderer3DD3D11::Set3DModelShaderUniformsInstanced( const Matrix4f *modelMatrices,
														 const Colour *modelColors,
														 int instanceCount )
{
	if ( m_matrices3DNeedUpdate || m_currentShaderProgram3D != m_shader3DModelProgram )
	{
		UpdateTransformBuffer();
	}

	if ( m_fog3DNeedsUpdate || m_currentShaderProgram3D != m_shader3DModelProgram )
	{
		UpdateFogBuffer();
	}

	UpdateModelBufferInstanced( modelMatrices, modelColors, instanceCount );

	if ( m_deviceContext )
	{
		m_deviceContext->GSSetShader( nullptr, nullptr, 0 );
		ID3D11Buffer *nullBuffer = nullptr;
		m_deviceContext->GSSetConstantBuffers( 0, 1, &nullBuffer );
		m_deviceContext->GSSetConstantBuffers( 1, 1, &nullBuffer );
		m_deviceContext->GSSetConstantBuffers( 2, 1, &nullBuffer );

		m_deviceContext->VSSetShader( m_modelVertexShader3D, nullptr, 0 );
		m_deviceContext->PSSetShader( m_modelPixelShader3D, nullptr, 0 );
		m_deviceContext->IASetInputLayout( m_inputLayout3D );

		ID3D11Buffer *constantBuffers[] = { m_transformConstantBuffer, m_fogConstantBuffer, m_modelConstantBuffer };
		m_deviceContext->VSSetConstantBuffers( 0, 3, constantBuffers );
		m_deviceContext->PSSetConstantBuffers( 0, 3, constantBuffers );

		RendererD3D11 *rendererD3D11 = dynamic_cast<RendererD3D11 *>( g_renderer );
		if ( rendererD3D11 )
		{
			rendererD3D11->UpdateBlendState();
			rendererD3D11->UpdateDepthState();
			rendererD3D11->UpdateRasterizerState();
		}
	}

	m_currentShaderProgram3D = m_shader3DModelProgram;
	m_matrices3DNeedUpdate = false;
	m_fog3DNeedsUpdate = false;
}


// ============================================================================
// VERTEX ARRAY SETUP
// ============================================================================

void Renderer3DD3D11::Setup3DVertexArrays()
{
	if ( !m_device )
		return;

	D3D11_BUFFER_DESC desc = {};
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	//
	// Line buffer

	desc.ByteWidth = sizeof( Vertex3D ) * MAX_LINE_VERTICES_3D;
	HRESULT hr = m_device->CreateBuffer( &desc, nullptr, &m_lineBuffer3D );
	CheckHR( hr, "create 3D line buffer" );

	//
	// Circle buffer

	desc.ByteWidth = sizeof( Vertex3D ) * MAX_CIRCLE_VERTICES_3D;
	hr = m_device->CreateBuffer( &desc, nullptr, &m_circleBuffer3D );
	CheckHR( hr, "create 3D circle buffer" );

	//
	// Circle fill buffer

	desc.ByteWidth = sizeof( Vertex3D ) * MAX_CIRCLE_FILL_VERTICES_3D;
	hr = m_device->CreateBuffer( &desc, nullptr, &m_circleFillBuffer3D );
	CheckHR( hr, "create 3D circle fill buffer" );

	//
	// Rect buffer

	desc.ByteWidth = sizeof( Vertex3D ) * MAX_RECT_VERTICES_3D;
	hr = m_device->CreateBuffer( &desc, nullptr, &m_rectBuffer3D );
	CheckHR( hr, "create 3D rect buffer" );

	//
	// Rect fill buffer

	desc.ByteWidth = sizeof( Vertex3D ) * MAX_RECT_FILL_VERTICES_3D;
	hr = m_device->CreateBuffer( &desc, nullptr, &m_rectFillBuffer3D );
	CheckHR( hr, "create 3D rect fill buffer" );

	//
	// Triangle fill buffer

	desc.ByteWidth = sizeof( Vertex3D ) * MAX_TRIANGLE_FILL_VERTICES_3D;
	hr = m_device->CreateBuffer( &desc, nullptr, &m_triangleFillBuffer3D );
	CheckHR( hr, "create 3D triangle fill buffer" );
}


void Renderer3DD3D11::Setup3DTexturedVertexArrays()
{
	if ( !m_device )
		return;

	//
	// Create persistent textured buffers

	D3D11_BUFFER_DESC desc = {};
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	//
	// Static sprite buffer

	desc.ByteWidth = sizeof( Vertex3DTextured ) * MAX_STATIC_SPRITE_VERTICES_3D;
	HRESULT hr = m_device->CreateBuffer( &desc, nullptr, &m_staticSpriteBuffer3D );
	CheckHR( hr, "create 3D static sprite buffer" );

	//
	// Rotating sprite buffer

	desc.ByteWidth = sizeof( Vertex3DTextured ) * MAX_ROTATING_SPRITE_VERTICES_3D;
	hr = m_device->CreateBuffer( &desc, nullptr, &m_rotatingSpriteBuffer3D );
	CheckHR( hr, "create 3D rotating sprite buffer" );

	//
	// Text buffer

	desc.ByteWidth = sizeof( Vertex3DTextured ) * MAX_TEXT_VERTICES_3D;
	hr = m_device->CreateBuffer( &desc, nullptr, &m_textBuffer3D );
	CheckHR( hr, "create 3D text buffer" );
}


bool Renderer3DD3D11::UpdateBufferData( ID3D11Buffer *buffer, const Vertex3D *vertices, int vertexCount )
{
	if ( !m_deviceContext || !buffer || vertexCount <= 0 || !vertices )
	{
		return false;
	}

	D3D11_MAPPED_SUBRESOURCE mapped;
	HRESULT hr = m_deviceContext->Map( buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped );

	if ( SUCCEEDED( hr ) )
	{
		memcpy( mapped.pData, vertices, vertexCount * sizeof( Vertex3D ) );
		m_deviceContext->Unmap( buffer, 0 );
		return true;
	}

	return false;
}


bool Renderer3DD3D11::UpdateBufferData( ID3D11Buffer *buffer, const Vertex3DTextured *vertices, int vertexCount )
{
	if ( !m_deviceContext || !buffer || vertexCount <= 0 || !vertices )
	{
		return false;
	}

	D3D11_MAPPED_SUBRESOURCE mapped;
	HRESULT hr = m_deviceContext->Map( buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped );

	if ( SUCCEEDED( hr ) )
	{
		memcpy( mapped.pData, vertices, vertexCount * sizeof( Vertex3DTextured ) );
		m_deviceContext->Unmap( buffer, 0 );
		return true;
	}

	return false;
}


// ============================================================================
// VERTEX DATA UPLOAD
// ============================================================================

void Renderer3DD3D11::UploadVertexDataTo3DVBO( unsigned int vbo, const Vertex3D *vertices,
											   int vertexCount, unsigned int usageHint )
{
	// This is handled by the MegaVBO system
	// Individual VBO uploads are not used in the same way as OpenGL
}


void Renderer3DD3D11::UploadVertexDataTo3DVBO( unsigned int vbo, const Vertex3DTextured *vertices,
											   int vertexCount, unsigned int usageHint )
{
	// This is handled by the MegaVBO system
}


// ============================================================================
// FLUSH FUNCTIONS
// ============================================================================

void Renderer3DD3D11::FlushLine3D( bool isImmediate )
{
	if ( m_lineVertexCount3D == 0 )
		return;

	if ( isImmediate )
	{
		g_renderer->StartFlushTiming( "Immediate_Lines_3D" );
		IncrementDrawCall3D( DRAW_CALL_IMMEDIATE_LINES );
	}
	else
	{
		g_renderer->StartFlushTiming( "Batched_Lines_3D" );
		IncrementDrawCall3D( DRAW_CALL_LINES );
	}

	g_renderer->SetDepthMask( false );

	if ( m_currentLineWidth3D == 1.0f )
	{
		Set3DShaderUniforms();
	}
	else
	{
		SetLineShaderUniforms3D( m_currentLineWidth3D );
	}

	if ( m_lineBuffer3D && UpdateBufferData( m_lineBuffer3D, m_lineVertices3D, m_lineVertexCount3D ) )
	{
		UINT stride = sizeof( Vertex3D );
		UINT offset = 0;
		m_deviceContext->IASetVertexBuffers( 0, 1, &m_lineBuffer3D, &stride, &offset );
		m_deviceContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_LINELIST );
		m_deviceContext->Draw( m_lineVertexCount3D, 0 );
	}

	g_renderer->SetDepthMask( true );

	m_lineVertexCount3D = 0;
	
	if ( isImmediate )
	{
		g_renderer->EndFlushTiming( "Immediate_Lines_3D" );
	}
	else
	{
		g_renderer->EndFlushTiming( "Batched_Lines_3D" );
	}
}


void Renderer3DD3D11::FlushStaticSprites3D( bool isImmediate )
{
	if ( m_staticSpriteVertexCount3D == 0 )
		return;

	if ( isImmediate )
	{
		g_renderer->StartFlushTiming( "Immediate_Static_Sprite_3D" );
		IncrementDrawCall3D( DRAW_CALL_IMMEDIATE_STATIC_SPRITES );
	}
	else
	{
		g_renderer->StartFlushTiming( "Static_Sprite_3D" );
		IncrementDrawCall3D( DRAW_CALL_STATIC_SPRITES );
	}

	bool previousDepthMask = g_renderer->GetDepthMask();
	g_renderer->SetDepthMask( false );

	BindTexture( m_currentStaticSpriteTexture3D );
	SetTextured3DShaderUniforms();

	if ( m_staticSpriteBuffer3D && UpdateBufferData( m_staticSpriteBuffer3D, m_staticSpriteVertices3D, m_staticSpriteVertexCount3D ) )
	{
		UINT stride = sizeof( Vertex3DTextured );
		UINT offset = 0;
		m_deviceContext->IASetVertexBuffers( 0, 1, &m_staticSpriteBuffer3D, &stride, &offset );
		m_deviceContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
		m_deviceContext->Draw( m_staticSpriteVertexCount3D, 0 );
	}

	g_renderer->SetDepthMask( previousDepthMask );

	m_staticSpriteVertexCount3D = 0;
	
	if ( isImmediate )
	{
		g_renderer->EndFlushTiming( "Immediate_Static_Sprite_3D" );
	}
	else
	{
		g_renderer->EndFlushTiming( "Static_Sprite_3D" );
	}
}


void Renderer3DD3D11::FlushRotatingSprite3D( bool isImmediate )
{
	if ( m_rotatingSpriteVertexCount3D == 0 )
		return;

	if ( isImmediate )
	{
		g_renderer->StartFlushTiming( "Immediate_Rotating_Sprite_3D" );
		IncrementDrawCall3D( DRAW_CALL_IMMEDIATE_ROTATING_SPRITES );
	}
	else
	{
		g_renderer->StartFlushTiming( "Rotating_Sprite_3D" );
		IncrementDrawCall3D( DRAW_CALL_ROTATING_SPRITES );
	}

	bool previousDepthMask = g_renderer->GetDepthMask();
	g_renderer->SetDepthMask( false );

	BindTexture( m_currentRotatingSpriteTexture3D );
	SetTextured3DShaderUniforms();

	if ( m_rotatingSpriteBuffer3D && UpdateBufferData( m_rotatingSpriteBuffer3D, m_rotatingSpriteVertices3D, m_rotatingSpriteVertexCount3D ) )
	{
		UINT stride = sizeof( Vertex3DTextured );
		UINT offset = 0;
		m_deviceContext->IASetVertexBuffers( 0, 1, &m_rotatingSpriteBuffer3D, &stride, &offset );
		m_deviceContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
		m_deviceContext->Draw( m_rotatingSpriteVertexCount3D, 0 );
	}

	g_renderer->SetDepthMask( previousDepthMask );

	m_rotatingSpriteVertexCount3D = 0;

	if ( isImmediate )
	{
		g_renderer->EndFlushTiming( "Immediate_Rotating_Sprite_3D" );
	}
	else
	{
		g_renderer->EndFlushTiming( "Rotating_Sprite_3D" );
	}
}


void Renderer3DD3D11::FlushTextBuffer3D( bool isImmediate )
{
	if ( m_textVertexCount3D == 0 )
		return;

	if ( isImmediate )
	{
		g_renderer->StartFlushTiming( "Immediate_Text_3D" );
		IncrementDrawCall3D( DRAW_CALL_IMMEDIATE_TEXT );
	}
	else
	{
		g_renderer->StartFlushTiming( "Text_3D" );
		IncrementDrawCall3D( DRAW_CALL_TEXT );
	}

	//
	// Save current blend state

	int currentBlendSrc = g_renderer->GetCurrentBlendSrcFactor();
	int currentBlendDst = g_renderer->GetCurrentBlendDstFactor();

	//
	// Disable depth writing for text

	g_renderer->SetDepthMask( false );

	//
	// Set additive blending for text rendering, unless subtractive is active

	if ( g_renderer->GetBlendMode() != Renderer::BlendModeSubtractive )
	{
		g_renderer->SetBlendMode( Renderer::BlendModeAdditive );
	}

	if ( m_currentTextTexture3D != 0 )
	{
		BindTexture( m_currentTextTexture3D );
		g_renderer->SetActiveTexture( 0 );
		g_renderer->SetBoundTexture( m_currentTextTexture3D );
	}
	else
	{
		m_currentTextureSRV = nullptr;
		m_currentTextureID = 0;
	}

	SetTextured3DShaderUniforms();

	if ( m_textBuffer3D && UpdateBufferData( m_textBuffer3D, m_textVertices3D, m_textVertexCount3D ) )
	{
		UINT stride = sizeof( Vertex3DTextured );
		UINT offset = 0;
		m_deviceContext->IASetVertexBuffers( 0, 1, &m_textBuffer3D, &stride, &offset );
		m_deviceContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
		m_deviceContext->Draw( m_textVertexCount3D, 0 );
	}

	g_renderer->SetBlendFunc( currentBlendSrc, currentBlendDst );
	g_renderer->SetDepthMask( true );

	m_textVertexCount3D = 0;
	m_currentTextTexture3D = 0;

	RendererD3D11 *rendererD3D11 = dynamic_cast<RendererD3D11 *>( g_renderer );
	if ( rendererD3D11 )
	{
		rendererD3D11->SetBoundTexture( 0 );
	}
	m_currentTextureSRV = nullptr;
	m_currentTextureID = 0;

	if ( isImmediate )
	{
		g_renderer->EndFlushTiming( "Immediate_Text_3D" );
	}
	else
	{
		g_renderer->EndFlushTiming( "Text_3D" );
	}
}


void Renderer3DD3D11::FlushCircles3D( bool isImmediate )
{
	if ( m_circleVertexCount3D == 0 )
		return;

	if ( isImmediate )
	{
		g_renderer->StartFlushTiming( "Immediate_Circles_3D" );
		IncrementDrawCall3D( DRAW_CALL_IMMEDIATE_CIRCLES );
	}
	else
	{
		g_renderer->StartFlushTiming( "Circles_3D" );
		IncrementDrawCall3D( DRAW_CALL_CIRCLES );
	}

	Set3DShaderUniforms();

	if ( m_circleBuffer3D && UpdateBufferData( m_circleBuffer3D, m_circleVertices3D, m_circleVertexCount3D ) )
	{
		UINT stride = sizeof( Vertex3D );
		UINT offset = 0;
		m_deviceContext->IASetVertexBuffers( 0, 1, &m_circleBuffer3D, &stride, &offset );
		m_deviceContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_LINELIST );
		m_deviceContext->Draw( m_circleVertexCount3D, 0 );
	}

	m_circleVertexCount3D = 0;
	
	if ( isImmediate )
	{
		g_renderer->EndFlushTiming( "Immediate_Circles_3D" );
	}
	else
	{
		g_renderer->EndFlushTiming( "Circles_3D" );
	}
}


void Renderer3DD3D11::FlushCircleFills3D( bool isImmediate )
{
	if ( m_circleFillVertexCount3D == 0 )
		return;

	if ( isImmediate )
	{
		g_renderer->StartFlushTiming( "Immediate_Circle_Fills_3D" );
		IncrementDrawCall3D( DRAW_CALL_IMMEDIATE_CIRCLE_FILLS );
	}
	else
	{
		g_renderer->StartFlushTiming( "Circle_Fills_3D" );
		IncrementDrawCall3D( DRAW_CALL_CIRCLE_FILLS );
	}

	Set3DShaderUniforms();

	if ( m_circleFillBuffer3D && UpdateBufferData( m_circleFillBuffer3D, m_circleFillVertices3D, m_circleFillVertexCount3D ) )
	{
		UINT stride = sizeof( Vertex3D );
		UINT offset = 0;
		m_deviceContext->IASetVertexBuffers( 0, 1, &m_circleFillBuffer3D, &stride, &offset );
		m_deviceContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
		m_deviceContext->Draw( m_circleFillVertexCount3D, 0 );
	}

	m_circleFillVertexCount3D = 0;
	
	if ( isImmediate )
	{
		g_renderer->EndFlushTiming( "Immediate_Circle_Fills_3D" );
	}
	else
	{
		g_renderer->EndFlushTiming( "Circle_Fills_3D" );
	}
}


void Renderer3DD3D11::FlushRects3D( bool isImmediate )
{
	if ( m_rectVertexCount3D == 0 )
		return;

	if ( isImmediate )
	{
		g_renderer->StartFlushTiming( "Immediate_Rects_3D" );
		IncrementDrawCall3D( DRAW_CALL_IMMEDIATE_RECTS );
	}
	else
	{
		g_renderer->StartFlushTiming( "Rects_3D" );
		IncrementDrawCall3D( DRAW_CALL_RECTS );
	}

	Set3DShaderUniforms();

	if ( m_rectBuffer3D && UpdateBufferData( m_rectBuffer3D, m_rectVertices3D, m_rectVertexCount3D ) )
	{
		UINT stride = sizeof( Vertex3D );
		UINT offset = 0;
		m_deviceContext->IASetVertexBuffers( 0, 1, &m_rectBuffer3D, &stride, &offset );
		m_deviceContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_LINELIST );
		m_deviceContext->Draw( m_rectVertexCount3D, 0 );
	}

	m_rectVertexCount3D = 0;
	
	if ( isImmediate )
	{
		g_renderer->EndFlushTiming( "Immediate_Rects_3D" );
	}
	else
	{
		g_renderer->EndFlushTiming( "Rects_3D" );
	}
}


void Renderer3DD3D11::FlushRectFills3D( bool isImmediate )
{
	if ( m_rectFillVertexCount3D == 0 )
		return;

	if ( isImmediate )
	{
		g_renderer->StartFlushTiming( "Immediate_Rect_Fills_3D" );
		IncrementDrawCall3D( DRAW_CALL_IMMEDIATE_RECT_FILLS );
	}
	else
	{
		g_renderer->StartFlushTiming( "Rect_Fills_3D" );
		IncrementDrawCall3D( DRAW_CALL_RECT_FILLS );
	}

	Set3DShaderUniforms();

	if ( m_rectFillBuffer3D && UpdateBufferData( m_rectFillBuffer3D, m_rectFillVertices3D, m_rectFillVertexCount3D ) )
	{
		UINT stride = sizeof( Vertex3D );
		UINT offset = 0;
		m_deviceContext->IASetVertexBuffers( 0, 1, &m_rectFillBuffer3D, &stride, &offset );
		m_deviceContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
		m_deviceContext->Draw( m_rectFillVertexCount3D, 0 );
	}

	m_rectFillVertexCount3D = 0;

	if ( isImmediate )
	{
		g_renderer->EndFlushTiming( "Immediate_Rect_Fills_3D" );
	}
	else
	{
		g_renderer->EndFlushTiming( "Rect_Fills_3D" );
	}
}


void Renderer3DD3D11::FlushTriangleFills3D( bool isImmediate )
{
	if ( m_triangleFillVertexCount3D == 0 )
		return;

	if ( isImmediate )
	{
		g_renderer->StartFlushTiming( "Immediate_Triangle_Fills_3D" );
		IncrementDrawCall3D( DRAW_CALL_IMMEDIATE_TRIANGLE_FILLS );
	}
	else
	{
		g_renderer->StartFlushTiming( "Triangle_Fills_3D" );
		IncrementDrawCall3D( DRAW_CALL_TRIANGLE_FILLS );
	}

	Set3DShaderUniforms();

	if ( m_triangleFillBuffer3D && UpdateBufferData( m_triangleFillBuffer3D, m_triangleFillVertices3D, m_triangleFillVertexCount3D ) )
	{
		UINT stride = sizeof( Vertex3D );
		UINT offset = 0;
		m_deviceContext->IASetVertexBuffers( 0, 1, &m_triangleFillBuffer3D, &stride, &offset );
		m_deviceContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
		m_deviceContext->Draw( m_triangleFillVertexCount3D, 0 );
	}

	m_triangleFillVertexCount3D = 0;

	if ( isImmediate )
	{
		g_renderer->EndFlushTiming( "Immediate_Triangle_Fills_3D" );
	}
	else
	{
		g_renderer->EndFlushTiming( "Triangle_Fills_3D" );
	}
}


// ============================================================================
// TEXTURE BINDING
// ============================================================================

void Renderer3DD3D11::BindTexture( unsigned int textureID )
{
	if ( textureID == m_currentTextureID && m_currentTextureSRV != nullptr )
	{
		return;
	}

	m_currentTextureID = textureID;
	
	RendererD3D11 *rendererD3D11 = dynamic_cast<RendererD3D11 *>( g_renderer );
	m_currentTextureSRV = rendererD3D11 ? rendererD3D11->GetTextureSRV( textureID ) : nullptr;

	if ( m_currentTextureSRV && m_deviceContext )
	{
		m_deviceContext->PSSetShaderResources( 0, 1, &m_currentTextureSRV );
	}
}


// ============================================================================
// VBO/IBO MANAGEMENT
// ============================================================================

void Renderer3DD3D11::RegisterVBO( unsigned int vboId, ID3D11Buffer *buffer )
{
	m_vboMap[vboId] = buffer;
}


void Renderer3DD3D11::RegisterIBO( unsigned int iboId, ID3D11Buffer *buffer )
{
	m_iboMap[iboId] = buffer;
}


ID3D11Buffer *Renderer3DD3D11::GetVBOFromID( unsigned int vbo )
{
	auto it = m_vboMap.find( vbo );
	if ( it != m_vboMap.end() )
	{
		return it->second;
	}
	return nullptr;
}


ID3D11Buffer *Renderer3DD3D11::GetIBOFromID( unsigned int ibo )
{
	auto it = m_iboMap.find( ibo );
	if ( it != m_iboMap.end() )
	{
		return it->second;
	}
	return nullptr;
}


// ============================================================================
// MEGAVBO FUNCTIONS
// ============================================================================

unsigned int Renderer3DD3D11::CreateMegaVBOVertexBuffer3D( size_t size, BufferUsageHint usageHint )
{
	if ( !m_device )
		return 0;

	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = (UINT)size;
	desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	if ( usageHint == BUFFER_USAGE_STATIC_DRAW )
	{
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.CPUAccessFlags = 0;
	}
	else
	{
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	}

	ID3D11Buffer *buffer = nullptr;
	HRESULT hr = m_device->CreateBuffer( &desc, nullptr, &buffer );

	if ( SUCCEEDED( hr ) )
	{
		unsigned int vboId = m_nextVBOId++;
		RegisterVBO( vboId, buffer );
		return vboId;
	}

	return 0;
}


unsigned int Renderer3DD3D11::CreateMegaVBOIndexBuffer3D( size_t size, BufferUsageHint usageHint )
{
	if ( !m_device )
		return 0;

	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = (UINT)size;
	desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	if ( usageHint == BUFFER_USAGE_STATIC_DRAW )
	{
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.CPUAccessFlags = 0;
	}
	else
	{
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	}

	ID3D11Buffer *buffer = nullptr;
	HRESULT hr = m_device->CreateBuffer( &desc, nullptr, &buffer );

	if ( SUCCEEDED( hr ) )
	{
		unsigned int iboId = m_nextVBOId++;
		RegisterIBO( iboId, buffer );
		return iboId;
	}

	return 0;
}


unsigned int Renderer3DD3D11::CreateMegaVBOVertexArray3D()
{
	//
	// DirectX11 doesnt have VAOs, return a simple ID

	unsigned int vaoId = m_nextVBOId++;
	return vaoId;
}


void Renderer3DD3D11::DeleteMegaVBOVertexBuffer3D( unsigned int buffer )
{
	if ( buffer == 0 )
		return;

	auto it = m_vboMap.find( buffer );
	if ( it != m_vboMap.end() )
	{
		if ( it->second )
		{
			it->second->Release();
		}
		m_vboMap.erase( it );
	}
}


void Renderer3DD3D11::DeleteMegaVBOIndexBuffer3D( unsigned int buffer )
{
	if ( buffer == 0 )
		return;

	auto it = m_iboMap.find( buffer );
	if ( it != m_iboMap.end() )
	{
		if ( it->second )
		{
			it->second->Release();
		}
		m_iboMap.erase( it );

		//
		// Remove associated line strip segments if any

		auto segmentIt = m_lineStripSegments.find( buffer );
		if ( segmentIt != m_lineStripSegments.end() )
		{
			m_lineStripSegments.erase( segmentIt );
		}
	}
}


void Renderer3DD3D11::DeleteMegaVBOVertexArray3D( unsigned int vao )
{
	if ( vao == 0 )
		return;

	auto it = m_vaoMap.find( vao );
	if ( it != m_vaoMap.end() )
	{
		m_vaoMap.erase( it );
	}
}


void Renderer3DD3D11::SetupMegaVBOVertexAttributes3D( unsigned int vao, unsigned int vbo, unsigned int ibo )
{
	VAOBinding binding;
	binding.vboId = vbo;
	binding.iboId = ibo;
	binding.vboUploaded = false;
	binding.iboUploaded = false;
	binding.isTextured = false;
	m_vaoMap[vao] = binding;

	m_currentVAO = vao;
}


void Renderer3DD3D11::SetupMegaVBOVertexAttributes3DTextured( unsigned int vao, unsigned int vbo, unsigned int ibo )
{
	VAOBinding binding;
	binding.vboId = vbo;
	binding.iboId = ibo;
	binding.vboUploaded = false;
	binding.iboUploaded = false;
	binding.isTextured = true;
	m_vaoMap[vao] = binding;

	m_currentVAO = vao;
}


void Renderer3DD3D11::UploadMegaVBOIndexData3D( unsigned int ibo, const unsigned int *indices,
												int indexCount, BufferUsageHint usageHint )
{
	if ( !m_deviceContext || indexCount <= 0 || !indices )
		return;

	ID3D11Buffer *buffer = GetIBOFromID( ibo );
	if ( !buffer )
		return;

	//
	// Check if already uploaded for static buffers

	if ( usageHint == BUFFER_USAGE_STATIC_DRAW )
	{
		for ( auto &pair : m_vaoMap )
		{
			if ( pair.second.iboId == ibo && pair.second.iboUploaded )
			{
				return;
			}
		}
	}

	size_t dataSize = indexCount * sizeof( unsigned int );

	if ( usageHint == BUFFER_USAGE_STATIC_DRAW )
	{
		m_deviceContext->UpdateSubresource( buffer, 0, nullptr, indices, 0, 0 );

		for ( auto &pair : m_vaoMap )
		{
			if ( pair.second.iboId == ibo )
			{
				pair.second.iboUploaded = true;
				break;
			}
		}
	}
	else
	{
		D3D11_MAPPED_SUBRESOURCE mapped;
		HRESULT hr = m_deviceContext->Map( buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped );
		if ( SUCCEEDED( hr ) )
		{
			memcpy( mapped.pData, indices, dataSize );
			m_deviceContext->Unmap( buffer, 0 );
		}
	}

	//
	// Check if this IBO has primitive restart enabled and needs segmentation

	if ( m_primitiveRestartEnabled )
	{
		std::vector<LineStripSegment> segments;
		unsigned int segmentStart = 0;

		for ( int i = 0; i < indexCount; i++ )
		{
			if ( indices[i] == m_primitiveRestartIndex )
			{
				if ( i > segmentStart )
				{
					LineStripSegment segment;
					segment.startIndex = segmentStart;
					segment.indexCount = i - segmentStart;
					segments.push_back( segment );
				}
				segmentStart = i + 1;
			}
		}

		//
		// Add final segment

		if ( segmentStart < (unsigned int)indexCount )
		{
			LineStripSegment segment;
			segment.startIndex = segmentStart;
			segment.indexCount = indexCount - segmentStart;
			segments.push_back( segment );
		}

		if ( !segments.empty() )
		{
			m_lineStripSegments[ibo] = segments;
		}
	}
}


void Renderer3DD3D11::UploadMegaVBOVertexData3D( unsigned int vbo, const Vertex3D *vertices,
												 int vertexCount, BufferUsageHint usageHint )
{
	if ( !m_deviceContext || vertexCount <= 0 || !vertices )
		return;

	ID3D11Buffer *buffer = GetVBOFromID( vbo );
	if ( !buffer )
		return;

	if ( usageHint == BUFFER_USAGE_STATIC_DRAW )
	{
		for ( auto &pair : m_vaoMap )
		{
			if ( pair.second.vboId == vbo && pair.second.vboUploaded )
			{
				return;
			}
		}
	}

	size_t dataSize = vertexCount * sizeof( Vertex3D );

	if ( usageHint == BUFFER_USAGE_STATIC_DRAW )
	{
		m_deviceContext->UpdateSubresource( buffer, 0, nullptr, vertices, 0, 0 );

		for ( auto &pair : m_vaoMap )
		{
			if ( pair.second.vboId == vbo )
			{
				pair.second.vboUploaded = true;
				break;
			}
		}
	}
	else
	{
		D3D11_MAPPED_SUBRESOURCE mapped;
		HRESULT hr = m_deviceContext->Map( buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped );
		if ( SUCCEEDED( hr ) )
		{
			memcpy( mapped.pData, vertices, dataSize );
			m_deviceContext->Unmap( buffer, 0 );
		}
	}
}


void Renderer3DD3D11::UploadMegaVBOVertexData3DTextured( unsigned int vbo, const Vertex3DTextured *vertices,
														 int vertexCount, BufferUsageHint usageHint )
{
	if ( !m_deviceContext || vertexCount <= 0 || !vertices )
		return;

	ID3D11Buffer *buffer = GetVBOFromID( vbo );
	if ( !buffer )
		return;

	if ( usageHint == BUFFER_USAGE_STATIC_DRAW )
	{
		for ( auto &pair : m_vaoMap )
		{
			if ( pair.second.vboId == vbo && pair.second.vboUploaded )
			{
				return;
			}
		}
	}

	size_t dataSize = vertexCount * sizeof( Vertex3DTextured );

	if ( usageHint == BUFFER_USAGE_STATIC_DRAW )
	{
		m_deviceContext->UpdateSubresource( buffer, 0, nullptr, vertices, 0, 0 );

		for ( auto &pair : m_vaoMap )
		{
			if ( pair.second.vboId == vbo )
			{
				pair.second.vboUploaded = true;
				break;
			}
		}
	}
	else
	{
		D3D11_MAPPED_SUBRESOURCE mapped;
		HRESULT hr = m_deviceContext->Map( buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped );
		if ( SUCCEEDED( hr ) )
		{
			memcpy( mapped.pData, vertices, dataSize );
			m_deviceContext->Unmap( buffer, 0 );
		}
	}
}


void Renderer3DD3D11::DrawMegaVBOIndexed3D( PrimitiveType primitiveType, unsigned int indexCount )
{
	if ( !m_deviceContext || indexCount == 0 )
		return;

	auto vaoIt = m_vaoMap.find( m_currentVAO );
	if ( vaoIt == m_vaoMap.end() )
	{
		if ( m_vaoMap.empty() )
			return;
		vaoIt = m_vaoMap.begin();
		m_currentVAO = vaoIt->first;
	}

	ID3D11Buffer *vbo = GetVBOFromID( vaoIt->second.vboId );
	ID3D11Buffer *ibo = GetIBOFromID( vaoIt->second.iboId );

	if ( !vbo || !ibo )
		return;

	//
	// Set appropriate shader and input layout based on vertex type

	if ( vaoIt->second.isTextured )
	{
		m_deviceContext->IASetInputLayout( m_inputLayoutTextured3D );
	}
	else
	{
		m_deviceContext->IASetInputLayout( m_inputLayout3D );
	}

	D3D11_PRIMITIVE_TOPOLOGY topology;
	switch ( primitiveType )
	{
		case PRIMITIVE_TRIANGLES:
			topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			break;
		case PRIMITIVE_LINE_STRIP:
			topology = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
			break;
		case PRIMITIVE_LINES:
			topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
			break;
		default:
			topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			break;
	}

	//
	// Set vertex and index buffers

	UINT stride = vaoIt->second.isTextured ? sizeof( Vertex3DTextured ) : sizeof( Vertex3D );
	UINT offset = 0;
	m_deviceContext->IASetVertexBuffers( 0, 1, &vbo, &stride, &offset );
	m_deviceContext->IASetIndexBuffer( ibo, DXGI_FORMAT_R32_UINT, 0 );
	m_deviceContext->IASetPrimitiveTopology( topology );

	RendererD3D11 *rendererD3D11 = dynamic_cast<RendererD3D11 *>( g_renderer );
	if ( rendererD3D11 )
	{
		rendererD3D11->UpdateBlendState();
		rendererD3D11->UpdateDepthState();
		rendererD3D11->UpdateRasterizerState();
	}

	if ( m_primitiveRestartEnabled && primitiveType == PRIMITIVE_LINE_STRIP )
	{
		auto segmentsIt = m_lineStripSegments.find( vaoIt->second.iboId );

		if ( segmentsIt != m_lineStripSegments.end() )
		{
			for ( const auto &segment : segmentsIt->second )
			{
				m_deviceContext->DrawIndexed( segment.indexCount, segment.startIndex, 0 );
			}
		}
		else
		{
			m_deviceContext->DrawIndexed( indexCount, 0, 0 );
		}
	}
	else
	{
		m_deviceContext->DrawIndexed( indexCount, 0, 0 );
	}
}


void Renderer3DD3D11::DrawMegaVBOIndexedInstanced3D( PrimitiveType primitiveType, unsigned int indexCount,
													 unsigned int instanceCount )
{
	if ( !m_deviceContext || indexCount == 0 || instanceCount == 0 )
		return;

	auto vaoIt = m_vaoMap.find( m_currentVAO );
	if ( vaoIt == m_vaoMap.end() )
		return;

	ID3D11Buffer *vbo = GetVBOFromID( vaoIt->second.vboId );
	ID3D11Buffer *ibo = GetIBOFromID( vaoIt->second.iboId );

	if ( !vbo || !ibo )
		return;

	D3D11_PRIMITIVE_TOPOLOGY topology;
	switch ( primitiveType )
	{
		case PRIMITIVE_TRIANGLES:
			topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			break;
		case PRIMITIVE_LINE_STRIP:
			topology = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
			break;
		case PRIMITIVE_LINES:
			topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
			break;
		default:
			topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			break;
	}

	//
	// Set vertex and index buffers

	UINT stride = vaoIt->second.isTextured ? sizeof( Vertex3DTextured ) : sizeof( Vertex3D );
	UINT offset = 0;
	m_deviceContext->IASetVertexBuffers( 0, 1, &vbo, &stride, &offset );
	m_deviceContext->IASetIndexBuffer( ibo, DXGI_FORMAT_R32_UINT, 0 );
	m_deviceContext->IASetPrimitiveTopology( topology );
	m_deviceContext->IASetInputLayout( m_inputLayout3D );

	RendererD3D11 *rendererD3D11 = dynamic_cast<RendererD3D11 *>( g_renderer );
	if ( rendererD3D11 )
	{
		rendererD3D11->UpdateBlendState();
		rendererD3D11->UpdateDepthState();
		rendererD3D11->UpdateRasterizerState();
	}

	m_deviceContext->DrawIndexedInstanced( indexCount, instanceCount, 0, 0, 0 );
}


void Renderer3DD3D11::SetupMegaVBOInstancedVertexAttributes3DTextured( unsigned int vao, unsigned int vbo,
																	   unsigned int ibo )
{
	//
	// For instanced rendering, we remap attributes similar to OpenGL

	VAOBinding binding;
	binding.vboId = vbo;
	binding.iboId = ibo;
	binding.vboUploaded = false;
	binding.iboUploaded = false;
	binding.isTextured = true;
	m_vaoMap[vao] = binding;

	m_currentVAO = vao;
}


void Renderer3DD3D11::UpdateCurrentVAO( unsigned int vao )
{
	if ( m_vaoMap.find( vao ) != m_vaoMap.end() )
	{
		m_currentVAO = vao;
	}
}


void Renderer3DD3D11::EnableMegaVBOPrimitiveRestart3D( unsigned int restartIndex )
{
	m_primitiveRestartEnabled = true;
	m_primitiveRestartIndex = restartIndex;
}


void Renderer3DD3D11::DisableMegaVBOPrimitiveRestart3D()
{
	m_primitiveRestartEnabled = false;
	m_primitiveRestartIndex = 0;
}


#endif // RENDERER_DIRECTX11
