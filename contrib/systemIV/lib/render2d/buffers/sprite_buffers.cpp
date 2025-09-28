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

void Renderer::UnitMainSprite(Image *src, float x, float y, float w, float h, Colour const &col) {
    FlushUnitMainSpritesIfFull(6);
    
    unsigned int effectiveTextureID = GetEffectiveTextureID(src);
    
    if (m_unitMainVertexCount > 0 && m_currentUnitMainTexture != effectiveTextureID) {
        FlushUnitMainSprites();
    }
    
    m_currentUnitMainTexture = effectiveTextureID;
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    float u1, v1, u2, v2;
    GetImageUVCoords(src, u1, v1, u2, v2);
    
    // First triangle: TL, TR, BR
    m_unitMainVertices[m_unitMainVertexCount++] = {x, y, r, g, b, a, u1, v2};
    m_unitMainVertices[m_unitMainVertexCount++] = {x + w, y, r, g, b, a, u2, v2};
    m_unitMainVertices[m_unitMainVertexCount++] = {x + w, y + h, r, g, b, a, u2, v1};
    
    // Second triangle: TL, BR, BL
    m_unitMainVertices[m_unitMainVertexCount++] = {x, y, r, g, b, a, u1, v2};
    m_unitMainVertices[m_unitMainVertexCount++] = {x + w, y + h, r, g, b, a, u2, v1};
    m_unitMainVertices[m_unitMainVertexCount++] = {x, y + h, r, g, b, a, u1, v1};
}

void Renderer::UnitRotating(Image *src, float x, float y, float w, float h, Colour const &col, float angle) {
    FlushUnitRotatingIfFull(6);
    
    unsigned int effectiveTextureID = GetEffectiveTextureID(src);
    
    if (m_unitRotatingVertexCount > 0 && m_currentUnitRotatingTexture != effectiveTextureID) {
        FlushUnitRotating();
    }
    
    m_currentUnitRotatingTexture = effectiveTextureID;
    
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
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    float u1, v1, u2, v2;
    GetImageUVCoords(src, u1, v1, u2, v2);
    
    // First triangle: vert1, vert2, vert3
    m_unitRotatingVertices[m_unitRotatingVertexCount++] = {vert1.x, vert1.y, r, g, b, a, u1, v2};
    m_unitRotatingVertices[m_unitRotatingVertexCount++] = {vert2.x, vert2.y, r, g, b, a, u2, v2};
    m_unitRotatingVertices[m_unitRotatingVertexCount++] = {vert3.x, vert3.y, r, g, b, a, u2, v1};
    
    // Second triangle: vert1, vert3, vert4
    m_unitRotatingVertices[m_unitRotatingVertexCount++] = {vert1.x, vert1.y, r, g, b, a, u1, v2};
    m_unitRotatingVertices[m_unitRotatingVertexCount++] = {vert3.x, vert3.y, r, g, b, a, u2, v1};
    m_unitRotatingVertices[m_unitRotatingVertexCount++] = {vert4.x, vert4.y, r, g, b, a, u1, v1};
}

void Renderer::UnitStateIcon(Image *stateSrc, float x, float y, float w, float h, Colour const &col) {
    FlushUnitStateIconsIfFull(6);
    
    unsigned int effectiveTextureID = GetEffectiveTextureID(stateSrc);
    
    if (m_unitStateVertexCount > 0 && m_currentUnitStateTexture != effectiveTextureID) {
        FlushUnitStateIcons();
    }
    
    m_currentUnitStateTexture = effectiveTextureID;
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    float u1, v1, u2, v2;
    GetImageUVCoords(stateSrc, u1, v1, u2, v2);
    
    // First triangle: TL, TR, BR
    m_unitStateVertices[m_unitStateVertexCount++] = {x, y, r, g, b, a, u1, v2};
    m_unitStateVertices[m_unitStateVertexCount++] = {x + w, y, r, g, b, a, u2, v2};
    m_unitStateVertices[m_unitStateVertexCount++] = {x + w, y + h, r, g, b, a, u2, v1};
    
    // Second triangle: TL, BR, BL
    m_unitStateVertices[m_unitStateVertexCount++] = {x, y, r, g, b, a, u1, v2};
    m_unitStateVertices[m_unitStateVertexCount++] = {x + w, y + h, r, g, b, a, u2, v1};
    m_unitStateVertices[m_unitStateVertexCount++] = {x, y + h, r, g, b, a, u1, v1};
}

void Renderer::UnitNukeIcon(float x, float y, float w, float h, Colour const &col) {
    FlushUnitNukeIconsIfFull(6);
    
    Image *nukeBmp = g_resource->GetImage("graphics/smallnuke.bmp");
    if (!nukeBmp) return;
    
    unsigned int effectiveTextureID = GetEffectiveTextureID(nukeBmp);

    if (m_unitNukeVertexCount > 0 && m_currentUnitNukeTexture != effectiveTextureID) {
        FlushUnitNukeIcons();
    }
    
    m_currentUnitNukeTexture = effectiveTextureID;
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    float u1, v1, u2, v2;
    
    AtlasImage* atlasImage = dynamic_cast<AtlasImage*>(nukeBmp);
    if (atlasImage) {
        GetImageUVCoords(nukeBmp, u1, v1, u2, v2);
        float shrinkX = (u2 - u1) * 0.0655f;  // 6.25% 
        u1 += shrinkX;
        u2 -= shrinkX;
    } else {
        GetImageUVCoords(nukeBmp, u1, v1, u2, v2);
    }
    
    // First triangle: TL, TR, BR
    m_unitNukeVertices[m_unitNukeVertexCount++] = {x, y, r, g, b, a, u1, v2};
    m_unitNukeVertices[m_unitNukeVertexCount++] = {x + w, y, r, g, b, a, u2, v2};
    m_unitNukeVertices[m_unitNukeVertexCount++] = {x + w, y + h, r, g, b, a, u2, v1};
    
    // Second triangle: TL, BR, BL
    m_unitNukeVertices[m_unitNukeVertexCount++] = {x, y, r, g, b, a, u1, v2};
    m_unitNukeVertices[m_unitNukeVertexCount++] = {x + w, y + h, r, g, b, a, u2, v1};
    m_unitNukeVertices[m_unitNukeVertexCount++] = {x, y + h, r, g, b, a, u1, v1};
}

void Renderer::UnitNukeIcon(float x, float y, float w, float h, Colour const &col, float angle) {
    FlushUnitNukeIconsIfFull(6);
    
    Image *nukeBmp = g_resource->GetImage("graphics/smallnuke.bmp");
    if (!nukeBmp) return;
    
    unsigned int effectiveTextureID = GetEffectiveTextureID(nukeBmp);
    
    if (m_unitNukeVertexCount > 0 && m_currentUnitNukeTexture != effectiveTextureID) {
        FlushUnitNukeIcons();
    }
    
    m_currentUnitNukeTexture = effectiveTextureID;
    
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
    
    float u1, v1, u2, v2;
    
    AtlasImage* atlasImage = dynamic_cast<AtlasImage*>(nukeBmp);
    if (atlasImage) {
        GetImageUVCoords(nukeBmp, u1, v1, u2, v2);
        float shrinkX = (u2 - u1) * 0.0625f; 
        u1 += shrinkX;
        u2 -= shrinkX;
    } else {
        GetImageUVCoords(nukeBmp, u1, v1, u2, v2);
    }
    
    // First triangle: vert1, vert2, vert3
    m_unitNukeVertices[m_unitNukeVertexCount++] = {vert1.x, vert1.y, r, g, b, a, u1, v2};
    m_unitNukeVertices[m_unitNukeVertexCount++] = {vert2.x, vert2.y, r, g, b, a, u2, v2};
    m_unitNukeVertices[m_unitNukeVertexCount++] = {vert3.x, vert3.y, r, g, b, a, u2, v1};
    
    // Second triangle: vert1, vert3, vert4
    m_unitNukeVertices[m_unitNukeVertexCount++] = {vert1.x, vert1.y, r, g, b, a, u1, v2};
    m_unitNukeVertices[m_unitNukeVertexCount++] = {vert3.x, vert3.y, r, g, b, a, u2, v1};
    m_unitNukeVertices[m_unitNukeVertexCount++] = {vert4.x, vert4.y, r, g, b, a, u1, v1};
}

void Renderer::EffectsSprite(Image *src, float x, float y, float w, float h, Colour const &col) {
    if (m_effectsSpriteVertexCount + 6 > MAX_EFFECTS_SPRITE_VERTICES) {
        FlushEffectsSprites();
    }
    
    unsigned int effectiveTextureID = GetEffectiveTextureID(src);

    if (m_effectsSpriteVertexCount > 0 && m_currentEffectsSpriteTexture != effectiveTextureID) {
        FlushEffectsSprites();
    }
    
    m_currentEffectsSpriteTexture = effectiveTextureID;
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    float u1, v1, u2, v2;
    GetImageUVCoords(src, u1, v1, u2, v2);
    
    // First triangle: TL, TR, BR
    m_effectsSpriteVertices[m_effectsSpriteVertexCount++] = {x, y, r, g, b, a, u1, v2};
    m_effectsSpriteVertices[m_effectsSpriteVertexCount++] = {x + w, y, r, g, b, a, u2, v2};
    m_effectsSpriteVertices[m_effectsSpriteVertexCount++] = {x + w, y + h, r, g, b, a, u2, v1};
    
    // Second triangle: TL, BR, BL
    m_effectsSpriteVertices[m_effectsSpriteVertexCount++] = {x, y, r, g, b, a, u1, v2};
    m_effectsSpriteVertices[m_effectsSpriteVertexCount++] = {x + w, y + h, r, g, b, a, u2, v1};
    m_effectsSpriteVertices[m_effectsSpriteVertexCount++] = {x, y + h, r, g, b, a, u1, v1};
}

void Renderer::UnitHighlight(Image *blurSrc, float x, float y, float w, float h, Colour const &col) {
    if (m_unitHighlightVertexCount + 6 > MAX_UNIT_HIGHLIGHT_VERTICES) {
        FlushUnitHighlights();
    }
    
    unsigned int effectiveTextureID = GetEffectiveTextureID(blurSrc);
    
    if (m_unitHighlightVertexCount > 0 && m_currentUnitHighlightTexture != effectiveTextureID) {
        FlushUnitHighlights();
    }
    
    m_currentUnitHighlightTexture = effectiveTextureID;
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    float u1, v1, u2, v2;
    GetImageUVCoords(blurSrc, u1, v1, u2, v2);
    
    // First triangle: TL, TR, BR
    m_unitHighlightVertices[m_unitHighlightVertexCount++] = {x, y, r, g, b, a, u1, v2};
    m_unitHighlightVertices[m_unitHighlightVertexCount++] = {x + w, y, r, g, b, a, u2, v2};
    m_unitHighlightVertices[m_unitHighlightVertexCount++] = {x + w, y + h, r, g, b, a, u2, v1};
    
    // Second triangle: TL, BR, BL
    m_unitHighlightVertices[m_unitHighlightVertexCount++] = {x, y, r, g, b, a, u1, v2};
    m_unitHighlightVertices[m_unitHighlightVertexCount++] = {x + w, y + h, r, g, b, a, u2, v1};
    m_unitHighlightVertices[m_unitHighlightVertexCount++] = {x, y + h, r, g, b, a, u1, v1};
}