#ifndef IMGUI_TEXT_EDITOR_H
#define IMGUI_TEXT_EDITOR_H

#include "lib/imgui/imgui.h"

class TextEditor : public ImGuiWindowBase
{
private:
    bool m_isOpen;
    char m_filePath[512];
    char* m_textBuffer;
    size_t m_bufferSize;
    bool m_modified;
    
public:
    TextEditor();
    ~TextEditor();
    
    void Draw() override;
    bool IsOpen() const override { return m_isOpen; }
    
    void SetOpen(bool open) { m_isOpen = open; }
    void Toggle() { m_isOpen = !m_isOpen; }
    
    bool LoadFile(const char* filePath);
    bool SaveFile();
    void Close();
    bool IsModified() const { return m_modified; }
};

#endif // IMGUI_TEXT_EDITOR_H
