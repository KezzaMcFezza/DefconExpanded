#include "lib/universal_include.h"

#include "lib/render/colour.h"

#include "renderer.h"

extern Renderer *g_renderer;

// ============================================================================
// IMMEDIATE MODE PRIMITIVE RENDERING
// ============================================================================

void Renderer::Rect(float x, float y, float w, float h, Colour const &col, float lineWidth) {

#ifndef TARGET_EMSCRIPTEN
    glLineWidth(lineWidth);
#endif

    if (m_lineVertexCount + 8 > MAX_VERTICES) {
        FlushLines();
    }
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    // create 4 lines to form rectangle outline
    // top line

    m_lineVertices[m_lineVertexCount++] = {x, y, r, g, b, a, 0.0f, 0.0f};
    m_lineVertices[m_lineVertexCount++] = {x + w, y, r, g, b, a, 0.0f, 0.0f};

    // right line
    m_lineVertices[m_lineVertexCount++] = {x + w, y, r, g, b, a, 0.0f, 0.0f};
    m_lineVertices[m_lineVertexCount++] = {x + w, y + h, r, g, b, a, 0.0f, 0.0f};

    // bottom line
    m_lineVertices[m_lineVertexCount++] = {x + w, y + h, r, g, b, a, 0.0f, 0.0f};
    m_lineVertices[m_lineVertexCount++] = {x, y + h, r, g, b, a, 0.0f, 0.0f};
    
    // left line
    m_lineVertices[m_lineVertexCount++] = {x, y + h, r, g, b, a, 0.0f, 0.0f};
    m_lineVertices[m_lineVertexCount++] = {x, y, r, g, b, a, 0.0f, 0.0f};
    
    FlushLines();
}

void Renderer::RectFill(float x, float y, float w, float h, Colour const &col) {
    RectFill(x, y, w, h, col, col, col, col);
}

void Renderer::RectFill(float x, float y, float w, float h, Colour const &col1, Colour const &col2, bool horizontal) {
    if (horizontal) {
        RectFill(x, y, w, h, col1, col1, col2, col2);
    } else {
        RectFill(x, y, w, h, col1, col2, col2, col1);
    }
}

void Renderer::RectFill(float x, float y, float w, float h, Colour const &colTL, Colour const &colTR, 
                        Colour const &colBR, Colour const &colBL) {
    if (m_triangleVertexCount + 6 > MAX_VERTICES) {
        FlushTriangles(false);
    }
    
    float rTL = colTL.m_r / 255.0f, gTL = colTL.m_g / 255.0f, bTL = colTL.m_b / 255.0f, aTL = colTL.m_a / 255.0f;
    float rTR = colTR.m_r / 255.0f, gTR = colTR.m_g / 255.0f, bTR = colTR.m_b / 255.0f, aTR = colTR.m_a / 255.0f;
    float rBR = colBR.m_r / 255.0f, gBR = colBR.m_g / 255.0f, bBR = colBR.m_b / 255.0f, aBR = colBR.m_a / 255.0f;
    float rBL = colBL.m_r / 255.0f, gBL = colBL.m_g / 255.0f, bBL = colBL.m_b / 255.0f, aBL = colBL.m_a / 255.0f;
    
    // first triangle: TL, TR, BR
    m_triangleVertices[m_triangleVertexCount++] = {x, y, rTL, gTL, bTL, aTL, 0.0f, 0.0f};
    m_triangleVertices[m_triangleVertexCount++] = {x + w, y, rTR, gTR, bTR, aTR, 1.0f, 0.0f};
    m_triangleVertices[m_triangleVertexCount++] = {x + w, y + h, rBR, gBR, bBR, aBR, 1.0f, 1.0f};
    
    // second triangle: TL, BR, BL
    m_triangleVertices[m_triangleVertexCount++] = {x, y, rTL, gTL, bTL, aTL, 0.0f, 0.0f};
    m_triangleVertices[m_triangleVertexCount++] = {x + w, y + h, rBR, gBR, bBR, aBR, 1.0f, 1.0f};
    m_triangleVertices[m_triangleVertexCount++] = {x, y + h, rBL, gBL, bBL, aBL, 0.0f, 1.0f};
    
    FlushTriangles(false);
}

void Renderer::Line(float x1, float y1, float x2, float y2, Colour const &col, float lineWidth) {
    
#ifndef TARGET_EMSCRIPTEN
    glLineWidth(lineWidth);
#endif
    
    if (m_lineVertexCount + 2 > MAX_VERTICES) {
        FlushLines();
    }
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    m_lineVertices[m_lineVertexCount++] = {x1, y1, r, g, b, a, 0.0f, 0.0f};
    m_lineVertices[m_lineVertexCount++] = {x2, y2, r, g, b, a, 0.0f, 0.0f};
    
    FlushLines();
}

void Renderer::Circle(float x, float y, float radius, int numPoints, Colour const &col, float lineWidth) {

#ifndef TARGET_EMSCRIPTEN
    glLineWidth(lineWidth);
#endif

    if (m_lineVertexCount + numPoints * 2 > MAX_VERTICES) {
        FlushLines();
    }
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
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
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
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

void Renderer::TriangleFill(float x1, float y1, float x2, float y2, float x3, float y3, Colour const &col) {
    if (m_triangleVertexCount + 3 > MAX_VERTICES) {
        FlushTriangles(false);
    }
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    m_triangleVertices[m_triangleVertexCount++] = {x1, y1, r, g, b, a, 0.0f, 0.0f};
    m_triangleVertices[m_triangleVertexCount++] = {x2, y2, r, g, b, a, 0.0f, 0.0f};
    m_triangleVertices[m_triangleVertexCount++] = {x3, y3, r, g, b, a, 0.0f, 0.0f};
    
    FlushTriangles(false);
}

void Renderer::BeginLines(Colour const &col, float lineWidth) {

#ifndef TARGET_EMSCRIPTEN
    glLineWidth(lineWidth);
#endif

    m_currentLineColor = col;
}

void Renderer::Line(float x, float y) {
    if (m_lineVertexCount + 1 > MAX_VERTICES) {
        FlushLines();
    }
    
    float r = m_currentLineColor.m_r / 255.0f, g = m_currentLineColor.m_g / 255.0f;
    float b = m_currentLineColor.m_b / 255.0f, a = m_currentLineColor.m_a / 255.0f;
    
    m_lineVertices[m_lineVertexCount++] = {x, y, r, g, b, a, 0.0f, 0.0f};
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
    glLineWidth(lineWidth);
#endif
}

void Renderer::LineStripVertex2D(float x, float y) {
    if (!m_lineStripActive) return;
    
    if (m_lineVertexCount >= MAX_VERTICES) {
        EndLineStrip2D();
        BeginLineStrip2D(m_lineStripColor, m_lineStripWidth);
    }
    
    float r = m_lineStripColor.m_r / 255.0f, g = m_lineStripColor.m_g / 255.0f;
    float b = m_lineStripColor.m_b / 255.0f, a = m_lineStripColor.m_a / 255.0f;
    
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