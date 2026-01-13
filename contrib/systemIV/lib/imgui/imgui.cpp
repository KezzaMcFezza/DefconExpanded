#include "systemiv.h"

#include <string.h>
#include "imgui-1.92.5/imgui.h"
#include "imgui-1.92.5/backends/imgui_impl_sdl2.h"
#include "lib/gucci/window_manager.h"
#include "lib/gucci/window_manager_opengl.h"
#include "lib/gucci/window_manager_d3d11.h"
#include "lib/debug/debug_utils.h"

#ifdef RENDERER_OPENGL
#include "imgui-1.92.5/backends/imgui_impl_opengl3.h"
#endif

#ifdef RENDERER_DIRECTX11
#include "imgui-1.92.5/backends/imgui_impl_dx11.h"
#endif

#include "imgui.h"

ImGuiManager* g_imguiManager = nullptr;

void ImGuiWindowBase::SetName(const char* name)
{
    if (strlen(name) >= SIZE_IMGUI_WINDOW_NAME)
    {
        AppReleaseAssert(false, "ImGui window name too long: %s", name);
        return;
    }
    
    strcpy(m_name, name);
}

ImGuiManager::ImGuiManager()
: m_initialized(false)
{
}

ImGuiManager::~ImGuiManager()
{
    Shutdown();
}

bool ImGuiManager::Initialize()
{
    if (m_initialized)
    {
        return true;
    }
    
    if (!g_windowManager)
    {
        return false;
    }
    
    SDL_Window* sdlWindow = g_windowManager->GetSDLWindow();
    
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;
    
    ImGui::StyleColorsDark();
    
    RendererType rendererType = GetRendererType();
    
#ifdef RENDERER_OPENGL
    if (rendererType == RENDERER_TYPE_OPENGL)
    {
        SDL_GLContext glContext = SDL_GL_GetCurrentContext();
        if (!glContext)
        {
            #ifdef _DEBUG
            AppDebugOut("Cannot initialize ImGui OpenGL backend: no OpenGL context\n");
            #endif
            ImGui::DestroyContext();
            return false;
        }
        
        const char* glsl_version = nullptr;
#if defined(TARGET_OS_MACOSX)
        glsl_version = "#version 150";
#elif defined(TARGET_EMSCRIPTEN)
        glsl_version = "#version 300 es";
#else
        glsl_version = "#version 330";
#endif
        
        if (!ImGui_ImplSDL2_InitForOpenGL(sdlWindow, glContext))
        {
            #ifdef _DEBUG
            AppDebugOut("Failed to initialize ImGui SDL2 backend for OpenGL\n");
            #endif
            ImGui::DestroyContext();
            return false;
        }
        
        if (!ImGui_ImplOpenGL3_Init(glsl_version))
        {
            #ifdef _DEBUG
            AppDebugOut("Failed to initialize ImGui OpenGL3 backend\n");
            #endif
            ImGui_ImplSDL2_Shutdown();
            ImGui::DestroyContext();
            return false;
        }
        
        m_initialized = true;
        return true;
    }
#endif

#ifdef RENDERER_DIRECTX11
    if (rendererType == RENDERER_TYPE_DIRECTX11)
    {
        WindowManagerD3D11* d3d11Manager = dynamic_cast<WindowManagerD3D11*>(g_windowManager);
        if (!d3d11Manager)
        {
            #ifdef _DEBUG
            AppDebugOut("Cannot initialize ImGui DirectX11 backend: window manager is not D3D11\n");
            #endif
            ImGui::DestroyContext();
            return false;
        }
        
        ID3D11Device* device = d3d11Manager->GetDevice();
        ID3D11DeviceContext* deviceContext = d3d11Manager->GetDeviceContext();
        
        if (!device || !deviceContext)
        {
            #ifdef _DEBUG
            AppDebugOut("Cannot initialize ImGui DirectX11 backend: device or context not available\n");
            #endif
            ImGui::DestroyContext();
            return false;
        }
        
        if (!ImGui_ImplSDL2_InitForD3D(sdlWindow))
        {
            #ifdef _DEBUG
            AppDebugOut("Failed to initialize ImGui SDL2 backend for D3D\n");
            #endif
            ImGui::DestroyContext();
            return false;
        }
        
        if (!ImGui_ImplDX11_Init(device, deviceContext))
        {
            #ifdef _DEBUG
            AppDebugOut("Failed to initialize ImGui DirectX11 backend\n");
            #endif
            ImGui_ImplSDL2_Shutdown();
            ImGui::DestroyContext();
            return false;
        }
        
        m_initialized = true;
        return true;
    }
#endif

    ImGui::DestroyContext();
    return false;
}

void ImGuiManager::Shutdown()
{
    if (!m_initialized)
        return;
    
    m_windows.Empty();
    
    RendererType rendererType = GetRendererType();
    
#ifdef RENDERER_OPENGL
    if (rendererType == RENDERER_TYPE_OPENGL)
    {
        ImGui_ImplOpenGL3_Shutdown();
    }
#endif

#ifdef RENDERER_DIRECTX11
    if (rendererType == RENDERER_TYPE_DIRECTX11)
    {
        ImGui_ImplDX11_Shutdown();
    }
#endif

    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    
    m_initialized = false;
}

static void ImGuiNewFrame()
{
    if (!g_imguiManager || !g_imguiManager->IsInitialized())
        return;
    
    RendererType rendererType = GetRendererType();
    
#ifdef RENDERER_OPENGL
    if (rendererType == RENDERER_TYPE_OPENGL)
    {
        ImGui_ImplOpenGL3_NewFrame();
    }
#endif

#ifdef RENDERER_DIRECTX11
    if (rendererType == RENDERER_TYPE_DIRECTX11)
    {
        ImGui_ImplDX11_NewFrame();
    }
#endif

    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void ImGuiManager::Render()
{
    if (!m_initialized)
        return;
    
    //
    // Check if any window wants to be rendered
    
    bool hasAnyOpen = false;
    for (int i = 0; i < m_windows.Size(); ++i)
    {
        ImGuiWindowBase* window = m_windows.GetData(i);
        if (window && window->IsOpen())
        {
            hasAnyOpen = true;
            break;
        }
    }
    
    if (!hasAnyOpen)
        return;
    
    //
    // Start ImGui frame and render all open windows
    
    ImGuiNewFrame();
    
    for (int i = 0; i < m_windows.Size(); ++i)
    {
        ImGuiWindowBase* window = m_windows.GetData(i);
        if (window && window->IsOpen())
        {
            window->Draw();
        }
    }
    
    ImGui::Render();
    
    RendererType rendererType = GetRendererType();
    
#ifdef RENDERER_OPENGL
    if (rendererType == RENDERER_TYPE_OPENGL)
    {
        ImDrawData* drawData = ImGui::GetDrawData();
        if (drawData)
        {
            ImGui_ImplOpenGL3_RenderDrawData(drawData);
        }
    }
#endif

#ifdef RENDERER_DIRECTX11
    if (rendererType == RENDERER_TYPE_DIRECTX11)
    {
        ImDrawData* drawData = ImGui::GetDrawData();
        if (drawData)
        {
            ImGui_ImplDX11_RenderDrawData(drawData);
        }
    }
#endif

    ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput)
    {
        SDL_StartTextInput();
    }
    else
    {
        SDL_StopTextInput();
    }
}

void ImGuiRegisterWindow(ImGuiWindowBase* window)
{
    if (!window)
    {
        AppDebugOut("ImGuiRegisterWindow: window is NULL\n");
        return;
    }
    
    if (!g_imguiManager)
    {
        AppDebugOut("ImGuiRegisterWindow: g_imguiManager is NULL\n");
        return;
    }
    
    //
    // Check if already registered
    
    for (int i = 0; i < g_imguiManager->m_windows.Size(); ++i)
    {
        if (g_imguiManager->m_windows.GetData(i) == window)
        {
            AppDebugOut("ImGuiRegisterWindow: window '%s' already registered\n", window->m_name);
            return;
        }
    }
    
    g_imguiManager->m_windows.PutData(window);
}

void ImGuiRemoveWindow(const char* name)
{
    if (!name || !g_imguiManager)
        return;
    
    for (int i = 0; i < g_imguiManager->m_windows.Size(); ++i)
    {
        ImGuiWindowBase* window = g_imguiManager->m_windows.GetData(i);
        if (window && strcmp(window->m_name, name) == 0)
        {
            g_imguiManager->m_windows.RemoveData(i);
            return;
        }
    }
    
    AppDebugOut("ImGuiRemoveWindow: window '%s' not found\n", name);
}

void ImGuiRemoveAllWindows()
{
    if (!g_imguiManager)
        return;
    
    g_imguiManager->m_windows.Empty();
}

ImGuiWindowBase* ImGuiGetWindow(const char* name)
{
    if (!name || !g_imguiManager)
        return NULL;
    
    for (int i = 0; i < g_imguiManager->m_windows.Size(); ++i)
    {
        ImGuiWindowBase* window = g_imguiManager->m_windows.GetData(i);
        if (window && strcmp(window->m_name, name) == 0)
        {
            return window;
        }
    }
    
    return NULL;
}

LList<ImGuiWindowBase*>* ImGuiGetWindows()
{
    if (!g_imguiManager)
        return NULL;
    
    return &g_imguiManager->m_windows;
}

bool ImGuiWantsMouse()
{
    if (!g_imguiManager || !g_imguiManager->IsInitialized())
        return false;
    
    for (int i = 0; i < g_imguiManager->m_windows.Size(); ++i)
    {
        ImGuiWindowBase* window = g_imguiManager->m_windows.GetData(i);
        if (window && window->IsOpen())
        {
            return true;
        }
    }
    
    return false;
}
