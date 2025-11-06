#include "lib/universal_include.h"

#include <stdarg.h>

#include "lib/resource/image.h"
#include "lib/render/colour.h"

#include "lib/render3d/renderer_3d.h"

extern Renderer3D *g_renderer3d;

// ============================================================================
// PRIMITIVE RENDERING FUNCTIONS
// ============================================================================

void Renderer3D::Line3D(float x1, float y1, float z1, float x2, float y2, float z2, Colour const &col, bool immediateFlush) {
    FlushLine3DIfFull(2);
    
    float r = col.GetRFloat(), g = col.GetGFloat(), b = col.GetBFloat(), a = col.GetAFloat();
    
    m_lineVertices3D[m_lineVertexCount3D++] = {x1, y1, z1, r, g, b, a};
    m_lineVertices3D[m_lineVertexCount3D++] = {x2, y2, z2, r, g, b, a};
    
#if IMMEDIATE_MODE_3D
    FlushLine3D();
#else
    if (immediateFlush) {
        FlushLine3D();
    }
#endif
}

void Renderer3D::Circle3D(float x, float y, float z, float radius, int numPoints, Colour const &col, bool immediateFlush) {
    FlushCircles3DIfFull(numPoints * 2);
    
    float r = col.GetRFloat(), g = col.GetGFloat(), b = col.GetBFloat(), a = col.GetAFloat();
    
    for (int i = 0; i < numPoints; ++i) {
        float angle1 = 2.0f * M_PI * (float) i / (float) numPoints;
        float angle2 = 2.0f * M_PI * (float) (i + 1) / (float) numPoints;
        
        float x1 = x + cosf(angle1) * radius;
        float y1 = y + sinf(angle1) * radius;
        float x2 = x + cosf(angle2) * radius;
        float y2 = y + sinf(angle2) * radius;
        
        m_circleVertices3D[m_circleVertexCount3D++] = {x1, y1, z, r, g, b, a};
        m_circleVertices3D[m_circleVertexCount3D++] = {x2, y2, z, r, g, b, a};
    }
    
#if IMMEDIATE_MODE_3D
    FlushCircles3D();
#else
    if (immediateFlush) {
        FlushCircles3D();
    }
#endif
}

void Renderer3D::CircleFill3D(float x, float y, float z, float radius, int numPoints, Colour const &col, bool immediateFlush) {
    FlushCircleFills3DIfFull(numPoints * 3);
    
    float r = col.GetRFloat(), g = col.GetGFloat(), b = col.GetBFloat(), a = col.GetAFloat();
    
    for (int i = 0; i < numPoints; ++i) {
        float angle1 = 2.0f * M_PI * (float) i / (float) numPoints;
        float angle2 = 2.0f * M_PI * (float) (i + 1) / (float) numPoints;
        
        float x1 = x + cosf(angle1) * radius;
        float y1 = y + sinf(angle1) * radius;
        float x2 = x + cosf(angle2) * radius;
        float y2 = y + sinf(angle2) * radius;
        
        // triangle: center, point i, point i+1
        m_circleFillVertices3D[m_circleFillVertexCount3D++] = {x, y, z, r, g, b, a};
        m_circleFillVertices3D[m_circleFillVertexCount3D++] = {x1, y1, z, r, g, b, a};
        m_circleFillVertices3D[m_circleFillVertexCount3D++] = {x2, y2, z, r, g, b, a};
    }
    
#if IMMEDIATE_MODE_3D
    FlushCircleFills3D();
#else
    if (immediateFlush) {
        FlushCircleFills3D();
    }
#endif
}

void Renderer3D::Rect3D(float x, float y, float z, float w, float h, Colour const &col, bool immediateFlush) {
    FlushRects3DIfFull(8);
    
    float r = col.GetRFloat(), g = col.GetGFloat(), b = col.GetBFloat(), a = col.GetAFloat();
    
    // top line
    m_rectVertices3D[m_rectVertexCount3D++] = {x, y, z, r, g, b, a};
    m_rectVertices3D[m_rectVertexCount3D++] = {x + w, y, z, r, g, b, a};

    // right line
    m_rectVertices3D[m_rectVertexCount3D++] = {x + w, y, z, r, g, b, a};
    m_rectVertices3D[m_rectVertexCount3D++] = {x + w, y + h, z, r, g, b, a};

    // bottom line
    m_rectVertices3D[m_rectVertexCount3D++] = {x + w, y + h, z, r, g, b, a};
    m_rectVertices3D[m_rectVertexCount3D++] = {x, y + h, z, r, g, b, a};
    
    // left line
    m_rectVertices3D[m_rectVertexCount3D++] = {x, y + h, z, r, g, b, a};
    m_rectVertices3D[m_rectVertexCount3D++] = {x, y, z, r, g, b, a};
    
#if IMMEDIATE_MODE_3D
    FlushRects3D();
#else
    if (immediateFlush) {
        FlushRects3D();
    }
#endif
}

void Renderer3D::RectFill3D(float x, float y, float z, float w, float h, Colour const &col, bool immediateFlush) {
    FlushRectFills3DIfFull(6);
    
    float r = col.GetRFloat(), g = col.GetGFloat(), b = col.GetBFloat(), a = col.GetAFloat();
    
    // first triangle: TL, TR, BR
    m_rectFillVertices3D[m_rectFillVertexCount3D++] = {x, y, z, r, g, b, a};
    m_rectFillVertices3D[m_rectFillVertexCount3D++] = {x + w, y, z, r, g, b, a};
    m_rectFillVertices3D[m_rectFillVertexCount3D++] = {x + w, y + h, z, r, g, b, a};
    
    // second triangle: TL, BR, BL
    m_rectFillVertices3D[m_rectFillVertexCount3D++] = {x, y, z, r, g, b, a};
    m_rectFillVertices3D[m_rectFillVertexCount3D++] = {x + w, y + h, z, r, g, b, a};
    m_rectFillVertices3D[m_rectFillVertexCount3D++] = {x, y + h, z, r, g, b, a};
    
#if IMMEDIATE_MODE_3D
    FlushRectFills3D();
#else
    if (immediateFlush) {
        FlushRectFills3D();
    }
#endif
}

void Renderer3D::TriangleFill3D(float x1, float y1, float z1, float x2, float y2, float z2, float x3, float y3, float z3, Colour const &col, bool immediateFlush) {
    FlushTriangleFills3DIfFull(3);
    
    float r = col.GetRFloat(), g = col.GetGFloat(), b = col.GetBFloat(), a = col.GetAFloat();
    
    m_triangleFillVertices3D[m_triangleFillVertexCount3D++] = {x1, y1, z1, r, g, b, a};
    m_triangleFillVertices3D[m_triangleFillVertexCount3D++] = {x2, y2, z2, r, g, b, a};
    m_triangleFillVertices3D[m_triangleFillVertexCount3D++] = {x3, y3, z3, r, g, b, a};
    
#if IMMEDIATE_MODE_3D
    FlushTriangleFills3D();
#else
    if (immediateFlush) {
        FlushTriangleFills3D();
    }
#endif
}