#include "lib/universal_include.h"

#include <stdarg.h>

#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/resource/bitmapfont.h"
#include "lib/resource/sprite_atlas.h"
#include "lib/math/vector3.h"
#include "lib/render/colour.h"

#include "renderer/map_renderer.h"
#include "../renderer_3d.h"

extern Renderer3D *g_renderer3d;

void Renderer3D::UnitMainSprite3D(Image *src, float x, float y, float z, float w, float h, Colour const &col) {
    // surface aligned billboard for units
    UnitMainSprite3D(src, x, y, z, w, h, col, BILLBOARD_SURFACE_ALIGNED);
}

void Renderer3D::UnitMainSprite3D(Image *src, float x, float y, float z, float w, float h, Colour const &col, BillboardMode3D mode) {
    FlushUnitMainSprites3DIfFull(6);
    
    unsigned int effectiveTextureID = GetEffectiveTextureID(src);
    
    if (m_unitMainVertexCount3D > 0 && m_currentUnitMainTexture3D != effectiveTextureID) {
        FlushUnitMainSprites3D();
    }
    
    m_currentUnitMainTexture3D = effectiveTextureID;
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    float u1, v1, u2, v2;
    GetImageUVCoords(src, u1, v1, u2, v2);
    
    // create billboard vertices based on the mode
    Vector3<float> position(x, y, z);
    Vertex3DTextured billboardVertices[6];
    
    if (mode == BILLBOARD_SURFACE_ALIGNED) {
        CreateSurfaceAlignedBillboard(position, w, h, billboardVertices, u1, v1, u2, v2, r, g, b, a);
    } else if (mode == BILLBOARD_CAMERA_FACING) {
        CreateCameraFacingBillboard(position, w, h, billboardVertices, u1, v1, u2, v2, r, g, b, a);
    } else {
        // old immediate mode rendering, would be used if we overflow
        billboardVertices[0] = {x - w/2, y + h/2, z, r, g, b, a, u1, v2};
        billboardVertices[1] = {x + w/2, y + h/2, z, r, g, b, a, u2, v2};
        billboardVertices[2] = {x + w/2, y - h/2, z, r, g, b, a, u2, v1};
        billboardVertices[3] = {x - w/2, y + h/2, z, r, g, b, a, u1, v2};
        billboardVertices[4] = {x + w/2, y - h/2, z, r, g, b, a, u2, v1};
        billboardVertices[5] = {x - w/2, y - h/2, z, r, g, b, a, u1, v1};
    }
    
    for (int i = 0; i < 6; i++) {
        m_unitMainVertices3D[m_unitMainVertexCount3D++] = billboardVertices[i];
    }
}

void Renderer3D::UnitRotating3D(Image *src, float x, float y, float z, float w, float h, Colour const &col, float angle) {
    UnitRotating3D(src, x, y, z, w, h, col, angle, BILLBOARD_CAMERA_FACING);
}

void Renderer3D::UnitRotating3D(Image *src, float x, float y, float z, float w, float h, Colour const &col, float angle, BillboardMode3D mode) {
    FlushUnitRotating3DIfFull(6);
    
    unsigned int effectiveTextureID = GetEffectiveTextureID(src);
    
    if (m_unitRotatingVertexCount3D > 0 && m_currentUnitRotatingTexture3D != effectiveTextureID) {
        FlushUnitRotating3D();
    }
    
    m_currentUnitRotatingTexture3D = effectiveTextureID;
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    float u1, v1, u2, v2;
    GetImageUVCoords(src, u1, v1, u2, v2);
    
    Vector3<float> position(x, y, z);
    Vertex3DTextured billboardVertices[6];
    
    if (mode == BILLBOARD_SURFACE_ALIGNED) {
        CreateSurfaceAlignedBillboard(position, w, h, billboardVertices, u1, v1, u2, v2, r, g, b, a, angle);
    } else if (mode == BILLBOARD_CAMERA_FACING) {
        CreateCameraFacingBillboard(position, w, h, billboardVertices, u1, v1, u2, v2, r, g, b, a, angle);
    } else {
        // immediate mode rendering, would be used if we overflow
        Vector3<float> vert1(-w/2, +h/2, 0);
        Vector3<float> vert2(+w/2, +h/2, 0);
        Vector3<float> vert3(+w/2, -h/2, 0);
        Vector3<float> vert4(-w/2, -h/2, 0);

        vert1.RotateAroundZ(angle);
        vert2.RotateAroundZ(angle);
        vert3.RotateAroundZ(angle);
        vert4.RotateAroundZ(angle);

        vert1 += Vector3<float>(x, y, z);
        vert2 += Vector3<float>(x, y, z);
        vert3 += Vector3<float>(x, y, z);
        vert4 += Vector3<float>(x, y, z);
        
        billboardVertices[0] = {vert1.x, vert1.y, vert1.z, r, g, b, a, u1, v2};
        billboardVertices[1] = {vert2.x, vert2.y, vert2.z, r, g, b, a, u2, v2};
        billboardVertices[2] = {vert3.x, vert3.y, vert3.z, r, g, b, a, u2, v1};
        billboardVertices[3] = {vert1.x, vert1.y, vert1.z, r, g, b, a, u1, v2};
        billboardVertices[4] = {vert3.x, vert3.y, vert3.z, r, g, b, a, u2, v1};
        billboardVertices[5] = {vert4.x, vert4.y, vert4.z, r, g, b, a, u1, v1};
    }
    
    for (int i = 0; i < 6; i++) {
        m_unitRotatingVertices3D[m_unitRotatingVertexCount3D++] = billboardVertices[i];
    }
}

void Renderer3D::UnitStateIcon3D(Image *stateSrc, float x, float y, float z, float w, float h, Colour const &col) {
    UnitStateIcon3D(stateSrc, x, y, z, w, h, col, BILLBOARD_SURFACE_ALIGNED);
}

void Renderer3D::UnitStateIcon3D(Image *stateSrc, float x, float y, float z, float w, float h, Colour const &col, BillboardMode3D mode) {
    FlushUnitStateIcons3DIfFull(6);
    
    unsigned int effectiveTextureID = GetEffectiveTextureID(stateSrc);
    
    if (m_unitStateVertexCount3D > 0 && m_currentUnitStateTexture3D != effectiveTextureID) {
        FlushUnitStateIcons3D();
    }
    
    m_currentUnitStateTexture3D = effectiveTextureID;
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    float u1, v1, u2, v2;
    GetImageUVCoords(stateSrc, u1, v1, u2, v2);
    
    Vector3<float> position(x, y, z);
    Vertex3DTextured billboardVertices[6];
    
    if (mode == BILLBOARD_SURFACE_ALIGNED) {
        CreateSurfaceAlignedBillboard(position, w, h, billboardVertices, u1, v1, u2, v2, r, g, b, a);
    } else if (mode == BILLBOARD_CAMERA_FACING) {
        CreateCameraFacingBillboard(position, w, h, billboardVertices, u1, v1, u2, v2, r, g, b, a);
    } else {
        billboardVertices[0] = {x - w/2, y + h/2, z, r, g, b, a, u1, v2};
        billboardVertices[1] = {x + w/2, y + h/2, z, r, g, b, a, u2, v2};
        billboardVertices[2] = {x + w/2, y - h/2, z, r, g, b, a, u2, v1};
        billboardVertices[3] = {x - w/2, y + h/2, z, r, g, b, a, u1, v2};
        billboardVertices[4] = {x + w/2, y - h/2, z, r, g, b, a, u2, v1};
        billboardVertices[5] = {x - w/2, y - h/2, z, r, g, b, a, u1, v1};
    }
    
    for (int i = 0; i < 6; i++) {
        m_unitStateVertices3D[m_unitStateVertexCount3D++] = billboardVertices[i];
    }
}

void Renderer3D::UnitNukeIcon3D(float x, float y, float z, float w, float h, Colour const &col) {
    UnitNukeIcon3D(x, y, z, w, h, col, BILLBOARD_SURFACE_ALIGNED);
}

void Renderer3D::UnitNukeIcon3D(float x, float y, float z, float w, float h, Colour const &col, BillboardMode3D mode) {
    FlushUnitNukeIcons3DIfFull(6);
    
    Image *nukeBmp = g_resource->GetImage("graphics/smallnuke.bmp");
    if (!nukeBmp) return;
    
    unsigned int effectiveTextureID = GetEffectiveTextureID(nukeBmp);

    if (m_unitNukeVertexCount3D > 0 && m_currentUnitNukeTexture3D != effectiveTextureID) {
        FlushUnitNukeIcons3D();
    }
    
    m_currentUnitNukeTexture3D = effectiveTextureID;
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    float u1, v1, u2, v2;
    
    // make smallnuke behave like old Blit() system by shrinking UV area 
    AtlasImage* atlasImage = dynamic_cast<AtlasImage*>(nukeBmp);
    if (atlasImage) {
        GetImageUVCoords(nukeBmp, u1, v1, u2, v2);
        
        // shrink to match the old look of smallnuke
        float shrinkX = (u2 - u1) * 0.0625f;
        float shrinkY = (v2 - v1) * 0.10f;
        u1 += shrinkX;
        u2 -= shrinkX;
        v1 += shrinkY;
        v2 -= shrinkY;
    } else {
        GetImageUVCoords(nukeBmp, u1, v1, u2, v2);
    }
    
    Vector3<float> position(x, y, z);
    Vertex3DTextured billboardVertices[6];
    
    if (mode == BILLBOARD_SURFACE_ALIGNED) {
        CreateSurfaceAlignedBillboard(position, w, h, billboardVertices, u1, v1, u2, v2, r, g, b, a);
    } else if (mode == BILLBOARD_CAMERA_FACING) {
        CreateCameraFacingBillboard(position, w, h, billboardVertices, u1, v1, u2, v2, r, g, b, a);
    } else {
        // immediate mode rendering, would be used if we overflow
        billboardVertices[0] = {x - w/2, y + h/2, z, r, g, b, a, u1, v2};
        billboardVertices[1] = {x + w/2, y + h/2, z, r, g, b, a, u2, v2};
        billboardVertices[2] = {x + w/2, y - h/2, z, r, g, b, a, u2, v1};
        billboardVertices[3] = {x - w/2, y + h/2, z, r, g, b, a, u1, v2};
        billboardVertices[4] = {x + w/2, y - h/2, z, r, g, b, a, u2, v1};
        billboardVertices[5] = {x - w/2, y - h/2, z, r, g, b, a, u1, v1};
    }
    
    for (int i = 0; i < 6; i++) {
        m_unitNukeVertices3D[m_unitNukeVertexCount3D++] = billboardVertices[i];
    }
}

void Renderer3D::UnitNukeIcon3D(float x, float y, float z, float w, float h, Colour const &col, float angle) {
    UnitNukeIcon3D(x, y, z, w, h, col, angle, BILLBOARD_CAMERA_FACING);
}

void Renderer3D::UnitNukeIcon3D(float x, float y, float z, float w, float h, Colour const &col, float angle, BillboardMode3D mode) {
    FlushUnitNukeIcons3DIfFull(6);
    
    Image *nukeBmp = g_resource->GetImage("graphics/smallnuke.bmp");
    if (!nukeBmp) return;
    
    unsigned int effectiveTextureID = GetEffectiveTextureID(nukeBmp);
    
    if (m_unitNukeVertexCount3D > 0 && m_currentUnitNukeTexture3D != effectiveTextureID) {
        FlushUnitNukeIcons3D();
    }
    
    m_currentUnitNukeTexture3D = effectiveTextureID;
    
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
    
    Vector3<float> position(x, y, z);
    Vertex3DTextured billboardVertices[6];
    
    if (mode == BILLBOARD_SURFACE_ALIGNED) {
        CreateSurfaceAlignedBillboard(position, w, h, billboardVertices, u1, v1, u2, v2, r, g, b, a, angle);
    } else if (mode == BILLBOARD_CAMERA_FACING) {
        CreateCameraFacingBillboard(position, w, h, billboardVertices, u1, v1, u2, v2, r, g, b, a, angle);
    } else {
        // immediate mode rendering, would be used if we overflow
        Vector3<float> vert1(-w/2, +h/2, 0);
        Vector3<float> vert2(+w/2, +h/2, 0);
        Vector3<float> vert3(+w/2, -h/2, 0);
        Vector3<float> vert4(-w/2, -h/2, 0);

        vert1.RotateAroundZ(angle);
        vert2.RotateAroundZ(angle);
        vert3.RotateAroundZ(angle);
        vert4.RotateAroundZ(angle);

        vert1 += Vector3<float>(x, y, z);
        vert2 += Vector3<float>(x, y, z);
        vert3 += Vector3<float>(x, y, z);
        vert4 += Vector3<float>(x, y, z);
        
        billboardVertices[0] = {vert1.x, vert1.y, vert1.z, r, g, b, a, u1, v2};
        billboardVertices[1] = {vert2.x, vert2.y, vert2.z, r, g, b, a, u2, v2};
        billboardVertices[2] = {vert3.x, vert3.y, vert3.z, r, g, b, a, u2, v1};
        billboardVertices[3] = {vert1.x, vert1.y, vert1.z, r, g, b, a, u1, v2};
        billboardVertices[4] = {vert3.x, vert3.y, vert3.z, r, g, b, a, u2, v1};
        billboardVertices[5] = {vert4.x, vert4.y, vert4.z, r, g, b, a, u1, v1};
    }
    
    for (int i = 0; i < 6; i++) {
        m_unitNukeVertices3D[m_unitNukeVertexCount3D++] = billboardVertices[i];
    }
}

void Renderer3D::UnitHighlight3D(Image *blurSrc, float x, float y, float z, float w, float h, Colour const &col) {
    if (m_unitHighlightVertexCount3D + 6 > MAX_UNIT_HIGHLIGHT_VERTICES_3D) {
        FlushUnitHighlights3D();
    }
    
    unsigned int effectiveTextureID = GetEffectiveTextureID(blurSrc);
    
    if (m_unitHighlightVertexCount3D > 0 && m_currentUnitHighlightTexture3D != effectiveTextureID) {
        FlushUnitHighlights3D();
    }
    
    m_currentUnitHighlightTexture3D = effectiveTextureID;
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    float u1, v1, u2, v2;
    GetImageUVCoords(blurSrc, u1, v1, u2, v2);
    
    // First triangle: TL, TR, BR
    m_unitHighlightVertices3D[m_unitHighlightVertexCount3D++] = {x - w/2, y + h/2, z, r, g, b, a, u1, v2};
    m_unitHighlightVertices3D[m_unitHighlightVertexCount3D++] = {x + w/2, y + h/2, z, r, g, b, a, u2, v2};
    m_unitHighlightVertices3D[m_unitHighlightVertexCount3D++] = {x + w/2, y - h/2, z, r, g, b, a, u2, v1};
    
    // Second triangle: TL, BR, BL
    m_unitHighlightVertices3D[m_unitHighlightVertexCount3D++] = {x - w/2, y + h/2, z, r, g, b, a, u1, v2};
    m_unitHighlightVertices3D[m_unitHighlightVertexCount3D++] = {x + w/2, y - h/2, z, r, g, b, a, u2, v1};
    m_unitHighlightVertices3D[m_unitHighlightVertexCount3D++] = {x - w/2, y - h/2, z, r, g, b, a, u1, v1};
}

void Renderer3D::EffectsSprite3D(Image *src, float x, float y, float z, float w, float h, Colour const &col) {
    EffectsSprite3D(src, x, y, z, w, h, col, BILLBOARD_SURFACE_ALIGNED);
}

void Renderer3D::EffectsSprite3D(Image *src, float x, float y, float z, float w, float h, Colour const &col, BillboardMode3D mode) {
    if (m_effectsSpriteVertexCount3D + 6 > MAX_EFFECTS_SPRITE_VERTICES_3D) {
        FlushEffectsSprites3D();
    }
    
    unsigned int effectiveTextureID = GetEffectiveTextureID(src);

    if (m_effectsSpriteVertexCount3D > 0 && m_currentEffectsSpriteTexture3D != effectiveTextureID) {
        FlushEffectsSprites3D();
    }
    
    m_currentEffectsSpriteTexture3D = effectiveTextureID;
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    float u1, v1, u2, v2;
    GetImageUVCoords(src, u1, v1, u2, v2);
    
    Vector3<float> position(x, y, z);
    Vertex3DTextured billboardVertices[6];
    
    if (mode == BILLBOARD_SURFACE_ALIGNED) {
        CreateSurfaceAlignedBillboard(position, w, h, billboardVertices, u1, v1, u2, v2, r, g, b, a);
    } else if (mode == BILLBOARD_CAMERA_FACING) {
        CreateCameraFacingBillboard(position, w, h, billboardVertices, u1, v1, u2, v2, r, g, b, a);
    } else {
        // immediate mode rendering, would be used if we overflow
        billboardVertices[0] = {x - w/2, y + h/2, z, r, g, b, a, u1, v2};
        billboardVertices[1] = {x + w/2, y + h/2, z, r, g, b, a, u2, v2};
        billboardVertices[2] = {x + w/2, y - h/2, z, r, g, b, a, u2, v1};
        billboardVertices[3] = {x - w/2, y + h/2, z, r, g, b, a, u1, v2};
        billboardVertices[4] = {x + w/2, y - h/2, z, r, g, b, a, u2, v1};
        billboardVertices[5] = {x - w/2, y - h/2, z, r, g, b, a, u1, v1};
    }
    
    for (int i = 0; i < 6; i++) {
        m_effectsSpriteVertices3D[m_effectsSpriteVertexCount3D++] = billboardVertices[i];
    }
}

void Renderer3D::StarFieldSprite3D(Image *src, float x, float y, float z, float w, float h, Colour const &col) {
    FlushStarField3DIfFull(6);
    
    unsigned int effectiveTextureID = GetEffectiveTextureID(src);
    
    if (m_starFieldVertexCount3D > 0 && m_currentStarFieldTexture3D != effectiveTextureID) {
        FlushStarField3D();
    }
    
    m_currentStarFieldTexture3D = effectiveTextureID;
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    float u1, v1, u2, v2;
    GetImageUVCoords(src, u1, v1, u2, v2);
    
    // create camera facing billboard for star sprites
    Vector3<float> position(x, y, z);
    Vertex3DTextured billboardVertices[6];
    CreateCameraFacingBillboard(position, w, h, billboardVertices, u1, v1, u2, v2, r, g, b, a);
    
    for (int i = 0; i < 6; i++) {
        m_starFieldVertices3D[m_starFieldVertexCount3D++] = billboardVertices[i];
    }
}
