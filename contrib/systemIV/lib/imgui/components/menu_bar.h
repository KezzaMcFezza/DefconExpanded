#ifndef IMGUI_MENU_BAR_H
#define IMGUI_MENU_BAR_H

#include "lib/imgui/imgui.h"

class PreferencesEdit;

typedef void (*MenuBarCallback)();

class MenuBar : public ImGuiWindowBase
{
private:
    bool m_isOpen;
    PreferencesEdit* m_preferencesEdit;
    bool m_immediateMode2D;
    bool m_immediateMode3D;
    bool m_frameDebugger;
    MenuBarCallback m_windowReinitCallback;
    
public:
    MenuBar();
    ~MenuBar();
    
    void Draw() override;
    bool IsOpen() const override { return m_isOpen; }
    
    void SetOpen(bool open) { m_isOpen = open; }
    void Toggle() { m_isOpen = !m_isOpen; }
    
    void SetWindowReinitCallback(MenuBarCallback callback) { m_windowReinitCallback = callback; }
    void ReregisterWindows();

    bool GetImmediateMode2D() const { return m_immediateMode2D; }
    bool GetImmediateMode3D() const { return m_immediateMode3D; }
};

#endif // IMGUI_MENU_BAR_H
