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
    //
    // update frame time history for smoothed averages

    m_frameTimeHistory[m_frameTimeIndex] = frameTime;
    m_frameTimeIndex = (m_frameTimeIndex + 1) % FRAME_TIME_HISTORY_SIZE;
    
    //
    // calculate rolling average

    double sum = 0.0;
    int validFrames = 0;
    for (int i = 0; i < FRAME_TIME_HISTORY_SIZE; i++) {
        if (m_frameTimeHistory[i] > 0.0) {
            sum += m_frameTimeHistory[i];
            validFrames++;
        }
    }
    
    //
    // only update average if we have valid frames

    if (validFrames > 0) {
        m_averageFrameTime = sum / validFrames;
    }
}

void RendererDebugMenu::RenderDebugMenu()
{
    if (!m_renderer) return;
    
    float baseY = 55.0f;  // Move debug menu down to avoid FPS overlap
    float yPos = baseY;
    
    //
    // render it

    RenderBufferStatistics(yPos);
    yPos += 2.0f; 
#ifndef TARGET_EMSCRIPTEN
    RenderFlushTimings(yPos);  
    yPos += 2.0f;
#endif
    RenderSystemInformation(yPos);
    yPos += 2.0f;
}

void RendererDebugMenu::RenderBufferStatistics(float& yPos)
{
    if (!m_renderer) return;
    
    char statsBuffer[512];
    float lineHeight = 18.0f;
    float indentSmall = 20.0f;
    float indentLarge = 200.0f;
    
    //
    // render text for both 2d and 3d

    m_renderer->TextSimple(25, yPos, Colour(255, 255, 255, 255), 13.0f, "Global Buffer Statistics");
    yPos += lineHeight;
    
    int totalDrawCalls, legacyTriangleCalls, legacyLineCalls;
    int textCalls;
    int lineCalls, staticSpriteCalls, rotatingSpriteCalls;
    int circleFillCalls, rectFillCalls, triangleFillCalls;
    int eclipseRectCalls, eclipseRectFillCalls, eclipseTriangleFillCalls, eclipseLineCalls, eclipseSpriteCalls;
    
    //
    // start with 2D renderer stats as base
    
    totalDrawCalls = m_renderer->GetTotalDrawCalls();
    legacyTriangleCalls = m_renderer->GetLegacyTriangleCalls();
    legacyLineCalls = m_renderer->GetLegacyLineCalls();
    textCalls = m_renderer->GetTextCalls();
    lineCalls = m_renderer->GetLineCalls();
    staticSpriteCalls = m_renderer->GetStaticSpriteCalls();
    rotatingSpriteCalls = m_renderer->GetRotatingSpriteCalls();
    circleFillCalls = m_renderer->GetCircleFillCalls();
    rectFillCalls = m_renderer->GetRectFillCalls();
    triangleFillCalls = m_renderer->GetTriangleFillCalls();
    eclipseRectCalls = m_renderer->GetEclipseRectCalls();
    eclipseRectFillCalls = m_renderer->GetEclipseRectFillCalls();
    eclipseTriangleFillCalls = m_renderer->GetEclipseTriangleFillCalls();
    eclipseLineCalls = m_renderer->GetEclipseLineCalls();
    eclipseSpriteCalls = m_renderer->GetEclipseSpriteCalls();

    //
    // add 3D stats to combine them

    if (g_renderer3d) {
        totalDrawCalls += g_renderer3d->GetTotalDrawCalls();
        legacyTriangleCalls += g_renderer3d->GetLegacyTriangleCalls();
        legacyLineCalls += g_renderer3d->GetLegacyLineCalls();
        textCalls += g_renderer3d->GetTextCalls();
        lineCalls += g_renderer3d->GetLineCalls();
        staticSpriteCalls += g_renderer3d->GetStaticSpriteCalls();
        rotatingSpriteCalls += g_renderer3d->GetRotatingSpriteCalls();
    }
    
    // Total draw calls header
    snprintf(statsBuffer, sizeof(statsBuffer), "Total Draw Calls: %d", totalDrawCalls);
    m_renderer->TextSimple(25, yPos, Colour(255, 255, 255, 255), 12.0f, statsBuffer);
    yPos += lineHeight;
    
    // Legacy immediate mode buffers 
    m_renderer->TextSimple(25, yPos, Colour(255, 100, 100, 255), 11.0f, "Immediate Mode:");
    snprintf(statsBuffer, sizeof(statsBuffer), "Triangles: %d  Lines: %d", 
             legacyTriangleCalls, legacyLineCalls);
    m_renderer->TextSimple(indentLarge, yPos, Colour(255, 100, 100, 255), 11.0f, statsBuffer);
    yPos += lineHeight;
    
    // Core UI buffers 
    m_renderer->TextSimple(25, yPos, Colour(100, 255, 100, 255), 11.0f, "UI Buffers:");
    snprintf(statsBuffer, sizeof(statsBuffer), "Sprites: %d TriangleFill: %d Text: %d  Rects: %d  RectFill: %d Lines: %d", 
             eclipseSpriteCalls, eclipseTriangleFillCalls, textCalls, eclipseRectCalls, eclipseRectFillCalls, eclipseLineCalls);
    m_renderer->TextSimple(100, yPos, Colour(100, 255, 100, 255), 11.0f, statsBuffer);
    yPos += lineHeight;
    
    // Unit buffer details 
    snprintf(statsBuffer, sizeof(statsBuffer), "  Lines: %d  Static: %d Rotating: %d CircleFill: %d RectFill: %d TriangleFill: %d", 
             lineCalls, staticSpriteCalls, rotatingSpriteCalls, circleFillCalls, rectFillCalls, triangleFillCalls);
    m_renderer->TextSimple(indentSmall, yPos, Colour(120, 170, 255, 255), 10.0f, statsBuffer);
    yPos += 14.0f;
    
    // Performance summary
    int legacyTotal = legacyTriangleCalls + legacyLineCalls;
    int eclipseUITotal = eclipseSpriteCalls + eclipseTriangleFillCalls + textCalls + eclipseRectCalls + eclipseRectFillCalls + eclipseLineCalls;
    int unitTotal = lineCalls + staticSpriteCalls + rotatingSpriteCalls + circleFillCalls + rectFillCalls + triangleFillCalls;
    int batchedTotal = unitTotal + eclipseUITotal;
    
    m_renderer->TextSimple(25, yPos, Colour(100, 255, 255, 255), 11.0f, "Conclusion:");
    yPos += lineHeight;
    
    snprintf(statsBuffer, sizeof(statsBuffer), "  Batched: %d  Legacy: %d", 
             batchedTotal, legacyTotal);
    m_renderer->TextSimple(35, yPos, Colour(100, 255, 255, 255), 11.0f, statsBuffer);
    yPos += 14.0f;
    
}

void RendererDebugMenu::RenderFlushTimings(float& yPos)
{
    if (!m_renderer) return;
    
    m_renderer->UpdateGpuTimings();
    
    int timingCount = 0;
    const Renderer::FlushTiming* timings = m_renderer->GetFlushTimings(timingCount);
    
    if (timingCount == 0) return;
    
    //
    // copy and sort by total time in a descending order

    Renderer::FlushTiming sorted[50];
    for (int i = 0; i < timingCount; i++) {
        sorted[i] = timings[i];
    }
    
    //
    // use bubble sort to make things simple

    for (int i = 0; i < timingCount - 1; i++) {
        for (int j = 0; j < timingCount - i - 1; j++) {
            if (sorted[j].totalTime < sorted[j + 1].totalTime) {
                Renderer::FlushTiming temp = sorted[j];
                sorted[j] = sorted[j + 1];
                sorted[j + 1] = temp;
            }
        }
    }
    
    float lineHeight = 16.0f;
    char buffer[256];
    
    m_renderer->TextSimple(25, yPos, Colour(255, 200, 100, 255), 13.0f, "Flush Timings (Top 5)");
    yPos += lineHeight;
    
    //
    // show top 5 flush functions

    for (int i = 0; i < 5 && i < timingCount; i++) {
        if (sorted[i].callCount == 0) continue;
        
        double avgCpuMs = (sorted[i].totalTime / sorted[i].callCount) * 1000.0;
        double avgGpuMs = sorted[i].totalGpuTime > 0 ? (sorted[i].totalGpuTime / sorted[i].callCount) : 0.0;
        
        snprintf(buffer, sizeof(buffer), "  %s: CPU %.3fms | GPU %.3fms (%d calls)",
                 sorted[i].name, avgCpuMs, avgGpuMs, sorted[i].callCount);
        
        // Color code based on which is higher
        Colour col = (avgGpuMs > avgCpuMs) ? 
            Colour(255, 100, 100, 255) :  // Red if GPU bound
            Colour(100, 255, 100, 255);   // Green if CPU bound
        
        m_renderer->TextSimple(35, yPos, col, 11.0f, buffer);
        yPos += 14.0f;
    }
}

void RendererDebugMenu::RenderSystemInformation(float& yPos)
{
    if (!m_renderer) return;
    
    char infoBuffer[256];
    float lineHeight = 18.0f;
    
    // Header
    m_renderer->TextSimple(25, yPos, Colour(255, 255, 255, 255), 13.0f, "General Information");
    yPos += lineHeight;
    
    // Frame timing information
    if (m_showFrameTiming) {
        m_renderer->TextSimple(25, yPos, Colour(255, 200, 100, 255), 12.0f, "Frame Times:");
        yPos += lineHeight;
        
        if (m_averageFrameTime > 0.0) {
            snprintf(infoBuffer, sizeof(infoBuffer), "  Average Frame Time: %.2f ms", m_averageFrameTime * 1000.0);
            m_renderer->TextSimple(35, yPos, Colour(200, 200, 200, 255), 11.0f, infoBuffer);
            yPos += 14.0f;
            
            double fps = 1.0 / m_averageFrameTime;
            snprintf(infoBuffer, sizeof(infoBuffer), "  Average FPS: %.1f", fps);
            m_renderer->TextSimple(35, yPos, Colour(200, 200, 200, 255), 11.0f, infoBuffer);
        } else {
            snprintf(infoBuffer, sizeof(infoBuffer), "  Frame Time: Collecting data...");
            m_renderer->TextSimple(35, yPos, Colour(255, 255, 100, 255), 11.0f, infoBuffer);
            yPos += 14.0f;
            
            snprintf(infoBuffer, sizeof(infoBuffer), "  FPS: Collecting data...");
            m_renderer->TextSimple(35, yPos, Colour(255, 255, 100, 255), 11.0f, infoBuffer);
        }
        yPos += lineHeight;
    }
    
    //
    // buffer memory breakdown

    if (m_showMemoryInfo) {
        m_renderer->TextSimple(25, yPos, Colour(100, 255, 200, 255), 12.0f, "Buffer Memory Breakdown:");
        yPos += lineHeight;
        
        //
        // current vertex buffer usage

        int currentVertices = GetActualBufferVertexCount();
        float currentMemoryMB = (currentVertices * sizeof(Vertex2D)) / (1024.0f * 1024.0f);
        snprintf(infoBuffer, sizeof(infoBuffer), "  Current Usage: %.2f MB", 
                 currentMemoryMB);
        m_renderer->TextSimple(35, yPos, Colour(200, 200, 200, 255), 11.0f, infoBuffer);
        yPos += 14.0f;
        
        //
        // total allocated memory

        size_t totalAllocated = m_renderer->GetTotalAllocatedBufferMemory();
        if (g_renderer3d) {
            totalAllocated += g_renderer3d->GetTotalAllocatedBufferMemory();
        }
        float totalMemoryMB = totalAllocated / (1024.0f * 1024.0f);
        snprintf(infoBuffer, sizeof(infoBuffer), "  Total Allocated: %.2f MB", totalMemoryMB);
        m_renderer->TextSimple(35, yPos, Colour(200, 200, 200, 255), 11.0f, infoBuffer);
        yPos += 14.0f;
        
        //
        // mega buffer memory (coastline and borders)

        int megaVertices = m_renderer->GetMegaBufferVertexCount();
        int megaIndices = m_renderer->GetMegaBufferIndexCount();
        float megaMemoryMB = ((megaVertices * sizeof(Vertex2D)) + (megaIndices * sizeof(unsigned int))) / (1024.0f * 1024.0f);
        snprintf(infoBuffer, sizeof(infoBuffer), "  Coastlines and Borders: %.2f MB", 
                 megaMemoryMB);
        m_renderer->TextSimple(35, yPos, Colour(255, 200, 100, 255), 11.0f, infoBuffer);
        yPos += 14.0f;
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

int RendererDebugMenu::GetActualBufferVertexCount()
{
    if (!m_renderer) return 0;
    
    int totalVertices = m_renderer->GetTotalCurrentVertexCount();
    
    //
    // add 3D renderer stats if available

    if (g_renderer3d) {
        totalVertices += g_renderer3d->GetTotalCurrentVertexCount();
    }
    
    return totalVertices;
}

int RendererDebugMenu::EstimateTextureSwitches()
{
    if (!m_renderer) return 0;
    
    // Rough estimation based on different texture types
    int switches = 0;
    
    // Each different type of textured call likely uses different textures
    if (m_renderer->GetTextCalls() > 0) switches++;
    if (m_renderer->GetStaticSpriteCalls() > 0) switches += 8; // Different unit types
    if (m_renderer->GetRotatingSpriteCalls() > 0) switches += 3; // Aircraft/rotating nuke textures
    
    // Add 3D renderer stats if available
    if (g_renderer3d) {
        if (g_renderer3d->GetTextCalls() > 0) switches++;
        if (g_renderer3d->GetStaticSpriteCalls() > 0) switches += 8; // Different unit types
        if (g_renderer3d->GetRotatingSpriteCalls() > 0) switches += 3; // Aircraft/rotating nuke textures
    }
    
    return switches;
}

 