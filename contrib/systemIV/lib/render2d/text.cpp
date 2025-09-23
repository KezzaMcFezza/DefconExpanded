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

#include "renderer.h"

extern Renderer *g_renderer;

// ============================================================================
// IMMEDIATE MODE TEXT RENDERING
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

void Renderer::TextCentreSimple(float x, float y, Colour const &col, float size, const char *text) {
    float actualX = x - TextWidth(text, size) / 2.0f;
    TextSimple(actualX, y, col, size, text);
}

void Renderer::TextRightSimple(float x, float y, Colour const &col, float size, const char *text) {
    float actualX = x - TextWidth(text, size);
    TextSimple(actualX, y, col, size, text);
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
                
        if (m_blendMode != BlendModeSubtractive) {			
            int blendSrc = m_blendSrcFactor, blendDst = m_blendDstFactor;
            SetBlendFunc(GL_SRC_ALPHA, GL_ONE);
            font->DrawText2DSimple(x, y, size, text, col);
            SetBlendFunc(blendSrc, blendDst);
        } else {
            font->DrawText2DSimple(x, y, size, text, col);
        }
    }
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