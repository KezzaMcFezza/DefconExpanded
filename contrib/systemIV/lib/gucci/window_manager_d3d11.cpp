#ifdef RENDERER_DIRECTX11

#include "systemiv.h"
#include "window_manager_d3d11.h"
#include "lib/debug_utils.h"
#include "lib/preferences.h"
#include "input.h"

#include <stdio.h>
#include <shellapi.h>

#ifndef ERROR_CLASS_ALREADY_EXISTS
#define ERROR_CLASS_ALREADY_EXISTS 1410
#endif


static WindowManagerD3D11* g_windowManagerD3D11 = nullptr;


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
      m_tryingToCaptureMouse(false),
      m_vsyncEnabled(true)
{
    g_windowManagerD3D11 = this;
    m_hInstance = GetModuleHandle(nullptr);
    
    ListAllDisplayModes();
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
    
    g_windowManagerD3D11 = nullptr;
}


void WindowManagerD3D11::ListAllDisplayModes()
{
    m_resolutions.EmptyAndDelete();
    
    //
    // Get primary monitor information
    
    HMONITOR hMonitor = MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO monitorInfo = {};
    monitorInfo.cbSize = sizeof(MONITORINFO);
    
    if (GetMonitorInfo(hMonitor, &monitorInfo))
    {
        int screenWidth = monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left;
        int screenHeight = monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top;
        
        //
        // Add common resolutions that fit within the screen
        
        int commonResolutions[][2] = {
            {640, 480}, {800, 600}, {1024, 768}, {1280, 720}, {1280, 1024},
            {1366, 768}, {1600, 900}, {1920, 1080}, {2560, 1440}, {3840, 2160}
        };
        
        for (int i = 0; i < sizeof(commonResolutions) / sizeof(commonResolutions[0]); i++)
        {
            int w = commonResolutions[i][0];
            int h = commonResolutions[i][1];
            
            if (w <= screenWidth && h <= screenHeight)
            {
                WindowResolution *res = new WindowResolution(w, h);
                res->m_refreshRates.PutDataAtEnd(60);
                m_resolutions.PutData(res);
            }
        }
        
        //
        // Add native resolution if not already in list
        
        if (GetResolutionId(screenWidth, screenHeight) == -1)
        {
            WindowResolution *res = new WindowResolution(screenWidth, screenHeight);
            res->m_refreshRates.PutDataAtEnd(60);
            m_resolutions.PutData(res);
        }
    }
}


void WindowManagerD3D11::SaveDesktop()
{
    HMONITOR hMonitor = MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO monitorInfo = {};
    monitorInfo.cbSize = sizeof(MONITORINFO);
    
    if (GetMonitorInfo(hMonitor, &monitorInfo))
    {
        m_desktopScreenW = monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left;
        m_desktopScreenH = monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top;
        m_desktopColourDepth = 32;
        m_desktopRefresh = 60;
        
        AppDebugOut("Desktop resolution determined to be: %dx%d\n", m_desktopScreenW, m_desktopScreenH);
    }
    else
    {
        AppDebugOut("Failed to get monitor info, using fallback resolution\n");
        m_desktopScreenW = 1024;
        m_desktopScreenH = 768;
        m_desktopColourDepth = 32;
        m_desktopRefresh = 60;
    }
}


void WindowManagerD3D11::RestoreDesktop()
{
}


LRESULT CALLBACK WindowManagerD3D11::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (g_windowManagerD3D11)
    {
        return g_windowManagerD3D11->HandleMessage(hwnd, uMsg, wParam, lParam);
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}


LRESULT WindowManagerD3D11::HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_CLOSE:
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
            
        case WM_SIZE:
        {
            int newWidth = LOWORD(lParam);
            int newHeight = HIWORD(lParam);
            if (newWidth > 0 && newHeight > 0)
            {
                HandleResize(newWidth, newHeight);
            }
            return 0;
        }
        
        case WM_ACTIVATE:
        {
            bool active = (LOWORD(wParam) != WA_INACTIVE);
            if (g_inputManager)
            {
                g_inputManager->m_windowHasFocus = active;
            }
            return 0;
        }
        
        //
        // Input events are handled by input manager
        
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_CHAR:
        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_MOUSEWHEEL:
        {
            if (g_inputManager)
            {
                int result = g_inputManager->EventHandler(uMsg, (long long)wParam, (long long)lParam);
                
                if (result == -1 && m_secondaryMessageHandler)
                {
                    m_secondaryMessageHandler(uMsg, (long long)wParam, (long long)lParam);
                }
            }
            return 0;
        }
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
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
    
    AppDebugOut("DirectX 11 device created successfully\n");
    AppDebugOut("Feature level: 0x%04x\n", m_featureLevel);
    
    //
    // Get DXGI factory from device
    
    IDXGIDevice* dxgiDevice = nullptr;
    hr = m_device->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice);
    if (FAILED(hr))
    {
        AppDebugOut("Failed to query DXGI device, error: 0x%08X\n", hr);
        return false;
    }
    
    IDXGIAdapter* dxgiAdapter = nullptr;
    hr = dxgiDevice->GetAdapter(&dxgiAdapter);
    if (FAILED(hr))
    {
        dxgiDevice->Release();
        AppDebugOut("Failed to get DXGI adapter, error: 0x%08X\n", hr);
        return false;
    }
    
    IDXGIFactory1* dxgiFactory = nullptr;
    hr = dxgiAdapter->GetParent(__uuidof(IDXGIFactory1), (void**)&dxgiFactory);
    if (FAILED(hr))
    {
        dxgiAdapter->Release();
        dxgiDevice->Release();
        AppDebugOut("Failed to get DXGI factory, error: 0x%08X\n", hr);
        return false;
    }
    
    //
    // Create swap chain
    
    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferCount = 2;
    swapChainDesc.BufferDesc.Width = width;
    swapChainDesc.BufferDesc.Height = height;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow = m_hwnd;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.Windowed = windowed ? TRUE : FALSE;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    
    hr = dxgiFactory->CreateSwapChain(m_device, &swapChainDesc, &m_swapChain);
    
    dxgiFactory->Release();
    dxgiAdapter->Release();
    dxgiDevice->Release();
    
    if (FAILED(hr))
    {
        AppDebugOut("Failed to create swap chain, error: 0x%08X\n", hr);
        return false;
    }
    
    AppDebugOut("Swap chain created successfully\n");
    
    //
    // Create render target view
    
    if (!CreateRenderTargetView())
    {
        AppDebugOut("Failed to create render target view\n");
        return false;
    }
    
    //
    // Create depth stencil view
    
    if (!CreateDepthStencilView(width, height))
    {
        AppDebugOut("Failed to create depth stencil view\n");
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
    
    AppDebugOut("DirectX 11 initialization complete\n");
    return true;
}


bool WindowManagerD3D11::CreateRenderTargetView()
{
    HRESULT hr;
    
    //
    // Get back buffer from swap chain
    
    ID3D11Texture2D* backBuffer = nullptr;
    hr = m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    if (FAILED(hr))
    {
        AppDebugOut("Failed to get back buffer, error: 0x%08X\n", hr);
        return false;
    }
    
    //
    // Create render target view
    
    hr = m_device->CreateRenderTargetView(backBuffer, nullptr, &m_renderTargetView);
    backBuffer->Release();
    
    if (FAILED(hr))
    {
        AppDebugOut("Failed to create render target view, error: 0x%08X\n", hr);
        return false;
    }
    
    AppDebugOut("Render target view created successfully\n");
    return true;
}


bool WindowManagerD3D11::CreateDepthStencilView(int width, int height)
{
    HRESULT hr;
    
    //
    // Create depth stencil texture
    
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
    
    hr = m_device->CreateTexture2D(&depthStencilDesc, nullptr, &m_depthStencilBuffer);
    if (FAILED(hr))
    {
        AppDebugOut("Failed to create depth stencil buffer, error: 0x%08X\n", hr);
        return false;
    }
    
    //
    // Create depth stencil view
    
    hr = m_device->CreateDepthStencilView(m_depthStencilBuffer, nullptr, &m_depthStencilView);
    if (FAILED(hr))
    {
        AppDebugOut("Failed to create depth stencil view, error: 0x%08X\n", hr);
        return false;
    }
    
    AppDebugOut("Depth stencil view created successfully\n");
    return true;
}

void WindowManagerD3D11::ShutdownDirectX()
{
    //
    // Clean up DirectX resources in reverse order of creation
    
    if (m_deviceContext)
    {
        m_deviceContext->ClearState();
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
        m_swapChain->SetFullscreenState(FALSE, nullptr);
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
    if (_width <= 0 || _height <= 0)
    {
        AppDebugOut("Invalid window dimensions: %dx%d\n", _width, _height);
        return false;
    }
    
    m_windowed = _windowed;
    m_screenW = _width;
    m_screenH = _height;
    
    //
    // Ensure we have a valid instance handle
    
    if (!m_hInstance)
    {
        m_hInstance = GetModuleHandle(nullptr);
        if (!m_hInstance)
        {
            AppDebugOut("Failed to get module handle\n");
            return false;
        }
    }
    
    AppDebugOut("Creating DirectX 11 window: %dx%d %s\n", 
                _width, _height, _windowed ? "windowed" : "fullscreen");
    
    //
    // Create window class structure
    
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = m_hInstance;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszMenuName = nullptr;
    wc.lpszClassName = "SystemIVD3D11Window";
    wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
    
    UnregisterClass("SystemIVD3D11Window", m_hInstance);
    
    //
    // Register the window class
    
    ATOM classAtom = RegisterClassEx(&wc);
    if (!classAtom)
    {
        DWORD error = GetLastError();
        if (error == ERROR_CLASS_ALREADY_EXISTS)
        {
            AppDebugOut("Window class already exists, will attempt to use it\n");
        }
        else
        {
            AppDebugOut("Failed to register window class, error: %lu (0x%08lX)\n", error, error);
            return false;
        }
    }
    
    //
    // Calculate window size including borders
    
    DWORD style = _windowed ? WS_OVERLAPPEDWINDOW : WS_POPUP;
    RECT windowRect = { 0, 0, _width, _height };
    
    if (_windowed)
    {
        AdjustWindowRect(&windowRect, style, FALSE);
    }
    
    int windowWidth = windowRect.right - windowRect.left;
    int windowHeight = windowRect.bottom - windowRect.top;
    
    //
    // Center window on screen
    
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int posX = (screenWidth - windowWidth) / 2;
    int posY = (screenHeight - windowHeight) / 2;
    
    if (posX < 0) posX = 0;
    if (posY < 0) posY = 0;
    
    //
    // Create window
    
    SetLastError(0);
    m_hwnd = CreateWindowEx(
        0,
        "SystemIVD3D11Window",
        _title,
        style,
        posX, posY,
        windowWidth, windowHeight,
        nullptr,
        nullptr,
        m_hInstance,
        nullptr
    );
    
    if (!m_hwnd)
    {
        DWORD error = GetLastError();
        AppDebugOut("Failed to create window, error: %lu (0x%08lX)\n", error, error);
        
        //
        // Try with simpler style as fallback
        
        AppDebugOut("Attempting fallback with simpler window style...\n");
        DWORD fallbackStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
        RECT fallbackRect = { 0, 0, _width, _height };
        AdjustWindowRect(&fallbackRect, fallbackStyle, FALSE);
        
        int fbWidth = fallbackRect.right - fallbackRect.left;
        int fbHeight = fallbackRect.bottom - fallbackRect.top;
        int fbPosX = (screenWidth - fbWidth) / 2;
        int fbPosY = (screenHeight - fbHeight) / 2;
        if (fbPosX < 0) fbPosX = 0;
        if (fbPosY < 0) fbPosY = 0;
        
        SetLastError(0);
        m_hwnd = CreateWindowEx(
            0,
            "SystemIVD3D11Window",
            _title,
            fallbackStyle,
            fbPosX, fbPosY,
            fbWidth, fbHeight,
            nullptr,
            nullptr,
            m_hInstance,
            nullptr
        );
        
        if (!m_hwnd)
        {
            error = GetLastError();
            AppDebugOut("Fallback window creation also failed, error: %lu (0x%08lX)\n", error, error);
            return false;
        }
        
        AppDebugOut("Fallback window created successfully\n");
    }
    
    AppDebugOut("Window created successfully: %dx%d %s\n", 
                _width, _height, _windowed ? "windowed" : "fullscreen");
    
    //
    // Show window before creating swap chain
    
    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
    
    //
    // Initialize DirectX
    
    AppDebugOut("Initializing DirectX 11...\n");
    if (!InitializeDirectX(_width, _height, _windowed, _antiAlias))
    {
        AppDebugOut("DirectX 11 initialization failed\n");
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
        return false;
    }
    
    //
    // Hide mouse pointer if requested
    
    if (!m_mousePointerVisible)
    {
        HideMousePointer();
    }
    
    if (m_mouseCaptured)
    {
        CaptureMouse();
    }
    
    return true;
}

void WindowManagerD3D11::DestroyWin()
{
    ShutdownDirectX();
    
    if (m_hwnd)
    {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    
    if (m_hInstance)
    {
        UnregisterClass("SystemIVD3D11Window", m_hInstance);
    }
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
    if (!m_hwnd || newWidth <= 0 || newHeight <= 0)
    {
        return;
    }
    
    if (newWidth == m_screenW && newHeight == m_screenH)
    {
        return;
    }
    
    int oldWidth = m_screenW;
    int oldHeight = m_screenH;
    
    m_screenW = newWidth;
    m_screenH = newHeight;
    
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
    // Update viewport
    
    D3D11_VIEWPORT viewport = {};
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = static_cast<float>(newWidth);
    viewport.Height = static_cast<float>(newHeight);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    m_deviceContext->RSSetViewports(1, &viewport);
    
    if (m_windowResizeHandler)
    {
        m_windowResizeHandler(newWidth, newHeight, oldWidth, oldHeight);
    }
}


void WindowManagerD3D11::PollForMessages()
{
    MSG msg = {};
    
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_QUIT)
        {
            PostQuitMessage(0);
        }
        
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    if (m_tryingToCaptureMouse)
    {
        CaptureMouse();
    }
}


void WindowManagerD3D11::SetMousePos(int x, int y)
{
    POINT point = { x, y };
    ClientToScreen(m_hwnd, &point);
    SetCursorPos(point.x, point.y);
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
    
    SetCapture(m_hwnd);
    
    //
    // Clip cursor to window
    
    RECT rect;
    GetClientRect(m_hwnd, &rect);
    POINT topLeft = { rect.left, rect.top };
    POINT bottomRight = { rect.right, rect.bottom };
    ClientToScreen(m_hwnd, &topLeft);
    ClientToScreen(m_hwnd, &bottomRight);
    
    RECT clipRect = { topLeft.x, topLeft.y, bottomRight.x, bottomRight.y };
    ClipCursor(&clipRect);
    
    m_mouseCaptured = true;
    m_tryingToCaptureMouse = false;
}


void WindowManagerD3D11::UncaptureMouse()
{
    ReleaseCapture();
    ClipCursor(nullptr);
    m_mouseCaptured = false;
    m_tryingToCaptureMouse = false;
}


void WindowManagerD3D11::HideMousePointer()
{
    ShowCursor(FALSE);
    m_mousePointerVisible = false;
}


void WindowManagerD3D11::UnhideMousePointer()
{
    ShowCursor(TRUE);
    m_mousePointerVisible = true;
}


void WindowManagerD3D11::HideWin()
{
    if (m_hwnd)
    {
        ShowWindow(m_hwnd, SW_HIDE);
    }
}

void WindowManagerD3D11::OpenWebsite(const char *_url)
{
    ShellExecute(nullptr, "open", _url, nullptr, nullptr, SW_SHOWNORMAL);
}

#endif // RENDERER_DIRECTX11