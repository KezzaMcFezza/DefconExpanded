#include "lib/universal_include.h"

#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/math/vector3.h"
#include "lib/render/colour.h"

#include "lib/render2d/renderer.h"

extern Renderer *g_renderer;

// ============================================================================
// BATCHED ECLIPSE SPRITE RENDERING
// ============================================================================

void Renderer::EclipseSprite(Image *src, float x, float y, float w, float h, Colour const &col) {
    if (m_uiTriangleVertexCount + 6 > MAX_UI_VERTICES) {
        FlushUITriangles();
    }
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    // Get UV coordinates - atlas sprites use specific regions, others use full texture
    float u1, v1, u2, v2;
    GetImageUVCoords(src, u1, v1, u2, v2);
    
    // First triangle: TL, TR, BR
    m_uiTriangleVertices[m_uiTriangleVertexCount++] = {x, y, r, g, b, a, u1, v2};
    m_uiTriangleVertices[m_uiTriangleVertexCount++] = {x + w, y, r, g, b, a, u2, v2};
    m_uiTriangleVertices[m_uiTriangleVertexCount++] = {x + w, y + h, r, g, b, a, u2, v1};
    
    // Second triangle: TL, BR, BL
    m_uiTriangleVertices[m_uiTriangleVertexCount++] = {x, y, r, g, b, a, u1, v2};
    m_uiTriangleVertices[m_uiTriangleVertexCount++] = {x + w, y + h, r, g, b, a, u2, v1};
    m_uiTriangleVertices[m_uiTriangleVertexCount++] = {x, y + h, r, g, b, a, u1, v1};
}

void Renderer::EclipseSprite(Image *src, float x, float y, float w, float h, Colour const &col, float angle) {
    if (m_uiTriangleVertexCount + 6 > MAX_UI_VERTICES) {
        FlushUITriangles();
    }
    
    // calculate rotated vertices
    Vector3<float> vert1(-w, +h, 0);
    Vector3<float> vert2(+w, +h, 0);
    Vector3<float> vert3(+w, -h, 0);
    Vector3<float> vert4(-w, -h, 0);

    vert1.RotateAroundZ(angle);
    vert2.RotateAroundZ(angle);
    vert3.RotateAroundZ(angle);
    vert4.RotateAroundZ(angle);

    vert1 += Vector3<float>(x, y, 0);
    vert2 += Vector3<float>(x, y, 0);
    vert3 += Vector3<float>(x, y, 0);
    vert4 += Vector3<float>(x, y, 0);

    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    // get UV coordinates, atlas sprites use specific regions, others use full texture
    float u1, v1, u2, v2;
    GetImageUVCoords(src, u1, v1, u2, v2);
    
    // first triangle: vert1, vert2, vert3
    m_uiTriangleVertices[m_uiTriangleVertexCount++] = {vert1.x, vert1.y, r, g, b, a, u1, v2};
    m_uiTriangleVertices[m_uiTriangleVertexCount++] = {vert2.x, vert2.y, r, g, b, a, u2, v2};
    m_uiTriangleVertices[m_uiTriangleVertexCount++] = {vert3.x, vert3.y, r, g, b, a, u2, v1};
    
    // second triangle: vert1, vert3, vert4
    m_uiTriangleVertices[m_uiTriangleVertexCount++] = {vert1.x, vert1.y, r, g, b, a, u1, v2};
    m_uiTriangleVertices[m_uiTriangleVertexCount++] = {vert3.x, vert3.y, r, g, b, a, u2, v1};
    m_uiTriangleVertices[m_uiTriangleVertexCount++] = {vert4.x, vert4.y, r, g, b, a, u1, v1};
}
