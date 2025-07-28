#include "lib/universal_include.h"
#include "lib/render2d/renderer.h"
#include "lib/render3d/renderer_3d.h"
#include "lib/render/renderer_debug_menu.h"
#include "lib/resource/resource.h"
#include "lib/hi_res_time.h"
#include "app/globals.h"
#include "app/app.h"
#include "interface/interface.h"
#include "renderer/map_renderer.h"

#ifdef TARGET_OS_WINDOWS
#include <windows.h>
#include <psapi.h>
#endif

#ifdef TARGET_OS_LINUX
#include <fstream>
#include <sstream>
#endif

#ifdef TARGET_OS_MACOSX
#include <mach/mach.h>
#include <sys/sysctl.h>
#endif

RendererDebugMenu::RendererDebugMenu(Renderer* renderer)
    : m_renderer(renderer)
    , m_showBufferStats(true)
    , m_showMemoryInfo(true)
    , m_showOpenGLInfo(true)
    , m_showFrameTiming(true)
    , m_showDetailedBuffers(true)
    , m_frameTimeHistory()
    , m_frameTimeIndex(0)
    , m_averageFrameTime(0.0)
{
    // Initialize frame time history
    for (int i = 0; i < FRAME_TIME_HISTORY_SIZE; i++) {
        m_frameTimeHistory[i] = 0.0;
    }
}

RendererDebugMenu::~RendererDebugMenu()
{
}

void RendererDebugMenu::Update(double frameTime)
{
    // Update frame time history for smoothed averages
    m_frameTimeHistory[m_frameTimeIndex] = frameTime;
    m_frameTimeIndex = (m_frameTimeIndex + 1) % FRAME_TIME_HISTORY_SIZE;
    
    // Calculate rolling average
    double sum = 0.0;
    for (int i = 0; i < FRAME_TIME_HISTORY_SIZE; i++) {
        sum += m_frameTimeHistory[i];
    }
    m_averageFrameTime = sum / FRAME_TIME_HISTORY_SIZE;
}

void RendererDebugMenu::RenderDebugMenu()
{
    if (!m_renderer) return;
    
    float baseY = 55.0f;  // Move debug menu down to avoid FPS overlap
    float lineHeight = 15.0f;
    float yPos = baseY;
    
    // Render all sections on one page
    RenderBufferStatistics(yPos);
    yPos += 5.0f; // Add some spacing between sections
    RenderSystemInformation(yPos);
    yPos += 5.0f;
}

void RendererDebugMenu::RenderBufferStatistics(float& yPos)
{
    char statsBuffer[512];
    float lineHeight = 18.0f;
    float indentSmall = 20.0f;
    float indentLarge = 200.0f;
    
    //
    // detect if we are in 3d mode

    bool is3DMode = g_app->GetMapRenderer()->Is3DGlobeModeEnabled();
    
    if (is3DMode) {
        m_renderer->TextSimple(25, yPos, Colour(255, 255, 255, 255), 13.0f, "Advanced Buffer Statistics (3D Mode)");
    } else {
        m_renderer->TextSimple(25, yPos, Colour(255, 255, 255, 255), 13.0f, "Advanced Buffer Statistics (2D Mode)");
    }
    yPos += lineHeight;
    
    int totalDrawCalls, legacyTriangleCalls, legacyLineCalls;
    int uiTriangleCalls, uiLineCalls, textCalls, spriteCalls;
    int unitTrailCalls, unitMainSpriteCalls, unitRotatingCalls, unitHighlightCalls;
    int unitStateIconCalls, unitCounterCalls, unitNukeIconCalls;
    int effectsLineCalls, effectsSpriteCalls;
    int totalUnitCalls, totalEffectCalls, totalSpecializedCalls;
    
    if (is3DMode && g_renderer3d) {
        totalDrawCalls = g_renderer3d->GetTotalDrawCalls() + m_renderer->GetTotalDrawCalls();
        legacyTriangleCalls = g_renderer3d->GetLegacyTriangleCalls() + m_renderer->GetLegacyTriangleCalls();
        legacyLineCalls = g_renderer3d->GetLegacyLineCalls() + m_renderer->GetLegacyLineCalls();
        uiTriangleCalls = m_renderer->GetUITriangleCalls(); 
        uiLineCalls = m_renderer->GetUILineCalls();
        textCalls = g_renderer3d->GetTextCalls() + m_renderer->GetTextCalls();
        spriteCalls = g_renderer3d->GetSpriteCalls() + m_renderer->GetSpriteCalls();
        unitTrailCalls = g_renderer3d->GetUnitTrailCalls();
        unitMainSpriteCalls = g_renderer3d->GetUnitMainSpriteCalls();
        unitRotatingCalls = g_renderer3d->GetUnitRotatingCalls();
        unitHighlightCalls = g_renderer3d->GetUnitHighlightCalls();
        unitStateIconCalls = g_renderer3d->GetUnitStateIconCalls();
        unitCounterCalls = g_renderer3d->GetUnitCounterCalls();
        unitNukeIconCalls = g_renderer3d->GetUnitNukeIconCalls();
        effectsLineCalls = g_renderer3d->GetEffectsLineCalls();
        effectsSpriteCalls = g_renderer3d->GetEffectsSpriteCalls();
        totalUnitCalls = g_renderer3d->GetTotalUnitCalls();
        totalEffectCalls = g_renderer3d->GetTotalEffectCalls();
        totalSpecializedCalls = g_renderer3d->GetTotalSpecializedCalls() + m_renderer->GetUITriangleCalls() + m_renderer->GetUILineCalls() + m_renderer->GetTextCalls() + m_renderer->GetSpriteCalls();
    } else {
        // 2d map renderer scene only
        totalDrawCalls = m_renderer->GetTotalDrawCalls();
        legacyTriangleCalls = m_renderer->GetLegacyTriangleCalls();
        legacyLineCalls = m_renderer->GetLegacyLineCalls();
        uiTriangleCalls = m_renderer->GetUITriangleCalls();
        uiLineCalls = m_renderer->GetUILineCalls();
        textCalls = m_renderer->GetTextCalls();
        spriteCalls = m_renderer->GetSpriteCalls();
        unitTrailCalls = m_renderer->GetUnitTrailCalls();
        unitMainSpriteCalls = m_renderer->GetUnitMainSpriteCalls();
        unitRotatingCalls = m_renderer->GetUnitRotatingCalls();
        unitHighlightCalls = m_renderer->GetUnitHighlightCalls();
        unitStateIconCalls = m_renderer->GetUnitStateIconCalls();
        unitCounterCalls = m_renderer->GetUnitCounterCalls();
        unitNukeIconCalls = m_renderer->GetUnitNukeIconCalls();
        effectsLineCalls = m_renderer->GetEffectsLineCalls();
        effectsSpriteCalls = m_renderer->GetEffectsSpriteCalls();
        totalUnitCalls = m_renderer->GetTotalUnitCalls();
        totalEffectCalls = m_renderer->GetTotalEffectCalls();
        totalSpecializedCalls = m_renderer->GetTotalSpecializedCalls();
    }
    
    // Total draw calls header
    snprintf(statsBuffer, sizeof(statsBuffer), "Total Draw Calls: %d", totalDrawCalls);
    m_renderer->TextSimple(25, yPos, Colour(255, 255, 255, 255), 12.0f, statsBuffer);
    yPos += lineHeight;
    
    // Legacy immediate mode buffers (RED - performance issues)
    m_renderer->TextSimple(25, yPos, Colour(255, 100, 100, 255), 11.0f, "Immediate Mode:");
    snprintf(statsBuffer, sizeof(statsBuffer), "Triangles: %d  Lines: %d", 
             legacyTriangleCalls, legacyLineCalls);
    m_renderer->TextSimple(indentLarge, yPos, Colour(255, 100, 100, 255), 11.0f, statsBuffer);
    yPos += lineHeight;
    
    // Core UI buffers (GREEN - optimized) - KEEP EXACT SAME LAYOUT
    m_renderer->TextSimple(25, yPos, Colour(100, 255, 100, 255), 11.0f, "UI Buffers:");
    snprintf(statsBuffer, sizeof(statsBuffer), "Triangles: %d  Lines: %d  Text: %d  Textures: %d", 
             uiTriangleCalls, uiLineCalls, textCalls, spriteCalls);
    m_renderer->TextSimple(175, yPos, Colour(100, 255, 100, 255), 11.0f, statsBuffer);
    yPos += lineHeight;
    
    // Unit rendering specialized buffers (BLUE - new system)
    m_renderer->TextSimple(25, yPos, Colour(100, 150, 255, 255), 11.0f, "Unit Rendering Buffers:");
    snprintf(statsBuffer, sizeof(statsBuffer), "Total: %d", totalUnitCalls);
    m_renderer->TextSimple(indentLarge, yPos, Colour(100, 150, 255, 255), 11.0f, statsBuffer);
    yPos += lineHeight;
    
    // Unit buffer details (indented)
    snprintf(statsBuffer, sizeof(statsBuffer), "  Trails: %d  Static: %d  Rotating: %d  Highlights: %d", 
             unitTrailCalls, unitMainSpriteCalls, 
             unitRotatingCalls, unitHighlightCalls);
    m_renderer->TextSimple(indentSmall, yPos, Colour(120, 170, 255, 255), 10.0f, statsBuffer);
    yPos += 14.0f;
    
    snprintf(statsBuffer, sizeof(statsBuffer), "  UnitIcons: %d  Counters: %d  Nukes: %d", 
             unitStateIconCalls, unitCounterCalls, 
             unitNukeIconCalls);
    m_renderer->TextSimple(indentSmall, yPos, Colour(120, 170, 255, 255), 10.0f, statsBuffer);
    yPos += lineHeight;
    
    // Effects rendering specialized buffers (YELLOW - new system)
    m_renderer->TextSimple(25, yPos, Colour(255, 255, 100, 255), 11.0f, "Effects Buffers:");
    snprintf(statsBuffer, sizeof(statsBuffer), "Lines: %d  Sprites: %d  (Total: %d)", 
             effectsLineCalls, effectsSpriteCalls, totalEffectCalls);
    m_renderer->TextSimple(140, yPos, Colour(255, 255, 100, 255), 11.0f, statsBuffer);
    yPos += lineHeight;
    
    // Performance summary (CYAN - analysis)
    int legacyTotal = legacyTriangleCalls + legacyLineCalls;
    m_renderer->TextSimple(25, yPos, Colour(100, 255, 255, 255), 11.0f, "Conclusion:");
    snprintf(statsBuffer, sizeof(statsBuffer), "Batched: %d  Legacy: %d", 
             totalSpecializedCalls, legacyTotal);
    m_renderer->TextSimple(indentLarge, yPos, Colour(100, 255, 255, 255), 11.0f, statsBuffer);
    yPos += lineHeight;
    
}

void RendererDebugMenu::RenderSystemInformation(float& yPos)
{
    char infoBuffer[256];
    float lineHeight = 18.0f;
    
    // Header
    m_renderer->TextSimple(25, yPos, Colour(255, 255, 255, 255), 13.0f, "General Information");
    yPos += lineHeight;
    
    // Frame timing information
    if (m_showFrameTiming) {
        m_renderer->TextSimple(25, yPos, Colour(255, 200, 100, 255), 12.0f, "Frame Times:");
        yPos += lineHeight;
        
        snprintf(infoBuffer, sizeof(infoBuffer), "  Average Frame Time: %.2f ms", m_averageFrameTime * 1000.0);
        m_renderer->TextSimple(35, yPos, Colour(200, 200, 200, 255), 11.0f, infoBuffer);
        yPos += 14.0f;
        
        double fps = m_averageFrameTime > 0.0 ? 1.0 / m_averageFrameTime : 0.0;
        snprintf(infoBuffer, sizeof(infoBuffer), "  Average FPS: %.1f", fps);
        m_renderer->TextSimple(35, yPos, Colour(200, 200, 200, 255), 11.0f, infoBuffer);
        yPos += lineHeight;
    }
    
    // Memory information
    if (m_showMemoryInfo) {
        m_renderer->TextSimple(25, yPos, Colour(100, 255, 200, 255), 12.0f, "Memory Usage:");
        yPos += lineHeight;
        
        size_t memoryUsage = GetMemoryUsage();
        if (memoryUsage > 0) {
            snprintf(infoBuffer, sizeof(infoBuffer), "  Process Memory: %.1f MB", memoryUsage / (1024.0 * 1024.0));
            m_renderer->TextSimple(35, yPos, Colour(200, 200, 200, 255), 11.0f, infoBuffer);
            yPos += 14.0f;
        }
        
        // Estimated buffer memory usage
        int totalBufferVertices = EstimateBufferVertexCount();
        float bufferMemoryMB = (totalBufferVertices * sizeof(Vertex2D)) / (1024.0f * 1024.0f);
        snprintf(infoBuffer, sizeof(infoBuffer), "  Buffer Memory: ~%.2f MB (%d vertices)", 
                 bufferMemoryMB, totalBufferVertices);
        m_renderer->TextSimple(35, yPos, Colour(200, 200, 200, 255), 11.0f, infoBuffer);
        yPos += lineHeight;
    }
}



size_t RendererDebugMenu::GetMemoryUsage()
{
#ifdef TARGET_OS_WINDOWS
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize;
    }
#endif

#ifdef TARGET_OS_LINUX
    std::ifstream file("/proc/self/status");
    std::string line;
    while (std::getline(file, line)) {
        if (line.substr(0, 6) == "VmRSS:") {
            std::istringstream iss(line);
            std::string key, value, unit;
            iss >> key >> value >> unit;
            return std::stoi(value) * 1024; // Convert from KB to bytes
        }
    }
#endif

#ifdef TARGET_OS_MACOSX
    task_info_data_t tinfo;
    mach_msg_type_number_t task_info_count = TASK_INFO_MAX;
    if (task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)tinfo, &task_info_count) == KERN_SUCCESS) {
        task_basic_info_t basic_info = (task_basic_info_t)tinfo;
        return basic_info->resident_size;
    }
#endif

    return 0; // Unable to determine
}

int RendererDebugMenu::EstimateBufferVertexCount()
{
    // Rough estimation based on typical vertex counts per draw call type
    int estimate = 0;
    
    // Lines use 2 vertices per call, triangles use 3+ vertices per call
    estimate += m_renderer->GetLegacyLineCalls() * 2;
    estimate += m_renderer->GetLegacyTriangleCalls() * 6; // Assuming quads
    estimate += m_renderer->GetUILineCalls() * 4; // UI lines often batched
    estimate += m_renderer->GetUITriangleCalls() * 6; // UI quads
    estimate += m_renderer->GetTextCalls() * 24; // Text often has multiple quads
    estimate += m_renderer->GetSpriteCalls() * 6; // Sprite quads
    
    // Unit rendering
    estimate += m_renderer->GetUnitTrailCalls() * 8; // Trail segments
    estimate += m_renderer->GetUnitMainSpriteCalls() * 6; // Unit sprites
    estimate += m_renderer->GetUnitRotatingCalls() * 6; // Rotating sprites (aircraft/nukes)
    estimate += m_renderer->GetUnitHighlightCalls() * 6; // Highlight quads
    estimate += m_renderer->GetUnitStateIconCalls() * 6; // State icon quads
    estimate += m_renderer->GetUnitNukeIconCalls() * 6; // Nuke icon quads
    
    // Effects
    estimate += m_renderer->GetEffectsLineCalls() * 6; // Effect lines often grouped
    estimate += m_renderer->GetEffectsSpriteCalls() * 6; // Effect sprites
    
    return estimate;
}

int RendererDebugMenu::EstimateTextureSwitches()
{
    // Rough estimation based on different texture types
    int switches = 0;
    
    // Each different type of textured call likely uses different textures
    if (m_renderer->GetTextCalls() > 0) switches++;
    if (m_renderer->GetSpriteCalls() > 0) switches += 5; // Assuming ~5 different sprite textures
    if (m_renderer->GetUnitMainSpriteCalls() > 0) switches += 8; // Different unit types
    if (m_renderer->GetUnitRotatingCalls() > 0) switches += 3; // Aircraft/rotating nuke textures
    if (m_renderer->GetUnitStateIconCalls() > 0) switches += 3; // Fighter, bomber, nuke icons
    if (m_renderer->GetUnitNukeIconCalls() > 0) switches++;
    if (m_renderer->GetEffectsSpriteCalls() > 0) switches += 3; // Different effect types
    
    return switches;
}

 