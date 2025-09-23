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

// batched font rendering function that works extremely well
void Renderer::BlitChar(unsigned int textureID, float x, float y, float w, float h, 
                        float texX, float texY, float texW, float texH, Colour const &col) {
    if (m_triangleVertexCount + 6 > MAX_VERTICES) {
        FlushTriangles(true);
    }
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    FlushIfTextureChanged(textureID, true);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    
    float u1 = texX, v1 = texY, u2 = texX + texW, v2 = texY + texH;
    
    // First triangle: TL, TR, BR
    m_triangleVertices[m_triangleVertexCount++] = {x, y, r, g, b, a, u1, v2};
    m_triangleVertices[m_triangleVertexCount++] = {x + w, y, r, g, b, a, u2, v2};
    m_triangleVertices[m_triangleVertexCount++] = {x + w, y + h, r, g, b, a, u2, v1};
    
    // Second triangle: TL, BR, BL
    m_triangleVertices[m_triangleVertexCount++] = {x, y, r, g, b, a, u1, v2};
    m_triangleVertices[m_triangleVertexCount++] = {x + w, y + h, r, g, b, a, u2, v1};
    m_triangleVertices[m_triangleVertexCount++] = {x, y + h, r, g, b, a, u1, v1};
}

// batched text rendering function - accumulates to text buffer instead of immediate flush
void Renderer::BatchBlitChar(unsigned int textureID, float x, float y, float w, float h, 
                             float texX, float texY, float texW, float texH, Colour const &col) {
    // check if we need to flush due to texture change
    if (m_currentTextTexture != 0 && m_currentTextTexture != textureID) {
        FlushTextBuffer();
    }
    
    // check if we need to flush due to buffer full
    if (m_textVertexCount + 6 > MAX_TEXT_VERTICES) {
        FlushTextBuffer();
    }
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    // set current texture for batching
    m_currentTextTexture = textureID;
    
    float u1 = texX, v1 = texY, u2 = texX + texW, v2 = texY + texH;
    
    // first triangle: TL, TR, BR
    m_textVertices[m_textVertexCount++] = {x, y, r, g, b, a, u1, v2};
    m_textVertices[m_textVertexCount++] = {x + w, y, r, g, b, a, u2, v2};
    m_textVertices[m_textVertexCount++] = {x + w, y + h, r, g, b, a, u2, v1};
    
    // second triangle: TL, BR, BL
    m_textVertices[m_textVertexCount++] = {x, y, r, g, b, a, u1, v2};
    m_textVertices[m_textVertexCount++] = {x + w, y + h, r, g, b, a, u2, v1};
    m_textVertices[m_textVertexCount++] = {x, y + h, r, g, b, a, u1, v1};
}

void Renderer::TextSimpleBatch(float x, float y, Colour const &col, float size, const char *text) {
    BitmapFont *font = g_resource->GetBitmapFont(m_currentFontFilename);
    if (font) {
        font->SetSpacing(GetFontSpacing(m_currentFontName));
    } else {
        font = g_resource->GetBitmapFont(m_defaultFontFilename);
        if (font) {
            font->SetSpacing(GetFontSpacing(m_defaultFontName));
        }
    }

    if (font) {    
        font->SetHoriztonalFlip(m_horizFlip);
        font->SetFixedWidth(m_fixedWidth);
        
        // blend mode is handled in FlushTextBuffer, just accumulate text
        font->DrawText2DSimpleBatch(x, y, size, text, col);
    }
}

void Renderer::TextCentreSimpleBatch(float x, float y, Colour const &col, float size, const char *text) {
    float actualX = x - TextWidth(text, size) / 2.0f;
    TextSimpleBatch(actualX, y, col, size, text);
}

void Renderer::TextRightSimpleBatch(float x, float y, Colour const &col, float size, const char *text) {
    float actualX = x - TextWidth(text, size);
    TextSimpleBatch(actualX, y, col, size, text);
}