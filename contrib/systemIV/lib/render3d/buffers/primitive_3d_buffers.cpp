#include "lib/universal_include.h"

#include <stdarg.h>

#include "lib/resource/image.h"
#include "lib/render/colour.h"

#include "lib/render3d/renderer_3d.h"

extern Renderer3D *g_renderer3d;

// ============================================================================
// PRIMITIVE RENDERING FUNCTIONS
// ============================================================================

void Renderer3D::Line3D(float x1, float y1, float z1, float x2, float y2, float z2, Colour const &col) {
    FlushLine3DIfFull(2);
    
    float r = col.GetRFloat(), g = col.GetGFloat(), b = col.GetBFloat(), a = col.GetAFloat();
    
    m_lineVertices3D[m_lineVertexCount3D++] = {x1, y1, z1, r, g, b, a};
    m_lineVertices3D[m_lineVertexCount3D++] = {x2, y2, z2, r, g, b, a};
}

void Renderer3D::GlobeSurfaceTriangle3D(float x1, float y1, float z1, float x2, float y2, float z2, float x3, float y3, float z3, Colour const &col) {
    FlushGlobeSurface3DIfFull(3);
    
    float r = col.GetRFloat(), g = col.GetGFloat(), b = col.GetBFloat(), a = col.GetAFloat();
    
    m_globeSurfaceVertices3D[m_globeSurfaceVertexCount3D++] = {x1, y1, z1, r, g, b, a};
    m_globeSurfaceVertices3D[m_globeSurfaceVertexCount3D++] = {x2, y2, z2, r, g, b, a};
    m_globeSurfaceVertices3D[m_globeSurfaceVertexCount3D++] = {x3, y3, z3, r, g, b, a};
}
