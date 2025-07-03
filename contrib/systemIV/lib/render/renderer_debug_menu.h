#ifndef RENDERER_DEBUG_MENU_H
#define RENDERER_DEBUG_MENU_H

#include "lib/render/colour.h"

class Renderer;

class RendererDebugMenu
{
public:
    static const int FRAME_TIME_HISTORY_SIZE = 60; // Track last 60 frames for smooth averages

private:
    Renderer* m_renderer;
    
    // Display options
    bool m_showBufferStats;
    bool m_showMemoryInfo;
    bool m_showOpenGLInfo;
    bool m_showFrameTiming;
    bool m_showDetailedBuffers;
    
    // Frame timing tracking
    double m_frameTimeHistory[FRAME_TIME_HISTORY_SIZE];
    int m_frameTimeIndex;
    double m_averageFrameTime;

public:
    RendererDebugMenu(Renderer* renderer);
    ~RendererDebugMenu();
    
    // Main interface
    void Update(double frameTime);
    void RenderDebugMenu();
    
    // Display toggles
    void SetShowBufferStats(bool show) { m_showBufferStats = show; }
    void SetShowMemoryInfo(bool show) { m_showMemoryInfo = show; }
    void SetShowOpenGLInfo(bool show) { m_showOpenGLInfo = show; }
    void SetShowFrameTiming(bool show) { m_showFrameTiming = show; }
    void SetShowDetailedBuffers(bool show) { m_showDetailedBuffers = show; }
    
    bool GetShowBufferStats() const { return m_showBufferStats; }
    bool GetShowMemoryInfo() const { return m_showMemoryInfo; }
    bool GetShowOpenGLInfo() const { return m_showOpenGLInfo; }
    bool GetShowFrameTiming() const { return m_showFrameTiming; }
    bool GetShowDetailedBuffers() const { return m_showDetailedBuffers; }

private:
    // Page rendering methods
    void RenderBufferStatistics(float& yPos);
    void RenderSystemInformation(float& yPos);
    
    // Analysis methods
    size_t GetMemoryUsage();
    int EstimateBufferVertexCount();
    int EstimateTextureSwitches();
};

#endif // RENDERER_DEBUG_MENU_H 