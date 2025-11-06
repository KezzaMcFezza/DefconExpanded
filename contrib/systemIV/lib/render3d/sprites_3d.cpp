#include "lib/universal_include.h"

#include <stdarg.h>

#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/resource/bitmapfont.h"
#include "lib/resource/sprite_atlas.h"
#include "lib/math/vector3.h"
#include "lib/render/colour.h"

#include "renderer/map_renderer.h"
#include "lib/render3d/renderer_3d.h"

extern Renderer3D *g_renderer3d;

void Renderer3D::StaticSprite3D(Image *src, float x, float y, float z, float w, float h, Colour const &col, bool immediateFlush) {
    // surface aligned billboard for units
    StaticSprite3D(src, x, y, z, w, h, col, BILLBOARD_SURFACE_ALIGNED, immediateFlush);
}

void Renderer3D::StaticSprite3D(Image *src, float x, float y, float z, float w, float h, Colour const &col, BillboardMode3D mode, bool immediateFlush) {
    FlushStaticSprites3DIfFull(6);
    
    unsigned int effectiveTextureID = GetEffectiveTextureID(src);
    
    if (m_staticSpriteVertexCount3D > 0 && m_currentStaticSpriteTexture3D != effectiveTextureID) {
        FlushStaticSprites3D();
    }
    
    m_currentStaticSpriteTexture3D = effectiveTextureID;
    
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
        m_staticSpriteVertices3D[m_staticSpriteVertexCount3D++] = billboardVertices[i];
    }
    
#if IMMEDIATE_MODE_3D
    FlushStaticSprites3D();
#else
    if (immediateFlush) {
        FlushStaticSprites3D();
    }
#endif
}

void Renderer3D::RotatingSprite3D(Image *src, float x, float y, float z, float w, float h, Colour const &col, float angle, bool immediateFlush) {
    RotatingSprite3D(src, x, y, z, w, h, col, angle, BILLBOARD_CAMERA_FACING, immediateFlush);
}

void Renderer3D::RotatingSprite3D(Image *src, float x, float y, float z, float w, float h, Colour const &col, float angle, BillboardMode3D mode, bool immediateFlush) {
    FlushRotatingSprite3DIfFull(6);
    
    unsigned int effectiveTextureID = GetEffectiveTextureID(src);
    
    if (m_rotatingSpriteVertexCount3D > 0 && m_currentRotatingSpriteTexture3D != effectiveTextureID) {
        FlushRotatingSprite3D();
    }
    
    m_currentRotatingSpriteTexture3D = effectiveTextureID;
    
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
        m_rotatingSpriteVertices3D[m_rotatingSpriteVertexCount3D++] = billboardVertices[i];
    }
    
#if IMMEDIATE_MODE_3D
    FlushRotatingSprite3D();
#else
    if (immediateFlush) {
        FlushRotatingSprite3D();
    }
#endif
}
