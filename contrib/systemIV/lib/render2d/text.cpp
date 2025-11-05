#include "lib/universal_include.h"

#include <stdarg.h>

#include "lib/resource/bitmapfont.h"
#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/resource/bitmap.h"
#include "lib/render3d/renderer_3d.h"
#include "lib/render/colour.h"

#include "lib/render2d/renderer.h"

extern Renderer *g_renderer;

void Renderer::BlitChar(unsigned int textureID, float x, float y, float w, float h, 
                        float texX, float texY, float texW, float texH, Colour const &col) {
    
    // find the font batch for this texture ID, or find an empty one
    int targetBatch = -1;
    
    // check if we already have a batch for this texture
    for (int i = 0; i < MAX_FONT_ATLASES; i++) {
        if (m_fontBatches[i].textureID == textureID && m_fontBatches[i].vertexCount > 0) {
            targetBatch = i;
            break;
        }
    }
    
    // ff not found, find the first empty batch
    if (targetBatch == -1) {
        for (int i = 0; i < MAX_FONT_ATLASES; i++) {
            if (m_fontBatches[i].vertexCount == 0) {
                targetBatch = i;
                m_fontBatches[i].textureID = textureID;
                break;
            }
        }
    }
    
    // if no empty batch found, use the current one
    if (targetBatch == -1) {
        targetBatch = m_currentFontBatchIndex;
    }
    
    FontBatch *batch = &m_fontBatches[targetBatch];
    
    if (batch->vertexCount + 6 > MAX_TEXT_VERTICES) {
        m_textVertices = batch->vertices;
        m_textVertexCount = batch->vertexCount;
        m_currentTextTexture = batch->textureID;
        FlushTextBuffer();
        batch->vertexCount = 0;
    }
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    float u1 = texX, v1 = texY, u2 = texX + texW, v2 = texY + texH;
    
    // first triangle: TL, TR, BR
    batch->vertices[batch->vertexCount++] = {x, y, r, g, b, a, u1, v2};
    batch->vertices[batch->vertexCount++] = {x + w, y, r, g, b, a, u2, v2};
    batch->vertices[batch->vertexCount++] = {x + w, y + h, r, g, b, a, u2, v1};
    
    // second triangle: TL, BR, BL  
    batch->vertices[batch->vertexCount++] = {x, y, r, g, b, a, u1, v2};
    batch->vertices[batch->vertexCount++] = {x + w, y + h, r, g, b, a, u2, v1};
    batch->vertices[batch->vertexCount++] = {x, y + h, r, g, b, a, u1, v1};
    
    m_currentFontBatchIndex = targetBatch;
    m_textVertices = batch->vertices;
    m_textVertexCount = batch->vertexCount;
    m_currentTextTexture = batch->textureID;
}

// ============================================================================
// SIMPLE TEXT RENDERING FUNCTIONS
// ============================================================================

void Renderer::Text(float x, float y, Colour const &col, float size, const char *text, ...) {   
    char buf[1024];
    va_list ap;
    va_start(ap, text);
    vsprintf(buf, text, ap);
    TextSimple(x, y, col, size, buf);
}

void Renderer::TextCentre(float x, float y, Colour const &col, float size, const char *text, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, text);
    vsprintf(buf, text, ap);
    float actualX = x - TextWidth(buf, size) / 2.0f;
    TextSimple(actualX, y, col, size, buf);
}

void Renderer::TextRight(float x, float y, Colour const &col, float size, const char *text, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, text);
    vsprintf(buf, text, ap);
    float actualX = x - TextWidth(buf, size);
    TextSimple(actualX, y, col, size, buf);
}

void Renderer::TextSimple(float x, float y, Colour const &col, float size, const char *text) {
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
        
        font->DrawText2DSimple(x, y, size, text, col);
    }
}

void Renderer::TextCentreSimple(float x, float y, Colour const &col, float size, const char *text) {
    float actualX = x - TextWidth(text, size) / 2.0f;
    TextSimple(actualX, y, col, size, text);
}

void Renderer::TextRightSimple(float x, float y, Colour const &col, float size, const char *text) {
    float actualX = x - TextWidth(text, size);
    TextSimple(actualX, y, col, size, text);
}

float Renderer::TextWidth(const char *text, float size) {
    BitmapFont *font = g_resource->GetBitmapFont(m_currentFontFilename);
    if (font) {
        font->SetSpacing(GetFontSpacing(m_currentFontName));
    } else {
        font = g_resource->GetBitmapFont(m_defaultFontFilename);
        if (font) {
            font->SetSpacing(GetFontSpacing(m_defaultFontName));
        }
    }

    if (!font) return -1;

    font->SetHoriztonalFlip(m_horizFlip);
    font->SetFixedWidth(m_fixedWidth);
    return font->GetTextWidth(text, size);
}

float Renderer::TextWidth(const char *text, unsigned int textLen, float size, BitmapFont *bitmapFont) {
    if (!bitmapFont) return -1;
    return bitmapFont->GetTextWidth(text, textLen, size);
}