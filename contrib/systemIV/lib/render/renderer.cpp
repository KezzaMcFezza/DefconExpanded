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

#include "renderer.h"
#include "renderer_3d.h"
#include "colour.h"

Renderer *g_renderer = NULL;

// ============================================================================
// MATRIX4F IMPLEMENTATION
// ============================================================================

Matrix4f::Matrix4f() {
    LoadIdentity();
}

void Matrix4f::LoadIdentity() {
    for (int i = 0; i < 16; i++) {
        m[i] = 0.0f;
    }
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

void Matrix4f::Ortho(float left, float right, float bottom, float top, float nearZ, float farZ) {
    LoadIdentity();
    
    float tx = -(right + left) / (right - left);
    float ty = -(top + bottom) / (top - bottom);
    float tz = -(farZ + nearZ) / (farZ - nearZ);
    
    m[0] = 2.0f / (right - left);
    m[5] = 2.0f / (top - bottom);
    m[10] = -2.0f / (farZ - nearZ);
    m[12] = tx;
    m[13] = ty;
    m[14] = tz;
}

void Matrix4f::Multiply(const Matrix4f& other) {
    Matrix4f result;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            result.m[i * 4 + j] = 0.0f;
            for (int k = 0; k < 4; k++) {
                result.m[i * 4 + j] += m[i * 4 + k] * other.m[k * 4 + j];
            }
        }
    }
    *this = result;
}

// ============================================================================
// FONT UTILITIES
// ============================================================================

static const char *GetFontFilename(const char *_fontName, const char *_langName, bool *_fontLanguageSpecific) {
    static char result[512];

    if (_langName || g_languageTable->m_lang) {
        *_fontLanguageSpecific = true;
        if (_langName) {
            snprintf(result, sizeof(result), "fonts/%s_%s.bmp", _fontName, _langName);
        } else {
            snprintf(result, sizeof(result), "fonts/%s_%s.bmp", _fontName, g_languageTable->m_lang->m_name);
        }
        result[sizeof(result) - 1] = '\x0';

        if (!g_resource->TestBitmapFont(result)) {
            *_fontLanguageSpecific = false;
            snprintf(result, sizeof(result), "fonts/%s.bmp", _fontName);
            result[sizeof(result) - 1] = '\x0';
        }
    } else {
        *_fontLanguageSpecific = false;
        snprintf(result, sizeof(result), "fonts/%s.bmp", _fontName);
        result[sizeof(result) - 1] = '\x0';
    }

    return result;
}

// ============================================================================
// CONSTRUCTOR & DESTRUCTOR
// ============================================================================

Renderer::Renderer()
    : m_alpha(1.0f),
      m_currentFontName(NULL),
      m_currentFontFilename(NULL),
      m_currentFontLanguageSpecific(false),
      m_defaultFontName(NULL),
      m_defaultFontFilename(NULL),
      m_defaultFontLanguageSpecific(false),
      m_horizFlip(false),
      m_fixedWidth(false),
      m_negative(false),
      m_blendSrcFactor(GL_ONE),
      m_blendDstFactor(GL_ZERO),
      m_currentBoundTexture(0),
      m_batchingTextures(true),
      m_shaderProgram(0),
      m_colorShaderProgram(0),
      m_textureShaderProgram(0),
      m_VAO(0),
      m_VBO(0),
      m_triangleVertexCount(0),
      m_lineVertexCount(0),
      m_lineStripActive(false),
      m_cachedLineStripActive(false),
      m_currentCacheKey(NULL),
      m_megaVBOActive(false),
      m_currentMegaVBOKey(NULL),
      m_megaVertices(NULL),
      m_megaVertexCount(0),
      m_allowImmedateFlush(true) {
    
    // Initialize specialized buffer counters
    m_uiTriangleVertexCount = 0;
    m_uiLineVertexCount = 0;
    m_textVertexCount = 0;
    m_currentTextTexture = 0;
    m_spriteVertexCount = 0;
    m_currentSpriteTexture = 0;
    
    // Unit rendering buffers
    m_unitTrailVertexCount = 0;
    m_unitMainVertexCount = 0;
    m_currentUnitMainTexture = 0;
    m_unitRotatingVertexCount = 0;
    m_currentUnitRotatingTexture = 0;
    m_unitHighlightVertexCount = 0;
    m_currentUnitHighlightTexture = 0;
    m_unitStateVertexCount = 0;
    m_currentUnitStateTexture = 0;
    m_unitCounterVertexCount = 0;
    m_currentUnitCounterTexture = 0;
    m_unitNukeVertexCount = 0;
    m_currentUnitNukeTexture = 0;
    
    // Effects rendering buffers
    m_effectsLineVertexCount = 0;
    m_effectsSpriteVertexCount = 0;
    m_currentEffectsSpriteTexture = 0;
    
    // Health bar rendering buffer
    m_healthBarVertexCount = 0;
    
    // Initialize performance counters
    m_drawCallsPerFrame = 0;
    m_legacyTriangleCalls = 0;
    m_legacyLineCalls = 0;
    m_uiTriangleCalls = 0;
    m_uiLineCalls = 0;
    m_textCalls = 0;
    m_spriteCalls = 0;
    m_unitTrailCalls = 0;
    m_unitMainSpriteCalls = 0;
    m_unitRotatingCalls = 0;
    m_unitHighlightCalls = 0;
    m_unitStateIconCalls = 0;
    m_unitCounterCalls = 0;
    m_unitNukeIconCalls = 0;
    m_effectsLineCalls = 0;
    m_effectsSpriteCalls = 0;
    m_healthBarCalls = 0;
    
    // Initialize previous frame counters
    m_prevDrawCallsPerFrame = 0;
    m_prevLegacyTriangleCalls = 0;
    m_prevLegacyLineCalls = 0;
    m_prevUiTriangleCalls = 0;
    m_prevUiLineCalls = 0;
    m_prevTextCalls = 0;
    m_prevSpriteCalls = 0;
    m_prevUnitTrailCalls = 0;
    m_prevUnitMainSpriteCalls = 0;
    m_prevUnitRotatingCalls = 0;
    m_prevUnitHighlightCalls = 0;
    m_prevUnitStateIconCalls = 0;
    m_prevUnitCounterCalls = 0;
    m_prevUnitNukeIconCalls = 0;
    m_prevEffectsLineCalls = 0;
    m_prevEffectsSpriteCalls = 0;
    m_prevHealthBarCalls = 0;
    
    // Initialize OpenGL components
    InitializeShaders();
    SetupVertexArrays();
    
    m_megaVertices = new Vertex2D[MAX_MEGA_VERTICES];
    g_renderer3d = new Renderer3D(this);
}

Renderer::~Renderer() {
    if (m_currentFontName) delete[] m_currentFontName;
    if (m_currentFontFilename) delete[] m_currentFontFilename;
    if (m_defaultFontName) delete[] m_defaultFontName;
    if (m_defaultFontFilename) delete[] m_defaultFontFilename;
    
    // Clean up OpenGL resources
    if (m_colorShaderProgram) glDeleteProgram(m_colorShaderProgram);
    if (m_textureShaderProgram) glDeleteProgram(m_textureShaderProgram);
    if (m_VAO) glDeleteVertexArrays(1, &m_VAO);
    if (m_VBO) glDeleteBuffers(1, &m_VBO);
    
    if (m_megaVertices) {
        delete[] m_megaVertices;
        m_megaVertices = NULL;
    }
    
    if (g_renderer3d) {
        delete g_renderer3d;
        g_renderer3d = NULL;
    }
}

// ============================================================================
// VIEWPORT & SCENE MANAGEMENT
// ============================================================================

void Renderer::Set2DViewport(float l, float r, float b, float t, int x, int y, int w, int h) {
    m_modelViewMatrix.LoadIdentity();
    m_projectionMatrix.Ortho(l - 0.325f, r - 0.325f, b - 0.325f, t - 0.325f, -1.0f, 1.0f);
    
    float scaleX = g_windowManager->GetHighDPIScaleX();
    float scaleY = g_windowManager->GetHighDPIScaleY();
    
    glViewport(scaleX * x, scaleY * (g_windowManager->WindowH() - h - y), scaleX * w, scaleY * h);
}

void Renderer::Reset2DViewport() {
    Set2DViewport(0, g_windowManager->WindowW(), g_windowManager->WindowH(), 0, 
                  0, 0, g_windowManager->WindowW(), g_windowManager->WindowH());
}

void Renderer::BeginScene() {
    m_currentBoundTexture = 0;
    SetBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);

#ifdef TARGET_OS_MACOSX
    // Line smoothing crashes some Mac OSX machines with Radeon graphics
    return;
#endif
}

void Renderer::ClearScreen(bool _colour, bool _depth) {
    if (_colour) glClear(GL_COLOR_BUFFER_BIT);
    if (_depth) glClear(GL_DEPTH_BUFFER_BIT);
}

void Renderer::Shutdown() {
    // Intentionally empty
}

// ============================================================================
// BLEND MODES & DEPTH
// ============================================================================

void Renderer::SetBlendMode(int _blendMode) {
    m_blendMode = _blendMode;
    
    switch (_blendMode) {
        case BlendModeDisabled:
            glDisable(GL_BLEND);
            break;
        case BlendModeNormal:
            SetBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glEnable(GL_BLEND);
            break;
        case BlendModeAdditive:
            SetBlendFunc(GL_SRC_ALPHA, GL_ONE);
            glEnable(GL_BLEND);
            break;
        case BlendModeSubtractive:
            SetBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_COLOR);
            glEnable(GL_BLEND);
            break;
    }
}

void Renderer::SetBlendFunc(int srcFactor, int dstFactor) {
    m_blendSrcFactor = srcFactor;
    m_blendDstFactor = dstFactor;
    glBlendFunc(srcFactor, dstFactor);
}

void Renderer::SetDepthBuffer(bool _enabled, bool _clearNow) {
    if (_enabled) {
        glEnable(GL_DEPTH_TEST);
        glDepthMask(true);
    } else {
        glDisable(GL_DEPTH_TEST);
        glDepthMask(false);
    }
 
    if (_clearNow) {
        glClear(GL_DEPTH_BUFFER_BIT);
    }
}

// ============================================================================
// FONT MANAGEMENT
// ============================================================================

void Renderer::SetDefaultFont(const char *font, const char *_langName) {
    if (m_currentFontName) {
        delete[] m_currentFontName;
        m_currentFontName = NULL;
    }
    if (m_currentFontFilename) {
        delete[] m_currentFontFilename;
        m_currentFontFilename = NULL;
    }
    m_currentFontLanguageSpecific = false;

    if (m_defaultFontName) delete[] m_defaultFontName;
    if (m_defaultFontFilename) delete[] m_defaultFontFilename;

    const char *fullFilename = GetFontFilename(font, _langName, &m_defaultFontLanguageSpecific);
    m_defaultFontName = newStr(font);
    m_defaultFontFilename = newStr(fullFilename);
}

void Renderer::SetFontSpacing(const char *font, float _spacing) {
    BTree<float> *fontSpacing = m_fontSpacings.LookupTree(font);
    if (fontSpacing) {
        fontSpacing->data = _spacing;
    } else {
        m_fontSpacings.PutData(font, _spacing);
    }
}

float Renderer::GetFontSpacing(const char *font) {
    BTree<float> *fontSpacing = m_fontSpacings.LookupTree(font);
    if (fontSpacing) {
        return fontSpacing->data;
    }
    return BitmapFont::DEFAULT_SPACING;
}

void Renderer::SetFont(const char *font, bool horizFlip, bool negative, bool fixedWidth, const char *_langName) {
    if (font || !_langName) {
        if (m_currentFontName) {
            delete[] m_currentFontName;
            m_currentFontName = NULL;
        }
    }
    if (m_currentFontFilename) {
        delete[] m_currentFontFilename;
        m_currentFontFilename = NULL;
    }
    m_currentFontLanguageSpecific = false;

    if (font) {
        m_currentFontName = newStr(font);
        const char *fontFilename = GetFontFilename(font, _langName, &m_currentFontLanguageSpecific);
        m_currentFontFilename = newStr(fontFilename);
    } else if (_langName) {
        if (m_currentFontName) {
            const char *fontFilename = GetFontFilename(m_currentFontName, _langName, &m_currentFontLanguageSpecific);
            m_currentFontFilename = newStr(fontFilename);
        } else if (m_defaultFontName) {
            m_currentFontName = newStr(m_defaultFontName);
            const char *fontFilename = GetFontFilename(m_defaultFontName, _langName, &m_currentFontLanguageSpecific);
            m_currentFontFilename = newStr(fontFilename);
        }
    }

    m_horizFlip = horizFlip;
    m_fixedWidth = fixedWidth;
    m_negative = negative;

    if (m_negative) {
        SetBlendMode(BlendModeSubtractive);
    } else {
        SetBlendMode(BlendModeNormal);
    }
}

void Renderer::SetFont(const char *font, const char *_langName) {
    SetFont(font, m_horizFlip, m_negative, m_fixedWidth, _langName);
}

bool Renderer::IsFontLanguageSpecific() {
    BitmapFont *font = g_resource->GetBitmapFont(m_currentFontFilename);
    if (font) {
        return m_currentFontLanguageSpecific;
    } else {
        font = g_resource->GetBitmapFont(m_defaultFontFilename);
        if (font) {
            return m_defaultFontLanguageSpecific;
        }
    }
    return false;
}

BitmapFont *Renderer::GetBitmapFont() {
    BitmapFont *font = g_resource->GetBitmapFont(m_currentFontFilename);
    if (font) {
        font->SetSpacing(GetFontSpacing(m_currentFontName));
    } else {
        font = g_resource->GetBitmapFont(m_defaultFontFilename);
        if (font) {
            font->SetSpacing(GetFontSpacing(m_defaultFontName));
        }
    }

    if (!font) return NULL;

    font->SetHoriztonalFlip(m_horizFlip);
    font->SetFixedWidth(m_fixedWidth);
    return font;
}

// text rendering functions, most original and just been converted to opengl 3.3 core

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

// primitive rendering functions, includes all the original immediate mode rendering functions
// contains some attempted batching methods but most are flushing to quickly

void Renderer::Rect(float x, float y, float w, float h, Colour const &col, float lineWidth) {
    if (m_lineVertexCount + 8 > MAX_VERTICES) {
        FlushLines();
    }
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    // create 4 lines to form rectangle outline
    // top line
    m_lineVertices[m_lineVertexCount++] = {x, y, r, g, b, a, 0.0f, 0.0f};
    m_lineVertices[m_lineVertexCount++] = {x + w, y, r, g, b, a, 0.0f, 0.0f};
    // right line
    m_lineVertices[m_lineVertexCount++] = {x + w, y, r, g, b, a, 0.0f, 0.0f};
    m_lineVertices[m_lineVertexCount++] = {x + w, y + h, r, g, b, a, 0.0f, 0.0f};
    // bottom line
    m_lineVertices[m_lineVertexCount++] = {x + w, y + h, r, g, b, a, 0.0f, 0.0f};
    m_lineVertices[m_lineVertexCount++] = {x, y + h, r, g, b, a, 0.0f, 0.0f};
    // left line
    m_lineVertices[m_lineVertexCount++] = {x, y + h, r, g, b, a, 0.0f, 0.0f};
    m_lineVertices[m_lineVertexCount++] = {x, y, r, g, b, a, 0.0f, 0.0f};
    
    FlushLines();
}

void Renderer::RectFill(float x, float y, float w, float h, Colour const &col) {
    RectFill(x, y, w, h, col, col, col, col);
}

void Renderer::RectFill(float x, float y, float w, float h, Colour const &col1, Colour const &col2, bool horizontal) {
    if (horizontal) {
        RectFill(x, y, w, h, col1, col1, col2, col2);
    } else {
        RectFill(x, y, w, h, col1, col2, col2, col1);
    }
}

void Renderer::RectFill(float x, float y, float w, float h, Colour const &colTL, Colour const &colTR, 
                        Colour const &colBR, Colour const &colBL) {
    if (m_triangleVertexCount + 6 > MAX_VERTICES) {
        FlushTriangles(false);
    }
    
    float rTL = colTL.m_r / 255.0f, gTL = colTL.m_g / 255.0f, bTL = colTL.m_b / 255.0f, aTL = colTL.m_a / 255.0f;
    float rTR = colTR.m_r / 255.0f, gTR = colTR.m_g / 255.0f, bTR = colTR.m_b / 255.0f, aTR = colTR.m_a / 255.0f;
    float rBR = colBR.m_r / 255.0f, gBR = colBR.m_g / 255.0f, bBR = colBR.m_b / 255.0f, aBR = colBR.m_a / 255.0f;
    float rBL = colBL.m_r / 255.0f, gBL = colBL.m_g / 255.0f, bBL = colBL.m_b / 255.0f, aBL = colBL.m_a / 255.0f;
    
    // first triangle: TL, TR, BR
    m_triangleVertices[m_triangleVertexCount++] = {x, y, rTL, gTL, bTL, aTL, 0.0f, 0.0f};
    m_triangleVertices[m_triangleVertexCount++] = {x + w, y, rTR, gTR, bTR, aTR, 1.0f, 0.0f};
    m_triangleVertices[m_triangleVertexCount++] = {x + w, y + h, rBR, gBR, bBR, aBR, 1.0f, 1.0f};
    
    // second triangle: TL, BR, BL
    m_triangleVertices[m_triangleVertexCount++] = {x, y, rTL, gTL, bTL, aTL, 0.0f, 0.0f};
    m_triangleVertices[m_triangleVertexCount++] = {x + w, y + h, rBR, gBR, bBR, aBR, 1.0f, 1.0f};
    m_triangleVertices[m_triangleVertexCount++] = {x, y + h, rBL, gBL, bBL, aBL, 0.0f, 1.0f};
    
    FlushTriangles(false);
}

void Renderer::Line(float x1, float y1, float x2, float y2, Colour const &col, float lineWidth) {
    if (m_lineVertexCount + 2 > MAX_VERTICES) {
        FlushLines();
    }
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    m_lineVertices[m_lineVertexCount++] = {x1, y1, r, g, b, a, 0.0f, 0.0f};
    m_lineVertices[m_lineVertexCount++] = {x2, y2, r, g, b, a, 0.0f, 0.0f};
    
    FlushLines();
}

void Renderer::Circle(float x, float y, float radius, int numPoints, Colour const &col, float lineWidth) {
    if (m_lineVertexCount + numPoints * 2 > MAX_VERTICES) {
        FlushLines();
    }
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    for (int i = 0; i < numPoints; ++i) {
        float angle1 = 2.0f * M_PI * (float) i / (float) numPoints;
        float angle2 = 2.0f * M_PI * (float) (i + 1) / (float) numPoints;
        
        float x1 = x + cosf(angle1) * radius;
        float y1 = y + sinf(angle1) * radius;
        float x2 = x + cosf(angle2) * radius;
        float y2 = y + sinf(angle2) * radius;
        
        m_lineVertices[m_lineVertexCount++] = {x1, y1, r, g, b, a, 0.0f, 0.0f};
        m_lineVertices[m_lineVertexCount++] = {x2, y2, r, g, b, a, 0.0f, 0.0f};
    }
    
    FlushLines();
}

void Renderer::CircleFill(float x, float y, float radius, int numPoints, Colour const &col) {
    if (m_triangleVertexCount + numPoints * 3 > MAX_VERTICES) {
        FlushTriangles(false);
    }
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    for (int i = 0; i < numPoints; ++i) {
        float angle1 = 2.0f * M_PI * (float) i / (float) numPoints;
        float angle2 = 2.0f * M_PI * (float) (i + 1) / (float) numPoints;
        
        float x1 = x + cosf(angle1) * radius;
        float y1 = y + sinf(angle1) * radius;
        float x2 = x + cosf(angle2) * radius;
        float y2 = y + sinf(angle2) * radius;
        
        // triangle: center, point i, point i+1
        m_triangleVertices[m_triangleVertexCount++] = {x, y, r, g, b, a, 0.5f, 0.5f};
        m_triangleVertices[m_triangleVertexCount++] = {x1, y1, r, g, b, a, 0.0f, 0.0f};
        m_triangleVertices[m_triangleVertexCount++] = {x2, y2, r, g, b, a, 1.0f, 0.0f};
    }
    
    FlushTriangles(false);
}

void Renderer::TriangleFill(float x1, float y1, float x2, float y2, float x3, float y3, Colour const &col) {
    if (m_triangleVertexCount + 3 > MAX_VERTICES) {
        FlushTriangles(false);
    }
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    m_triangleVertices[m_triangleVertexCount++] = {x1, y1, r, g, b, a, 0.0f, 0.0f};
    m_triangleVertices[m_triangleVertexCount++] = {x2, y2, r, g, b, a, 0.0f, 0.0f};
    m_triangleVertices[m_triangleVertexCount++] = {x3, y3, r, g, b, a, 0.0f, 0.0f};
    
    FlushTriangles(false);
}

void Renderer::BeginLines(Colour const &col, float lineWidth) {
    m_currentLineColor = col;
}

void Renderer::Line(float x, float y) {
    if (m_lineVertexCount + 1 > MAX_VERTICES) {
        FlushLines();
    }
    
    float r = m_currentLineColor.m_r / 255.0f, g = m_currentLineColor.m_g / 255.0f;
    float b = m_currentLineColor.m_b / 255.0f, a = m_currentLineColor.m_a / 255.0f;
    
    m_lineVertices[m_lineVertexCount++] = {x, y, r, g, b, a, 0.0f, 0.0f};
}

void Renderer::EndLines() {
    if (m_lineVertexCount < 2) {
        m_lineVertexCount = 0;
        return;
    }
    
    // convert line strip to individual line segments
    int tempLineVertexCount = (m_lineVertexCount - 1) * 2;
    Vertex2D* tempLineVertices = new Vertex2D[tempLineVertexCount];
    
    int lineIndex = 0;
    for (int i = 0; i < m_lineVertexCount - 1; i++) {
        tempLineVertices[lineIndex++] = m_lineVertices[i];
        tempLineVertices[lineIndex++] = m_lineVertices[i + 1];
    }
    
    m_lineVertexCount = 0;
    
    for (int i = 0; i < tempLineVertexCount; i++) {
        if (m_lineVertexCount >= MAX_VERTICES) {
            FlushLines();
        }
        m_lineVertices[m_lineVertexCount++] = tempLineVertices[i];
    }
    
    delete[] tempLineVertices;
    FlushLines();
}

void Renderer::BeginLineStrip2D(Colour const &col, float lineWidth) {
    m_lineStripActive = true;
    m_lineStripColor = col;
    m_lineStripWidth = lineWidth;
    m_lineVertexCount = 0;
}

void Renderer::LineStripVertex2D(float x, float y) {
    if (!m_lineStripActive) return;
    
    if (m_lineVertexCount >= MAX_VERTICES) {
        EndLineStrip2D();
        BeginLineStrip2D(m_lineStripColor, m_lineStripWidth);
    }
    
    float r = m_lineStripColor.m_r / 255.0f, g = m_lineStripColor.m_g / 255.0f;
    float b = m_lineStripColor.m_b / 255.0f, a = m_lineStripColor.m_a / 255.0f;
    
    m_lineVertices[m_lineVertexCount++] = {x, y, r, g, b, a, 0.0f, 0.0f};
}

void Renderer::EndLineStrip2D() {
    if (!m_lineStripActive) return;
    
    if (m_lineVertexCount < 2) {
        m_lineVertexCount = 0;
        m_lineStripActive = false;
        return;
    }
    
    // convert to line segments
    int tempVertexCount = (m_lineVertexCount - 1) * 2;
    Vertex2D* tempVertices = new Vertex2D[tempVertexCount];
    
    int lineIndex = 0;
    for (int i = 0; i < m_lineVertexCount - 1; i++) {
        tempVertices[lineIndex++] = m_lineVertices[i];
        tempVertices[lineIndex++] = m_lineVertices[i + 1];
    }
    
    m_lineVertexCount = 0;
    
    for (int i = 0; i < tempVertexCount; i++) {
        if (m_lineVertexCount >= MAX_VERTICES) {
            FlushLines();
        }
        m_lineVertices[m_lineVertexCount++] = tempVertices[i];
    }
    
    delete[] tempVertices;
    FlushLines();
    m_lineStripActive = false;
}

void Renderer::SetClip(int x, int y, int w, int h) {
    float scaleX = g_windowManager->GetHighDPIScaleX();
    float scaleY = g_windowManager->GetHighDPIScaleY();

    glScissor(scaleX * x, scaleY * (g_windowManager->WindowH() - h - y), scaleX * w, scaleY * h);
    glEnable(GL_SCISSOR_TEST);
}

void Renderer::ResetClip() {
    glDisable(GL_SCISSOR_TEST);
}


// opengl 3.3 core converted blit functions, which in the original opengl 1.2 method
// it was immediate mode rendering and immediately flushed after each draw call
// the codebase heavily uses blit so the slow conversion process is still underway
void Renderer::Blit(Image *src, float x, float y, Colour const &col) {
    float w = src->Width();
    float h = src->Height();
    Blit(src, x, y, w, h, col);
}

void Renderer::Blit(Image *src, float x, float y, float w, float h, Colour const &col) {    
    if (m_triangleVertexCount + 6 > MAX_VERTICES) {
        FlushTriangles(true);
    }
    
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, src->m_textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    if (src->m_mipmapping) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    
    // Get UV coordinates - atlas sprites use specific regions, others use full texture
    float u1, v1, u2, v2;
    GetImageUVCoords(src, u1, v1, u2, v2);
    
    // First triangle: TL, TR, BR
    m_triangleVertices[m_triangleVertexCount++] = {x, y, r, g, b, a, u1, v2};
    m_triangleVertices[m_triangleVertexCount++] = {x + w, y, r, g, b, a, u2, v2};
    m_triangleVertices[m_triangleVertexCount++] = {x + w, y + h, r, g, b, a, u2, v1};
    
    // Second triangle: TL, BR, BL
    m_triangleVertices[m_triangleVertexCount++] = {x, y, r, g, b, a, u1, v2};
    m_triangleVertices[m_triangleVertexCount++] = {x + w, y + h, r, g, b, a, u2, v1};
    m_triangleVertices[m_triangleVertexCount++] = {x, y + h, r, g, b, a, u1, v1};
    
    FlushTriangles(true);
}

void Renderer::Blit(Image *src, float x, float y, float w, float h, Colour const &col, float angle) {    
    if (m_triangleVertexCount + 6 > MAX_VERTICES) {
        FlushTriangles(true);
    }
    
    // calculate rotated vertices
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
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, src->m_textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    if (src->m_mipmapping) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    
    // get UV coordinates, atlas sprites use specific regions, others use full texture
    float u1, v1, u2, v2;
    GetImageUVCoords(src, u1, v1, u2, v2);
    
    // first triangle: vert1, vert2, vert3
    m_triangleVertices[m_triangleVertexCount++] = {vert1.x, vert1.y, r, g, b, a, u1, v2};
    m_triangleVertices[m_triangleVertexCount++] = {vert2.x, vert2.y, r, g, b, a, u2, v2};
    m_triangleVertices[m_triangleVertexCount++] = {vert3.x, vert3.y, r, g, b, a, u2, v1};
    
    // second triangle: vert1, vert3, vert4
    m_triangleVertices[m_triangleVertexCount++] = {vert1.x, vert1.y, r, g, b, a, u1, v2};
    m_triangleVertices[m_triangleVertexCount++] = {vert3.x, vert3.y, r, g, b, a, u2, v1};
    m_triangleVertices[m_triangleVertexCount++] = {vert4.x, vert4.y, r, g, b, a, u1, v1};
    
    FlushTriangles(true);
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

void Renderer::SaveScreenshot() {
    float timeNow = GetHighResTime();
    char *screenshotsDir = ScreenshotsDirectory();

    int screenW = g_windowManager->GetHighDPIScaleX() * g_windowManager->WindowW();
    int screenH = g_windowManager->GetHighDPIScaleY() * g_windowManager->WindowH();
    
    unsigned char *screenBuffer = new unsigned char[screenW * screenH * 3];
    glReadPixels(0, 0, screenW, screenH, GL_RGB, GL_UNSIGNED_BYTE, screenBuffer);

    Bitmap bitmap(screenW, screenH);
    bitmap.Clear(Black);
    
    for (int y = 0; y < screenH; ++y) {
        unsigned const char *line = &screenBuffer[y * screenW * 3];
        for (int x = 0; x < screenW; ++x) {
            unsigned const char *p = &line[x * 3];
            bitmap.PutPixel(x, y, Colour(p[0], p[1], p[2]));
        }
    }

    int screenshotIndex = 1;
    while (true) {
        time_t theTimeT = time(NULL);
        tm *theTime = localtime(&theTimeT);
        char filename[2048];
        
        snprintf(filename, sizeof(filename), "%s/screenshot %02d-%02d-%02d %02d.bmp", 
                screenshotsDir, 1900+theTime->tm_year, theTime->tm_mon+1, theTime->tm_mday, screenshotIndex);
        
        if (!DoesFileExist(filename)) {
            bitmap.SaveBmp(filename);
            break;
        }
        ++screenshotIndex;
    }

    delete[] screenBuffer;
    delete[] screenshotsDir;

    float timeTaken = GetHighResTime() - timeNow;
    AppDebugOut("Screenshot %dms\n", int(timeTaken*1000));
}

char *Renderer::ScreenshotsDirectory() {
#ifdef TARGET_OS_MACOSX
    if (getenv("HOME"))
        return ConcatPaths(getenv("HOME"), "Pictures", NULL);
#endif
    return newStr(".");
}

//
// shader and buffer managment
//

unsigned int Renderer::CreateShader(const char* vertexSource, const char* fragmentSource) {
    // create vertex shader
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSource, NULL);
    glCompileShader(vertexShader);
    
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        AppDebugOut("Vertex shader compilation failed: %s\n", infoLog);
    }
    
    // create fragment shader
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
    glCompileShader(fragmentShader);
    
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        AppDebugOut("Fragment shader compilation failed: %s\n", infoLog);
    }
    
    // create shader program
    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        AppDebugOut("Shader program linking failed: %s\n", infoLog);
    }
    
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    return shaderProgram;
}

// here we create different shaders for the emscripten build and the desktop build
// this is because emscripten does not support the core profile, but the core profile
// is the closest to WebGL 2.0.
//
// we have to use ES3.0 for emscripten which this function handles
void Renderer::InitializeShaders() {
#ifdef TARGET_EMSCRIPTEN
    // WebGL 2.0 shaders
    const char* vertexShaderSource = R"(#version 300 es
precision highp float;
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec4 aColor;
layout (location = 2) in vec2 aTexCoord;
uniform mat4 uProjection;
uniform mat4 uModelView;
out vec4 vertexColor;
out vec2 texCoord;
void main() {
    gl_Position = uProjection * uModelView * vec4(aPos, 0.0, 1.0);
    vertexColor = aColor;
    texCoord = aTexCoord;
})";

    const char* colorFragmentShaderSource = R"(#version 300 es
precision mediump float;
in vec4 vertexColor;
out vec4 FragColor;
void main() {
    FragColor = vertexColor;
})";

    const char* textureFragmentShaderSource = R"(#version 300 es
precision mediump float;
in vec4 vertexColor;
in vec2 texCoord;
uniform sampler2D ourTexture;
out vec4 FragColor;
void main() {
    FragColor = texture(ourTexture, texCoord) * vertexColor;
})";
#else
    // Desktop OpenGL 3.3 shaders
    const char* vertexShaderSource = R"(#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec4 aColor;
layout (location = 2) in vec2 aTexCoord;
uniform mat4 uProjection;
uniform mat4 uModelView;
out vec4 vertexColor;
out vec2 texCoord;
void main() {
    gl_Position = uProjection * uModelView * vec4(aPos, 0.0, 1.0);
    vertexColor = aColor;
    texCoord = aTexCoord;
})";

    const char* colorFragmentShaderSource = R"(#version 330 core
in vec4 vertexColor;
out vec4 FragColor;
void main() {
    FragColor = vertexColor;
})";

    const char* textureFragmentShaderSource = R"(#version 330 core
in vec4 vertexColor;
in vec2 texCoord;
uniform sampler2D ourTexture;
out vec4 FragColor;
void main() {
    FragColor = texture(ourTexture, texCoord) * vertexColor;
})";
#endif
    
    m_colorShaderProgram = CreateShader(vertexShaderSource, colorFragmentShaderSource);
    m_textureShaderProgram = CreateShader(vertexShaderSource, textureFragmentShaderSource);
    m_shaderProgram = m_colorShaderProgram;
}

void Renderer::SetupVertexArrays() {
    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex2D) * MAX_VERTICES, NULL, GL_DYNAMIC_DRAW);
    
    // Position attribute
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Color attribute
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    // Texture coordinate attribute
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

//
// buffer flush methods

// this function is problematic but necessary without a texture atlas
void Renderer::FlushIfTextureChanged(unsigned int newTextureID, bool useTexture) {
    if (!m_batchingTextures) {
        FlushTriangles(useTexture);
        return;
    }
    
    if (m_triangleVertexCount > 0 && m_currentBoundTexture != newTextureID) {
        FlushTriangles(useTexture);
    }
    
    m_currentBoundTexture = newTextureID;
}

void Renderer::FlushVertices(unsigned int primitiveType, bool useTexture) {
    if (primitiveType == GL_TRIANGLES) {
        FlushTriangles(useTexture);
    } else if (primitiveType == GL_LINES) {
        FlushLines();
    }
}

void Renderer::FlushTriangles(bool useTexture) {
    if (m_triangleVertexCount == 0) return;
    
    IncrementDrawCall("legacy_triangles");
    
    unsigned int shaderToUse = useTexture ? m_textureShaderProgram : m_colorShaderProgram;
    glUseProgram(shaderToUse);
    
    int projLoc = glGetUniformLocation(shaderToUse, "uProjection");
    int modelViewLoc = glGetUniformLocation(shaderToUse, "uModelView");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    
    if (useTexture) {
        int texLoc = glGetUniformLocation(shaderToUse, "ourTexture");
        glUniform1i(texLoc, 0);
    }
    
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_triangleVertexCount * sizeof(Vertex2D), m_triangleVertices);
    
    glDrawArrays(GL_TRIANGLES, 0, m_triangleVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_triangleVertexCount = 0;
}

void Renderer::FlushLines() {
    if (m_lineVertexCount == 0) return;
    
    IncrementDrawCall("legacy_lines");
    
    glUseProgram(m_colorShaderProgram);
    
    int projLoc = glGetUniformLocation(m_colorShaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_colorShaderProgram, "uModelView");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_lineVertexCount * sizeof(Vertex2D), m_lineVertices);
    
    glDrawArrays(GL_LINES, 0, m_lineVertexCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    m_lineVertexCount = 0;
}

void Renderer::FlushAllBuffers() {
    FlushTriangles(false);
    FlushLines();
}

//
// frame managment

// this is a funny one, back when i was learning opengl i was trying to over engineer a flush mechanism before the buffer overflowed
// well its safe to say i was an absolute idiot, not only did it not work i also embarrased myself, even claude.ai was dissapointed :)
bool Renderer::ShouldFlushThisFrame() {
    //if (m_triangleVertexCount >= MAX_VERTICES - 100 || m_lineVertexCount >= MAX_VERTICES - 100) {
    //    return true;
    //}
    
    //float currentTime = GetHighResTime();
    //if (currentTime - m_lastFlushTime >= 1.0f) {
    //    m_lastFlushTime = currentTime;
    //    return true;
    //}
    
    return false; // return false so msvc does not shit itself, emscriptens compiler does not care about functions not returning values
}

void Renderer::ResetFrameCounters() {
    // store previous frames data
    m_prevDrawCallsPerFrame = m_drawCallsPerFrame;
    m_prevLegacyTriangleCalls = m_legacyTriangleCalls;
    m_prevLegacyLineCalls = m_legacyLineCalls;
    m_prevUiTriangleCalls = m_uiTriangleCalls;
    m_prevUiLineCalls = m_uiLineCalls;
    m_prevTextCalls = m_textCalls;
    m_prevSpriteCalls = m_spriteCalls;
    m_prevUnitTrailCalls = m_unitTrailCalls;
    m_prevUnitMainSpriteCalls = m_unitMainSpriteCalls;
    m_prevUnitRotatingCalls = m_unitRotatingCalls;
    m_prevUnitHighlightCalls = m_unitHighlightCalls;
    m_prevUnitStateIconCalls = m_unitStateIconCalls;
    m_prevUnitCounterCalls = m_unitCounterCalls;
    m_prevUnitNukeIconCalls = m_unitNukeIconCalls;
    m_prevEffectsLineCalls = m_effectsLineCalls;
    m_prevEffectsSpriteCalls = m_effectsSpriteCalls;
    m_prevHealthBarCalls = m_healthBarCalls;
    
    // reset current frame counters
    m_drawCallsPerFrame = 0;
    m_legacyTriangleCalls = 0;
    m_legacyLineCalls = 0;
    m_uiTriangleCalls = 0;
    m_uiLineCalls = 0;
    m_textCalls = 0;
    m_spriteCalls = 0;
    m_unitTrailCalls = 0;
    m_unitMainSpriteCalls = 0;
    m_unitRotatingCalls = 0;
    m_unitHighlightCalls = 0;
    m_unitStateIconCalls = 0;
    m_unitCounterCalls = 0;
    m_unitNukeIconCalls = 0;
    m_effectsLineCalls = 0;
    m_effectsSpriteCalls = 0;
    m_healthBarCalls = 0;
}

// i know the infamous if else block, shut up
void Renderer::IncrementDrawCall(const char* bufferType) {
    m_drawCallsPerFrame++;
    
    if (strcmp(bufferType, "legacy_triangles") == 0) {
        m_legacyTriangleCalls++;
    } else if (strcmp(bufferType, "legacy_lines") == 0) {
        m_legacyLineCalls++;
    } else if (strcmp(bufferType, "ui_triangles") == 0) {
        m_uiTriangleCalls++;
    } else if (strcmp(bufferType, "ui_lines") == 0) {
        m_uiLineCalls++;
    } else if (strcmp(bufferType, "text") == 0) {
        m_textCalls++;
    } else if (strcmp(bufferType, "sprites") == 0) {
        m_spriteCalls++;
    } else if (strcmp(bufferType, "unit_trails") == 0) {
        m_unitTrailCalls++;
    } else if (strcmp(bufferType, "unit_main_sprites") == 0) {
        m_unitMainSpriteCalls++;
    } else if (strcmp(bufferType, "unit_rotating") == 0) {
        m_unitRotatingCalls++;
    } else if (strcmp(bufferType, "unit_highlights") == 0) {
        m_unitHighlightCalls++;
    } else if (strcmp(bufferType, "unit_state_icons") == 0) {
        m_unitStateIconCalls++;
    } else if (strcmp(bufferType, "unit_counters") == 0) {
        m_unitCounterCalls++;
    } else if (strcmp(bufferType, "unit_nuke_icons") == 0) {
        m_unitNukeIconCalls++;
    } else if (strcmp(bufferType, "effects_lines") == 0) {
        m_effectsLineCalls++;
    } else if (strcmp(bufferType, "effects_sprites") == 0) {
        m_effectsSpriteCalls++;
    } else if (strcmp(bufferType, "health_bars") == 0) {
        m_healthBarCalls++;
    }
}

//
// frame begin and end

void Renderer::BeginFrame() {
    ResetFrameCounters();
}

void Renderer::EndFrame() {
    FlushAllSpecializedBuffers();
}
