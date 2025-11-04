#include "lib/universal_include.h"

#include <stdarg.h>

#include "lib/resource/bitmapfont.h"
#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/resource/bitmap.h"
#include "lib/resource/sprite_atlas.h"
#include "lib/math/vector3.h"
#include "lib/render3d/renderer_3d.h"
#include "lib/render/colour.h"

#include "lib/render2d/renderer.h"

extern Renderer *g_renderer;

void Renderer::StaticSprite(Image *src, float x, float y, float w, float h, Colour const &col) {
    FlushStaticSpritesIfFull(6);
    
    unsigned int effectiveTextureID = GetEffectiveTextureID(src);
    
    m_currentStaticSpriteTexture = effectiveTextureID;
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    float u1, v1, u2, v2;
    
    AtlasImage* atlasImage = dynamic_cast<AtlasImage*>(src);
    
    if (atlasImage) {
        const char* filename = atlasImage->GetFilename();
        if (filename && strstr(filename, "smallnuke.bmp")) {
            GetImageUVCoords(src, u1, v1, u2, v2);

            //
            // shrink to match the old look of smallnuke

            float shrinkX = (u2 - u1) * 0.0625f;
            float shrinkY = (v2 - v1) * 0.10f;
            u1 += shrinkX;
            u2 -= shrinkX;
            v1 += shrinkY;
            v2 -= shrinkY;
        } else {
            GetImageUVCoords(src, u1, v1, u2, v2);
        }
    } else {
        GetImageUVCoords(src, u1, v1, u2, v2);
    }
    
    // First triangle: TL, TR, BR
    m_staticSpriteVertices[m_staticSpriteVertexCount++] = {x, y, r, g, b, a, u1, v2};
    m_staticSpriteVertices[m_staticSpriteVertexCount++] = {x + w, y, r, g, b, a, u2, v2};
    m_staticSpriteVertices[m_staticSpriteVertexCount++] = {x + w, y + h, r, g, b, a, u2, v1};
    
    // Second triangle: TL, BR, BL
    m_staticSpriteVertices[m_staticSpriteVertexCount++] = {x, y, r, g, b, a, u1, v2};
    m_staticSpriteVertices[m_staticSpriteVertexCount++] = {x + w, y + h, r, g, b, a, u2, v1};
    m_staticSpriteVertices[m_staticSpriteVertexCount++] = {x, y + h, r, g, b, a, u1, v1};
}

void Renderer::RotatingSprite(Image *src, float x, float y, float w, float h, Colour const &col, float angle) {
    FlushRotatingSpritesIfFull(6);
    
    unsigned int effectiveTextureID = GetEffectiveTextureID(src);
    
    m_currentRotatingSpriteTexture = effectiveTextureID;

    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    float u1, v1, u2, v2;
    
    AtlasImage* atlasImage = dynamic_cast<AtlasImage*>(src);
    
    if (atlasImage) {
        const char* filename = atlasImage->GetFilename();
        if (filename && strstr(filename, "smallnuke.bmp")) {
            GetImageUVCoords(src, u1, v1, u2, v2);

            //
            // shrink to match the old look of smallnuke

            float shrinkX = (u2 - u1) * 0.0625f;
            float shrinkY = (v2 - v1) * 0.10f;
            u1 += shrinkX;
            u2 -= shrinkX;
            v1 += shrinkY;
            v2 -= shrinkY;
        } else {
            GetImageUVCoords(src, u1, v1, u2, v2);
        }
    } else {
        GetImageUVCoords(src, u1, v1, u2, v2);
    }
    
    // Calculate rotated vertices
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

    
    // First triangle: vert1, vert2, vert3
    m_rotatingSpriteVertices[m_rotatingSpriteVertexCount++] = {vert1.x, vert1.y, r, g, b, a, u1, v2};
    m_rotatingSpriteVertices[m_rotatingSpriteVertexCount++] = {vert2.x, vert2.y, r, g, b, a, u2, v2};
    m_rotatingSpriteVertices[m_rotatingSpriteVertexCount++] = {vert3.x, vert3.y, r, g, b, a, u2, v1};
    
    // Second triangle: vert1, vert3, vert4
    m_rotatingSpriteVertices[m_rotatingSpriteVertexCount++] = {vert1.x, vert1.y, r, g, b, a, u1, v2};
    m_rotatingSpriteVertices[m_rotatingSpriteVertexCount++] = {vert3.x, vert3.y, r, g, b, a, u2, v1};
    m_rotatingSpriteVertices[m_rotatingSpriteVertexCount++] = {vert4.x, vert4.y, r, g, b, a, u1, v1};
}