#include "lib/universal_include.h"

#include <stdarg.h>

#include "lib/resource/bitmapfont.h"
#include "lib/resource/image.h"
#include "lib/resource/bitmap.h"
#include "lib/math/math_utils.h"
#include "lib/render3d/renderer_3d.h"
#include "lib/render/colour.h"

#include "lib/render2d/renderer.h"

extern Renderer *g_renderer;

void Renderer::Line(float x1, float y1, float x2, float y2, Colour const &col) {
    FlushLinesIfFull(2);
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    m_lineVertices[m_lineVertexCount++] = {x1, y1, r, g, b, a, 0.0f, 0.0f};
    m_lineVertices[m_lineVertexCount++] = {x2, y2, r, g, b, a, 0.0f, 0.0f};
}

void Renderer::Circle(float x, float y, float radius, int numPoints, Colour const &col, float lineWidth) {

#ifndef TARGET_EMSCRIPTEN
    SetLineWidth(lineWidth);
#endif

    if (m_lineVertexCount + numPoints * 2 > MAX_VERTICES) {
        FlushLines();
    }
    
    float r = col.GetRFloat(), g = col.GetGFloat(), b = col.GetBFloat(), a = col.GetAFloat();
    
    for (int i = 0; i < numPoints; ++i) {
        float angle1 = 2.0f * M_PI * (float) i / (float) numPoints;
        float angle2 = 2.0f * M_PI * (float) (i + 1) / (float) numPoints;
        
        float x1 = x + cosf(angle1) * radius;
        float y1 = y + sinf(angle1) * radius;
        float x2 = x + cosf(angle2) * radius;
        float y2 = y + sinf(angle2) * radius;
        
        m_lineVertices[m_lineVertexCount++] = {x1, y1, r, g, b, a, 0.0f, 0.0f};
        m_lineVertices[m_lineVertexCount++] = {x2, y2, r, g, b, a, 0.0f, 0.0f};
    }
    
    FlushLines();
}

void Renderer::CircleFill(float x, float y, float radius, int numPoints, Colour const &col) {
    if (m_triangleVertexCount + numPoints * 3 > MAX_VERTICES) {
        FlushTriangles(false);
    }
    
    float r = col.GetRFloat(), g = col.GetGFloat(), b = col.GetBFloat(), a = col.GetAFloat();
    
    for (int i = 0; i < numPoints; ++i) {
        float angle1 = 2.0f * M_PI * (float) i / (float) numPoints;
        float angle2 = 2.0f * M_PI * (float) (i + 1) / (float) numPoints;
        
        float x1 = x + cosf(angle1) * radius;
        float y1 = y + sinf(angle1) * radius;
        float x2 = x + cosf(angle2) * radius;
        float y2 = y + sinf(angle2) * radius;
        
        // triangle: center, point i, point i+1
        m_triangleVertices[m_triangleVertexCount++] = {x, y, r, g, b, a, 0.5f, 0.5f};
        m_triangleVertices[m_triangleVertexCount++] = {x1, y1, r, g, b, a, 0.0f, 0.0f};
        m_triangleVertices[m_triangleVertexCount++] = {x2, y2, r, g, b, a, 1.0f, 0.0f};
    }
    
    FlushTriangles(false);
}

void Renderer::BeginLines(Colour const &col, float lineWidth) {

#ifndef TARGET_EMSCRIPTEN
    SetLineWidth(lineWidth);
#endif

    m_currentLineColor = col;
}

void Renderer::EndLines() {
    if (m_lineVertexCount < 2) {
        m_lineVertexCount = 0;
        return;
    }
    
    // convert line strip to individual line segments
    int tempLineVertexCount = (m_lineVertexCount - 1) * 2;
    Vertex2D* tempLineVertices = new Vertex2D[tempLineVertexCount];
    
    int lineIndex = 0;
    for (int i = 0; i < m_lineVertexCount - 1; i++) {
        tempLineVertices[lineIndex++] = m_lineVertices[i];
        tempLineVertices[lineIndex++] = m_lineVertices[i + 1];
    }
    
    m_lineVertexCount = 0;
    
    for (int i = 0; i < tempLineVertexCount; i++) {
        if (m_lineVertexCount >= MAX_VERTICES) {
            FlushLines();
        }
        m_lineVertices[m_lineVertexCount++] = tempLineVertices[i];
    }
    
    delete[] tempLineVertices;
    FlushLines();
}

void Renderer::BeginLineStrip2D(Colour const &col, float lineWidth) {
    m_lineStripActive = true;
    m_lineStripColor = col;
    m_lineStripWidth = lineWidth;
    m_lineVertexCount = 0;

#ifndef TARGET_EMSCRIPTEN
    SetLineWidth(lineWidth);
#endif
}

void Renderer::LineStripVertex2D(float x, float y) {
    if (!m_lineStripActive) return;
    
    if (m_lineVertexCount >= MAX_VERTICES) {
        EndLineStrip2D();
        BeginLineStrip2D(m_lineStripColor, m_lineStripWidth);
    }
    
    float r = m_lineStripColor.GetRFloat(), g = m_lineStripColor.GetGFloat();
    float b = m_lineStripColor.GetBFloat(), a = m_lineStripColor.GetAFloat();
    
    m_lineVertices[m_lineVertexCount++] = {x, y, r, g, b, a, 0.0f, 0.0f};
}

void Renderer::EndLineStrip2D() {
    if (!m_lineStripActive) return;
    
    if (m_lineVertexCount < 2) {
        m_lineVertexCount = 0;
        m_lineStripActive = false;
        return;
    }
    
    // convert to line segments
    int tempVertexCount = (m_lineVertexCount - 1) * 2;
    Vertex2D* tempVertices = new Vertex2D[tempVertexCount];
    
    int lineIndex = 0;
    for (int i = 0; i < m_lineVertexCount - 1; i++) {
        tempVertices[lineIndex++] = m_lineVertices[i];
        tempVertices[lineIndex++] = m_lineVertices[i + 1];
    }
    
    m_lineVertexCount = 0;
    
    for (int i = 0; i < tempVertexCount; i++) {
        if (m_lineVertexCount >= MAX_VERTICES) {
            FlushLines();
        }
        m_lineVertices[m_lineVertexCount++] = tempVertices[i];
    }
    
    delete[] tempVertices;
    FlushLines();
    m_lineStripActive = false;
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