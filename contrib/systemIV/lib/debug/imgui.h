#ifndef INCLUDED_IMGUI_H
#define INCLUDED_IMGUI_H

#include "systemiv.h"
#include "lib/tosser/llist.h"

//
// Percentages of viewport
// This means the console has the same layout and position 
// no matter the users resolution or aspect ratio.

#define DEBUG_CONSOLE_DEFAULT_X_PERCENT      37.3f
#define DEBUG_CONSOLE_DEFAULT_Y_PERCENT      2.7f
#define DEBUG_CONSOLE_DEFAULT_WIDTH_PERCENT  61.6f
#define DEBUG_CONSOLE_DEFAULT_HEIGHT_PERCENT 74.7f

#define SIZE_IMGUI_WINDOW_NAME 256

class ImGuiWindowBase
{
public:
    char m_name[SIZE_IMGUI_WINDOW_NAME];
    
    ImGuiWindowBase() { m_name[0] = '\0'; }
    virtual ~ImGuiWindowBase() {}
    
    void SetName(const char* name);
    
    virtual void Draw() = 0;
    virtual bool IsOpen() const = 0;
};

class ImGuiManager
{
private:
    bool m_initialized;
    LList<ImGuiWindowBase*> m_windows;
    
    friend void ImGuiRegisterWindow    (ImGuiWindowBase* window);
    friend void ImGuiRemoveWindow      (const char* name);
    friend void ImGuiRemoveAllWindows  ();

    friend ImGuiWindowBase         *ImGuiGetWindow(const char* name);
    friend LList<ImGuiWindowBase*> *ImGuiGetWindows();

    friend bool ImGuiWantsMouse();
    
public:
    ImGuiManager();
    ~ImGuiManager();
    
    bool Initialize();
    void Shutdown();
    void Render();
    
    bool IsInitialized() const { return m_initialized; }
};

extern ImGuiManager* g_imguiManager;

void ImGuiRegisterWindow  (ImGuiWindowBase* window);
void ImGuiRemoveWindow    (const char* name);

void ImGuiRemoveAllWindows();

ImGuiWindowBase         *ImGuiGetWindow(const char* name);
LList<ImGuiWindowBase*> *ImGuiGetWindows();

bool ImGuiWantsMouse();

#endif // INCLUDED_IMGUI_H
