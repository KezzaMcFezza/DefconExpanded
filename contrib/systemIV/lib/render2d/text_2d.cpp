#include "lib/universal_include.h"

#include <stdarg.h>

#include "lib/resource/bitmapfont.h"
#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/resource/bitmap.h"
#include "lib/render3d/renderer_3d.h"
#include "lib/render/colour.h"

#include "lib/render/renderer.h"
#include "lib/render2d/renderer_2d.h"

void Renderer2D::BlitChar(unsigned int textureID, float x, float y, float w, float h, 
                        float texX, float texY, float texW, float texH, Colour const &col, bool immediateFlush) {
    
    //
    // find the font batch for this texture ID, or find an empty one

    int targetBatch = -1;
    
    //
    // check if we already have a batch for this texture

    for (int i = 0; i < Renderer::MAX_FONT_ATLASES; i++) {
        if (m_fontBatches[i].textureID == textureID && m_fontBatches[i].vertexCount > 0) {
            targetBatch = i;
            break;
        }
    }
    
    //
    // if not found, find the first empty batch

    if (targetBatch == -1) {
        for (int i = 0; i < Renderer::MAX_FONT_ATLASES; i++) {
            if (m_fontBatches[i].vertexCount == 0) {
                targetBatch = i;
                m_fontBatches[i].textureID = textureID;
                break;
            }
        }
    }
    
    //
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
    
#if IMMEDIATE_MODE_2D
    FlushTextBuffer();
    batch->vertexCount = 0;
#else
    if (immediateFlush) {
        FlushTextBuffer();
        batch->vertexCount = 0;
    }
#endif
}

// ============================================================================
// SIMPLE TEXT RENDERING FUNCTIONS
// ============================================================================

void Renderer2D::Text(float x, float y, Colour const &col, float size, const char *text, ...) {   
    char buf[1024];
    va_list ap;
    va_start(ap, text);
    vsnprintf(buf, sizeof(buf), text, ap);
    buf[sizeof(buf) - 1] = '\0';
    TextSimple(x, y, col, size, buf);
}

void Renderer2D::TextCentre(float x, float y, Colour const &col, float size, const char *text, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, text);
    vsnprintf(buf, sizeof(buf), text, ap);
    buf[sizeof(buf) - 1] = '\0';
    float actualX = x - TextWidth(buf, size) / 2.0f;
    TextSimple(actualX, y, col, size, buf);
}

void Renderer2D::TextRight(float x, float y, Colour const &col, float size, const char *text, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, text);
    vsnprintf(buf, sizeof(buf), text, ap);
    buf[sizeof(buf) - 1] = '\0';
    float actualX = x - TextWidth(buf, size);
    TextSimple(actualX, y, col, size, buf);
}

void Renderer2D::TextSimple(float x, float y, Colour const &col, float size, const char *text, bool immediateFlush) {
    BitmapFont *font = g_resource->GetBitmapFont(g_renderer->GetCurrentFontFilename());
    if (font) {
        font->SetSpacing(g_renderer->GetFontSpacing(g_renderer->GetCurrentFontName()));
    } else {
        font = g_resource->GetBitmapFont(g_renderer->GetDefaultFontFilename());
        if (font) {
            font->SetSpacing(g_renderer->GetFontSpacing(g_renderer->GetDefaultFontName()));
        }
    }

    if (font) {    
        font->SetHoriztonalFlip(g_renderer->GetHorizFlip());
        font->SetFixedWidth(g_renderer->GetFixedWidth());
        
        font->DrawText2DSimple(x, y, size, text, col);
        
        if (immediateFlush) {
            for (int i = 0; i < Renderer::MAX_FONT_ATLASES; i++) {
                if (m_fontBatches[i].vertexCount > 0) {
                    m_textVertices = m_fontBatches[i].vertices;
                    m_textVertexCount = m_fontBatches[i].vertexCount;
                    m_currentTextTexture = m_fontBatches[i].textureID;
                    FlushTextBuffer();
                    m_fontBatches[i].vertexCount = 0;
                }
            }
        }
    }
}

void Renderer2D::TextCentreSimple(float x, float y, Colour const &col, float size, const char *text, bool immediateFlush) {
    float actualX = x - TextWidth(text, size) / 2.0f;
    TextSimple(actualX, y, col, size, text, immediateFlush);
}

void Renderer2D::TextRightSimple(float x, float y, Colour const &col, float size, const char *text, bool immediateFlush) {
    float actualX = x - TextWidth(text, size);
    TextSimple(actualX, y, col, size, text, immediateFlush);
}

float Renderer2D::TextWidth(const char *text, float size) {
    BitmapFont *font = g_resource->GetBitmapFont(g_renderer->GetCurrentFontFilename());
    if (font) {
        font->SetSpacing(g_renderer->GetFontSpacing(g_renderer->GetCurrentFontName()));
    } else {
        font = g_resource->GetBitmapFont(g_renderer->GetDefaultFontFilename());
        if (font) {
            font->SetSpacing(g_renderer->GetFontSpacing(g_renderer->GetDefaultFontName()));
        }
    }

    if (!font) return -1;

    font->SetHoriztonalFlip(g_renderer->GetHorizFlip());
    font->SetFixedWidth(g_renderer->GetFixedWidth());
    return font->GetTextWidth(text, size);
}

float Renderer2D::TextWidth(const char *text, unsigned int textLen, float size, BitmapFont *bitmapFont) {
    if (!bitmapFont) return -1;
    return bitmapFont->GetTextWidth(text, textLen, size);
}