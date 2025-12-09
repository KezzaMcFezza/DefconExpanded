#include "lib/universal_include.h"

#include <stdarg.h>

#include "lib/resource/bitmapfont.h"
#include "lib/resource/image.h"
#include "lib/resource/bitmap.h"
#include "lib/math/math_utils.h"
#include "lib/render3d/renderer_3d.h"
#include "lib/render/colour.h"

#include "lib/render/renderer.h"
#include "lib/render2d/renderer_2d.h"

void Renderer2D::Line(float x1, float y1, float x2, float y2, Colour const &col, float lineWidth, bool immediateFlush) {
    FlushLinesIfFull(2);
    
    if (m_lineVertexCount > 0 && m_currentLineWidth != lineWidth) {
        FlushLines();
    }
   
#ifndef TARGET_EMSCRIPTEN
    g_renderer->SetLineWidth(lineWidth);
#endif
    
    m_currentLineWidth = lineWidth;

    float r = col.GetRFloat(), g = col.GetGFloat(), b = col.GetBFloat(), a = col.GetAFloat();
    
    m_lineVertices[m_lineVertexCount++] = {x1, y1, r, g, b, a, 0.0f, 0.0f};
    m_lineVertices[m_lineVertexCount++] = {x2, y2, r, g, b, a, 0.0f, 0.0f};
    
#if IMMEDIATE_MODE_2D
    FlushLines();
#else
    if (immediateFlush) {
        FlushLines();
    }
#endif
}

void Renderer2D::Line(float x, float y) {
    FlushLinesIfFull(1);
    
    float r = m_currentLineColor.GetRFloat();
    float g = m_currentLineColor.GetGFloat();
    float b = m_currentLineColor.GetBFloat();
    float a = m_currentLineColor.GetAFloat();
    
    m_lineVertices[m_lineVertexCount++] = {x, y, r, g, b, a, 0.0f, 0.0f};
}

void Renderer2D::Circle(float x, float y, float radius, int numPoints, Colour const &col, float lineWidth, bool immediateFlush) {
    FlushCirclesIfFull(numPoints * 2);
    
    if (m_circleVertexCount > 0 && m_currentCircleWidth != lineWidth) {
        FlushCircles();
    }

#ifndef TARGET_EMSCRIPTEN
    g_renderer->SetLineWidth(lineWidth);
#endif
    
    m_currentCircleWidth = lineWidth;

    float r = col.GetRFloat(), g = col.GetGFloat(), b = col.GetBFloat(), a = col.GetAFloat();
    
    for (int i = 0; i < numPoints; ++i) {
        float angle1 = 2.0f * M_PI * (float) i / (float) numPoints;
        float angle2 = 2.0f * M_PI * (float) (i + 1) / (float) numPoints;
        
        float x1 = x + cosf(angle1) * radius;
        float y1 = y + sinf(angle1) * radius;
        float x2 = x + cosf(angle2) * radius;
        float y2 = y + sinf(angle2) * radius;
        
        m_circleVertices[m_circleVertexCount++] = {x1, y1, r, g, b, a, 0.0f, 0.0f};
        m_circleVertices[m_circleVertexCount++] = {x2, y2, r, g, b, a, 0.0f, 0.0f};
    }
    
#if IMMEDIATE_MODE_2D
    FlushCircles();
#else
    if (immediateFlush) {
        FlushCircles();
    }
#endif
}

void Renderer2D::CircleFill(float x, float y, float radius, int numPoints, Colour const &col, bool immediateFlush) {
    FlushCircleFillsIfFull(numPoints * 3);

    float r = col.GetRFloat(), g = col.GetGFloat(), b = col.GetBFloat(), a = col.GetAFloat();
    
    for (int i = 0; i < numPoints; ++i) {
        float angle1 = 2.0f * M_PI * (float) i / (float) numPoints;
        float angle2 = 2.0f * M_PI * (float) (i + 1) / (float) numPoints;
        
        float x1 = x + cosf(angle1) * radius;
        float y1 = y + sinf(angle1) * radius;
        float x2 = x + cosf(angle2) * radius;
        float y2 = y + sinf(angle2) * radius;
        
        // triangle: center, point i, point i+1
        m_circleFillVertices[m_circleFillVertexCount++] = {x, y, r, g, b, a, 0.5f, 0.5f};
        m_circleFillVertices[m_circleFillVertexCount++] = {x1, y1, r, g, b, a, 0.0f, 0.0f};
        m_circleFillVertices[m_circleFillVertexCount++] = {x2, y2, r, g, b, a, 1.0f, 0.0f};
    }
    
#if IMMEDIATE_MODE_2D
    FlushCircleFills();
#else
    if (immediateFlush) {
        FlushCircleFills();
    }
#endif
}

void Renderer2D::Rect(float x, float y, float w, float h, Colour const &col, float lineWidth, bool immediateFlush) {
    FlushRectsIfFull(8);
    
    if (m_rectVertexCount > 0 && m_currentRectWidth != lineWidth) {
        FlushRects();
    }
    
#ifndef TARGET_EMSCRIPTEN
    g_renderer->SetLineWidth(lineWidth);
#endif
    
    m_currentRectWidth = lineWidth;

    float r = col.GetRFloat(), g = col.GetGFloat(), b = col.GetBFloat(), a = col.GetAFloat();
    
    // top line
    m_rectVertices[m_rectVertexCount++] = {x, y, r, g, b, a, 0.0f, 0.0f};
    m_rectVertices[m_rectVertexCount++] = {x + w, y, r, g, b, a, 0.0f, 0.0f};

    // right line
    m_rectVertices[m_rectVertexCount++] = {x + w, y, r, g, b, a, 0.0f, 0.0f};
    m_rectVertices[m_rectVertexCount++] = {x + w, y + h, r, g, b, a, 0.0f, 0.0f};

    // bottom line
    m_rectVertices[m_rectVertexCount++] = {x + w, y + h, r, g, b, a, 0.0f, 0.0f};
    m_rectVertices[m_rectVertexCount++] = {x, y + h, r, g, b, a, 0.0f, 0.0f};
    
    // left line
    m_rectVertices[m_rectVertexCount++] = {x, y + h, r, g, b, a, 0.0f, 0.0f};
    m_rectVertices[m_rectVertexCount++] = {x, y, r, g, b, a, 0.0f, 0.0f};
    
#if IMMEDIATE_MODE_2D
    FlushRects();
#else
    if (immediateFlush) {
        FlushRects();
    }
#endif
}


void Renderer2D::RectFill(float x, float y, float w, float h, Colour const &col, bool immediateFlush) {
    RectFill(x, y, w, h, col, col, col, col, immediateFlush);
}

void Renderer2D::RectFill(float x, float y, float w, float h, Colour const &col1, Colour const &col2, bool horizontal, bool immediateFlush) {
    if (horizontal) {
        RectFill(x, y, w, h, col1, col1, col2, col2, immediateFlush);
    } else {
        RectFill(x, y, w, h, col1, col2, col2, col1, immediateFlush);
    }
}

void Renderer2D::RectFill(float x, float y, float w, float h, Colour const &colTL, Colour const &colTR, 
                        Colour const &colBR, Colour const &colBL, bool immediateFlush) {
    FlushRectFillsIfFull(6);
    
    float rTL = colTL.GetRFloat(), gTL = colTL.GetGFloat(), bTL = colTL.GetBFloat(), aTL = colTL.GetAFloat();
    float rTR = colTR.GetRFloat(), gTR = colTR.GetGFloat(), bTR = colTR.GetBFloat(), aTR = colTR.GetAFloat();
    float rBR = colBR.GetRFloat(), gBR = colBR.GetGFloat(), bBR = colBR.GetBFloat(), aBR = colBR.GetAFloat();
    float rBL = colBL.GetRFloat(), gBL = colBL.GetGFloat(), bBL = colBL.GetBFloat(), aBL = colBL.GetAFloat();
    
    // first triangle: TL, TR, BR
    m_rectFillVertices[m_rectFillVertexCount++] = {x, y, rTL, gTL, bTL, aTL, 0.0f, 0.0f};
    m_rectFillVertices[m_rectFillVertexCount++] = {x + w, y, rTR, gTR, bTR, aTR, 1.0f, 0.0f};
    m_rectFillVertices[m_rectFillVertexCount++] = {x + w, y + h, rBR, gBR, bBR, aBR, 1.0f, 1.0f};
    
    // second triangle: TL, BR, BL
    m_rectFillVertices[m_rectFillVertexCount++] = {x, y, rTL, gTL, bTL, aTL, 0.0f, 0.0f};
    m_rectFillVertices[m_rectFillVertexCount++] = {x + w, y + h, rBR, gBR, bBR, aBR, 1.0f, 1.0f};
    m_rectFillVertices[m_rectFillVertexCount++] = {x, y + h, rBL, gBL, bBL, aBL, 0.0f, 1.0f};
    
#if IMMEDIATE_MODE_2D
    FlushRectFills();
#else
    if (immediateFlush) {
        FlushRectFills();
    }
#endif
}

void Renderer2D::TriangleFill(float x1, float y1, float x2, float y2, float x3, float y3, Colour const &col, bool immediateFlush) {
    FlushTriangleFillsIfFull(3);
    
    float r = col.GetRFloat(), g = col.GetGFloat(), b = col.GetBFloat(), a = col.GetAFloat();
    
    m_triangleFillVertices[m_triangleFillVertexCount++] = {x1, y1, r, g, b, a, 0.0f, 0.0f};
    m_triangleFillVertices[m_triangleFillVertexCount++] = {x2, y2, r, g, b, a, 0.0f, 0.0f};
    m_triangleFillVertices[m_triangleFillVertexCount++] = {x3, y3, r, g, b, a, 0.0f, 0.0f};
    
#if IMMEDIATE_MODE_2D
    FlushTriangleFills();
#else
    if (immediateFlush) {
        FlushTriangleFills();
    }
#endif
}

void Renderer2D::BeginLines(Colour const &col, float lineWidth) {
    if (m_lineVertexCount > 0 && m_currentLineWidth != lineWidth) {
        FlushLines();
    }

#ifndef TARGET_EMSCRIPTEN
    g_renderer->SetLineWidth(lineWidth);
#endif

    m_currentLineColor = col;
    m_currentLineWidth = lineWidth;
}

void Renderer2D::EndLines() {
    if (m_lineVertexCount < 2) {
        m_lineVertexCount = 0;
        return;
    }
    
    int stripVertexCount = m_lineVertexCount;
    
    for (int i = 0; i < stripVertexCount; i++) {
        m_lineConversionBuffer[i] = m_lineVertices[i];
    }
    
    m_lineVertexCount = 0;
    
    for (int i = 0; i < stripVertexCount - 1; i++) {
        FlushLinesIfFull(2);
        
        m_lineVertices[m_lineVertexCount++] = m_lineConversionBuffer[i];
        m_lineVertices[m_lineVertexCount++] = m_lineConversionBuffer[i + 1];
    }
    
}

void Renderer2D::BeginLineStrip2D(Colour const &col, float lineWidth) {
    m_lineStripActive = true;
    m_lineStripColor = col;
    m_lineStripWidth = lineWidth;
    m_lineVertexCount = 0;

#ifndef TARGET_EMSCRIPTEN
    g_renderer->SetLineWidth(lineWidth);
#endif

}

void Renderer2D::LineStripVertex2D(float x, float y) {
    if (!m_lineStripActive) return;
    
    if (m_lineVertexCount >= MAX_LINE_VERTICES) {
        EndLineStrip2D();
        BeginLineStrip2D(m_lineStripColor, m_lineStripWidth);
    }
    
    float r = m_lineStripColor.GetRFloat(), g = m_lineStripColor.GetGFloat();
    float b = m_lineStripColor.GetBFloat(), a = m_lineStripColor.GetAFloat();
    
    m_lineVertices[m_lineVertexCount++] = {x, y, r, g, b, a, 0.0f, 0.0f};
}

void Renderer2D::EndLineStrip2D() {
    if (!m_lineStripActive) return;
    
    if (m_lineVertexCount < 2) {
        m_lineVertexCount = 0;
        m_lineStripActive = false;
        return;
    }
    
    int stripVertexCount = m_lineVertexCount;
    
    for (int i = 0; i < stripVertexCount; i++) {
        m_lineConversionBuffer[i] = m_lineVertices[i];
    }
    
    m_lineVertexCount = 0;
    
    for (int i = 0; i < stripVertexCount - 1; i++) {
        FlushLinesIfFull(2);
        
        m_lineVertices[m_lineVertexCount++] = m_lineConversionBuffer[i];
        m_lineVertices[m_lineVertexCount++] = m_lineConversionBuffer[i + 1];
    }
    
    m_lineStripActive = false;
}