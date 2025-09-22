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
#include "lib/preferences.h"

#include "renderer/map_renderer.h"
#include "renderer.h"

extern Renderer *g_renderer;

// get UV coordinates for an image from the atlas
void Renderer::GetImageUVCoords(Image* image, float& u1, float& v1, float& u2, float& v2) {
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
unsigned int Renderer::GetEffectiveTextureID(Image* image) {
    AtlasImage* atlasImage = dynamic_cast<AtlasImage*>(image);
    if (atlasImage) {
        return atlasImage->GetAtlasTextureID();
    }
    return image->m_textureID;
}

void Renderer::FlushUITriangles() {
    if (m_uiTriangleVertexCount == 0) return;
    
    IncrementDrawCall("ui_triangles");
    
    glUseProgram(m_colorShaderProgram);
    
    int projLoc = glGetUniformLocation(m_colorShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_colorShaderProgram, "uModelView");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    
    
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_uiTriangleVertexCount * sizeof(Vertex2D), m_uiTriangleVertices);
        
    glDrawArrays(GL_TRIANGLES, 0, m_uiTriangleVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_uiTriangleVertexCount = 0;
}

void Renderer::FlushUILines() {
    if (m_uiLineVertexCount == 0) return;
    
    IncrementDrawCall("ui_lines");
    
    glUseProgram(m_colorShaderProgram);
    
    int projLoc = glGetUniformLocation(m_colorShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_colorShaderProgram, "uModelView");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_uiLineVertexCount * sizeof(Vertex2D), m_uiLineVertices);
           
    glDrawArrays(GL_LINES, 0, m_uiLineVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_uiLineVertexCount = 0;
}

void Renderer::BeginUIBatch() {
    m_allowImmedateFlush = false;
}

void Renderer::EndUIBatch() {
    FlushUIContext();
    m_allowImmedateFlush = true;
}

void Renderer::FlushUIContext() {
    FlushUITriangles();
    FlushUILines();
}

void Renderer::FlushTextBuffer() {
    if (m_textVertexCount == 0) return;
    
    IncrementDrawCall("text");
    
    // save current blend state
    int currentBlendSrc = m_blendSrcFactor;
    int currentBlendDst = m_blendDstFactor;
    
    // set correct blend mode for text rendering (additive for smooth fonts)
    if (m_blendMode != BlendModeSubtractive) {
        SetBlendFunc(GL_SRC_ALPHA, GL_ONE);
    }
    
    glUseProgram(m_textureShaderProgram);
    
    int projLoc = glGetUniformLocation(m_textureShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_textureShaderProgram, "uModelView");
    int texLoc = glGetUniformLocation(m_textureShaderProgram, "ourTexture");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    glUniform1i(texLoc, 0);
    
    // bind the current text texture
    if (m_currentTextTexture != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_currentTextTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    }
    
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_textVertexCount * sizeof(Vertex2D), m_textVertices);  
    
    glDrawArrays(GL_TRIANGLES, 0, m_textVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    // restore previous blend state
    SetBlendFunc(currentBlendSrc, currentBlendDst);
    
    m_textVertexCount = 0;
    m_currentTextTexture = 0;  // reset texture tracking
}

void Renderer::BeginTextBatch() {
    m_allowImmedateFlush = false;
}

void Renderer::EndTextBatch() {
    FlushTextContext();
    m_allowImmedateFlush = true;
}

void Renderer::FlushTextContext() {
    FlushTextBuffer();
}

void Renderer::FlushSpriteBuffer() {
    if (m_spriteVertexCount == 0) return;
    
    IncrementDrawCall("sprites");
    
    glUseProgram(m_textureShaderProgram);
    
    int projLoc = glGetUniformLocation(m_textureShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_textureShaderProgram, "uModelView");
    int texLoc = glGetUniformLocation(m_textureShaderProgram, "ourTexture");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    glUniform1i(texLoc, 0);
    
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_spriteVertexCount * sizeof(Vertex2D), m_spriteVertices);
        
    glDrawArrays(GL_TRIANGLES, 0, m_spriteVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_spriteVertexCount = 0;
}

void Renderer::BeginSpriteBatch() {
    m_allowImmedateFlush = false;
}

void Renderer::EndSpriteBatch() {
    FlushSpriteContext();
    m_allowImmedateFlush = true;
}

void Renderer::FlushSpriteContext() {
    FlushSpriteBuffer();
}

void Renderer::UnitTrailLine(float x1, float y1, float x2, float y2, Colour const &col) {
    FlushUnitTrailsIfFull(2);
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    m_unitTrailVertices[m_unitTrailVertexCount++] = {x1, y1, r, g, b, a, 0.0f, 0.0f};
    m_unitTrailVertices[m_unitTrailVertexCount++] = {x2, y2, r, g, b, a, 0.0f, 0.0f};
}

void Renderer::BeginUnitTrailBatch() {
    // trail segments will be batched until EndUnitTrailBatch()
}

void Renderer::EndUnitTrailBatch() {
    FlushUnitTrails();
}

void Renderer::FlushUnitTrailsIfFull(int segmentsNeeded) {
    if (m_unitTrailVertexCount + segmentsNeeded > MAX_UNIT_TRAIL_VERTICES) {
        FlushUnitTrails();
    }
}

void Renderer::FlushUnitTrails() {
    if (m_unitTrailVertexCount == 0) return;
    
    IncrementDrawCall("unit_trails");
    
// im not sure where the linewidth was defined before the 
// refactor but here seems like a good place to set line width
#ifndef TARGET_EMSCRIPTEN
    glLineWidth(g_preferences->GetFloat(PREFS_GRAPHICS_UNIT_TRAIL_THICKNESS)); 
#endif
    
    glUseProgram(m_colorShaderProgram);
    
    int projLoc = glGetUniformLocation(m_colorShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_colorShaderProgram, "uModelView");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_unitTrailVertexCount * sizeof(Vertex2D), m_unitTrailVertices); 
    
    glDrawArrays(GL_LINES, 0, m_unitTrailVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_unitTrailVertexCount = 0;
}

void Renderer::UnitMainSprite(Image *src, float x, float y, float w, float h, Colour const &col) {
    FlushUnitMainSpritesIfFull(6);
    
    unsigned int effectiveTextureID = GetEffectiveTextureID(src);
    
    if (m_unitMainVertexCount > 0 && m_currentUnitMainTexture != effectiveTextureID) {
        FlushUnitMainSprites();
    }
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, effectiveTextureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    if (src->m_mipmapping) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
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

void Renderer::BeginUnitMainBatch() {
    m_unitMainVertexCount = 0;
    m_currentUnitMainTexture = 0;
}

void Renderer::EndUnitMainBatch() {
    if (m_unitMainVertexCount > 0) {
        FlushUnitMainSprites();
    }
}

void Renderer::FlushUnitMainSpritesIfFull(int verticesNeeded) {
    if (m_unitMainVertexCount + verticesNeeded > MAX_UNIT_MAIN_VERTICES) {
        FlushUnitMainSprites();
    }
}

void Renderer::FlushUnitMainSprites() {
    if (m_unitMainVertexCount == 0) return;
    
    IncrementDrawCall("unit_main_sprites");
    
    glUseProgram(m_textureShaderProgram);
    
    if (m_currentUnitMainTexture != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_currentUnitMainTexture);
    }
    
    int projLoc = glGetUniformLocation(m_textureShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_textureShaderProgram, "uModelView");
    int texLoc = glGetUniformLocation(m_textureShaderProgram, "ourTexture");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    glUniform1i(texLoc, 0);
    
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    
    
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_unitMainVertexCount * sizeof(Vertex2D), m_unitMainVertices);   
    
    glDrawArrays(GL_TRIANGLES, 0, m_unitMainVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_unitMainVertexCount = 0;
}

void Renderer::UnitRotating(Image *src, float x, float y, float w, float h, Colour const &col, float angle) {
    FlushUnitRotatingIfFull(6);
    
    unsigned int effectiveTextureID = GetEffectiveTextureID(src);
    
    // only flush if texture changes
    if (m_unitRotatingVertexCount > 0 && m_currentUnitRotatingTexture != effectiveTextureID) {
        FlushUnitRotating();
    }
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, effectiveTextureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    if (src->m_mipmapping) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
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

void Renderer::BeginUnitRotatingBatch() {
    m_unitRotatingVertexCount = 0;
    m_currentUnitRotatingTexture = 0;
}

void Renderer::EndUnitRotatingBatch() {
    if (m_unitRotatingVertexCount > 0) {
        FlushUnitRotating();
    }
}

void Renderer::FlushUnitRotatingIfFull(int verticesNeeded) {
    if (m_unitRotatingVertexCount + verticesNeeded > MAX_UNIT_ROTATING_VERTICES) {
        FlushUnitRotating();
    }
}

void Renderer::FlushUnitRotating() {
    if (m_unitRotatingVertexCount == 0) return;
    
    IncrementDrawCall("unit_rotating");
    
    glUseProgram(m_textureShaderProgram);
    
    if (m_currentUnitRotatingTexture != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_currentUnitRotatingTexture);
    }
    
    int projLoc = glGetUniformLocation(m_textureShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_textureShaderProgram, "uModelView");
    int texLoc = glGetUniformLocation(m_textureShaderProgram, "ourTexture");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    glUniform1i(texLoc, 0);
    
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_unitRotatingVertexCount * sizeof(Vertex2D), m_unitRotatingVertices);
    
    glDrawArrays(GL_TRIANGLES, 0, m_unitRotatingVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_unitRotatingVertexCount = 0;
}

void Renderer::UnitStateIcon(Image *stateSrc, float x, float y, float w, float h, Colour const &col) {
    FlushUnitStateIconsIfFull(6);
    
    unsigned int effectiveTextureID = GetEffectiveTextureID(stateSrc);
    
    if (m_unitStateVertexCount > 0 && m_currentUnitStateTexture != effectiveTextureID) {
        FlushUnitStateIcons();
    }
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, effectiveTextureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    if (stateSrc->m_mipmapping) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
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

void Renderer::BeginUnitStateBatch() {
    m_unitStateVertexCount = 0;
    m_currentUnitStateTexture = 0;
}

void Renderer::EndUnitStateBatch() {
    if (m_unitStateVertexCount > 0) {
        FlushUnitStateIcons();
    }
}

void Renderer::FlushUnitStateIconsIfFull(int verticesNeeded) {
    if (m_unitStateVertexCount + verticesNeeded > MAX_UNIT_STATE_VERTICES) {
        FlushUnitStateIcons();
    }
}

void Renderer::FlushUnitStateIcons() {
    if (m_unitStateVertexCount == 0) return;
    
    IncrementDrawCall("unit_state_icons");
    
    glUseProgram(m_textureShaderProgram);

    if (m_currentUnitStateTexture != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_currentUnitStateTexture);
    }
    
    int projLoc = glGetUniformLocation(m_textureShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_textureShaderProgram, "uModelView");
    int texLoc = glGetUniformLocation(m_textureShaderProgram, "ourTexture");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    glUniform1i(texLoc, 0);
    
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_unitStateVertexCount * sizeof(Vertex2D), m_unitStateVertices); 
        
    glDrawArrays(GL_TRIANGLES, 0, m_unitStateVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_unitStateVertexCount = 0;
}

void Renderer::UnitCounterText(float x, float y, Colour const &col, float size, const char *text) {
    // Use the font system to render unit counter text
    BitmapFont *font = GetBitmapFont();
    if (!font) return;
    
    // Set font properties for unit counters
    font->SetHoriztonalFlip(false);
    font->SetFixedWidth(false);
    
    // Render text using optimized font system
    if (m_blendMode != BlendModeSubtractive) {
        int blendSrc = m_blendSrcFactor, blendDst = m_blendDstFactor;
        SetBlendFunc(GL_SRC_ALPHA, GL_ONE);
        font->DrawText2DSimple(x, y, size, text, col);
        SetBlendFunc(blendSrc, blendDst);
    } else {
        font->DrawText2DSimple(x, y, size, text, col);
    }
}

void Renderer::BeginUnitCounterBatch() {
    m_unitCounterVertexCount = 0;
    m_currentUnitCounterTexture = 0;
}

void Renderer::EndUnitCounterBatch() {
    if (m_unitCounterVertexCount > 0) {
        FlushUnitCounters();
    }
}

void Renderer::FlushUnitCounters() {
    if (m_unitCounterVertexCount == 0) return;
    
    IncrementDrawCall("unit_counters");
    
    glUseProgram(m_textureShaderProgram);
    
    int projLoc = glGetUniformLocation(m_textureShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_textureShaderProgram, "uModelView");
    int texLoc = glGetUniformLocation(m_textureShaderProgram, "ourTexture");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    glUniform1i(texLoc, 0);
    
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_unitCounterVertexCount * sizeof(Vertex2D), m_unitCounterVertices);
    
    glDrawArrays(GL_TRIANGLES, 0, m_unitCounterVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_unitCounterVertexCount = 0;
}


void Renderer::UnitNukeIcon(float x, float y, float w, float h, Colour const &col) {
    FlushUnitNukeIconsIfFull(6);
    
    Image *nukeBmp = g_resource->GetImage("graphics/smallnuke.bmp");
    if (!nukeBmp) return;
    
    unsigned int effectiveTextureID = GetEffectiveTextureID(nukeBmp);

    if (m_unitNukeVertexCount > 0 && m_currentUnitNukeTexture != effectiveTextureID) {
        FlushUnitNukeIcons();
    }
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, effectiveTextureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    if (nukeBmp->m_mipmapping) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    
    m_currentUnitNukeTexture = effectiveTextureID;
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    float u1, v1, u2, v2;
    
    // make smallnuke behave like old Blit() system by shrinking UV area 
    // to sample just the core nuke sprite, then stretch it to fill the entire render area
    // this is a hacky workaround but i dont care it works. changing the atlas cords caused texture
    // bleeding and positioning issues
    AtlasImage* atlasImage = dynamic_cast<AtlasImage*>(nukeBmp);
    if (atlasImage) {
        GetImageUVCoords(nukeBmp, u1, v1, u2, v2);
        
        // simulate 14x14 area within 16x16 atlas coordinates
        // 14 x 14 simulates what the old smallnuke looks like
        float shrinkX = (u2 - u1) * 0.0625f;  // 6.25% 
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
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, effectiveTextureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    if (nukeBmp->m_mipmapping) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
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

void Renderer::BeginUnitNukeBatch() {
    m_unitNukeVertexCount = 0;
    m_currentUnitNukeTexture = 0;
}

void Renderer::EndUnitNukeBatch() {
    if (m_unitNukeVertexCount > 0) {
        FlushUnitNukeIcons();
    }
}

void Renderer::FlushUnitNukeIconsIfFull(int verticesNeeded) {
    if (m_unitNukeVertexCount + verticesNeeded > MAX_UNIT_NUKE_VERTICES) {
        FlushUnitNukeIcons();
    }
}

void Renderer::FlushUnitNukeIcons() {
    if (m_unitNukeVertexCount == 0) return;
    
    IncrementDrawCall("unit_nuke_icons");
    
    glUseProgram(m_textureShaderProgram);

    if (m_currentUnitNukeTexture != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_currentUnitNukeTexture);
    }
    
    int projLoc = glGetUniformLocation(m_textureShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_textureShaderProgram, "uModelView");
    int texLoc = glGetUniformLocation(m_textureShaderProgram, "ourTexture");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    glUniform1i(texLoc, 0);
    
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_unitNukeVertexCount * sizeof(Vertex2D), m_unitNukeVertices);
    
    glDrawArrays(GL_TRIANGLES, 0, m_unitNukeVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_unitNukeVertexCount = 0;
}

void Renderer::EffectsLine(float x1, float y1, float x2, float y2, Colour const &col) {
    FlushEffectsLinesIfFull(2);
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    m_effectsLineVertices[m_effectsLineVertexCount++] = {x1, y1, r, g, b, a, 0.0f, 0.0f};
    m_effectsLineVertices[m_effectsLineVertexCount++] = {x2, y2, r, g, b, a, 0.0f, 0.0f};
}

void Renderer::BeginEffectsLineBatch() {
    // Effects line segments will be batched until EndEffectsLineBatch()
}

void Renderer::EndEffectsLineBatch() {
    FlushEffectsLines();
}

void Renderer::FlushEffectsLinesIfFull(int segmentsNeeded) {
    if (m_effectsLineVertexCount + segmentsNeeded > MAX_EFFECTS_LINE_VERTICES) {
        FlushEffectsLines();
    }
}

void Renderer::FlushEffectsLines() {
    if (m_effectsLineVertexCount == 0) return;
    
    IncrementDrawCall("effects_lines");

// im not sure where the linewidth was defined before the 
// refactor but here seems like a good place to set line width
#ifndef TARGET_EMSCRIPTEN
    glLineWidth(1.5f);
#endif
    
    glUseProgram(m_colorShaderProgram);
    
    int projLoc = glGetUniformLocation(m_colorShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_colorShaderProgram, "uModelView");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_effectsLineVertexCount * sizeof(Vertex2D), m_effectsLineVertices);
    
    glDrawArrays(GL_LINES, 0, m_effectsLineVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_effectsLineVertexCount = 0;
}

void Renderer::EffectsSprite(Image *src, float x, float y, float w, float h, Colour const &col) {
    if (m_effectsSpriteVertexCount + 6 > MAX_EFFECTS_SPRITE_VERTICES) {
        FlushEffectsSprites();
    }
    
    unsigned int effectiveTextureID = GetEffectiveTextureID(src);

    if (m_effectsSpriteVertexCount > 0 && m_currentEffectsSpriteTexture != effectiveTextureID) {
        FlushEffectsSprites();
    }
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, effectiveTextureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    if (src->m_mipmapping) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
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

void Renderer::BeginEffectsSpriteBatch() {
    m_effectsSpriteVertexCount = 0;
    m_currentEffectsSpriteTexture = 0;
}

void Renderer::EndEffectsSpriteBatch() {
    if (m_effectsSpriteVertexCount > 0) {
        FlushEffectsSprites();
    }
}

void Renderer::FlushEffectsSprites() {
    if (m_effectsSpriteVertexCount == 0) return;
    
    IncrementDrawCall("effects_sprites");
    
    glUseProgram(m_textureShaderProgram);
    
    if (m_currentEffectsSpriteTexture != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_currentEffectsSpriteTexture);
    }
    
    int projLoc = glGetUniformLocation(m_textureShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_textureShaderProgram, "uModelView");
    int texLoc = glGetUniformLocation(m_textureShaderProgram, "ourTexture");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    glUniform1i(texLoc, 0);
    
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_effectsSpriteVertexCount * sizeof(Vertex2D), m_effectsSpriteVertices);
    
    glDrawArrays(GL_TRIANGLES, 0, m_effectsSpriteVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_effectsSpriteVertexCount = 0;
}

void Renderer::FlushAllSpecializedBuffers() {
    // Core specialized buffers
    FlushUITriangles();
    FlushUILines();
    FlushTextBuffer();
    FlushSpriteBuffer();
    
    // Unit rendering specialized buffers
    FlushUnitTrails();
    FlushUnitMainSprites();
    FlushUnitRotating();
    FlushUnitHighlights();
    FlushUnitStateIcons();
    FlushUnitCounters();
    FlushUnitNukeIcons();
    FlushHealthBars();
    
    // Effect rendering specialized buffers
    FlushEffectsLines();
    FlushEffectsSprites();
}

void Renderer::FlushAllUnitBuffers() {
    FlushUnitTrails();
    FlushUnitMainSprites();
    FlushUnitRotating();
    FlushUnitHighlights();
    FlushUnitStateIcons();
    FlushUnitCounters();
    FlushUnitNukeIcons();
    FlushHealthBars();
}

void Renderer::FlushAllEffectBuffers() {
    FlushEffectsLines();
    FlushEffectsSprites();
}

void Renderer::UnitHighlight(Image *blurSrc, float x, float y, float w, float h, Colour const &col) {
    if (m_unitHighlightVertexCount + 6 > MAX_UNIT_HIGHLIGHT_VERTICES) {
        FlushUnitHighlights();
    }
    
    unsigned int effectiveTextureID = GetEffectiveTextureID(blurSrc);
    
    if (m_unitHighlightVertexCount > 0 && m_currentUnitHighlightTexture != effectiveTextureID) {
        FlushUnitHighlights();
    }
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, effectiveTextureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    if (blurSrc->m_mipmapping) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
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

void Renderer::BeginUnitHighlightBatch() {
    m_unitHighlightVertexCount = 0;
    m_currentUnitHighlightTexture = 0;
}

void Renderer::EndUnitHighlightBatch() {
    if (m_unitHighlightVertexCount > 0) {
        FlushUnitHighlights();
    }
}

void Renderer::FlushUnitHighlights() {
    if (m_unitHighlightVertexCount == 0) return;
    
    IncrementDrawCall("unit_highlights");
    
    glUseProgram(m_textureShaderProgram);
    
    int projLoc = glGetUniformLocation(m_textureShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_textureShaderProgram, "uModelView");
    int texLoc = glGetUniformLocation(m_textureShaderProgram, "ourTexture");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    glUniform1i(texLoc, 0);
    
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_unitHighlightVertexCount * sizeof(Vertex2D), m_unitHighlightVertices);
 
    glDrawArrays(GL_TRIANGLES, 0, m_unitHighlightVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_unitHighlightVertexCount = 0;
}

// map-specific text batching methods - convenience wrappers
void Renderer::BeginMapTextBatch() {
    BeginTextBatch();
}

void Renderer::EndMapTextBatch() {
    EndTextBatch();
}

// frame-level text batching for entire scene
void Renderer::BeginFrameTextBatch() {
    // reset text buffer for the new frame
    m_textVertexCount = 0;
    m_currentTextTexture = 0;
    BeginTextBatch();
}

void Renderer::EndFrameTextBatch() {
    // flush any remaining text at the end of the frame
    EndTextBatch();
}

// micro batching for the health bar, reduces draw calls by 800 when health bars are enabled
void Renderer::BeginHealthBarBatch() {
    m_healthBarVertexCount = 0;
}

void Renderer::HealthBarRect(float x, float y, float w, float h, Colour const &col) {
    if (m_healthBarVertexCount + 6 > MAX_HEALTH_BAR_VERTICES) {
        FlushHealthBars();
    }
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    // First triangle: TL, TR, BR
    m_healthBarVertices[m_healthBarVertexCount++] = {x, y, r, g, b, a, 0.0f, 0.0f};
    m_healthBarVertices[m_healthBarVertexCount++] = {x + w, y, r, g, b, a, 0.0f, 0.0f};
    m_healthBarVertices[m_healthBarVertexCount++] = {x + w, y + h, r, g, b, a, 0.0f, 0.0f};
    
    // Second triangle: TL, BR, BL
    m_healthBarVertices[m_healthBarVertexCount++] = {x, y, r, g, b, a, 0.0f, 0.0f};
    m_healthBarVertices[m_healthBarVertexCount++] = {x + w, y + h, r, g, b, a, 0.0f, 0.0f};
    m_healthBarVertices[m_healthBarVertexCount++] = {x, y + h, r, g, b, a, 0.0f, 0.0f};
}

void Renderer::EndHealthBarBatch() {
    if (m_healthBarVertexCount > 0) {
        FlushHealthBars();
    }
}

// flush the buffer
void Renderer::FlushHealthBars() {
    if (m_healthBarVertexCount == 0) return;
    
    IncrementDrawCall("health_bars");
    
    glUseProgram(m_colorShaderProgram);
    
    int projLoc = glGetUniformLocation(m_colorShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_colorShaderProgram, "uModelView");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_healthBarVertexCount * sizeof(Vertex2D), m_healthBarVertices);
    
    glDrawArrays(GL_TRIANGLES, 0, m_healthBarVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_healthBarVertexCount = 0;
}