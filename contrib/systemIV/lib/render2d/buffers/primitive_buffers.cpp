#include "lib/universal_include.h"

#include <time.h>
#include <stdarg.h>

#include "lib/eclipse/eclipse.h"
#include "lib/gucci/window_manager.h"
#include "lib/gucci/input.h"
#include "lib/resource/bitmapfont.h"
#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/resource/bitmap.h"
#include "lib/math/vector3.h"
#include "lib/math/math_utils.h"
#include "lib/hi_res_time.h"
#include "lib/debug_utils.h"
#include "lib/preferences.h"
#include "lib/string_utils.h"
#include "lib/filesys/filesys_utils.h"
#include "lib/language_table.h"
#include "lib/render3d/renderer_3d.h"
#include "lib/render/colour.h"

#include "lib/render2d/renderer.h"

extern Renderer *g_renderer;

void Renderer::UnitTrailLine(float x1, float y1, float x2, float y2, Colour const &col) {
    FlushUnitTrailsIfFull(2);
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    m_unitTrailVertices[m_unitTrailVertexCount++] = {x1, y1, r, g, b, a, 0.0f, 0.0f};
    m_unitTrailVertices[m_unitTrailVertexCount++] = {x2, y2, r, g, b, a, 0.0f, 0.0f};
}

void Renderer::EffectsRect(float x, float y, float w, float h, Colour const &col, float lineWidth) {
#ifndef TARGET_EMSCRIPTEN
    glLineWidth(1.0f);
#endif

    FlushEffectsRectsIfFull(8);
        
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
        
    // create 4 lines to form rectangle outline
    // top line
    m_effectsRectVertices[m_effectsRectVertexCount++] = {x, y, r, g, b, a, 0.0f, 0.0f};
    m_effectsRectVertices[m_effectsRectVertexCount++] = {x + w, y, r, g, b, a, 0.0f, 0.0f};

    // right line
    m_effectsRectVertices[m_effectsRectVertexCount++] = {x + w, y, r, g, b, a, 0.0f, 0.0f};
    m_effectsRectVertices[m_effectsRectVertexCount++] = {x + w, y + h, r, g, b, a, 0.0f, 0.0f};

    // bottom line
    m_effectsRectVertices[m_effectsRectVertexCount++] = {x + w, y + h, r, g, b, a, 0.0f, 0.0f};
    m_effectsRectVertices[m_effectsRectVertexCount++] = {x, y + h, r, g, b, a, 0.0f, 0.0f};

    // left line
    m_effectsRectVertices[m_effectsRectVertexCount++] = {x, y + h, r, g, b, a, 0.0f, 0.0f};
    m_effectsRectVertices[m_effectsRectVertexCount++] = {x, y, r, g, b, a, 0.0f, 0.0f};
        
}

void Renderer::EffectsLine(float x1, float y1, float x2, float y2, Colour const &col) {

    FlushEffectsLinesIfFull(2);
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    m_effectsLineVertices[m_effectsLineVertexCount++] = {x1, y1, r, g, b, a, 0.0f, 0.0f};
    m_effectsLineVertices[m_effectsLineVertexCount++] = {x2, y2, r, g, b, a, 0.0f, 0.0f};
}

void Renderer::HealthBarRect(float x, float y, float w, float h, Colour const &col) {
    if (m_healthBarVertexCount + 6 > MAX_HEALTH_BAR_VERTICES) {
        FlushHealthBars();
    }
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    // First triangle: TL, TR, BR
    m_healthBarVertices[m_healthBarVertexCount++] = {x, y, r, g, b, a, 0.0f, 0.0f};
    m_healthBarVertices[m_healthBarVertexCount++] = {x + w, y, r, g, b, a, 0.0f, 0.0f};
    m_healthBarVertices[m_healthBarVertexCount++] = {x + w, y + h, r, g, b, a, 0.0f, 0.0f};
    
    // Second triangle: TL, BR, BL
    m_healthBarVertices[m_healthBarVertexCount++] = {x, y, r, g, b, a, 0.0f, 0.0f};
    m_healthBarVertices[m_healthBarVertexCount++] = {x + w, y + h, r, g, b, a, 0.0f, 0.0f};
    m_healthBarVertices[m_healthBarVertexCount++] = {x, y + h, r, g, b, a, 0.0f, 0.0f};
}

void Renderer::EffectsCircleFill(float x, float y, float radius, int segments, Colour const &col) {
    if (m_effectsCircleFillVertexCount + segments * 3 > MAX_EFFECTS_SPRITE_VERTICES) {
        FlushEffectsCircleFills();
    }
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    //
    // generate filled circle as triangles (fan from center)

    for (int i = 0; i < segments; ++i) {
        float angle1 = 2.0f * M_PI * (float) i / (float) segments;
        float angle2 = 2.0f * M_PI * (float) (i + 1) / (float) segments;
        
        float x1 = x + cosf(angle1) * radius;
        float y1 = y + sinf(angle1) * radius;
        float x2 = x + cosf(angle2) * radius;
        float y2 = y + sinf(angle2) * radius;
        
        // Triangle: center, point i, point i+1
        m_radarFillVertices[m_effectsCircleFillVertexCount++] = {x, y, r, g, b, a, 0.5f, 0.5f};
        m_radarFillVertices[m_effectsCircleFillVertexCount++] = {x1, y1, r, g, b, a, 0.0f, 0.0f};
        m_radarFillVertices[m_effectsCircleFillVertexCount++] = {x2, y2, r, g, b, a, 1.0f, 0.0f};
    }
}

void Renderer::EffectsCircleOutline(float x, float y, float radius, int segments, Colour const &col, float lineWidth) {
    Vertex2D* targetBuffer;
    int* targetCount;
    int maxVertices;
    
    if (lineWidth > 2.0f) {

        //
        // thick lines (own radar)

        targetBuffer = m_effectsCircleOutlineThickVertices;
        targetCount = &m_effectsCircleOutlineThickVertexCount;
        maxVertices = MAX_EFFECTS_LINE_VERTICES;
    } else {
        
        //
        // thin lines (allied radar)  
        
        targetBuffer = m_effectsCircleOutlineVertices;
        targetCount = &m_effectsCircleOutlineVertexCount;
        maxVertices = MAX_EFFECTS_LINE_VERTICES;
    }
    
    if (*targetCount + segments * 2 > maxVertices) {
        if (lineWidth > 2.0f) {
            FlushEffectsCircleOutlinesThick();
        } else {
            FlushEffectsCircleOutlines();
        }
    }
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    //
    // generate circle outline as line segments

    for (int i = 0; i < segments; ++i) {
        float angle1 = 2.0f * M_PI * (float) i / (float) segments;
        float angle2 = 2.0f * M_PI * (float) (i + 1) / (float) segments;
        
        float x1 = x + cosf(angle1) * radius;
        float y1 = y + sinf(angle1) * radius;
        float x2 = x + cosf(angle2) * radius;
        float y2 = y + sinf(angle2) * radius;
        
        targetBuffer[*targetCount] = {x1, y1, r, g, b, a, 0.0f, 0.0f};
        (*targetCount)++;
        targetBuffer[*targetCount] = {x2, y2, r, g, b, a, 0.0f, 0.0f};
        (*targetCount)++;
    }
}

void Renderer::BeginWhiteboardBatch() {
    m_whiteboardVertexCount = 0;
}

void Renderer::WhiteboardLine(float x1, float y1, float x2, float y2, Colour const &col) {
    if (m_whiteboardVertexCount + 2 > MAX_WHITEBOARD_VERTICES) {
        FlushWhiteboard();
    }
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f;
    float b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    m_whiteboardVertices[m_whiteboardVertexCount++] = {x1, y1, r, g, b, a, 0.0f, 0.0f};
    m_whiteboardVertices[m_whiteboardVertexCount++] = {x2, y2, r, g, b, a, 0.0f, 0.0f};
}

void Renderer::EndWhiteboardBatch() {
    FlushWhiteboard();
}