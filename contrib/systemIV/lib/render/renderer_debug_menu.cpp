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

//
// Common colors used throughout debug menu

static const Colour white(255, 255, 255, 255);
static const Colour grey(200, 200, 200, 255);
static const Colour red(255, 100, 100, 255);
static const Colour green(100, 255, 100, 255);
static const Colour yellow(255, 255, 100, 255);
static const Colour orange(255, 200, 100, 255);
static const Colour cyan(120, 255, 255, 255);
static const Colour lightcyan(100, 255, 255, 255);

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
    // Update frame time history for smoothed averages

    m_frameTimeHistory[m_frameTimeIndex] = frameTime;
    m_frameTimeIndex = (m_frameTimeIndex + 1) % FRAME_TIME_HISTORY_SIZE;
    
    //
    // Calculate rolling average

    double sum = 0.0;
    int validFrames = 0;
    for (int i = 0; i < FRAME_TIME_HISTORY_SIZE; i++) {
        if (m_frameTimeHistory[i] > 0.0) {
            sum += m_frameTimeHistory[i];
            validFrames++;
        }
    }
    
    //
    // Only update average if we have valid frames

    if (validFrames > 0) {
        m_averageFrameTime = sum / validFrames;
    }
}

void RendererDebugMenu::RenderDebugMenu()
{
    if (!m_renderer) return;
    
    float baseY = 55.0f;  // Move debug menu down to avoid FPS overlap with FPS counter 
    float yPos = baseY;
    
    //
    // Render it

    RenderBufferStatistics(yPos);
    yPos += 2.0f; 
#ifndef TARGET_EMSCRIPTEN
    RenderFlushTimings(yPos);  
    yPos += 2.0f;
#endif
    RenderSystemInformation(yPos);
    yPos += 2.0f;
    RenderVBOCacheStatistics(yPos);
    yPos += 2.0f;
    RenderTextureAndFontStatistics(yPos);
    yPos += 2.0f;
}

void RendererDebugMenu::RenderBufferStatistics(float& yPos)
{
    if (!m_renderer) return;
    
    char statsBuffer[512];
    float lineHeight = 18.0f;
    float indentSmall = 35.0f;
    float indentLarge = 200.0f;
    
    //
    // Render text for both 2d and 3d

    m_renderer->TextSimple(25, yPos, white, 13.0f, "Global Buffer Statistics");
    yPos += lineHeight;
    
    int totalDrawCalls, immediateTriangleCalls, immediateLineCalls;
    int textCalls;
    int lineCalls, staticSpriteCalls, rotatingSpriteCalls;
    int circleCalls, circleFillCalls, rectCalls, rectFillCalls, triangleFillCalls;
    
    totalDrawCalls = m_renderer->GetTotalDrawCalls();
    immediateTriangleCalls = m_renderer->GetImmediateTriangleCalls();
    immediateLineCalls = m_renderer->GetImmediateLineCalls();
    textCalls = m_renderer->GetTextCalls();
    lineCalls = m_renderer->GetLineCalls();
    staticSpriteCalls = m_renderer->GetStaticSpriteCalls();
    rotatingSpriteCalls = m_renderer->GetRotatingSpriteCalls();
    circleCalls = m_renderer->GetCircleCalls();
    circleFillCalls = m_renderer->GetCircleFillCalls();
    rectCalls = m_renderer->GetRectCalls();
    rectFillCalls = m_renderer->GetRectFillCalls();
    triangleFillCalls = m_renderer->GetTriangleFillCalls();

    if (g_renderer3d) {
        totalDrawCalls += g_renderer3d->GetTotalDrawCalls();
        immediateTriangleCalls += g_renderer3d->GetImmediateTriangleCalls();
        immediateLineCalls += g_renderer3d->GetImmediateLineCalls();
        textCalls += g_renderer3d->GetTextCalls();
        lineCalls += g_renderer3d->GetLineCalls();
        staticSpriteCalls += g_renderer3d->GetStaticSpriteCalls();
        rotatingSpriteCalls += g_renderer3d->GetRotatingSpriteCalls();
        circleCalls += g_renderer3d->GetCircleCalls();
        circleFillCalls += g_renderer3d->GetCircleFillCalls();
        rectCalls += g_renderer3d->GetRectCalls();
        rectFillCalls += g_renderer3d->GetRectFillCalls();
        triangleFillCalls += g_renderer3d->GetTriangleFillCalls();
    }
    
    //
    // Total draw calls header

    snprintf(statsBuffer, sizeof(statsBuffer), "Total Draw Calls: %d", totalDrawCalls);
    m_renderer->TextSimple(25, yPos, white, 12.0f, statsBuffer);
    yPos += lineHeight;
    
    //
    // Immediate mode

    m_renderer->TextSimple(25, yPos, red, 11.0f, "Immediate Mode:");
    snprintf(statsBuffer, sizeof(statsBuffer), "Triangles: %d  Lines: %d", 
             immediateTriangleCalls, immediateLineCalls);
    m_renderer->TextSimple(indentLarge, yPos, red, 11.0f, statsBuffer);
    yPos += lineHeight;
    
    //
    // All buffers

    // First section
    snprintf(statsBuffer, sizeof(statsBuffer), "  Lines: %d  Static: %d Rotating: %d Text: %d", 
             lineCalls, staticSpriteCalls, rotatingSpriteCalls, textCalls);
    m_renderer->TextSimple(indentSmall, yPos, cyan, 11.0f, statsBuffer);
    yPos += 14.0f;

    // Second section
    snprintf(statsBuffer, sizeof(statsBuffer), "  Circle: %d CircleFill: %d Rect: %d RectFill: %d TriangleFill: %d", 
             circleCalls, circleFillCalls, rectCalls, rectFillCalls, triangleFillCalls);
    m_renderer->TextSimple(indentSmall, yPos, cyan, 11.0f, statsBuffer);
    yPos += 14.0f;
    
    //
    // Performance summary

    int immediateTotal = immediateTriangleCalls + immediateLineCalls;
    int bufferTotal = textCalls + lineCalls + staticSpriteCalls + rotatingSpriteCalls + circleCalls + circleFillCalls + rectCalls + rectFillCalls + triangleFillCalls;
    int batchedTotal = bufferTotal;
    
    m_renderer->TextSimple(25, yPos, white, 13.0f, "Conclusion:");
    yPos += lineHeight;
    
    snprintf(statsBuffer, sizeof(statsBuffer), "  Batched: %d  Immediate: %d", 
             batchedTotal, immediateTotal);
    m_renderer->TextSimple(35, yPos, lightcyan, 11.0f, statsBuffer);
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
    // Copy and sort by total time in a descending order

    Renderer::FlushTiming sorted[50];
    for (int i = 0; i < timingCount; i++) {
        sorted[i] = timings[i];
    }
    
    //
    // Use bubble sort to make things simple

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
    
    m_renderer->TextSimple(25, yPos, orange, 13.0f, "Flush Timings (Top 5)");
    yPos += lineHeight;
    
    //
    // Show top 5 flush functions

    for (int i = 0; i < 5 && i < timingCount; i++) {
        if (sorted[i].callCount == 0) continue;
        
        double avgCpuMs = (sorted[i].totalTime / sorted[i].callCount) * 1000.0;
        double avgGpuMs = sorted[i].totalGpuTime > 0 ? (sorted[i].totalGpuTime / sorted[i].callCount) : 0.0;
        
        snprintf(buffer, sizeof(buffer), "  %s: CPU %.3fms | GPU %.3fms (%d calls)",
                 sorted[i].name, avgCpuMs, avgGpuMs, sorted[i].callCount);
        
        //          
        // Color code based on which is higher

        Colour col = (avgGpuMs > avgCpuMs) ? red : green;  // red if GPU bound, green if CPU bound
        
        m_renderer->TextSimple(35, yPos, col, 11.0f, buffer);
        yPos += 14.0f;
    }
}

void RendererDebugMenu::RenderSystemInformation(float& yPos)
{
    if (!m_renderer) return;
    
    char infoBuffer[256];
    float lineHeight = 18.0f;
    
    //
    // Header

    m_renderer->TextSimple(25, yPos, white, 13.0f, "General Information");
    yPos += lineHeight;
    
    //
    // Frame timing information

    if (m_showFrameTiming) {
        m_renderer->TextSimple(25, yPos, orange, 12.0f, "Frame Times:");
        yPos += lineHeight;
        
        if (m_averageFrameTime > 0.0) {
            snprintf(infoBuffer, sizeof(infoBuffer), "  Average Frame Time: %.2f ms", m_averageFrameTime * 1000.0);
            m_renderer->TextSimple(35, yPos, grey, 11.0f, infoBuffer);
            yPos += 14.0f;
            
            double fps = 1.0 / m_averageFrameTime;
            snprintf(infoBuffer, sizeof(infoBuffer), "  Average FPS: %.1f", fps);
            m_renderer->TextSimple(35, yPos, grey, 11.0f, infoBuffer);
        } else {
            snprintf(infoBuffer, sizeof(infoBuffer), "  Frame Time: Collecting data...");
            m_renderer->TextSimple(35, yPos, yellow, 11.0f, infoBuffer);
            yPos += 14.0f;
            
            snprintf(infoBuffer, sizeof(infoBuffer), "  FPS: Collecting data...");
            m_renderer->TextSimple(35, yPos, yellow, 11.0f, infoBuffer);
        }
        yPos += lineHeight;
    }
    
    //
    // Buffer memory breakdown

    if (m_showMemoryInfo) {
        m_renderer->TextSimple(25, yPos, white, 12.0f, "Buffer Memory Breakdown:");
        yPos += lineHeight;
        
        //
        // Current vertex buffer usage

        int currentVertices = GetActualBufferVertexCount();
        float currentMemoryMB = (currentVertices * sizeof(Vertex2D)) / (1024.0f * 1024.0f);
        snprintf(infoBuffer, sizeof(infoBuffer), "  Current Usage: %.2f MB", 
                 currentMemoryMB);
        m_renderer->TextSimple(35, yPos, grey, 11.0f, infoBuffer);
        yPos += 14.0f;
        
        //
        // Total allocated memory

        size_t totalAllocated = m_renderer->GetTotalAllocatedBufferMemory();
        if (g_renderer3d) {
            totalAllocated += g_renderer3d->GetTotalAllocatedBufferMemory();
        }
        float totalMemoryMB = totalAllocated / (1024.0f * 1024.0f);
        snprintf(infoBuffer, sizeof(infoBuffer), "  Total Allocated: %.2f MB", totalMemoryMB);
        m_renderer->TextSimple(35, yPos, grey, 11.0f, infoBuffer);
        yPos += 14.0f;
        
        //
        // Mega buffer memory, include both 2D and 3D VBOs

        float totalVBOMemoryMB = 0.0f;
        
        //
        // Calculate 2D VBO memory

        int coastline2DVertices = m_renderer->GetCachedVBOVertexCount("all_coastlines");
        int coastline2DIndices = m_renderer->GetCachedVBOIndexCount("all_coastlines");
        totalVBOMemoryMB += ((coastline2DVertices * sizeof(Vertex2D)) + (coastline2DIndices * sizeof(unsigned int))) / (1024.0f * 1024.0f);
        
        int border2DVertices = m_renderer->GetCachedVBOVertexCount("all_borders");
        int border2DIndices = m_renderer->GetCachedVBOIndexCount("all_borders");
        totalVBOMemoryMB += ((border2DVertices * sizeof(Vertex2D)) + (border2DIndices * sizeof(unsigned int))) / (1024.0f * 1024.0f);
        
        //
        // Calculate 3D VBO memory

        if (g_renderer3d) {
            int globeCoastlineVertices = g_renderer3d->GetCached3DVBOVertexCount("GlobeCoastlines");
            int globeCoastlineIndices = g_renderer3d->GetCached3DVBOIndexCount("GlobeCoastlines");
            totalVBOMemoryMB += ((globeCoastlineVertices * sizeof(Vertex3D)) + (globeCoastlineIndices * sizeof(unsigned int))) / (1024.0f * 1024.0f);
            
            int globeBorderVertices = g_renderer3d->GetCached3DVBOVertexCount("GlobeBorders");
            int globeBorderIndices = g_renderer3d->GetCached3DVBOIndexCount("GlobeBorders");
            totalVBOMemoryMB += ((globeBorderVertices * sizeof(Vertex3D)) + (globeBorderIndices * sizeof(unsigned int))) / (1024.0f * 1024.0f);
            
            int globeGridlineVertices = g_renderer3d->GetCached3DVBOVertexCount("GlobeGridlines");
            int globeGridlineIndices = g_renderer3d->GetCached3DVBOIndexCount("GlobeGridlines");
            totalVBOMemoryMB += ((globeGridlineVertices * sizeof(Vertex3D)) + (globeGridlineIndices * sizeof(unsigned int))) / (1024.0f * 1024.0f);
        }
        
        //
        // Print the results

        snprintf(infoBuffer, sizeof(infoBuffer), "  Coastlines and Borders: %.2f MB", 
                 totalVBOMemoryMB);
        m_renderer->TextSimple(35, yPos, orange, 11.0f, infoBuffer);
        yPos += 14.0f;
    }
}

void RendererDebugMenu::RenderVBOCacheStatistics(float& yPos)
{
    if (!m_renderer) return;
    
    char infoBuffer[256];
    float lineHeight = 18.0f;
    const int MAX_VBO_VERTICES = 1500000;
    
    m_renderer->TextSimple(25, yPos, white, 13.0f, "VBO Cache Statistics");
    yPos += lineHeight;
    
    //
    // Count total cached VBOs from both 2D and 3D renderers
    
    int cached2DVBOCount = m_renderer->GetCachedVBOCount();
    int cached3DVBOCount = g_renderer3d ? g_renderer3d->GetCached3DVBOCount() : 0;
    int totalCachedVBOs = cached2DVBOCount + cached3DVBOCount;
    
    snprintf(infoBuffer, sizeof(infoBuffer), "  Total Cached VBOs: %d", totalCachedVBOs);
    m_renderer->TextSimple(35, yPos, grey, 11.0f, infoBuffer);
    yPos += 14.0f;
    
    //
    // Change color depending on how much of the max defined verts/indicies are used
    //
    // Color code: 
    //   Grey < 20%
    //   Yellow >= 20%
    //   Red >= 40%
    
    auto renderVBOStat = [&](const char* label, int vertices, int indices, size_t vertexSize, bool is3D = false) {
        if (vertices <= 0) return;
        
        float vertexPercent = (float)vertices / (float)MAX_VBO_VERTICES * 100.0f;
        Colour vboColor = (vertexPercent >= 40.0f) ? red :
                          (vertexPercent >= 20.0f) ? yellow : grey;
        
        float vboMB = ((vertices * vertexSize) + (indices * sizeof(unsigned int))) / (1024.0f * 1024.0f);
        snprintf(infoBuffer, sizeof(infoBuffer), "  %s: %d Vertices, %d Indices (%.2f MB)", 
                 label, vertices, indices, vboMB);
        m_renderer->TextSimple(35, yPos, vboColor, 11.0f, infoBuffer);
        yPos += 14.0f;
    };
    
    //
    // 2D VBO stats
    
    renderVBOStat("2D Coastlines", 
                  m_renderer->GetCachedVBOVertexCount("all_coastlines"),
                  m_renderer->GetCachedVBOIndexCount("all_coastlines"),
                  sizeof(Vertex2D));
    
    renderVBOStat("2D Borders", 
                  m_renderer->GetCachedVBOVertexCount("all_borders"),
                  m_renderer->GetCachedVBOIndexCount("all_borders"),
                  sizeof(Vertex2D));
    
    //
    // 3D VBO stats
    
    if (g_renderer3d) {
        renderVBOStat("3D Coastlines",
                      g_renderer3d->GetCached3DVBOVertexCount("GlobeCoastlines"),
                      g_renderer3d->GetCached3DVBOIndexCount("GlobeCoastlines"),
                      sizeof(Vertex3D), true);
        
        renderVBOStat("3D Borders",
                      g_renderer3d->GetCached3DVBOVertexCount("GlobeBorders"),
                      g_renderer3d->GetCached3DVBOIndexCount("GlobeBorders"),
                      sizeof(Vertex3D), true);
        
        renderVBOStat("3D Gridlines",
                      g_renderer3d->GetCached3DVBOVertexCount("GlobeGridlines"),
                      g_renderer3d->GetCached3DVBOIndexCount("GlobeGridlines"),
                      sizeof(Vertex3D), true);
    }
}

void RendererDebugMenu::RenderTextureAndFontStatistics(float& yPos)
{
    if (!m_renderer) return;
    
    char infoBuffer[256];
    float lineHeight = 18.0f;
    
    m_renderer->TextSimple(25, yPos, white, 13.0f, "Font Statistics");
    yPos += lineHeight;
    
    //
    // Texture switches per frame
    
    int textureSwitches = m_renderer->GetTextureSwitches();
    snprintf(infoBuffer, sizeof(infoBuffer), "  Texture Switches: %d", textureSwitches);
    
    //
    // Add color codes based on texture switch count
    
    Colour switchColor = (textureSwitches > 100) ? red : green;  // red if too many switches, green if okay
    
    m_renderer->TextSimple(35, yPos, switchColor, 11.0f, infoBuffer);
    yPos += 14.0f;
    
    int fontBatches = m_renderer->GetActiveFontBatches();
    snprintf(infoBuffer, sizeof(infoBuffer), "  Active Font Batches: %d", fontBatches);
    m_renderer->TextSimple(35, yPos, grey, 11.0f, infoBuffer);
    yPos += 14.0f;
}

int RendererDebugMenu::GetActualBufferVertexCount()
{
    if (!m_renderer) return 0;
    
    int totalVertices = m_renderer->GetTotalCurrentVertexCount();

    if (g_renderer3d) {
        totalVertices += g_renderer3d->GetTotalCurrentVertexCount();
    }
    
    return totalVertices;
}

int RendererDebugMenu::EstimateTextureSwitches()
{
    if (!m_renderer) return 0;
    
    int switches = 0;
    
    if (m_renderer->GetTextCalls() > 0) switches++;
    if (m_renderer->GetStaticSpriteCalls() > 0) switches += 8;
    if (m_renderer->GetRotatingSpriteCalls() > 0) switches += 3; 
    
    // Add 3D renderer stats if available
    if (g_renderer3d) {
        if (g_renderer3d->GetTextCalls() > 0) switches++;
        if (g_renderer3d->GetStaticSpriteCalls() > 0) switches += 8;
        if (g_renderer3d->GetRotatingSpriteCalls() > 0) switches += 3;
    }
    
    return switches;
}

 