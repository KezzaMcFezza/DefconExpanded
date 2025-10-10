#include <stdarg.h>
#include "lib/universal_include.h"

#include "lib/resource/image.h"
#include "lib/render/colour.h"

#include "lib/render3d/renderer_3d.h"

extern Renderer3D *g_renderer3d;

// ============================================================================
// PRIMITIVE RENDERING FUNCTIONS
// ============================================================================

void Renderer3D::UnitTrailLine3D(float x1, float y1, float z1, float x2, float y2, float z2, Colour const &col) {
    FlushUnitTrails3DIfFull(2);
    
    float r = col.GetRFloat(), g = col.GetGFloat(), b = col.GetBFloat(), a = col.GetAFloat();
    
    m_unitTrailVertices3D[m_unitTrailVertexCount3D++] = {x1, y1, z1, r, g, b, a};
    m_unitTrailVertices3D[m_unitTrailVertexCount3D++] = {x2, y2, z2, r, g, b, a};
}

void Renderer3D::EffectsLine3D(float x1, float y1, float z1, float x2, float y2, float z2, Colour const &col) {
    FlushEffectsLines3DIfFull(2);
    
    float r = col.GetRFloat(), g = col.GetGFloat(), b = col.GetBFloat(), a = col.GetAFloat();
    
    m_effectsLineVertices3D[m_effectsLineVertexCount3D++] = {x1, y1, z1, r, g, b, a};
    m_effectsLineVertices3D[m_effectsLineVertexCount3D++] = {x2, y2, z2, r, g, b, a};
}

void Renderer3D::HealthBarRect3D(float x, float y, float z, float w, float h, Colour const &col) {
    if (m_healthBarVertexCount3D + 6 > MAX_HEALTH_BAR_VERTICES_3D) {
        FlushHealthBars3D();
    }
    
    float r = col.GetRFloat(), g = col.GetGFloat(), b = col.GetBFloat(), a = col.GetAFloat();
    
    // First triangle: TL, TR, BR
    m_healthBarVertices3D[m_healthBarVertexCount3D++] = {x - w/2, y + h/2, z, r, g, b, a, 0.0f, 0.0f};
    m_healthBarVertices3D[m_healthBarVertexCount3D++] = {x + w/2, y + h/2, z, r, g, b, a, 0.0f, 0.0f};
    m_healthBarVertices3D[m_healthBarVertexCount3D++] = {x + w/2, y - h/2, z, r, g, b, a, 0.0f, 0.0f};
    
    // Second triangle: TL, BR, BL
    m_healthBarVertices3D[m_healthBarVertexCount3D++] = {x - w/2, y + h/2, z, r, g, b, a, 0.0f, 0.0f};
    m_healthBarVertices3D[m_healthBarVertexCount3D++] = {x + w/2, y - h/2, z, r, g, b, a, 0.0f, 0.0f};
    m_healthBarVertices3D[m_healthBarVertexCount3D++] = {x - w/2, y - h/2, z, r, g, b, a, 0.0f, 0.0f};
}

void Renderer3D::GlobeSurfaceTriangle3D(float x1, float y1, float z1, float x2, float y2, float z2, float x3, float y3, float z3, Colour const &col) {
    FlushGlobeSurface3DIfFull(3);
    
    float r = col.GetRFloat(), g = col.GetGFloat(), b = col.GetBFloat(), a = col.GetAFloat();
    
    m_globeSurfaceVertices3D[m_globeSurfaceVertexCount3D++] = {x1, y1, z1, r, g, b, a};
    m_globeSurfaceVertices3D[m_globeSurfaceVertexCount3D++] = {x2, y2, z2, r, g, b, a};
    m_globeSurfaceVertices3D[m_globeSurfaceVertexCount3D++] = {x3, y3, z3, r, g, b, a};
}
