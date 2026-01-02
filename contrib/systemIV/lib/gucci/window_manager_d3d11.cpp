#ifdef RENDERER_DIRECTX11

#include "systemiv.h"
#include "window_manager_d3d11.h"
#include "lib/debug_utils.h"
#include "lib/preferences.h"
#include "lib/profiler.h"
#include "input.h"

#include "lib/render/renderer_d3d11.h"

#include <stdio.h>
#include <shellapi.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

// *** Constructor
WindowManagerD3D11::WindowManagerD3D11()
    : m_hwnd(nullptr),
      m_hInstance(nullptr),
      m_sdlWindow(nullptr),
      m_device(nullptr),
      m_deviceContext(nullptr),
      m_swapChain(nullptr),
      m_renderTargetView(nullptr),
      m_depthStencilBuffer(nullptr),
      m_depthStencilView(nullptr),
      m_featureLevel(D3D_FEATURE_LEVEL_11_0),
      m_tryingToCaptureMouse(false),
      m_vsyncEnabled(false),
      m_windowDisplayIndex(0),
      m_isMaximized(false)
{
    m_hInstance = GetModuleHandle(nullptr);
    
    AppReleaseAssert(SDL_Init(SDL_INIT_VIDEO) == 0, "Couldn't initialise SDL");
    
    m_windowDisplayIndex = GetDefaultDisplayIndex();
    ListAllDisplayModes(m_windowDisplayIndex);
    SaveDesktop();
}

WindowManagerD3D11::~WindowManagerD3D11()
{
    DestroyWin();
    
    while (m_resolutions.ValidIndex(0))
    {
        WindowResolution *res = m_resolutions.GetData(0);
        delete res;
        m_resolutions.RemoveData(0);
    }
    m_resolutions.EmptyAndDelete();
    
    SDL_Quit();
}


static int GetDisplayIndexForPoint(int x, int y)
{
    int numVideoDisplays = SDL_GetNumVideoDisplays();
    
    for (int displayIndex = 0; displayIndex < numVideoDisplays; ++displayIndex)
    {
        SDL_Rect bounds;
        SDL_GetDisplayBounds(displayIndex, &bounds);
        
        SDL_Point mouse = { x, y };
        if (SDL_PointInRect(&mouse, &bounds))
        {
            return displayIndex;
        }
    }
    
#ifdef _DEBUG
    AppDebugOut("GetDisplayIndexForPoint() could not determine which display is under (%d, %d), returning first display.\n", x, y);
#endif
    return 0;
}


int WindowManagerD3D11::GetDefaultDisplayIndex()
{
    int x, y;
    SDL_GetGlobalMouseState(&x, &y);
    
    return GetDisplayIndexForPoint(x, y);
}


void WindowManagerD3D11::ListAllDisplayModes(int displayIndex)
{
    SDL_Rect bounds;
    SDL_GetDisplayBounds(displayIndex, &bounds);
    
    m_resolutions.EmptyAndDelete();
    
    for (int modeIndex = 0; modeIndex < SDL_GetNumDisplayModes(displayIndex); ++modeIndex)
    {
        SDL_DisplayMode mode;
        SDL_GetDisplayMode(displayIndex, modeIndex, &mode);
        
        //
        // Skip low quality modes

        if (SDL_BITSPERPIXEL(mode.format) < 15 || mode.w < 640 || mode.h < 480)
            continue;
        
        //
        // Skip high DPI resolutions

        if (mode.w > bounds.w || mode.h > bounds.h)
            continue;
        
        int resId = GetResolutionId(mode.w, mode.h);
        WindowResolution *res;
        
        if (resId == -1)
        {
            res = new WindowResolution(mode.w, mode.h);
            m_resolutions.PutData(res);
        }
        else
        {
            res = m_resolutions[resId];
        }
        
        if (mode.refresh_rate != 0)
        {
            //
            // Only add if this refresh rate isn't already in the list

            if (res->m_refreshRates.FindData(mode.refresh_rate) == -1)
            {
                res->m_refreshRates.PutDataAtEnd(mode.refresh_rate);
            }
        }
    }
}


void WindowManagerD3D11::SaveDesktop()
{
    int displayIndex = GetDefaultDisplayIndex();
    
    SDL_DisplayMode desktopMode;
    if (SDL_GetDesktopDisplayMode(displayIndex, &desktopMode) != 0)
    {
        #ifdef _DEBUG
        AppDebugOut("Couldn't get desktop display mode for display: %d, error: %s\n", displayIndex, SDL_GetError());
        #endif

        SDL_Rect displayBounds;
        if (SDL_GetDisplayBounds(displayIndex, &displayBounds) != 0)
        {
            #ifdef _DEBUG
            AppDebugOut("Still couldn't get display bounds, error: %s\n", SDL_GetError());
            AppDebugOut("Using 1024x768 as fallback resolution\n");
            #endif
            m_desktopScreenW = 1024;
            m_desktopScreenH = 768;
        }
        else
        {
            m_desktopScreenW = displayBounds.w;
            m_desktopScreenH = displayBounds.h;
            #ifdef _DEBUG
            AppDebugOut("Successfully got display bounds: %dx%d\n", m_desktopScreenW, m_desktopScreenH);
            #endif
        }
    }
    else
    {
        m_desktopScreenW = desktopMode.w;
        m_desktopScreenH = desktopMode.h;
        AppDebugOut("Desktop resolution determined to be: %dx%d\n", m_desktopScreenW, m_desktopScreenH);
    }
    
    m_desktopColourDepth = SDL_BITSPERPIXEL(desktopMode.format);
    m_desktopRefresh = desktopMode.refresh_rate;
}


void WindowManagerD3D11::RestoreDesktop()
{
    // SDL handles this
}


void WindowManagerD3D11::CalculateHighDPIScaleFactors()
{
    if (!m_sdlWindow)
        return;
    
    int clientW, clientH;
    SDL_GetWindowSize(m_sdlWindow, &clientW, &clientH);
    
    int drawableW, drawableH;
    SDL_GL_GetDrawableSize(m_sdlWindow, &drawableW, &drawableH);
    
    m_highDPIScaleX = (float)drawableW / clientW;
    m_highDPIScaleY = (float)drawableH / clientH;
}


void WindowManagerD3D11::WindowHasMoved()
{
    CalculateHighDPIScaleFactors();
    
    m_windowDisplayIndex = SDL_GetWindowDisplayIndex(m_sdlWindow);
    ListAllDisplayModes(m_windowDisplayIndex);
}


void WindowManagerD3D11::UpdateStoredMaximizedState()
{
    if (!m_sdlWindow || !g_preferences)
        return;
    
    Uint32 windowFlags = SDL_GetWindowFlags(m_sdlWindow);
    bool currentlyMaximized = false;
    
    if (m_windowed && !(windowFlags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP)))
    {
        currentlyMaximized = (windowFlags & SDL_WINDOW_MAXIMIZED) != 0;
    }
    
    if (currentlyMaximized == m_isMaximized)
        return;
    
    m_isMaximized = currentlyMaximized;
    g_preferences->SetInt(PREFS_SCREEN_MAXIMIZED, m_isMaximized ? 1 : 0);
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
    // Set flags for creating the window

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
    
    CalculateHighDPIScaleFactors();
    UpdateStoredMaximizedState();

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
    
    UpdateStoredMaximizedState();
    
    //
    // Dont resize in fullscreen mode

    if (windowFlags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP))
        return;
    
    if (newWidth == m_screenW && newHeight == m_screenH)
        return;
    
    int oldWidth = m_screenW;
    int oldHeight = m_screenH;
    
    m_screenW = newWidth;
    m_screenH = newHeight;
    
    CalculateHighDPIScaleFactors();
    
    //
    // Check if window moved to a different display

    int newDisplayIndex = SDL_GetWindowDisplayIndex(m_sdlWindow);
    if (newDisplayIndex >= 0 && newDisplayIndex != m_windowDisplayIndex)
    {
        m_windowDisplayIndex = newDisplayIndex;
        ListAllDisplayModes(m_windowDisplayIndex);
    }
    
    //
    // Add current resolution to list if not present

    if (GetResolutionId(newWidth, newHeight) == -1)
    {
        WindowResolution *res = new WindowResolution(newWidth, newHeight);
        m_resolutions.PutData(res);
    }
    
    //
    // Release old DirectX resources

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
    
    //
    // Recreate render target and depth stencil views

    CreateRenderTargetView();
    CreateDepthStencilView(newWidth, newHeight);
    
    m_deviceContext->OMSetRenderTargets(1, &m_renderTargetView, m_depthStencilView);
    
    //
    // Resize MSAA framebuffer if the renderer is using it

    if (g_renderer) 
    {
        RendererD3D11* rendererD3D11 = dynamic_cast<RendererD3D11*>(g_renderer);
        if (rendererD3D11)
        {
            rendererD3D11->ResizeMSAAFramebuffer(newWidth, newHeight);
        }
    }
    
    //
    // Call user resize handler

    if (m_windowResizeHandler)
    {
        m_windowResizeHandler(newWidth, newHeight, oldWidth, oldHeight);
    }
}


void WindowManagerD3D11::PollForMessages()
{
    SDL_Event sdlEvent;
    
    while (SDL_PollEvent(&sdlEvent))
    {
        if (sdlEvent.type == SDL_WINDOWEVENT)
        {
            switch (sdlEvent.window.event)
            {
                case SDL_WINDOWEVENT_MOVED:
                    WindowHasMoved();
                    break;
                
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                case SDL_WINDOWEVENT_RESIZED:
                case SDL_WINDOWEVENT_MAXIMIZED:
                case SDL_WINDOWEVENT_RESTORED:
                    HandleResize(sdlEvent.window.data1, sdlEvent.window.data2);
                    break;
                
                default:
                    break;
            }
        }
            
        //
        // Quit event

        if (sdlEvent.type == SDL_QUIT)
        {
            if (g_inputManager)
            {
                g_inputManager->m_quitRequested = true;
            }
        }
        
        //
        // Pass all SDL events to input manager

        if (g_inputManager)
        {
            int result = g_inputManager->EventHandler(sdlEvent.type, (long long)&sdlEvent, 0);
            
            if (result == -1 && m_secondaryMessageHandler)
            {
                m_secondaryMessageHandler(sdlEvent.type, (long long)&sdlEvent, 0);
            }
        }
    }
    
    if (m_tryingToCaptureMouse)
    {
        CaptureMouse();
    }
}


void WindowManagerD3D11::SetMousePos(int x, int y)
{
    if (!m_sdlWindow)
        return;
    
    float scaleX = (float)PhysicalWindowW() / (float)WindowW();
    float scaleY = (float)PhysicalWindowH() / (float)WindowH();
    
    int physicalX = (int)(x * scaleX);
    int physicalY = (int)(y * scaleY);
    
    SDL_WarpMouseInWindow(m_sdlWindow, physicalX, physicalY);
}


void WindowManagerD3D11::CaptureMouse()
{
    //
    // Dont grab if we don't have focus

    if (!g_inputManager || !g_inputManager->m_windowHasFocus)
    {
        m_tryingToCaptureMouse = true;
        return;
    }
    
    if (!m_sdlWindow)
        return;
    
    //
    // Dont grab until mouse is in the window

    int windowFlags = SDL_GetWindowFlags(m_sdlWindow);
    if (!(windowFlags & SDL_WINDOW_MOUSE_FOCUS))
    {
        m_tryingToCaptureMouse = true;
        return;
    }
    
    SDL_SetRelativeMouseMode(SDL_TRUE);
    m_mouseCaptured = true;
    m_tryingToCaptureMouse = false;
}


void WindowManagerD3D11::UncaptureMouse()
{
    if (m_sdlWindow)
    {
        SDL_SetRelativeMouseMode(SDL_FALSE);
    }
    
    m_mouseCaptured = false;
    m_tryingToCaptureMouse = false;
}


void WindowManagerD3D11::HideMousePointer()
{
    SDL_ShowCursor(SDL_DISABLE);
    m_mousePointerVisible = false;
}


void WindowManagerD3D11::UnhideMousePointer()
{
    SDL_ShowCursor(SDL_ENABLE);
    m_mousePointerVisible = true;
}


void WindowManagerD3D11::HideWin()
{
    if (m_sdlWindow)
    {
        SDL_HideWindow(m_sdlWindow);
    }
}


void WindowManagerD3D11::OpenWebsite(const char *_url)
{
    ShellExecute(nullptr, "open", _url, nullptr, nullptr, SW_SHOWNORMAL);
}

#endif // RENDERER_DIRECTX11