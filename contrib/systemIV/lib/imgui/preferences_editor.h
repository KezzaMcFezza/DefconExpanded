#ifndef IMGUI_PREFERENCES_EDIT_H
#define IMGUI_PREFERENCES_EDIT_H

#include "lib/imgui/imgui.h"

class TextEditor;

class PreferencesEdit : public ImGuiWindowBase
{
private:
    TextEditor* m_textEditor;
    
public:
    PreferencesEdit();
    ~PreferencesEdit();
    
    void Draw() override;
    bool IsOpen() const override;
    
    void Open();
    void Close();
    void Toggle();
};

#endif // IMGUI_PREFERENCES_EDIT_H
