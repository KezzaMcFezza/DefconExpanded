#ifndef INCLUDED_WINDOW_MANAGER_D3D11_H
#define INCLUDED_WINDOW_MANAGER_D3D11_H

#ifdef RENDERER_DIRECTX11

#include "window_manager.h"

#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <SDL2/SDL.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

class WindowManagerD3D11 : public WindowManager
{
public:
    WindowManagerD3D11();
    virtual ~WindowManagerD3D11();

    virtual bool CreateWin(int _width, int _height, bool _windowed, int _colourDepth,
                          int _refreshRate, int _zDepth, int _antiAlias, bool _borderless,
                          const char *_title) override;
    
    virtual void HideWin() override;
    virtual void DestroyWin() override;
    virtual void Flip() override;
    virtual void PollForMessages() override;
    virtual void SetMousePos(int x, int y) override;
    virtual void CaptureMouse() override;
    virtual void UncaptureMouse() override;
    virtual void HideMousePointer() override;
    virtual void UnhideMousePointer() override;
    virtual void OpenWebsite(const char *_url) override;

    ID3D11Device          * GetDevice()           const { return m_device; }
    ID3D11DeviceContext   * GetDeviceContext()    const { return m_deviceContext; }
    IDXGISwapChain        * GetSwapChain()        const { return m_swapChain; }
    ID3D11RenderTargetView* GetRenderTargetView() const { return m_renderTargetView; }
    ID3D11DepthStencilView* GetDepthStencilView() const { return m_depthStencilView; }

private:
    HWND m_hwnd;                    // Native Windows handle (extracted from SDL)
    HINSTANCE m_hInstance;          // Windows instance handle
    SDL_Window* m_sdlWindow;        // SDL window (primary window manager)
    
    ID3D11Device          * m_device;
    ID3D11DeviceContext   * m_deviceContext;
    IDXGISwapChain        * m_swapChain;
    ID3D11RenderTargetView* m_renderTargetView;
    ID3D11Texture2D       * m_depthStencilBuffer;
    ID3D11DepthStencilView* m_depthStencilView;
    
    D3D_FEATURE_LEVEL m_featureLevel;
    
    bool m_tryingToCaptureMouse;
    bool m_vsyncEnabled;
    int  m_windowDisplayIndex;
    bool m_isMaximized;
    
    void ListAllDisplayModes(int displayIndex);
    void SaveDesktop();
    void RestoreDesktop();
    void ShutdownDirectX();

    bool CreateRenderTargetView();
    
    bool CreateDepthStencilView(int width, int height);
    void HandleResize          (int newWidth, int newHeight);
    bool InitializeDirectX     (int width, int height, bool windowed, int msaaSamples);

    void CalculateHighDPIScaleFactors();
    void WindowHasMoved();
    void UpdateStoredMaximizedState();
    int  GetDefaultDisplayIndex();
};

#endif // RENDERER_DIRECTX11
#endif // INCLUDED_WINDOW_MANAGER_D3D11_H