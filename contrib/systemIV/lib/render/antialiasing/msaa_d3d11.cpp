#include "systemiv.h"

#ifdef RENDERER_DIRECTX11

#include "msaa_d3d11.h"
#include "lib/render/renderer.h"
#include "lib/render/renderer_d3d11.h"
#include "lib/gucci/window_manager.h"
#include "lib/gucci/window_manager_d3d11.h"
#include "lib/debug/debug_utils.h"

AntiAliasingMSAA_D3D11::AntiAliasingMSAA_D3D11( Renderer *renderer, ID3D11Device *device, ID3D11DeviceContext *deviceContext )
	: AntiAliasing( renderer ),
	  m_device( device ),
	  m_deviceContext( deviceContext ),
	  m_renderTarget( nullptr ),
	  m_renderTargetView( nullptr ),
	  m_depthStencil( nullptr ),
	  m_depthStencilView( nullptr ),
	  m_width( 0 ),
	  m_height( 0 ),
	  m_samples( 0 ),
	  m_enabled( false )
{
	if ( m_device )
		m_device->AddRef();
	if ( m_deviceContext )
		m_deviceContext->AddRef();
}


AntiAliasingMSAA_D3D11::~AntiAliasingMSAA_D3D11()
{
	Destroy();

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


void AntiAliasingMSAA_D3D11::Initialize( int width, int height, int samples )
{
	m_width = width;
	m_height = height;
	m_samples = samples;

	if ( !m_device || !m_deviceContext || samples <= 0 )
	{
		m_enabled = false;
		m_renderTarget = nullptr;
		m_renderTargetView = nullptr;
		m_depthStencil = nullptr;
		m_depthStencilView = nullptr;
		return;
	}

	Destroy();

	m_samples = samples;

	HRESULT hr;

	//
	// Check MSAA support
	// Note: On a GTX 1080, 16x MSAA is not supported

	UINT numQualityLevels = 0;
	hr = m_device->CheckMultisampleQualityLevels( DXGI_FORMAT_R8G8B8A8_UNORM, samples, &numQualityLevels );
	if ( FAILED( hr ) || numQualityLevels == 0 )
	{
		AppDebugOut( "MSAA %dx not supported, falling back to no MSAA\n", samples );
		m_enabled = false;
		return;
	}

	//
	// Create MSAA render target

	D3D11_TEXTURE2D_DESC renderTargetDesc = {};
	renderTargetDesc.Width = width;
	renderTargetDesc.Height = height;
	renderTargetDesc.MipLevels = 1;
	renderTargetDesc.ArraySize = 1;
	renderTargetDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	renderTargetDesc.SampleDesc.Count = samples;
	renderTargetDesc.SampleDesc.Quality = numQualityLevels - 1;
	renderTargetDesc.Usage = D3D11_USAGE_DEFAULT;
	renderTargetDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
	renderTargetDesc.CPUAccessFlags = 0;
	renderTargetDesc.MiscFlags = 0;

	hr = m_device->CreateTexture2D( &renderTargetDesc, nullptr, &m_renderTarget );
	if ( FAILED( hr ) )
	{
		AppDebugOut( "Failed to create MSAA render target, error: 0x%08X\n", hr );
		m_enabled = false;
		return;
	}

	hr = m_device->CreateRenderTargetView( m_renderTarget, nullptr, &m_renderTargetView );
	if ( FAILED( hr ) )
	{
		AppDebugOut( "Failed to create MSAA render target view, error: 0x%08X\n", hr );
		m_renderTarget->Release();
		m_renderTarget = nullptr;
		m_enabled = false;
		return;
	}

	//
	// Create MSAA depth stencil

	D3D11_TEXTURE2D_DESC depthDesc = {};
	depthDesc.Width = width;
	depthDesc.Height = height;
	depthDesc.MipLevels = 1;
	depthDesc.ArraySize = 1;
	depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthDesc.SampleDesc.Count = samples;
	depthDesc.SampleDesc.Quality = numQualityLevels - 1;
	depthDesc.Usage = D3D11_USAGE_DEFAULT;
	depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	depthDesc.CPUAccessFlags = 0;
	depthDesc.MiscFlags = 0;

	hr = m_device->CreateTexture2D( &depthDesc, nullptr, &m_depthStencil );
	if ( FAILED( hr ) )
	{
		AppDebugOut( "Failed to create MSAA depth stencil, error: 0x%08X\n", hr );
		m_renderTargetView->Release();
		m_renderTarget->Release();
		m_renderTargetView = nullptr;
		m_renderTarget = nullptr;
		m_enabled = false;
		return;
	}

	hr = m_device->CreateDepthStencilView( m_depthStencil, nullptr, &m_depthStencilView );
	if ( FAILED( hr ) )
	{
		AppDebugOut( "Failed to create MSAA depth stencil view, error: 0x%08X\n", hr );
		m_depthStencil->Release();
		m_renderTargetView->Release();
		m_renderTarget->Release();
		m_depthStencil = nullptr;
		m_renderTargetView = nullptr;
		m_renderTarget = nullptr;
		m_enabled = false;
		return;
	}

	m_enabled = true;

#ifdef _DEBUG
	AppDebugOut( "MSAA %dx applied successfully\n", samples );
#endif
}


void AntiAliasingMSAA_D3D11::Resize( int width, int height )
{
	if ( !m_enabled || m_samples <= 0 )
	{
		return;
	}

	if ( width == m_width && height == m_height )
	{
		return;
	}

	int preservedSamples = m_samples;

	Destroy();
	Initialize( width, height, preservedSamples );
}


void AntiAliasingMSAA_D3D11::Destroy()
{
	if ( m_renderTargetView )
	{
		m_renderTargetView->Release();
		m_renderTargetView = nullptr;
	}

	if ( m_renderTarget )
	{
		m_renderTarget->Release();
		m_renderTarget = nullptr;
	}

	if ( m_depthStencilView )
	{
		m_depthStencilView->Release();
		m_depthStencilView = nullptr;
	}

	if ( m_depthStencil )
	{
		m_depthStencil->Release();
		m_depthStencil = nullptr;
	}

	m_enabled = false;
	m_samples = 0;
	m_width = 0;
	m_height = 0;
}


void AntiAliasingMSAA_D3D11::BeginRendering()
{
	if ( !m_deviceContext )
		return;

	if ( m_enabled && m_renderTargetView )
	{
		m_deviceContext->OMSetRenderTargets( 1, &m_renderTargetView, m_depthStencilView );
	}
}


void AntiAliasingMSAA_D3D11::EndRendering()
{
	if ( m_enabled && m_renderTarget && m_deviceContext )
	{
		WindowManagerD3D11 *windowManager = dynamic_cast<WindowManagerD3D11 *>( g_windowManager );
		if ( windowManager && windowManager->GetRenderTargetView() )
		{
			//
			// Resolve MSAA render target to back buffer

			ID3D11Resource *backBuffer = nullptr;
			windowManager->GetRenderTargetView()->GetResource( &backBuffer );

			if ( backBuffer )
			{
				m_deviceContext->ResolveSubresource( backBuffer, 0, m_renderTarget, 0,
													 DXGI_FORMAT_R8G8B8A8_UNORM );
				backBuffer->Release();
			}

			//
			// Restore normal render target

			ID3D11RenderTargetView *renderTarget = windowManager->GetRenderTargetView();
			ID3D11DepthStencilView *depthStencil = windowManager->GetDepthStencilView();
			m_deviceContext->OMSetRenderTargets( 1, &renderTarget, depthStencil );
		}
	}
}


AntiAliasingRenderTargetHandle AntiAliasingMSAA_D3D11::GetRenderTargetHandle() const
{
	AntiAliasingRenderTargetHandle handle = {};
	if ( m_enabled )
	{
		handle.renderTarget = m_renderTargetView;
		handle.depthStencil = m_depthStencilView;
	}
	return handle;
}

#endif // RENDERER_DIRECTX11
