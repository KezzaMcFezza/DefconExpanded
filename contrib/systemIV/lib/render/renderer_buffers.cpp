#include "lib/universal_include.h"

#include <stdarg.h>

#include "lib/debug_utils.h"
#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/resource/bitmapfont.h"
#include "lib/math/vector3.h"
#include "lib/math/math_utils.h"
#include "lib/string_utils.h"

#include "renderer.h"
#include "colour.h"

// External globals
extern Renderer *g_renderer;

// ============================================================================
// SPECIALIZED BUFFER FLUSH METHODS - UI RENDERING
// ============================================================================

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

// ============================================================================
// SPECIALIZED BUFFER FLUSH METHODS - TEXT RENDERING
// ============================================================================

void Renderer::FlushTextBuffer() {
    if (m_textVertexCount == 0) return;
    
    IncrementDrawCall("text");
    
    glUseProgram(m_textureShaderProgram);
    
    int projLoc = glGetUniformLocation(m_textureShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_textureShaderProgram, "uModelView");
    int texLoc = glGetUniformLocation(m_textureShaderProgram, "ourTexture");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    glUniform1i(texLoc, 0);
    
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_textVertexCount * sizeof(Vertex2D), m_textVertices);
    
    glDrawArrays(GL_TRIANGLES, 0, m_textVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_textVertexCount = 0;
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

// ============================================================================
// SPECIALIZED BUFFER FLUSH METHODS - SPRITE RENDERING
// ============================================================================

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

// ============================================================================
// UNIT RENDERING SYSTEM - TRAIL RENDERING
// ============================================================================

void Renderer::UnitTrailLine(float x1, float y1, float x2, float y2, Colour const &col) {
    FlushUnitTrailsIfFull(2);
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    m_unitTrailVertices[m_unitTrailVertexCount++] = {x1, y1, r, g, b, a, 0.0f, 0.0f};
    m_unitTrailVertices[m_unitTrailVertexCount++] = {x2, y2, r, g, b, a, 0.0f, 0.0f};
}

void Renderer::BeginUnitTrailBatch() {
    // Trail segments will be batched until EndUnitTrailBatch()
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

// ============================================================================
// UNIT RENDERING SYSTEM - MAIN SPRITE RENDERING
// ============================================================================

void Renderer::UnitMainSprite(Image *src, float x, float y, float w, float h, Colour const &col) {
    FlushUnitMainSpritesIfFull(6);
    
    // Smart flush only when texture changes for batching
    if (m_unitMainVertexCount > 0 && m_currentUnitMainTexture != src->m_textureID) {
        FlushUnitMainSprites();
    }
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, src->m_textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    if (src->m_mipmapping) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    
    m_currentUnitMainTexture = src->m_textureID;
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    float onePixelW = 1.0f / (float) src->Width();
    float onePixelH = 1.0f / (float) src->Height();
    
    // First triangle: TL, TR, BR
    m_unitMainVertices[m_unitMainVertexCount++] = {x, y, r, g, b, a, onePixelW, 1.0f - onePixelH};
    m_unitMainVertices[m_unitMainVertexCount++] = {x + w, y, r, g, b, a, 1.0f - onePixelW, 1.0f - onePixelH};
    m_unitMainVertices[m_unitMainVertexCount++] = {x + w, y + h, r, g, b, a, 1.0f - onePixelW, onePixelH};
    
    // Second triangle: TL, BR, BL
    m_unitMainVertices[m_unitMainVertexCount++] = {x, y, r, g, b, a, onePixelW, 1.0f - onePixelH};
    m_unitMainVertices[m_unitMainVertexCount++] = {x + w, y + h, r, g, b, a, 1.0f - onePixelW, onePixelH};
    m_unitMainVertices[m_unitMainVertexCount++] = {x, y + h, r, g, b, a, onePixelW, onePixelH};
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

// ============================================================================
// UNIT RENDERING SYSTEM - ROTATING SPRITE RENDERING
// ============================================================================

void Renderer::UnitRotating(Image *src, float x, float y, float w, float h, Colour const &col, float angle) {
    FlushUnitRotatingIfFull(6);
    
    // Smart flush only when texture changes for batching
    if (m_unitRotatingVertexCount > 0 && m_currentUnitRotatingTexture != src->m_textureID) {
        FlushUnitRotating();
    }
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, src->m_textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    if (src->m_mipmapping) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    
    m_currentUnitRotatingTexture = src->m_textureID;
    
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
    float onePixelW = 1.0f / (float) src->Width();
    float onePixelH = 1.0f / (float) src->Height();
    
    // First triangle: vert1, vert2, vert3
    m_unitRotatingVertices[m_unitRotatingVertexCount++] = {vert1.x, vert1.y, r, g, b, a, onePixelW, 1.0f - onePixelH};
    m_unitRotatingVertices[m_unitRotatingVertexCount++] = {vert2.x, vert2.y, r, g, b, a, 1.0f - onePixelW, 1.0f - onePixelH};
    m_unitRotatingVertices[m_unitRotatingVertexCount++] = {vert3.x, vert3.y, r, g, b, a, 1.0f - onePixelW, onePixelH};
    
    // Second triangle: vert1, vert3, vert4
    m_unitRotatingVertices[m_unitRotatingVertexCount++] = {vert1.x, vert1.y, r, g, b, a, onePixelW, 1.0f - onePixelH};
    m_unitRotatingVertices[m_unitRotatingVertexCount++] = {vert3.x, vert3.y, r, g, b, a, 1.0f - onePixelW, onePixelH};
    m_unitRotatingVertices[m_unitRotatingVertexCount++] = {vert4.x, vert4.y, r, g, b, a, onePixelW, onePixelH};
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

// ============================================================================
// UNIT RENDERING SYSTEM - STATE ICON RENDERING
// ============================================================================

void Renderer::UnitStateIcon(Image *stateSrc, float x, float y, float w, float h, Colour const &col) {
    FlushUnitStateIconsIfFull(6);
    
    // Smart flush only when texture changes for batching
    if (m_unitStateVertexCount > 0 && m_currentUnitStateTexture != stateSrc->m_textureID) {
        FlushUnitStateIcons();
    }
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, stateSrc->m_textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    if (stateSrc->m_mipmapping) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    
    m_currentUnitStateTexture = stateSrc->m_textureID;
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    float onePixelW = 1.0f / (float) stateSrc->Width();
    float onePixelH = 1.0f / (float) stateSrc->Height();
    
    // First triangle: TL, TR, BR
    m_unitStateVertices[m_unitStateVertexCount++] = {x, y, r, g, b, a, onePixelW, 1.0f - onePixelH};
    m_unitStateVertices[m_unitStateVertexCount++] = {x + w, y, r, g, b, a, 1.0f - onePixelW, 1.0f - onePixelH};
    m_unitStateVertices[m_unitStateVertexCount++] = {x + w, y + h, r, g, b, a, 1.0f - onePixelW, onePixelH};
    
    // Second triangle: TL, BR, BL
    m_unitStateVertices[m_unitStateVertexCount++] = {x, y, r, g, b, a, onePixelW, 1.0f - onePixelH};
    m_unitStateVertices[m_unitStateVertexCount++] = {x + w, y + h, r, g, b, a, 1.0f - onePixelW, onePixelH};
    m_unitStateVertices[m_unitStateVertexCount++] = {x, y + h, r, g, b, a, onePixelW, onePixelH};
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

// ============================================================================
// UNIT RENDERING SYSTEM - COUNTER TEXT RENDERING
// ============================================================================

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

// ============================================================================
// UNIT RENDERING SYSTEM - NUKE ICON RENDERING
// ============================================================================

void Renderer::UnitNukeIcon(float x, float y, float w, float h, Colour const &col) {
    FlushUnitNukeIconsIfFull(6);
    
    Image *nukeBmp = g_resource->GetImage("graphics/smallnuke.bmp");
    if (!nukeBmp) return;
    
    // Smart flush only when texture changes for batching
    if (m_unitNukeVertexCount > 0 && m_currentUnitNukeTexture != nukeBmp->m_textureID) {
        FlushUnitNukeIcons();
    }
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, nukeBmp->m_textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    if (nukeBmp->m_mipmapping) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    
    m_currentUnitNukeTexture = nukeBmp->m_textureID;
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    float onePixelW = 1.0f / (float) nukeBmp->Width();
    float onePixelH = 1.0f / (float) nukeBmp->Height();
    
    // First triangle: TL, TR, BR
    m_unitNukeVertices[m_unitNukeVertexCount++] = {x, y, r, g, b, a, onePixelW, 1.0f - onePixelH};
    m_unitNukeVertices[m_unitNukeVertexCount++] = {x + w, y, r, g, b, a, 1.0f - onePixelW, 1.0f - onePixelH};
    m_unitNukeVertices[m_unitNukeVertexCount++] = {x + w, y + h, r, g, b, a, 1.0f - onePixelW, onePixelH};
    
    // Second triangle: TL, BR, BL
    m_unitNukeVertices[m_unitNukeVertexCount++] = {x, y, r, g, b, a, onePixelW, 1.0f - onePixelH};
    m_unitNukeVertices[m_unitNukeVertexCount++] = {x + w, y + h, r, g, b, a, 1.0f - onePixelW, onePixelH};
    m_unitNukeVertices[m_unitNukeVertexCount++] = {x, y + h, r, g, b, a, onePixelW, onePixelH};
}

void Renderer::UnitNukeIcon(float x, float y, float w, float h, Colour const &col, float angle) {
    FlushUnitNukeIconsIfFull(6);
    
    Image *nukeBmp = g_resource->GetImage("graphics/smallnuke.bmp");
    if (!nukeBmp) return;
    
    // Smart flush only when texture changes for batching
    if (m_unitNukeVertexCount > 0 && m_currentUnitNukeTexture != nukeBmp->m_textureID) {
        FlushUnitNukeIcons();
    }
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, nukeBmp->m_textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    if (nukeBmp->m_mipmapping) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    
    m_currentUnitNukeTexture = nukeBmp->m_textureID;
    
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
    float onePixelW = 1.0f / (float) nukeBmp->Width();
    float onePixelH = 1.0f / (float) nukeBmp->Height();
    
    // First triangle: vert1, vert2, vert3
    m_unitNukeVertices[m_unitNukeVertexCount++] = {vert1.x, vert1.y, r, g, b, a, onePixelW, 1.0f - onePixelH};
    m_unitNukeVertices[m_unitNukeVertexCount++] = {vert2.x, vert2.y, r, g, b, a, 1.0f - onePixelW, 1.0f - onePixelH};
    m_unitNukeVertices[m_unitNukeVertexCount++] = {vert3.x, vert3.y, r, g, b, a, 1.0f - onePixelW, onePixelH};
    
    // Second triangle: vert1, vert3, vert4
    m_unitNukeVertices[m_unitNukeVertexCount++] = {vert1.x, vert1.y, r, g, b, a, onePixelW, 1.0f - onePixelH};
    m_unitNukeVertices[m_unitNukeVertexCount++] = {vert3.x, vert3.y, r, g, b, a, 1.0f - onePixelW, onePixelH};
    m_unitNukeVertices[m_unitNukeVertexCount++] = {vert4.x, vert4.y, r, g, b, a, onePixelW, onePixelH};
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

// ============================================================================
// EFFECTS RENDERING SYSTEM - LINE RENDERING
// ============================================================================

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

// ============================================================================
// EFFECTS RENDERING SYSTEM - SPRITE RENDERING
// ============================================================================

void Renderer::EffectsSprite(Image *src, float x, float y, float w, float h, Colour const &col) {
    if (m_effectsSpriteVertexCount + 6 > MAX_EFFECTS_SPRITE_VERTICES) {
        FlushEffectsSprites();
    }
    
    // Smart flush only when texture changes for batching
    if (m_effectsSpriteVertexCount > 0 && m_currentEffectsSpriteTexture != src->m_textureID) {
        FlushEffectsSprites();
    }
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, src->m_textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    if (src->m_mipmapping) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    
    m_currentEffectsSpriteTexture = src->m_textureID;
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    float onePixelW = 1.0f / (float) src->Width();
    float onePixelH = 1.0f / (float) src->Height();
    
    // First triangle: TL, TR, BR
    m_effectsSpriteVertices[m_effectsSpriteVertexCount++] = {x, y, r, g, b, a, onePixelW, 1.0f - onePixelH};
    m_effectsSpriteVertices[m_effectsSpriteVertexCount++] = {x + w, y, r, g, b, a, 1.0f - onePixelW, 1.0f - onePixelH};
    m_effectsSpriteVertices[m_effectsSpriteVertexCount++] = {x + w, y + h, r, g, b, a, 1.0f - onePixelW, onePixelH};
    
    // Second triangle: TL, BR, BL
    m_effectsSpriteVertices[m_effectsSpriteVertexCount++] = {x, y, r, g, b, a, onePixelW, 1.0f - onePixelH};
    m_effectsSpriteVertices[m_effectsSpriteVertexCount++] = {x + w, y + h, r, g, b, a, 1.0f - onePixelW, onePixelH};
    m_effectsSpriteVertices[m_effectsSpriteVertexCount++] = {x, y + h, r, g, b, a, onePixelW, onePixelH};
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

// ============================================================================
// COMPLETE BUFFER SYSTEM MANAGEMENT
// ============================================================================

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
}

void Renderer::FlushAllEffectBuffers() {
    FlushEffectsLines();
    FlushEffectsSprites();
}

// ============================================================================
// UNIT HIGHLIGHT RENDERING (SELECTION EFFECTS)
// ============================================================================

void Renderer::UnitHighlight(Image *blurSrc, float x, float y, float w, float h, Colour const &col) {
    if (m_unitHighlightVertexCount + 6 > MAX_UNIT_HIGHLIGHT_VERTICES) {
        FlushUnitHighlights();
    }
    
    // Smart flush only when texture changes for batching
    if (m_unitHighlightVertexCount > 0 && m_currentUnitHighlightTexture != blurSrc->m_textureID) {
        FlushUnitHighlights();
    }
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, blurSrc->m_textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    if (blurSrc->m_mipmapping) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    
    m_currentUnitHighlightTexture = blurSrc->m_textureID;
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    float onePixelW = 1.0f / (float) blurSrc->Width();
    float onePixelH = 1.0f / (float) blurSrc->Height();
    
    // First triangle: TL, TR, BR
    m_unitHighlightVertices[m_unitHighlightVertexCount++] = {x, y, r, g, b, a, onePixelW, 1.0f - onePixelH};
    m_unitHighlightVertices[m_unitHighlightVertexCount++] = {x + w, y, r, g, b, a, 1.0f - onePixelW, 1.0f - onePixelH};
    m_unitHighlightVertices[m_unitHighlightVertexCount++] = {x + w, y + h, r, g, b, a, 1.0f - onePixelW, onePixelH};
    
    // Second triangle: TL, BR, BL
    m_unitHighlightVertices[m_unitHighlightVertexCount++] = {x, y, r, g, b, a, onePixelW, 1.0f - onePixelH};
    m_unitHighlightVertices[m_unitHighlightVertexCount++] = {x + w, y + h, r, g, b, a, 1.0f - onePixelW, onePixelH};
    m_unitHighlightVertices[m_unitHighlightVertexCount++] = {x, y + h, r, g, b, a, onePixelW, onePixelH};
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