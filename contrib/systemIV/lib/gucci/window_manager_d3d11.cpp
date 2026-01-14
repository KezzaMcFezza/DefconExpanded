#ifdef RENDERER_DIRECTX11

#include "systemiv.h"
#include "window_manager_d3d11.h"
#include "lib/debug/debug_utils.h"
#include "lib/preferences.h"
#include "lib/debug/profiler.h"
#include "input.h"

#include <stdio.h>
#include <shellapi.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

// *** Constructor
WindowManagerD3D11::WindowManagerD3D11()
	: m_hwnd( nullptr ),
	  m_hInstance( nullptr ),
	  m_device( nullptr ),
	  m_deviceContext( nullptr ),
	  m_swapChain( nullptr ),
	  m_renderTargetView( nullptr ),
	  m_depthStencilBuffer( nullptr ),
	  m_depthStencilView( nullptr ),
	  m_featureLevel( D3D_FEATURE_LEVEL_11_0 ),
	  m_tearingSupported( false ),
	  m_swapChainFlags( 0 ),
	  m_isExclusiveFullscreen( false )
{
	m_hInstance = GetModuleHandle( nullptr );
}


WindowManagerD3D11::~WindowManagerD3D11()
{
	DestroyWin();
}


bool WindowManagerD3D11::InitializeDirectX( int width, int height, bool windowed, bool borderless, int msaaSamples )
{
	HRESULT hr;

	//
	// Create device and device context

	UINT createDeviceFlags = 0;
#ifdef _DEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};
	UINT numFeatureLevels = ARRAYSIZE( featureLevels );

	hr = D3D11CreateDevice(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		createDeviceFlags,
		featureLevels,
		numFeatureLevels,
		D3D11_SDK_VERSION,
		&m_device,
		&m_featureLevel,
		&m_deviceContext );

	if ( FAILED( hr ) )
	{
#ifdef _DEBUG
		AppDebugOut( "Failed to create D3D11 device, error: 0x%08X\n", hr );
#else
		AppDebugOut( "Failed to Initialize DirectX11\n" );
#endif
		return false;
	}

#ifdef _DEBUG
	AppDebugOut( "DirectX 11 device created successfully\n" );
	AppDebugOut( "Feature level: 0x%04x\n", m_featureLevel );
#endif

	//
	// Get DXGI factory from device

	IDXGIDevice *dxgiDevice = nullptr;
	hr = m_device->QueryInterface( __uuidof( IDXGIDevice ), (void **)&dxgiDevice );
	if ( FAILED( hr ) )
	{
#ifdef _DEBUG
		AppDebugOut( "Failed to query DXGI device, error: 0x%08X\n", hr );
#endif
		return false;
	}

	IDXGIAdapter *dxgiAdapter = nullptr;
	hr = dxgiDevice->GetAdapter( &dxgiAdapter );
	if ( FAILED( hr ) )
	{
		dxgiDevice->Release();
#ifdef _DEBUG
		AppDebugOut( "Failed to get DXGI adapter, error: 0x%08X\n", hr );
#endif
		return false;
	}

	IDXGIFactory1 *dxgiFactory = nullptr;
	hr = dxgiAdapter->GetParent( __uuidof( IDXGIFactory1 ), (void **)&dxgiFactory );
	if ( FAILED( hr ) )
	{
		dxgiAdapter->Release();
		dxgiDevice->Release();
#ifdef _DEBUG
		AppDebugOut( "Failed to get DXGI factory, error: 0x%08X\n", hr );
#endif
		return false;
	}

	//
	// Check for tearing support
	// Tearing allows uncapped framerates even when vsync is not disabled

	m_tearingSupported = false;
	m_swapChainFlags = 0;

	IDXGIFactory5 *dxgiFactory5 = nullptr;
	if ( SUCCEEDED( dxgiFactory->QueryInterface( __uuidof( IDXGIFactory5 ), (void **)&dxgiFactory5 ) ) )
	{
		BOOL allowTearing = FALSE;
		hr = dxgiFactory5->CheckFeatureSupport( DXGI_FEATURE_PRESENT_ALLOW_TEARING,
												&allowTearing, sizeof( allowTearing ) );
		if ( SUCCEEDED( hr ) && allowTearing )
		{
			m_tearingSupported = true;
			m_swapChainFlags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
#ifdef _DEBUG
			AppDebugOut( "Tearing support detected and enabled\n" );
#endif
		}
		dxgiFactory5->Release();
	}

	//
	// Create swap chain

	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	swapChainDesc.BufferCount = 3;
	swapChainDesc.BufferDesc.Width = width;
	swapChainDesc.BufferDesc.Height = height;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferDesc.RefreshRate.Numerator = 0;
	swapChainDesc.BufferDesc.RefreshRate.Denominator = 0;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.OutputWindow = m_hwnd;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.Flags = m_swapChainFlags;

	if ( windowed || borderless )
	{
		swapChainDesc.Windowed = TRUE;
	}
	else
	{
		swapChainDesc.Windowed = FALSE;
	}

	hr = dxgiFactory->CreateSwapChain( m_device, &swapChainDesc, &m_swapChain );

	//
	// Disable Alt+Enter fullscreen toggle, SDL handles it

	dxgiFactory->MakeWindowAssociation( m_hwnd, DXGI_MWA_NO_ALT_ENTER );

	dxgiFactory->Release();
	dxgiAdapter->Release();
	dxgiDevice->Release();

	if ( FAILED( hr ) )
	{
#ifdef _DEBUG
		AppDebugOut( "Failed to create swap chain, error: 0x%08X\n", hr );
#else
		AppDebugOut( "Failed to create swap chain\n" );
#endif
		return false;
	}

	if ( !CreateRenderTargetView() )
	{
#ifdef _DEBUG
		AppDebugOut( "Failed to create render target view\n" );
#endif
		return false;
	}

	if ( !CreateDepthStencilView( width, height ) )
	{
#ifdef _DEBUG
		AppDebugOut( "Failed to create depth stencil view\n" );
#endif
		return false;
	}

	//
	// Set render targets
	m_deviceContext->OMSetRenderTargets( 1, &m_renderTargetView, m_depthStencilView );

	//
	// Setup viewport

	D3D11_VIEWPORT viewport = {};
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = static_cast<float>( width );
	viewport.Height = static_cast<float>( height );
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	m_deviceContext->RSSetViewports( 1, &viewport );

	//
	// Transition to exclusive fullscreen if needed

	m_isExclusiveFullscreen = ( !windowed && !borderless );

	if ( m_isExclusiveFullscreen )
	{
		hr = m_swapChain->SetFullscreenState( TRUE, NULL );
	}
	else
	{
		m_isExclusiveFullscreen = false;
	}

	return true;
}


bool WindowManagerD3D11::CreateRenderTargetView()
{
	if ( !m_swapChain )
		return false;

	ID3D11Texture2D *backBuffer = nullptr;
	HRESULT hr = m_swapChain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), (void **)&backBuffer );
	if ( FAILED( hr ) )
	{
#ifdef _DEBUG
		AppDebugOut( "Failed to get back buffer, error: 0x%08X\n", hr );
#endif
		return false;
	}

	hr = m_device->CreateRenderTargetView( backBuffer, nullptr, &m_renderTargetView );
	backBuffer->Release();

	if ( FAILED( hr ) )
	{
#ifdef _DEBUG
		AppDebugOut( "Failed to create render target view, error: 0x%08X\n", hr );
#endif
		return false;
	}

	return true;
}


bool WindowManagerD3D11::CreateDepthStencilView( int width, int height )
{
	D3D11_TEXTURE2D_DESC depthStencilDesc = {};
	depthStencilDesc.Width = width;
	depthStencilDesc.Height = height;
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.ArraySize = 1;
	depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthStencilDesc.SampleDesc.Count = 1;
	depthStencilDesc.SampleDesc.Quality = 0;
	depthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
	depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	depthStencilDesc.CPUAccessFlags = 0;
	depthStencilDesc.MiscFlags = 0;

	HRESULT hr = m_device->CreateTexture2D( &depthStencilDesc, nullptr, &m_depthStencilBuffer );
	if ( FAILED( hr ) )
	{
#ifdef _DEBUG
		AppDebugOut( "Failed to create depth stencil buffer, error: 0x%08X\n", hr );
#endif
		return false;
	}

	hr = m_device->CreateDepthStencilView( m_depthStencilBuffer, nullptr, &m_depthStencilView );
	if ( FAILED( hr ) )
	{
#ifdef _DEBUG
		AppDebugOut( "Failed to create depth stencil view, error: 0x%08X\n", hr );
#endif
		return false;
	}

	return true;
}


void WindowManagerD3D11::ShutdownDirectX()
{
	if ( m_deviceContext )
	{
		m_deviceContext->ClearState();
		m_deviceContext->Flush();
	}

	if ( m_depthStencilView )
	{
		m_depthStencilView->Release();
		m_depthStencilView = nullptr;
	}

	if ( m_depthStencilBuffer )
	{
		m_depthStencilBuffer->Release();
		m_depthStencilBuffer = nullptr;
	}

	if ( m_renderTargetView )
	{
		m_renderTargetView->Release();
		m_renderTargetView = nullptr;
	}

	if ( m_swapChain )
	{
		if ( m_isExclusiveFullscreen )
		{
			m_swapChain->SetFullscreenState( FALSE, NULL );
			m_isExclusiveFullscreen = false;
		}

		m_swapChain->Release();
		m_swapChain = nullptr;
	}

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


bool WindowManagerD3D11::CreateWin( int _width, int _height, bool _windowed, int _colourDepth,
									int _refreshRate, int _zDepth, int _antiAlias, bool _borderless,
									const char *_title )
{
	int displayIndex = GetDefaultDisplayIndex();
	AppReleaseAssert( displayIndex >= 0, "Failed to get current SDL display index.\n" );

	SDL_DisplayMode displayMode;
	SDL_GetCurrentDisplayMode( displayIndex, &displayMode );

	m_windowed = _windowed;

	SDL_Rect displayBounds;
	SDL_GetDisplayBounds( displayIndex, &displayBounds );

	//
	// Set the flags for creating the mode

	int flags = SDL_WINDOW_ALLOW_HIGHDPI;
	bool requestMaximized = false;

	if ( !_windowed )
	{
		//
		// Fullscreen mode

		if ( _borderless )
		{
			flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
		}
		else
		{
			flags |= SDL_WINDOW_FULLSCREEN;
		}

		m_screenW = _width;
		m_screenH = _height;
	}
	else
	{

		//
		// Windowed mode

		SDL_Rect usableBounds;
		SDL_GetDisplayUsableBounds( displayIndex, &usableBounds );

		if ( g_preferences && g_preferences->GetInt( PREFS_SCREEN_MAXIMIZED, 0 ) )
		{
			flags |= SDL_WINDOW_MAXIMIZED;
			requestMaximized = true;
			m_screenW = usableBounds.w;
			m_screenH = usableBounds.h;
		}
		else
		{
			m_screenW = _width;
			m_screenH = _height;

			if ( m_screenW > usableBounds.w )
				m_screenW = usableBounds.w;
			if ( m_screenH > usableBounds.h )
				m_screenH = usableBounds.h;
		}

		if ( _borderless )
		{
			flags |= SDL_WINDOW_BORDERLESS;
		}
		else
		{
			flags |= SDL_WINDOW_RESIZABLE;
		}
	}

#ifdef _DEBUG
	AppDebugOut( "Creating SDL window: %dx%d (%s)\n", m_screenW, m_screenH, _windowed ? "windowed" : "fullscreen" );
#endif

	m_sdlWindow = SDL_CreateWindow(
		_title,
		SDL_WINDOWPOS_CENTERED_DISPLAY( displayIndex ),
		SDL_WINDOWPOS_CENTERED_DISPLAY( displayIndex ),
		m_screenW,
		m_screenH,
		flags );

	if ( !m_sdlWindow )
	{
#ifdef _DEBUG
		AppDebugOut( "Failed to create SDL window: %s\n", SDL_GetError() );
#else
		AppDebugOut( "Failed to create window\n" );
#endif
		return false;
	}

#ifdef _DEBUG
	AppDebugOut( "SDL window created successfully\n" );
#endif

	//
	// Get native HWND from SDL window

	SDL_SysWMinfo wmInfo;
	SDL_VERSION( &wmInfo.version );

	if ( !SDL_GetWindowWMInfo( m_sdlWindow, &wmInfo ) )
	{
#ifdef _DEBUG
		AppDebugOut( "Failed to get window info: %s\n", SDL_GetError() );
#endif
		SDL_DestroyWindow( m_sdlWindow );
		m_sdlWindow = nullptr;
		return false;
	}

	if ( wmInfo.subsystem != SDL_SYSWM_WINDOWS )
	{
#ifdef _DEBUG
		AppDebugOut( "Not running on Windows!\n" );
#endif
		SDL_DestroyWindow( m_sdlWindow );
		m_sdlWindow = nullptr;
		return false;
	}

	m_hwnd = wmInfo.info.win.window;
#ifdef _DEBUG
	AppDebugOut( "Got HWND from SDL: 0x%p\n", m_hwnd );
#endif

	//
	// Initialize DirectX using the SDL-provided HWND

	if ( !InitializeDirectX( m_screenW, m_screenH, _windowed, _borderless, _antiAlias ) )
	{
		AppDebugOut( "DirectX 11 initialization failed\n" );
		SDL_DestroyWindow( m_sdlWindow );
		m_sdlWindow = nullptr;
		m_hwnd = nullptr;
		return false;
	}

	m_windowDisplayIndex = displayIndex;

	int actualW, actualH;
	SDL_GetWindowSize( m_sdlWindow, &actualW, &actualH );

	if ( actualW != m_screenW || actualH != m_screenH )
	{
#ifdef _DEBUG
		AppDebugOut( "Window size adjusted from requested %dx%d to actual %dx%d\n",
					 m_screenW, m_screenH, actualW, actualH );
#endif

		m_screenW = actualW;
		m_screenH = actualH;

		//
		// Recreate DirectX resources with correct size
		// This prevents black bar at the bottom of the screen

		if ( m_renderTargetView )
		{
			m_renderTargetView->Release();
			m_renderTargetView = nullptr;
		}
		if ( m_depthStencilView )
		{
			m_depthStencilView->Release();
			m_depthStencilView = nullptr;
		}
		if ( m_depthStencilBuffer )
		{
			m_depthStencilBuffer->Release();
			m_depthStencilBuffer = nullptr;
		}

		HRESULT hr = m_swapChain->ResizeBuffers(
			0,
			actualW, actualH,
			DXGI_FORMAT_UNKNOWN,
			m_swapChainFlags );

		if ( FAILED( hr ) )
		{
#ifdef _DEBUG
			AppDebugOut( "Failed to resize swap chain buffers after window creation, error: 0x%08X\n", hr );
#endif
		}
		else
		{
			CreateRenderTargetView();
			CreateDepthStencilView( actualW, actualH );

			m_deviceContext->OMSetRenderTargets( 1, &m_renderTargetView, m_depthStencilView );

			//
			// Update viewport to match new size

			D3D11_VIEWPORT viewport = {};
			viewport.TopLeftX = 0;
			viewport.TopLeftY = 0;
			viewport.Width = static_cast<float>( actualW );
			viewport.Height = static_cast<float>( actualH );
			viewport.MinDepth = 0.0f;
			viewport.MaxDepth = 1.0f;
			m_deviceContext->RSSetViewports( 1, &viewport );
		}
	}

	UpdateStoredMaximizedState();

	if ( requestMaximized )
	{
		SDL_GetWindowSize( m_sdlWindow, &m_screenW, &m_screenH );
		UpdateStoredMaximizedState();
	}

	if ( !m_mousePointerVisible )
		HideMousePointer();

	if ( m_mouseCaptured )
		CaptureMouse();

	return true;
}


void WindowManagerD3D11::DestroyWin()
{
	ShutdownDirectX();

	if ( m_sdlWindow )
	{
		SDL_DestroyWindow( m_sdlWindow );
		m_sdlWindow = nullptr;
	}

	m_hwnd = nullptr;
}


void WindowManagerD3D11::Flip()
{
	if ( !m_swapChain || !m_deviceContext || !m_renderTargetView )
		return;

	if ( m_tryingToCaptureMouse )
	{
		CaptureMouse();
	}

	//
	// Present the back buffer

	UINT syncInterval = 0;
	UINT presentFlags = 0;

	if ( m_tearingSupported && !m_isExclusiveFullscreen &&
		 ( m_swapChainFlags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING ) )
	{
		presentFlags = DXGI_PRESENT_ALLOW_TEARING;
	}

	HRESULT hr = m_swapChain->Present( syncInterval, presentFlags );

	if ( hr == DXGI_STATUS_OCCLUDED )
	{
		// Window is occluded, just continue
	}
	else if ( hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET )
	{
// Device was lost or reset, attempt recovery
#ifdef _DEBUG
		AppDebugOut( "Device lost/reset detected (0x%08X), attempting recovery...\n", hr );
#endif

		if ( hr == DXGI_ERROR_DEVICE_REMOVED )
		{
#ifdef _DEBUG
			HRESULT deviceRemovedReason = m_device->GetDeviceRemovedReason();
			AppDebugOut( "Device removed reason: 0x%08X\n", deviceRemovedReason );
#endif
		}

		if ( RecoverSwapChain() )
		{
			WindowManager::HandleResize( m_screenW, m_screenH );
		}
	}
	else if ( hr == DXGI_ERROR_INVALID_CALL )
	{
#ifdef _DEBUG
		AppDebugOut( "Swap chain invalid, attempting recovery...\n" );
#endif

		if ( RecoverSwapChain() )
		{
			WindowManager::HandleResize( m_screenW, m_screenH );
		}
	}
	else if ( FAILED( hr ) )
	{
#ifdef _DEBUG
		AppDebugOut( "Present failed, error: 0x%08X\n", hr );
#endif
	}
}


bool WindowManagerD3D11::RecoverSwapChain()
{
	if ( !m_sdlWindow || !m_swapChain || !m_device )
		return false;

	//
	// Release existing render targets

	if ( m_renderTargetView )
	{
		m_renderTargetView->Release();
		m_renderTargetView = nullptr;
	}

	if ( m_depthStencilView )
	{
		m_depthStencilView->Release();
		m_depthStencilView = nullptr;
	}

	if ( m_depthStencilBuffer )
	{
		m_depthStencilBuffer->Release();
		m_depthStencilBuffer = nullptr;
	}

	//
	// Clear the device context state

	if ( m_deviceContext )
	{
		m_deviceContext->ClearState();
		m_deviceContext->Flush();
	}

	int width, height;
	SDL_GetWindowSize( m_sdlWindow, &width, &height );

	//
	// Exit fullscreen if in exclusive mode before resizing buffers

	bool wasExclusiveFullscreen = m_isExclusiveFullscreen;
	if ( m_isExclusiveFullscreen )
	{
		m_swapChain->SetFullscreenState( FALSE, NULL );
		m_isExclusiveFullscreen = false;
	}

	//
	// Resize swap chain buffers
	// We must preserve the swap chain flags for correct recovery

	HRESULT hr = m_swapChain->ResizeBuffers(
		3,
		width,
		height,
		DXGI_FORMAT_UNKNOWN,
		m_swapChainFlags );

	if ( FAILED( hr ) )
	{
#ifdef _DEBUG
		AppDebugOut( "Failed to resize swap chain buffers during recovery, error: 0x%08X\n", hr );
#endif
		return false;
	}

	//
	// Recreate render targets

	if ( !CreateRenderTargetView() )
	{
#ifdef _DEBUG
		AppDebugOut( "Failed to recreate render target view during recovery\n" );
#endif
		return false;
	}

	if ( !CreateDepthStencilView( width, height ) )
	{
#ifdef _DEBUG
		AppDebugOut( "Failed to recreate depth stencil view during recovery\n" );
#endif
		return false;
	}

	//
	// Restore render targets and viewport

	m_deviceContext->OMSetRenderTargets( 1, &m_renderTargetView, m_depthStencilView );

	D3D11_VIEWPORT viewport = {};
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = static_cast<float>( width );
	viewport.Height = static_cast<float>( height );
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	m_deviceContext->RSSetViewports( 1, &viewport );

	m_screenW = width;
	m_screenH = height;

	//
	// Restore exclusive fullscreen state if needed

	if ( wasExclusiveFullscreen )
	{
		hr = m_swapChain->SetFullscreenState( TRUE, NULL );
		if ( SUCCEEDED( hr ) )
		{
			m_isExclusiveFullscreen = true;
		}
	}

#ifdef _DEBUG
	AppDebugOut( "Swap chain recovered successfully (%dx%d)\n", width, height );
#endif

	return true;
}


void WindowManagerD3D11::HandleWindowFocusGained()
{
	if ( !m_sdlWindow || !m_swapChain )
		return;

	//
	// When window regains focus, especially after being minimized or tabbed out
	// in fullscreen mode, the swap chain may need to be recreated

	Uint32 windowFlags = SDL_GetWindowFlags( m_sdlWindow );

	if ( windowFlags & ( SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP ) )
	{
		//
		// Test if the swap chain is still valid by trying to get the back buffer

		ID3D11Texture2D *testBuffer = nullptr;
		HRESULT hr = m_swapChain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), (void **)&testBuffer );

		if ( FAILED( hr ) || !testBuffer )
		{
#ifdef _DEBUG
			AppDebugOut( "Swap chain appears invalid, attempting recovery...\n" );
#endif
			if ( RecoverSwapChain() )
			{
				WindowManager::HandleResize( m_screenW, m_screenH );
			}
		}
		else
		{
			testBuffer->Release();
		}
	}
}


void WindowManagerD3D11::HandleResize( int newWidth, int newHeight )
{
	if ( !m_sdlWindow || newWidth <= 0 || newHeight <= 0 )
		return;

	Uint32 windowFlags = SDL_GetWindowFlags( m_sdlWindow );

	UpdateStoredMaximizedState();

	//
	// Dont resize in fullscreen mode

	if ( windowFlags & ( SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP ) )
		return;

	if ( newWidth == m_screenW && newHeight == m_screenH )
		return;

	int oldWidth = m_screenW;
	int oldHeight = m_screenH;

	//
	// Release render targets before resizing

	if ( m_renderTargetView )
	{
		m_renderTargetView->Release();
		m_renderTargetView = nullptr;
	}

	if ( m_depthStencilView )
	{
		m_depthStencilView->Release();
		m_depthStencilView = nullptr;
	}

	if ( m_depthStencilBuffer )
	{
		m_depthStencilBuffer->Release();
		m_depthStencilBuffer = nullptr;
	}

	HRESULT hr = m_swapChain->ResizeBuffers(
		3,
		newWidth,
		newHeight,
		DXGI_FORMAT_UNKNOWN,
		m_swapChainFlags );

	if ( FAILED( hr ) )
	{
#ifdef _DEBUG
		AppDebugOut( "Failed to resize swap chain buffers, error: 0x%08X\n", hr );
#endif
		return;
	}

	CreateRenderTargetView();
	CreateDepthStencilView( newWidth, newHeight );
	m_deviceContext->OMSetRenderTargets( 1, &m_renderTargetView, m_depthStencilView );

	//
	// Update viewport to match new size

	D3D11_VIEWPORT viewport = {};
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = static_cast<float>( newWidth );
	viewport.Height = static_cast<float>( newHeight );
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	m_deviceContext->RSSetViewports( 1, &viewport );

	m_screenW = newWidth;
	m_screenH = newHeight;

	// CalculateHighDPIScaleFactors();

	//
	// Check if window moved to a different display

	int newDisplayIndex = SDL_GetWindowDisplayIndex( m_sdlWindow );
	if ( newDisplayIndex >= 0 && newDisplayIndex != m_windowDisplayIndex )
	{
		m_windowDisplayIndex = newDisplayIndex;
		ListAllDisplayModes( m_windowDisplayIndex );
	}

	//
	// Add current resolution to list if not present

	if ( GetResolutionId( newWidth, newHeight ) == -1 )
	{
		WindowResolution *res = new WindowResolution( newWidth, newHeight );
		m_resolutions.PutData( res );
	}

	//
	// Call resize handler

	if ( m_windowResizeHandler )
	{
		m_windowResizeHandler( newWidth, newHeight, oldWidth, oldHeight );
	}
}


#endif // RENDERER_DIRECTX11