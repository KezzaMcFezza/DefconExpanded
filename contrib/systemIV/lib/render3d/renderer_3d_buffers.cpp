#include "lib/universal_include.h"

#include <stdarg.h>

#include "lib/debug_utils.h"
#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/resource/bitmapfont.h"
#include "lib/math/vector3.h"
#include "lib/math/math_utils.h"
#include "lib/string_utils.h"
#include "lib/resource/sprite_atlas.h"
#include "lib/render/colour.h"

#include "renderer_3d.h"

extern Renderer3D *g_renderer3d;

// get UV coordinates for an image from the atlas
void Renderer3D::GetImageUVCoords(Image* image, float& u1, float& v1, float& u2, float& v2) {
    AtlasImage* atlasImage = dynamic_cast<AtlasImage*>(image);
    if (atlasImage) {
        // use atlas coordinates
        const AtlasCoord* coord = atlasImage->GetAtlasCoord();
        if (coord) {
            u1 = coord->u1;
            v1 = coord->v1;
            u2 = coord->u2;
            v2 = coord->v2;
            return;
        }
    }
    
    // regular image, use full texture with edge padding
    float onePixelW = 1.0f / (float) image->Width();
    float onePixelH = 1.0f / (float) image->Height();
    u1 = onePixelW;
    v1 = onePixelH;
    u2 = 1.0f - onePixelW;
    v2 = 1.0f - onePixelH;
}

// get texture ID for batching
unsigned int Renderer3D::GetEffectiveTextureID(Image* image) {
    AtlasImage* atlasImage = dynamic_cast<AtlasImage*>(image);
    if (atlasImage) {
        return atlasImage->GetAtlasTextureID();
    }
    return image->m_textureID;
}

void Renderer3D::UnitTrailLine3D(float x1, float y1, float z1, float x2, float y2, float z2, Colour const &col) {
    FlushUnitTrails3DIfFull(2);
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    m_unitTrailVertices3D[m_unitTrailVertexCount3D++] = {x1, y1, z1, r, g, b, a};
    m_unitTrailVertices3D[m_unitTrailVertexCount3D++] = {x2, y2, z2, r, g, b, a};
}

void Renderer3D::BeginUnitTrailBatch3D() {
    // trail segments will be batched until EndUnitTrailBatch3D()
}

void Renderer3D::EndUnitTrailBatch3D() {
    FlushUnitTrails3D();
}

void Renderer3D::FlushUnitTrails3DIfFull(int segmentsNeeded) {
    if (m_unitTrailVertexCount3D + segmentsNeeded > MAX_UNIT_TRAIL_VERTICES_3D) {
        FlushUnitTrails3D();
    }
}

void Renderer3D::FlushUnitTrails3D() {
    if (m_unitTrailVertexCount3D == 0) return;

    IncrementDrawCall3D("unit_trails");
    
    // use 3D shader program
    glUseProgram(m_shader3DProgram);
    
    int projLoc = glGetUniformLocation(m_shader3DProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_shader3DProgram, "uModelView");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix3D.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix3D.m);
    
    // set fog uniforms
    SetFogUniforms3D(m_shader3DProgram);
    
    glBindVertexArray(m_VAO3D);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO3D);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_unitTrailVertexCount3D * sizeof(Vertex3D), m_unitTrailVertices3D);
    
    glDrawArrays(GL_LINES, 0, m_unitTrailVertexCount3D);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_unitTrailVertexCount3D = 0;
}

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
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, effectiveTextureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    if (src->m_mipmapping) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
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

void Renderer3D::BeginUnitMainBatch3D() {
    m_unitMainVertexCount3D = 0;
    m_currentUnitMainTexture3D = 0;
}

void Renderer3D::EndUnitMainBatch3D() {
    if (m_unitMainVertexCount3D > 0) {
        FlushUnitMainSprites3D();
    }
}

void Renderer3D::FlushUnitMainSprites3DIfFull(int verticesNeeded) {
    if (m_unitMainVertexCount3D + verticesNeeded > MAX_UNIT_MAIN_VERTICES_3D) {
        FlushUnitMainSprites3D();
    }
}

void Renderer3D::FlushUnitMainSprites3D() {
    if (m_unitMainVertexCount3D == 0) return;
    
    IncrementDrawCall3D("unit_main_sprites");
    
    glDepthMask(GL_FALSE);
    
    glUseProgram(m_shader3DTexturedProgram);
    
    if (m_currentUnitMainTexture3D != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_currentUnitMainTexture3D);
    }
    
    int projLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uModelView");
    int texLoc = glGetUniformLocation(m_shader3DTexturedProgram, "ourTexture");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix3D.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix3D.m);
    glUniform1i(texLoc, 0);
    
    SetFogUniforms3D(m_shader3DTexturedProgram);
    
    glBindVertexArray(m_VAO3DTextured);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO3DTextured);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_unitMainVertexCount3D * sizeof(Vertex3DTextured), m_unitMainVertices3D);
    
    glDrawArrays(GL_TRIANGLES, 0, m_unitMainVertexCount3D);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    glDepthMask(GL_TRUE);
    
    m_unitMainVertexCount3D = 0;
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
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, effectiveTextureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    if (src->m_mipmapping) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
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

void Renderer3D::BeginUnitRotatingBatch3D() {
    m_unitRotatingVertexCount3D = 0;
    m_currentUnitRotatingTexture3D = 0;
}

void Renderer3D::EndUnitRotatingBatch3D() {
    if (m_unitRotatingVertexCount3D > 0) {
        FlushUnitRotating3D();
    }
}

void Renderer3D::FlushUnitRotating3DIfFull(int verticesNeeded) {
    if (m_unitRotatingVertexCount3D + verticesNeeded > MAX_UNIT_ROTATING_VERTICES_3D) {
        FlushUnitRotating3D();
    }
}

void Renderer3D::FlushUnitRotating3D() {
    if (m_unitRotatingVertexCount3D == 0) return;
    
    IncrementDrawCall3D("unit_rotating");
    
    glDepthMask(GL_FALSE);
    
    glUseProgram(m_shader3DTexturedProgram);
    
    if (m_currentUnitRotatingTexture3D != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_currentUnitRotatingTexture3D);
    }
    
    int projLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uModelView");
    int texLoc = glGetUniformLocation(m_shader3DTexturedProgram, "ourTexture");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix3D.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix3D.m);
    glUniform1i(texLoc, 0);
    
    SetFogUniforms3D(m_shader3DTexturedProgram);
    
    glBindVertexArray(m_VAO3DTextured);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO3DTextured);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_unitRotatingVertexCount3D * sizeof(Vertex3DTextured), m_unitRotatingVertices3D);
    
    glDrawArrays(GL_TRIANGLES, 0, m_unitRotatingVertexCount3D);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    glDepthMask(GL_TRUE);
    
    m_unitRotatingVertexCount3D = 0;
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
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, effectiveTextureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    if (stateSrc->m_mipmapping) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
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

void Renderer3D::BeginUnitStateBatch3D() {
    m_unitStateVertexCount3D = 0;
    m_currentUnitStateTexture3D = 0;
}

void Renderer3D::EndUnitStateBatch3D() {
    if (m_unitStateVertexCount3D > 0) {
        FlushUnitStateIcons3D();
    }
}

void Renderer3D::FlushUnitStateIcons3DIfFull(int verticesNeeded) {
    if (m_unitStateVertexCount3D + verticesNeeded > MAX_UNIT_STATE_VERTICES_3D) {
        FlushUnitStateIcons3D();
    }
}

void Renderer3D::FlushUnitStateIcons3D() {
    if (m_unitStateVertexCount3D == 0) return;
    
    IncrementDrawCall3D("unit_state_icons");
    
    glDepthMask(GL_FALSE);
    
    glUseProgram(m_shader3DTexturedProgram);
    
    int projLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uModelView");
    int texLoc = glGetUniformLocation(m_shader3DTexturedProgram, "ourTexture");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix3D.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix3D.m);
    glUniform1i(texLoc, 0);
    
    SetFogUniforms3D(m_shader3DTexturedProgram);
    
    glBindVertexArray(m_VAO3DTextured);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO3DTextured);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_unitStateVertexCount3D * sizeof(Vertex3DTextured), m_unitStateVertices3D);
    
    glDrawArrays(GL_TRIANGLES, 0, m_unitStateVertexCount3D);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    glDepthMask(GL_TRUE);
    
    m_unitStateVertexCount3D = 0;
}

//void Renderer3D::UnitCounterText3D(float x, float y, float z, Colour const &col, float size, const char *text) {
//    return 0; // placeholder for now, replay viewer doesnt even use this anyway
//}

void Renderer3D::BeginUnitCounterBatch3D() {
    m_unitCounterVertexCount3D = 0;
    m_currentUnitCounterTexture3D = 0;
}

void Renderer3D::EndUnitCounterBatch3D() {
    if (m_unitCounterVertexCount3D > 0) {
        FlushUnitCounters3D();
    }
}

void Renderer3D::FlushUnitCounters3D() {
    if (m_unitCounterVertexCount3D == 0) return;
    
    IncrementDrawCall3D("unit_counters");
    
    glUseProgram(m_shader3DTexturedProgram);
    
    int projLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uModelView");
    int texLoc = glGetUniformLocation(m_shader3DTexturedProgram, "ourTexture");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix3D.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix3D.m);
    glUniform1i(texLoc, 0);
    
    SetFogUniforms3D(m_shader3DTexturedProgram);
    
    glBindVertexArray(m_VAO3DTextured);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO3DTextured);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_unitCounterVertexCount3D * sizeof(Vertex3DTextured), m_unitCounterVertices3D);
    
    glDrawArrays(GL_TRIANGLES, 0, m_unitCounterVertexCount3D);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_unitCounterVertexCount3D = 0;
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
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, effectiveTextureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    if (nukeBmp->m_mipmapping) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    
    m_currentUnitNukeTexture3D = effectiveTextureID;
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    float u1, v1, u2, v2;
    
    // make smallnuke behave like old Blit() system by shrinking UV area 
    AtlasImage* atlasImage = dynamic_cast<AtlasImage*>(nukeBmp);
    if (atlasImage) {
        GetImageUVCoords(nukeBmp, u1, v1, u2, v2);
        
        // simulate 14x14 area within 16x16 atlas coordinates
        float shrinkX = (u2 - u1) * 0.0625f;  // 6.25% 
        u1 += shrinkX;
        u2 -= shrinkX;
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
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, effectiveTextureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    if (nukeBmp->m_mipmapping) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
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

void Renderer3D::BeginUnitNukeBatch3D() {
    m_unitNukeVertexCount3D = 0;
    m_currentUnitNukeTexture3D = 0;
}

void Renderer3D::EndUnitNukeBatch3D() {
    if (m_unitNukeVertexCount3D > 0) {
        FlushUnitNukeIcons3D();
    }
}

void Renderer3D::FlushUnitNukeIcons3DIfFull(int verticesNeeded) {
    if (m_unitNukeVertexCount3D + verticesNeeded > MAX_UNIT_NUKE_VERTICES_3D) {
        FlushUnitNukeIcons3D();
    }
}

void Renderer3D::FlushUnitNukeIcons3D() {
    if (m_unitNukeVertexCount3D == 0) return;
    
    IncrementDrawCall3D("unit_nuke_icons");
    
    glDepthMask(GL_FALSE);
    
    glUseProgram(m_shader3DTexturedProgram);
    
    int projLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uModelView");
    int texLoc = glGetUniformLocation(m_shader3DTexturedProgram, "ourTexture");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix3D.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix3D.m);
    glUniform1i(texLoc, 0);
    
    SetFogUniforms3D(m_shader3DTexturedProgram);
    
    glBindVertexArray(m_VAO3DTextured);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO3DTextured);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_unitNukeVertexCount3D * sizeof(Vertex3DTextured), m_unitNukeVertices3D);
    
    glDrawArrays(GL_TRIANGLES, 0, m_unitNukeVertexCount3D);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    glDepthMask(GL_TRUE);
    
    m_unitNukeVertexCount3D = 0;
}

void Renderer3D::EffectsLine3D(float x1, float y1, float z1, float x2, float y2, float z2, Colour const &col) {
    FlushEffectsLines3DIfFull(2);
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    m_effectsLineVertices3D[m_effectsLineVertexCount3D++] = {x1, y1, z1, r, g, b, a};
    m_effectsLineVertices3D[m_effectsLineVertexCount3D++] = {x2, y2, z2, r, g, b, a};
}

void Renderer3D::BeginEffectsLineBatch3D() {
    // Effects line segments will be batched until EndEffectsLineBatch3D()
}

void Renderer3D::EndEffectsLineBatch3D() {
    FlushEffectsLines3D();
}

void Renderer3D::FlushEffectsLines3DIfFull(int segmentsNeeded) {
    if (m_effectsLineVertexCount3D + segmentsNeeded > MAX_EFFECTS_LINE_VERTICES_3D) {
        FlushEffectsLines3D();
    }
}

void Renderer3D::FlushEffectsLines3D() {
    if (m_effectsLineVertexCount3D == 0) return;
    
    IncrementDrawCall3D("effects_lines");
    
    glUseProgram(m_shader3DProgram);
    
    int projLoc = glGetUniformLocation(m_shader3DProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_shader3DProgram, "uModelView");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix3D.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix3D.m);
    
    SetFogUniforms3D(m_shader3DProgram);
    
    glBindVertexArray(m_VAO3D);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO3D);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_effectsLineVertexCount3D * sizeof(Vertex3D), m_effectsLineVertices3D);
    
    glDrawArrays(GL_LINES, 0, m_effectsLineVertexCount3D);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_effectsLineVertexCount3D = 0;
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
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, effectiveTextureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    if (src->m_mipmapping) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
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

void Renderer3D::BeginEffectsSpriteBatch3D() {
    m_effectsSpriteVertexCount3D = 0;
    m_currentEffectsSpriteTexture3D = 0;
}

void Renderer3D::EndEffectsSpriteBatch3D() {
    if (m_effectsSpriteVertexCount3D > 0) {
        FlushEffectsSprites3D();
    }
}

void Renderer3D::FlushEffectsSprites3D() {
    if (m_effectsSpriteVertexCount3D == 0) return;
    
    IncrementDrawCall3D("effects_sprites");
    
    glDepthMask(GL_FALSE);
    
    glUseProgram(m_shader3DTexturedProgram);
    
    if (m_currentEffectsSpriteTexture3D != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_currentEffectsSpriteTexture3D);
    }
    
    int projLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uModelView");
    int texLoc = glGetUniformLocation(m_shader3DTexturedProgram, "ourTexture");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix3D.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix3D.m);
    glUniform1i(texLoc, 0);
    
    SetFogUniforms3D(m_shader3DTexturedProgram);
    
    glBindVertexArray(m_VAO3DTextured);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO3DTextured);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_effectsSpriteVertexCount3D * sizeof(Vertex3DTextured), m_effectsSpriteVertices3D);
    
    glDrawArrays(GL_TRIANGLES, 0, m_effectsSpriteVertexCount3D);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    glDepthMask(GL_TRUE);
    
    m_effectsSpriteVertexCount3D = 0;
}

// Helper function to set fog uniforms consistently
void Renderer3D::SetFogUniforms3D(unsigned int shaderProgram) {
    int fogEnabledLoc = glGetUniformLocation(shaderProgram, "uFogEnabled");
    int fogOrientationLoc = glGetUniformLocation(shaderProgram, "uFogOrientationBased");
    int fogStartLoc = glGetUniformLocation(shaderProgram, "uFogStart");
    int fogEndLoc = glGetUniformLocation(shaderProgram, "uFogEnd");
    int fogColorLoc = glGetUniformLocation(shaderProgram, "uFogColor");
    int cameraPosLoc = glGetUniformLocation(shaderProgram, "uCameraPos");
    
    glUniform1i(fogEnabledLoc, m_fogEnabled ? 1 : 0);
    glUniform1i(fogOrientationLoc, m_fogOrientationBased ? 1 : 0);
    glUniform1f(fogStartLoc, m_fogStart);
    glUniform1f(fogEndLoc, m_fogEnd);
    glUniform4f(fogColorLoc, m_fogColor[0], m_fogColor[1], m_fogColor[2], m_fogColor[3]);
    glUniform3f(cameraPosLoc, m_cameraPos[0], m_cameraPos[1], m_cameraPos[2]);
}

void Renderer3D::FlushAllSpecializedBuffers3D() {
    FlushUnitTrails3D();
    FlushUnitMainSprites3D();
    FlushUnitRotating3D();
    FlushUnitStateIcons3D();
    FlushUnitNukeIcons3D();
    FlushUnitHighlights3D();
    FlushUnitCounters3D();
    FlushEffectsLines3D();
    FlushEffectsSprites3D();
    FlushHealthBars3D();
    FlushTextBuffer3D();
    FlushNuke3DModels3D();
}

void Renderer3D::FlushAllUnitBuffers3D() {
    FlushUnitTrails3D();
    FlushUnitMainSprites3D();
    FlushUnitRotating3D();
    FlushUnitStateIcons3D();
    FlushUnitNukeIcons3D();
    FlushUnitHighlights3D();
    FlushUnitCounters3D();
}

void Renderer3D::FlushAllEffectBuffers3D() {
    FlushEffectsLines3D();
    FlushEffectsSprites3D();
}

void Renderer3D::UnitHighlight3D(Image *blurSrc, float x, float y, float z, float w, float h, Colour const &col) {
    if (m_unitHighlightVertexCount3D + 6 > MAX_UNIT_HIGHLIGHT_VERTICES_3D) {
        FlushUnitHighlights3D();
    }
    
    unsigned int effectiveTextureID = GetEffectiveTextureID(blurSrc);
    
    if (m_unitHighlightVertexCount3D > 0 && m_currentUnitHighlightTexture3D != effectiveTextureID) {
        FlushUnitHighlights3D();
    }
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, effectiveTextureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    if (blurSrc->m_mipmapping) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
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

void Renderer3D::BeginUnitHighlightBatch3D() {
    m_unitHighlightVertexCount3D = 0;
    m_currentUnitHighlightTexture3D = 0;
}

void Renderer3D::EndUnitHighlightBatch3D() {
    if (m_unitHighlightVertexCount3D > 0) {
        FlushUnitHighlights3D();
    }
}

void Renderer3D::FlushUnitHighlights3D() {
    if (m_unitHighlightVertexCount3D == 0) return;
    
    IncrementDrawCall3D("unit_highlights");
    
    glUseProgram(m_shader3DTexturedProgram);
    
    int projLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uModelView");
    int texLoc = glGetUniformLocation(m_shader3DTexturedProgram, "ourTexture");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix3D.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix3D.m);
    glUniform1i(texLoc, 0);
    
    SetFogUniforms3D(m_shader3DTexturedProgram);
    
    glBindVertexArray(m_VAO3DTextured);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO3DTextured);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_unitHighlightVertexCount3D * sizeof(Vertex3DTextured), m_unitHighlightVertices3D);
    
    glDrawArrays(GL_TRIANGLES, 0, m_unitHighlightVertexCount3D);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_unitHighlightVertexCount3D = 0;
}

void Renderer3D::BeginMapTextBatch3D() {
    BeginTextBatch3D();
}

void Renderer3D::EndMapTextBatch3D() {
    EndTextBatch3D();
}

// frame-level text batching for entire scene
void Renderer3D::BeginFrameTextBatch3D() {
    // reset text buffer for the new frame
    m_textVertexCount3D = 0;
    m_currentTextTexture3D = 0;
    BeginTextBatch3D();
}

void Renderer3D::EndFrameTextBatch3D() {
    // flush any remaining text at the end of the frame
    EndTextBatch3D();
}

// micro batching for the health bar, reduces draw calls by 800 when health bars are enabled
void Renderer3D::BeginHealthBarBatch3D() {
    m_healthBarVertexCount3D = 0;
}

void Renderer3D::HealthBarRect3D(float x, float y, float z, float w, float h, Colour const &col) {
    if (m_healthBarVertexCount3D + 6 > MAX_HEALTH_BAR_VERTICES_3D) {
        FlushHealthBars3D();
    }
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    // First triangle: TL, TR, BR
    m_healthBarVertices3D[m_healthBarVertexCount3D++] = {x - w/2, y + h/2, z, r, g, b, a, 0.0f, 0.0f};
    m_healthBarVertices3D[m_healthBarVertexCount3D++] = {x + w/2, y + h/2, z, r, g, b, a, 0.0f, 0.0f};
    m_healthBarVertices3D[m_healthBarVertexCount3D++] = {x + w/2, y - h/2, z, r, g, b, a, 0.0f, 0.0f};
    
    // Second triangle: TL, BR, BL
    m_healthBarVertices3D[m_healthBarVertexCount3D++] = {x - w/2, y + h/2, z, r, g, b, a, 0.0f, 0.0f};
    m_healthBarVertices3D[m_healthBarVertexCount3D++] = {x + w/2, y - h/2, z, r, g, b, a, 0.0f, 0.0f};
    m_healthBarVertices3D[m_healthBarVertexCount3D++] = {x - w/2, y - h/2, z, r, g, b, a, 0.0f, 0.0f};
}

void Renderer3D::EndHealthBarBatch3D() {
    if (m_healthBarVertexCount3D > 0) {
        FlushHealthBars3D();
    }
}

// flush the buffer
void Renderer3D::FlushHealthBars3D() {
    if (m_healthBarVertexCount3D == 0) return;
    
    IncrementDrawCall3D("health_bars");
    
    glUseProgram(m_shader3DProgram);
    
    int projLoc = glGetUniformLocation(m_shader3DProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_shader3DProgram, "uModelView");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix3D.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix3D.m);
    
    // Set fog uniforms
    SetFogUniforms3D(m_shader3DProgram);
    
    glBindVertexArray(m_VAO3D);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO3D);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_healthBarVertexCount3D * sizeof(Vertex3D), m_healthBarVertices3D);
    
    glDrawArrays(GL_TRIANGLES, 0, m_healthBarVertexCount3D);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_healthBarVertexCount3D = 0;
}

void Renderer3D::BeginTextBatch3D() {
    m_textVertexCount3D = 0;
    m_currentTextTexture3D = 0;
}

void Renderer3D::EndTextBatch3D() {
    FlushTextContext3D();
}

void Renderer3D::FlushTextContext3D() {
    FlushTextBuffer3D();
}

void Renderer3D::FlushTextBuffer3D() {
    if (m_textVertexCount3D == 0) return;
    
    IncrementDrawCall3D("text");
    
    glUseProgram(m_shader3DTexturedProgram);
    
    int projLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uModelView");
    int texLoc = glGetUniformLocation(m_shader3DTexturedProgram, "ourTexture");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix3D.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix3D.m);
    glUniform1i(texLoc, 0);
    
    // set fog 
    SetFogUniforms3D(m_shader3DTexturedProgram);
    
    // bind the current text texture
    if (m_currentTextTexture3D != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_currentTextTexture3D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    }
    
    glBindVertexArray(m_VAO3DTextured);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO3DTextured);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_textVertexCount3D * sizeof(Vertex3DTextured), m_textVertices3D);
    
    glDrawArrays(GL_TRIANGLES, 0, m_textVertexCount3D);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_textVertexCount3D = 0;
    m_currentTextTexture3D = 0;  // reset texture tracking
}

void Renderer3D::BeginNuke3DModelBatch3D() {
    m_nuke3DModelVertexCount3D = 0;
}

void Renderer3D::EndNuke3DModelBatch3D() {
    FlushNuke3DModels3D();
}

void Renderer3D::FlushNuke3DModels3D() {
    if (m_nuke3DModelVertexCount3D == 0) return;
    
    IncrementDrawCall3D("nuke_3d_models");
    
    glUseProgram(m_shader3DProgram);
    
    int projLoc = glGetUniformLocation(m_shader3DProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_shader3DProgram, "uModelView");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix3D.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix3D.m);
    
    SetFogUniforms3D(m_shader3DProgram);
    
    glBindVertexArray(m_VAO3D);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO3D);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_nuke3DModelVertexCount3D * sizeof(Vertex3D), m_nuke3DModelVertices3D);
    
    // enable face culling for proper 3D model rendering
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    
    // draw as triangles
    glDrawArrays(GL_TRIANGLES, 0, m_nuke3DModelVertexCount3D);
    
    glDisable(GL_CULL_FACE);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_nuke3DModelVertexCount3D = 0;
}

void Renderer3D::FlushNuke3DModels3DIfFull(int verticesNeeded) {
    if (m_nuke3DModelVertexCount3D + verticesNeeded >= MAX_NUKE_3D_MODEL_VERTICES_3D) {
        FlushNuke3DModels3D();
    }
}

void Renderer3D::FlushAllNuke3DModelBuffers3D() {
    FlushNuke3DModels3D();
} 