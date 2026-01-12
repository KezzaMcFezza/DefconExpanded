#ifndef DEBUG_CONSOLE_H
#define DEBUG_CONSOLE_H

#include <string>
#include <vector>
#include "imgui.h"

#define MAX_BUFFER_SIZE 50000
#define MAX_HISTORY_SIZE 1000

class DebugConsole : public ImGuiWindowBase
{
private:
    bool m_isOpen;
    bool m_autoScroll;
    bool m_wasOpen;
    
    std::vector<std::string> m_items;
    std::vector<std::string> m_history;
    int m_historyPos;
    
    char m_inputBuf[256];
    bool m_scrollToBottom;
    
    void Window();
    
public:
    DebugConsole();
    ~DebugConsole();
    
    void Clear();
    void AddLog(const char* fmt, ...);
    
    void Draw() override;
    bool IsOpen() const override { return m_isOpen; }
    
    void SetOpen(bool open) { m_isOpen = open; }
    void Toggle() { m_isOpen = !m_isOpen; }
    
    static DebugConsole* GetInstance();
};

#endif // DEBUG_CONSOLE_H
