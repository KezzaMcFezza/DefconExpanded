#ifdef RENDERER_DIRECTX11

#include "systemiv.h"
#include "window_manager_d3d11.h"
#include "lib/debug_utils.h"
#include "lib/preferences.h"
#include "lib/profiler.h"
#include "input.h"

#include <stdio.h>
#include <shellapi.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

// *** Constructor
WindowManagerD3D11::WindowManagerD3D11()
    : m_hwnd(nullptr),
      m_hInstance(nullptr),
      m_device(nullptr),
      m_deviceContext(nullptr),
      m_swapChain(nullptr),
      m_renderTargetView(nullptr),
      m_depthStencilBuffer(nullptr),
      m_depthStencilView(nullptr),
      m_featureLevel(D3D_FEATURE_LEVEL_11_0),
      m_vsyncEnabled(false)
{
    m_hInstance = GetModuleHandle(nullptr);
}

WindowManagerD3D11::~WindowManagerD3D11()
{
    DestroyWin();
}

bool WindowManagerD3D11::InitializeDirectX(int width, int height, bool windowed, int msaaSamples)
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
    UINT numFeatureLevels = ARRAYSIZE(featureLevels);
    
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
        &m_deviceContext
    );
    
    if (FAILED(hr))
    {
        AppDebugOut("Failed to create D3D11 device, error: 0x%08X\n", hr);
        return false;
    }
    
#ifdef _DEBUG
    AppDebugOut("DirectX 11 device created successfully\n");
    AppDebugOut("Feature level: 0x%04x\n", m_featureLevel);
#endif
    
    //
    // Get DXGI factory from device

    IDXGIDevice* dxgiDevice = nullptr;
    hr = m_device->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice);
    if (FAILED(hr))
    {
        #ifdef _DEBUG
        AppDebugOut("Failed to query DXGI device, error: 0x%08X\n", hr);
        #endif
        return false;
    }
    
    IDXGIAdapter* dxgiAdapter = nullptr;
    hr = dxgiDevice->GetAdapter(&dxgiAdapter);
    if (FAILED(hr))
    {
        dxgiDevice->Release();
        #ifdef _DEBUG
        AppDebugOut("Failed to get DXGI adapter, error: 0x%08X\n", hr);
        #endif
        return false;
    }
    
    IDXGIFactory1* dxgiFactory = nullptr;
    hr = dxgiAdapter->GetParent(__uuidof(IDXGIFactory1), (void**)&dxgiFactory);
    if (FAILED(hr))
    {
        dxgiAdapter->Release();
        dxgiDevice->Release();
        #ifdef _DEBUG
        AppDebugOut("Failed to get DXGI factory, error: 0x%08X\n", hr);
        #endif
        return false;
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
    swapChainDesc.Windowed = windowed ? TRUE : FALSE;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    
    hr = dxgiFactory->CreateSwapChain(m_device, &swapChainDesc, &m_swapChain);
    
    //
    // Disable Alt+Enter fullscreen toggle, SDL handles it

    dxgiFactory->MakeWindowAssociation(m_hwnd, DXGI_MWA_NO_ALT_ENTER);
    
    dxgiFactory->Release();
    dxgiAdapter->Release();
    dxgiDevice->Release();
    
    if (FAILED(hr))
    {
        #ifdef _DEBUG
        AppDebugOut("Failed to create swap chain, error: 0x%08X\n", hr);
        #endif
        return false;
    }
    
#ifdef _DEBUG
    AppDebugOut("Swap chain created successfully\n");
#endif

    if (!CreateRenderTargetView())
    {
        #ifdef _DEBUG
        AppDebugOut("Failed to create render target view\n");
        #endif
        return false;
    }
    
    if (!CreateDepthStencilView(width, height))
    {
        #ifdef _DEBUG
        AppDebugOut("Failed to create depth stencil view\n");
        #endif
        return false;
    }
    
    //
    // Set render targets
    m_deviceContext->OMSetRenderTargets(1, &m_renderTargetView, m_depthStencilView);
    
    //
    // Setup viewport

    D3D11_VIEWPORT viewport = {};
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = static_cast<float>(width);
    viewport.Height = static_cast<float>(height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    m_deviceContext->RSSetViewports(1, &viewport);
    
    return true;
}


bool WindowManagerD3D11::CreateRenderTargetView()
{
    if (!m_swapChain)
        return false;
    
    ID3D11Texture2D* backBuffer = nullptr;
    HRESULT hr = m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    if (FAILED(hr))
    {
        #ifdef _DEBUG
        AppDebugOut("Failed to get back buffer, error: 0x%08X\n", hr);
        #endif
        return false;
    }
    
    hr = m_device->CreateRenderTargetView(backBuffer, nullptr, &m_renderTargetView);
    backBuffer->Release();
    
    if (FAILED(hr))
    {
        #ifdef _DEBUG
        AppDebugOut("Failed to create render target view, error: 0x%08X\n", hr);
        #endif
        return false;
    }
    
    return true;
}


bool WindowManagerD3D11::CreateDepthStencilView(int width, int height)
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
    
    HRESULT hr = m_device->CreateTexture2D(&depthStencilDesc, nullptr, &m_depthStencilBuffer);
    if (FAILED(hr))
    {
        #ifdef _DEBUG
        AppDebugOut("Failed to create depth stencil buffer, error: 0x%08X\n", hr);
        #endif
        return false;
    }
    
    hr = m_device->CreateDepthStencilView(m_depthStencilBuffer, nullptr, &m_depthStencilView);
    if (FAILED(hr))
    {
        #ifdef _DEBUG
        AppDebugOut("Failed to create depth stencil view, error: 0x%08X\n", hr);
        #endif
        return false;
    }
    
    return true;
}


void WindowManagerD3D11::ShutdownDirectX()
{
    if (m_deviceContext)
    {
        m_deviceContext->ClearState();
        m_deviceContext->Flush();
    }
    
    if (m_depthStencilView)
    {
        m_depthStencilView->Release();
        m_depthStencilView = nullptr;
    }
    
    if (m_depthStencilBuffer)
    {
        m_depthStencilBuffer->Release();
        m_depthStencilBuffer = nullptr;
    }
    
    if (m_renderTargetView)
    {
        m_renderTargetView->Release();
        m_renderTargetView = nullptr;
    }
    
    if (m_swapChain)
    {
        m_swapChain->Release();
        m_swapChain = nullptr;
    }
    
    if (m_deviceContext)
    {
        m_deviceContext->Release();
        m_deviceContext = nullptr;
    }
    
    if (m_device)
    {
        m_device->Release();
        m_device = nullptr;
    }
}


bool WindowManagerD3D11::CreateWin(int _width, int _height, bool _windowed, int _colourDepth,
                                   int _refreshRate, int _zDepth, int _antiAlias, bool _borderless,
                                   const char *_title)
{
    int displayIndex = GetDefaultDisplayIndex();
    AppReleaseAssert(displayIndex >= 0, "Failed to get current SDL display index.\n");
    
    SDL_DisplayMode displayMode;
    SDL_GetCurrentDisplayMode(displayIndex, &displayMode);
    
    m_windowed = _windowed;

    SDL_Rect displayBounds;
    SDL_GetDisplayBounds(displayIndex, &displayBounds);

    //
    // Set the flags for creating the mode

    int flags = SDL_WINDOW_ALLOW_HIGHDPI;
    bool requestMaximized = false;
    
    if (!_windowed)
    {
        //
        // Fullscreen mode
        
        if (_borderless)
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
        SDL_GetDisplayUsableBounds(displayIndex, &usableBounds);
        
        if (g_preferences && g_preferences->GetInt(PREFS_SCREEN_MAXIMIZED, 0))
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
            
            if (m_screenW > usableBounds.w)
                m_screenW = usableBounds.w;
            if (m_screenH > usableBounds.h)
                m_screenH = usableBounds.h;
        }
        
        if (_borderless)
        {
            flags |= SDL_WINDOW_BORDERLESS;
        }
        else
        {
            flags |= SDL_WINDOW_RESIZABLE;
        }
    }
    
#ifdef _DEBUG
    AppDebugOut("Creating SDL window: %dx%d (%s)\n", m_screenW, m_screenH, _windowed ? "windowed" : "fullscreen");
#endif

    m_sdlWindow = SDL_CreateWindow(
        _title,
        SDL_WINDOWPOS_CENTERED_DISPLAY(displayIndex),
        SDL_WINDOWPOS_CENTERED_DISPLAY(displayIndex),
        m_screenW,
        m_screenH,
        flags
    );
    
    if (!m_sdlWindow)
    {
        AppDebugOut("Failed to create SDL window: %s\n", SDL_GetError());
        return false;
    }
    
#ifdef _DEBUG
    AppDebugOut("SDL window created successfully\n");
#endif

    //
    // Get native HWND from SDL window

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    
    if (!SDL_GetWindowWMInfo(m_sdlWindow, &wmInfo))
    {
        #ifdef _DEBUG
        AppDebugOut("Failed to get window info: %s\n", SDL_GetError());
        #endif
        SDL_DestroyWindow(m_sdlWindow);
        m_sdlWindow = nullptr;
        return false;
    }
    
    if (wmInfo.subsystem != SDL_SYSWM_WINDOWS)
    {
        #ifdef _DEBUG
        AppDebugOut("Not running on Windows!\n");
        #endif
        SDL_DestroyWindow(m_sdlWindow);
        m_sdlWindow = nullptr;
        return false;
    }
    
    m_hwnd = wmInfo.info.win.window;
    #ifdef _DEBUG
    AppDebugOut("Got HWND from SDL: 0x%p\n", m_hwnd);
    #endif
    
    //
    // Initialize DirectX using the SDL-provided HWND

    if (!InitializeDirectX(m_screenW, m_screenH, _windowed, _antiAlias))
    {
        AppDebugOut("DirectX 11 initialization failed\n");
        SDL_DestroyWindow(m_sdlWindow);
        m_sdlWindow = nullptr;
        m_hwnd = nullptr;
        return false;
    }
    
    m_windowDisplayIndex = displayIndex;
    
    int actualW, actualH;
    SDL_GetWindowSize(m_sdlWindow, &actualW, &actualH);
    
    if (actualW != m_screenW || actualH != m_screenH) 
    {
        #ifdef _DEBUG
        AppDebugOut("Window size adjusted from requested %dx%d to actual %dx%d\n", 
                    m_screenW, m_screenH, actualW, actualH);
        #endif
        
        m_screenW = actualW;
        m_screenH = actualH;
        
        //
        // Recreate DirectX resources with correct size
        // This prevents black bar at the bottom of the screen
        
        if (m_renderTargetView) 
        {
            m_renderTargetView->Release();
            m_renderTargetView = nullptr;
        }
        if (m_depthStencilView) 
        {
            m_depthStencilView->Release();
            m_depthStencilView = nullptr;
        }
        if (m_depthStencilBuffer) 
        {
            m_depthStencilBuffer->Release();
            m_depthStencilBuffer = nullptr;
        }
        
        HRESULT hr = m_swapChain->ResizeBuffers(
            0,
            actualW, actualH,
            DXGI_FORMAT_UNKNOWN,
            DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
        );
        
        if (FAILED(hr)) 
        {
            #ifdef _DEBUG
            AppDebugOut("Failed to resize swap chain buffers after window creation, error: 0x%08X\n", hr);
            #endif
        }
        else 
        {
            CreateRenderTargetView();
            CreateDepthStencilView(actualW, actualH);
            
            m_deviceContext->OMSetRenderTargets(1, &m_renderTargetView, m_depthStencilView);
            
            //
            // Update viewport to match new size
            
            D3D11_VIEWPORT viewport = {};
            viewport.TopLeftX = 0;
            viewport.TopLeftY = 0;
            viewport.Width = static_cast<float>(actualW);
            viewport.Height = static_cast<float>(actualH);
            viewport.MinDepth = 0.0f;
            viewport.MaxDepth = 1.0f;
            m_deviceContext->RSSetViewports(1, &viewport);
        }
    }
    
    UpdateStoredMaximizedState();

    if (requestMaximized)
    {
        SDL_GetWindowSize(m_sdlWindow, &m_screenW, &m_screenH);
        UpdateStoredMaximizedState();
    }

    if (!m_mousePointerVisible)
        HideMousePointer();
    
    if (m_mouseCaptured)
        CaptureMouse();
    
    return true;
}


void WindowManagerD3D11::DestroyWin()
{
    ShutdownDirectX();
    
    if (m_sdlWindow)
    {
        SDL_DestroyWindow(m_sdlWindow);
        m_sdlWindow = nullptr;
    }
    
    m_hwnd = nullptr;
}


void WindowManagerD3D11::Flip()
{
    if (!m_swapChain || !m_deviceContext || !m_renderTargetView)
        return;
    
    if (m_tryingToCaptureMouse)
    {
        CaptureMouse();
    }
    
    //
    // Present the back buffer

    UINT syncInterval = m_vsyncEnabled ? 1 : 0;
    HRESULT hr = m_swapChain->Present(syncInterval, 0);
    
    if (FAILED(hr))
    {
        AppDebugOut("Present failed, error: 0x%08X\n", hr);
    }
}


void WindowManagerD3D11::HandleResize(int newWidth, int newHeight)
{
    if (!m_sdlWindow || newWidth <= 0 || newHeight <= 0)
        return;
    
    Uint32 windowFlags = SDL_GetWindowFlags(m_sdlWindow);
    
    //
    // Dont resize in fullscreen mode

    if (windowFlags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP))
        return;
    
    if (newWidth == m_screenW && newHeight == m_screenH)
        return;
    
    //
    // Release render targets before resizing

    if (m_renderTargetView)
    {
        m_renderTargetView->Release();
        m_renderTargetView = nullptr;
    }
    
    if (m_depthStencilView)
    {
        m_depthStencilView->Release();
        m_depthStencilView = nullptr;
    }
    
    if (m_depthStencilBuffer)
    {
        m_depthStencilBuffer->Release();
        m_depthStencilBuffer = nullptr;
    }
    
    HRESULT hr = m_swapChain->ResizeBuffers(0, newWidth, newHeight, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);
    if (FAILED(hr))
    {
        AppDebugOut("Failed to resize swap chain buffers, error: 0x%08X\n", hr);
        return;
    }
    
    CreateRenderTargetView();
    CreateDepthStencilView(newWidth, newHeight);
    m_deviceContext->OMSetRenderTargets(1, &m_renderTargetView, m_depthStencilView);
    
    WindowManager::HandleResize(newWidth, newHeight);
}



#endif // RENDERER_DIRECTX11