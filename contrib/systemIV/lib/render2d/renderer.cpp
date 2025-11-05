#include "lib/universal_include.h"

#include <time.h>
#include <stdarg.h>

#include "lib/gucci/window_manager.h"
#include "lib/resource/bitmapfont.h"
#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/resource/bitmap.h"
#include "lib/resource/sprite_atlas.h"
#include "lib/hi_res_time.h"
#include "lib/debug_utils.h"
#include "lib/preferences.h"
#include "lib/string_utils.h"
#include "lib/filesys/filesys_utils.h"
#include "lib/language_table.h"
#include "lib/render3d/renderer_3d.h"
#include "lib/render/colour.h"
#include "shaders/vertex.glsl.h"
#include "shaders/color_fragment.glsl.h"
#include "shaders/texture_fragment.glsl.h"

#include "renderer.h"

Renderer *g_renderer = NULL;

// ============================================================================
// MATRIX4F IMPLEMENTATION
// ============================================================================

constexpr Matrix4f::Matrix4f() 
    : m{1,0,0,0,  0,1,0,0,  0,0,1,0,  0,0,0,1}
{
}

constexpr void Matrix4f::LoadIdentity() {
    m[0] = 1.0f; m[1] = 0.0f; m[2] = 0.0f; m[3] = 0.0f;
    m[4] = 0.0f; m[5] = 1.0f; m[6] = 0.0f; m[7] = 0.0f;
    m[8] = 0.0f; m[9] = 0.0f; m[10] = 1.0f; m[11] = 0.0f;
    m[12] = 0.0f; m[13] = 0.0f; m[14] = 0.0f; m[15] = 1.0f;
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

constexpr void Matrix4f::Multiply(const Matrix4f& other) {
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
// GET TEXTURE ATLAS COORDINATES
// ============================================================================

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
      m_colourDepth(32),
      m_mouseMode(0),
      m_blendMode(BlendModeNormal),
      m_blendSrcFactor(GL_ONE),
      m_blendDstFactor(GL_ZERO),
      m_blendEnabled(false),
      m_depthTestEnabled(false),
      m_depthMaskEnabled(false),
      m_currentBlendSrcFactor(-1),
      m_currentBlendDstFactor(-1),
      m_currentViewportX(-1),
      m_currentViewportY(-1),
      m_currentViewportWidth(-1),
      m_currentViewportHeight(-1),
      m_currentActiveTexture(0),
      m_currentShaderProgram(0),
      m_currentVAO(0),
      m_currentArrayBuffer(0),
      m_currentElementBuffer(0),
      m_currentLineWidth(-1.0f),
      m_scissorTestEnabled(false),
      m_currentTextureMagFilter(-1),
      m_currentTextureMinFilter(-1),
      m_currentScissorX(-1),
      m_currentScissorY(-1),
      m_currentScissorWidth(-1),
      m_currentScissorHeight(-1),
      m_defaultFontName(NULL),
      m_defaultFontFilename(NULL),
      m_defaultFontLanguageSpecific(false),
      m_currentFontName(NULL),
      m_currentFontFilename(NULL),
      m_currentFontLanguageSpecific(false),
      m_horizFlip(false),
      m_fixedWidth(false),
      m_negative(false),
      m_shaderProgram(0),
      m_colorShaderProgram(0),
      m_textureShaderProgram(0),
      m_VAO(0), 
      m_VBO(0),
      m_textVAO(0), 
      m_textVBO(0), 
      m_spriteVAO(0), 
      m_spriteVBO(0),
      m_circleVAO(0), 
      m_circleVBO(0),
      m_circleFillVAO(0), 
      m_circleFillVBO(0),
      m_rectVAO(0), 
      m_rectVBO(0),
      m_rectFillVAO(0), 
      m_rectFillVBO(0),
      m_triangleFillVAO(0), 
      m_triangleFillVBO(0),
      m_legacyVAO(0), 
      m_legacyVBO(0),
      m_bufferNeedsUpload(true),
      m_projMatrixLocation(-1),
      m_modelViewMatrixLocation(-1),
      m_colorLocation(-1),
      m_textureLocation(-1),
      m_triangleVertexCount(0),
      m_lineVertexCount(0),
      m_currentFontBatchIndex(0),
      m_textVertices(NULL),
      m_textVertexCount(0),
      m_currentTextTexture(0),
      m_staticSpriteVertexCount(0),
      m_currentStaticSpriteTexture(0),
      m_rotatingSpriteVertexCount(0),
      m_currentRotatingSpriteTexture(0),
      m_circleVertexCount(0),
      m_circleFillVertexCount(0),
      m_rectVertexCount(0),
      m_rectFillVertexCount(0),
      m_triangleFillVertexCount(0),
      m_allowImmedateFlush(true),
      m_lineConversionBuffer(NULL),
      m_lineConversionBufferSize(0),
      m_currentLineColor(0, 0, 0, 0),
      m_lineStripActive(false),
      m_lineStripColor(0, 0, 0, 0),
      m_lineStripWidth(1.0f),
      m_cachedLineStripActive(false),
      m_currentCacheKey(NULL),
      m_cachedLineStripColor(0, 0, 0, 0),
      m_cachedLineStripWidth(1.0f),
      m_maxMegaVertices(2500000),
      m_maxMegaIndices(5000000),
      m_megaVBOActive(false),
      m_currentMegaVBOKey(NULL),
      m_megaVBOColor(0, 0, 0, 0),
      m_megaVBOWidth(1.0f),
      m_megaVertices(NULL),
      m_megaVertexCount(0),
      m_megaIndices(NULL),
      m_megaIndexCount(0),
      m_cachedVBOs(),
      m_currentBoundTexture(0),
      m_batchingTextures(true),
      m_msaaEnabled(false),
      m_msaaSamples(0),
      m_msaaFBO(0),
      m_msaaColorRBO(0),
      m_msaaDepthRBO(0),
      m_msaaWidth(0),
      m_msaaHeight(0) {
      m_currentFontBatchIndex = 0;
      m_textVertices = m_fontBatches[0].vertices;
      m_textVertexCount = 0;
      m_currentTextTexture = 0;
      m_lineVertexCount = 0;
      m_staticSpriteVertexCount = 0;
      m_currentStaticSpriteTexture = 0;
      m_rotatingSpriteVertexCount = 0;
      m_currentRotatingSpriteTexture = 0;
      m_circleVertexCount = 0;
      m_circleFillVertexCount = 0;
      m_rectVertexCount = 0;
      m_rectFillVertexCount = 0;
      m_triangleFillVertexCount = 0;
      m_drawCallsPerFrame = 0;
      m_legacyTriangleCalls = 0;
      m_legacyLineCalls = 0;
      m_textCalls = 0;
      m_lineCalls = 0;
      m_staticSpriteCalls = 0;
      m_rotatingSpriteCalls = 0;
      m_circleCalls = 0;
      m_circleFillCalls = 0;
      m_rectCalls = 0;
      m_rectFillCalls = 0;
      m_triangleFillCalls = 0;
      m_prevDrawCallsPerFrame = 0;
      m_prevLegacyTriangleCalls = 0;
      m_prevLegacyLineCalls = 0;
      m_prevTextCalls = 0;
      m_prevLineCalls = 0;
      m_prevStaticSpriteCalls = 0;
      m_prevRotatingSpriteCalls = 0;
      m_prevCircleCalls = 0;
      m_prevCircleFillCalls = 0;
      m_prevRectCalls = 0;
      m_prevRectFillCalls = 0;
      m_prevTriangleFillCalls = 0;
      m_flushTimingCount = 0;
      m_currentFlushStartTime = 0.0;

      //
      // initialize font-aware batching system

      for (int i = 0; i < MAX_FONT_ATLASES; i++) {
          m_fontBatches[i].vertexCount = 0;
          m_fontBatches[i].textureID = 0;
      }
    
      //
      // initialize OpenGL components

      InitializeShaders();
      CacheUniformLocations();
      SetupVertexArrays();
      ResetFlushTimings();
    
    m_megaVertices = new Vertex2D[m_maxMegaVertices];
    m_megaIndices = new unsigned int[m_maxMegaIndices];
    
    m_lineConversionBufferSize = std::max(MAX_VERTICES * 2, MAX_LINE_VERTICES * 2);
    m_lineConversionBuffer = new Vertex2D[m_lineConversionBufferSize];
    
    int msaaSamples = g_preferences ? g_preferences->GetInt(PREFS_SCREEN_ANTIALIAS, 0) : 0;
    
    //
    // ensure the FBO is applied to the correct screen dimensions
    //
    // use physical dimensions to prevent logical resolution scaling
    // from being applied to the framebuffer and causing clipping
    
#ifndef TARGET_EMSCRIPTEN
    int screenW = (int)(g_windowManager->PhysicalWindowW() * g_windowManager->GetHighDPIScaleX());
    int screenH = (int)(g_windowManager->PhysicalWindowH() * g_windowManager->GetHighDPIScaleY());
#else
    int screenW = g_windowManager->WindowW();
    int screenH = g_windowManager->WindowH();
#endif

    InitializeMSAAFramebuffer(screenW, screenH, msaaSamples);
    
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
    if (m_textVAO) glDeleteVertexArrays(1, &m_textVAO);
    if (m_textVBO) glDeleteBuffers(1, &m_textVBO);
    if (m_spriteVAO) glDeleteVertexArrays(1, &m_spriteVAO);
    if (m_spriteVBO) glDeleteBuffers(1, &m_spriteVBO);
    if (m_circleVAO) glDeleteVertexArrays(1, &m_circleVAO);
    if (m_circleVBO) glDeleteBuffers(1, &m_circleVBO);
    if (m_circleFillVAO) glDeleteVertexArrays(1, &m_circleFillVAO);
    if (m_circleFillVBO) glDeleteBuffers(1, &m_circleFillVBO);
    if (m_rectVAO) glDeleteVertexArrays(1, &m_rectVAO);
    if (m_rectVBO) glDeleteBuffers(1, &m_rectVBO);
    if (m_rectFillVAO) glDeleteVertexArrays(1, &m_rectFillVAO);
    if (m_rectFillVBO) glDeleteBuffers(1, &m_rectFillVBO);
    if (m_triangleFillVAO) glDeleteVertexArrays(1, &m_triangleFillVAO);
    if (m_triangleFillVBO) glDeleteBuffers(1, &m_triangleFillVBO);
    if (m_legacyVAO) glDeleteVertexArrays(1, &m_legacyVAO);
    if (m_legacyVBO) glDeleteBuffers(1, &m_legacyVBO);
    
    for (int i = 0; i < m_flushTimingCount; i++) {
        if (m_flushTimings[i].queryObject != 0) {
            glDeleteQueries(1, &m_flushTimings[i].queryObject);
        }
    }
    
    if (m_megaVertices) {
        delete[] m_megaVertices;
        m_megaVertices = NULL;
    }
    if (m_megaIndices) {
        delete[] m_megaIndices;
        m_megaIndices = NULL;
    }
    
    if (m_lineConversionBuffer) {
        delete[] m_lineConversionBuffer;
        m_lineConversionBuffer = NULL;
    }
    
    if (g_renderer3d) {
        delete g_renderer3d;
        g_renderer3d = NULL;
    }

    DestroyMSAAFramebuffer();
}

// ============================================================================
// BLIT
// ============================================================================

void Renderer::Blit(Image *src, float x, float y, Colour const &col) {
    float w = src->Width();
    float h = src->Height();
    Blit(src, x, y, w, h, col);
}

void Renderer::Blit(Image *src, float x, float y, float w, float h, Colour const &col) {    
    if (m_triangleVertexCount + 6 > MAX_VERTICES) {
        FlushTriangles(true);
    }
    
    float r = col.GetRFloat(), g = col.GetGFloat(), b = col.GetBFloat(), a = col.GetAFloat();
    
    SetActiveTexture(GL_TEXTURE0);
    SetBoundTexture(src->m_textureID);
    SetTextureParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    if (src->m_mipmapping) {
        SetTextureParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        SetTextureParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
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

    float r = col.GetRFloat(), g = col.GetGFloat(), b = col.GetBFloat(), a = col.GetAFloat();
    
    SetActiveTexture(GL_TEXTURE0);
    SetBoundTexture(src->m_textureID);
    SetTextureParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    if (src->m_mipmapping) {
        SetTextureParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        SetTextureParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
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

// ============================================================================
// VIEWPORT & SCENE MANAGEMENT
// ============================================================================

void Renderer::Set2DViewport(float l, float r, float b, float t, int x, int y, int w, int h) {
    m_modelViewMatrix.LoadIdentity();
    m_projectionMatrix.Ortho(l - 0.325f, r - 0.325f, b - 0.325f, t - 0.325f, -1.0f, 1.0f);

    //
    // Calculate scale factors between logical and physical resolution

    float scaleX = (float)g_windowManager->PhysicalWindowW() / (float)g_windowManager->WindowW();
    float scaleY = (float)g_windowManager->PhysicalWindowH() / (float)g_windowManager->WindowH();
    
    //
    // Apply scaling to convert logical coordinates to physical viewport coordinates
    
    int physicalX = (int)(x * scaleX);
    int physicalY = (int)((g_windowManager->WindowH() - h - y) * scaleY);
    int physicalW = (int)(w * scaleX);
    int physicalH = (int)(h * scaleY);

    SetViewport(physicalX, physicalY, physicalW, physicalH);
}

void Renderer::Reset2DViewport() {
    Set2DViewport(0, g_windowManager->WindowW(), g_windowManager->WindowH(), 0, 
                  0, 0, g_windowManager->WindowW(), g_windowManager->WindowH());
    ResetClip();
}

void Renderer::HandleWindowResize() {
#ifndef TARGET_EMSCRIPTEN
    int screenW = (int)(g_windowManager->PhysicalWindowW() * g_windowManager->GetHighDPIScaleX());
    int screenH = (int)(g_windowManager->PhysicalWindowH() * g_windowManager->GetHighDPIScaleY());
#else
    int screenW = g_windowManager->WindowW();
    int screenH = g_windowManager->WindowH();
#endif

    ResizeMSAAFramebuffer(screenW, screenH);
    Reset2DViewport();
}

void Renderer::BeginScene() {
    SetBoundTexture(0);
    SetBlendMode(BlendModeNormal);
}

void Renderer::BeginFrame() {
    ResetFrameCounters();
    ResetFlushTimings(); 
    m_bufferNeedsUpload = true; // mark buffer as needing upload for new frame
}

void Renderer::EndFrame() {
    UpdateGpuTimings();
}

void Renderer::ClearScreen(bool _colour, bool _depth) {
    if (_colour) glClear(GL_COLOR_BUFFER_BIT);
    if (_depth) glClear(GL_DEPTH_BUFFER_BIT);
}

void Renderer::Shutdown() {
    // Intentionally empty
}

// ============================================================================
// MSAA FRAMEBUFFER IMPLEMENTATION
// ============================================================================

void Renderer::InitializeMSAAFramebuffer(int width, int height, int samples) {
    m_msaaWidth = width;
    m_msaaHeight = height;
    m_msaaSamples = samples;
    
    if (samples <= 0) {
        m_msaaEnabled = false;
        m_msaaFBO = 0;
        m_msaaColorRBO = 0;
        m_msaaDepthRBO = 0;
        return;
    }
    
    //
    // query the maximum supported samples
    
    GLint maxSamples = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
    
    //
    // generate and bind the MSAA framebuffer
    
    glGenFramebuffers(1, &m_msaaFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_msaaFBO);
    
    //
    // create the multisampled color renderbuffer
    
    glGenRenderbuffers(1, &m_msaaColorRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, m_msaaColorRBO);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_msaaSamples, 
                                     GL_RGBA8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 
                             GL_RENDERBUFFER, m_msaaColorRBO);
    
    //
    // create the multisampled depth renderbuffer
    
    glGenRenderbuffers(1, &m_msaaDepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, m_msaaDepthRBO);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_msaaSamples, 
                                     GL_DEPTH_COMPONENT24, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, 
                             GL_RENDERBUFFER, m_msaaDepthRBO);
    
    //
    // check the framebuffer for completeness
    
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        AppDebugOut("MSAA creation failed with status: 0x%x\n", status);
        
        if (m_msaaColorRBO) glDeleteRenderbuffers(1, &m_msaaColorRBO);
        if (m_msaaDepthRBO) glDeleteRenderbuffers(1, &m_msaaDepthRBO);
        if (m_msaaFBO) glDeleteFramebuffers(1, &m_msaaFBO);
        
        m_msaaFBO = 0;
        m_msaaColorRBO = 0;
        m_msaaDepthRBO = 0;
        m_msaaEnabled = false;
        m_msaaSamples = 0;
    } else {
        m_msaaEnabled = true;
        AppDebugOut("MSAA applied successfully\n");
    }
    
    //
    // unbind the framebuffer

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

//
// if the window is resized, we need to resize the FBO

void Renderer::ResizeMSAAFramebuffer(int width, int height) {
    if (!m_msaaEnabled || m_msaaSamples <= 0) {
        return;
    }
    
    if (width == m_msaaWidth && height == m_msaaHeight) {
        return;
    }

    //
    // destroy and reincarnate the FBO with new dimensions
    
    DestroyMSAAFramebuffer();
    InitializeMSAAFramebuffer(width, height, m_msaaSamples);
}

void Renderer::DestroyMSAAFramebuffer() {
    if (m_msaaColorRBO) {
        glDeleteRenderbuffers(1, &m_msaaColorRBO);
        m_msaaColorRBO = 0;
    }
    
    if (m_msaaDepthRBO) {
        glDeleteRenderbuffers(1, &m_msaaDepthRBO);
        m_msaaDepthRBO = 0;
    }
    
    if (m_msaaFBO) {
        glDeleteFramebuffers(1, &m_msaaFBO);
        m_msaaFBO = 0;
    }
    
    m_msaaEnabled = false;
}

// ============================================================================
// MSAA SCENE MANAGEMENT
// ============================================================================

void Renderer::BeginMSAARendering() {
    if (m_msaaEnabled && m_msaaFBO != 0) {
        glBindFramebuffer(GL_FRAMEBUFFER, m_msaaFBO);
    }
}

void Renderer::EndMSAARendering() {
    if (m_msaaEnabled && m_msaaFBO != 0) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_msaaFBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        
        glBlitFramebuffer(0, 0, m_msaaWidth, m_msaaHeight,
                         0, 0, m_msaaWidth, m_msaaHeight,
                         GL_COLOR_BUFFER_BIT, GL_NEAREST);
        
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
}

// ============================================================================
// BLEND MODES & DEPTH
// ============================================================================

void Renderer::SetBlendMode(int _blendMode) {
    if (m_blendMode != _blendMode && m_staticSpriteVertexCount > 0) {
        FlushStaticSprites();
    }

    m_blendMode = _blendMode;
    
    switch (_blendMode) {
        case BlendModeDisabled:
            if (m_blendEnabled) {
                glDisable(GL_BLEND);
                m_blendEnabled = false;
            }
            break;
        case BlendModeNormal:
            SetBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            if (!m_blendEnabled) {
                glEnable(GL_BLEND);
                m_blendEnabled = true;
            }
            break;
        case BlendModeAdditive:
            SetBlendFunc(GL_SRC_ALPHA, GL_ONE);
            if (!m_blendEnabled) {
                glEnable(GL_BLEND);
                m_blendEnabled = true;
            }
            break;
        case BlendModeSubtractive:
            SetBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_COLOR);
            if (!m_blendEnabled) {
                glEnable(GL_BLEND);
                m_blendEnabled = true;
            }
            break;
    }
}

void Renderer::SetBlendFunc(int srcFactor, int dstFactor) {
    m_blendSrcFactor = srcFactor;
    m_blendDstFactor = dstFactor;
    
    //
    // only call glBlendFunc if the parameters have actually changed

    if (m_currentBlendSrcFactor != srcFactor || m_currentBlendDstFactor != dstFactor) {
        glBlendFunc(srcFactor, dstFactor);
        m_currentBlendSrcFactor = srcFactor;
        m_currentBlendDstFactor = dstFactor;
    }
}

void Renderer::SetDepthBuffer(bool _enabled, bool _clearNow) {
    if (_enabled) {
        if (!m_depthTestEnabled) {
            glEnable(GL_DEPTH_TEST);
            m_depthTestEnabled = true;
        }
        if (!m_depthMaskEnabled) {
            glDepthMask(true);
            m_depthMaskEnabled = true;
        }
    } else {
        if (m_depthTestEnabled) {
            glDisable(GL_DEPTH_TEST);
            m_depthTestEnabled = false;
        }
        if (m_depthMaskEnabled) {
            glDepthMask(false);
            m_depthMaskEnabled = false;
        }
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

void Renderer::SetViewport(int x, int y, int width, int height) {
    if (m_currentViewportX != x || m_currentViewportY != y || 
        m_currentViewportWidth != width || m_currentViewportHeight != height) {
        glViewport(x, y, width, height);
        m_currentViewportX = x;
        m_currentViewportY = y;
        m_currentViewportWidth = width;
        m_currentViewportHeight = height;
    }
}

void Renderer::SetActiveTexture(GLenum texture) {
    if (m_currentActiveTexture != texture) {
        glActiveTexture(texture);
        m_currentActiveTexture = texture;
    }
}

void Renderer::SetShaderProgram(GLuint program) {
    if (m_currentShaderProgram != program) {
        glUseProgram(program);
        m_currentShaderProgram = program;
    }
}

void Renderer::SetVertexArray(GLuint vao) {
    if (m_currentVAO != vao) {
        glBindVertexArray(vao);
        m_currentVAO = vao;
    }
}

void Renderer::SetArrayBuffer(GLuint buffer) {
    if (m_currentArrayBuffer != buffer) {
        glBindBuffer(GL_ARRAY_BUFFER, buffer);
        m_currentArrayBuffer = buffer;
    }
}

void Renderer::SetElementBuffer(GLuint buffer) {
    if (m_currentElementBuffer != buffer) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer);
        m_currentElementBuffer = buffer;
    }
}

void Renderer::SetLineWidth(GLfloat width) {
    if (m_currentLineWidth != width) {
        glLineWidth(width);
        m_currentLineWidth = width;
    }
}

void Renderer::SetBoundTexture(GLuint texture) {
    if (m_currentBoundTexture != texture) {
        glBindTexture(GL_TEXTURE_2D, texture);
        m_currentBoundTexture = texture;
    }
}

void Renderer::SetScissorTest(bool enabled) {
    if (m_scissorTestEnabled != enabled) {
        if (enabled) {
            glEnable(GL_SCISSOR_TEST);
        } else {
            glDisable(GL_SCISSOR_TEST);
        }
        m_scissorTestEnabled = enabled;
    }
}

void Renderer::SetScissor(int x, int y, int width, int height) {
    if (m_currentScissorX != x || m_currentScissorY != y || 
        m_currentScissorWidth != width || m_currentScissorHeight != height) {
        glScissor(x, y, width, height);
        m_currentScissorX = x;
        m_currentScissorY = y;
        m_currentScissorWidth = width;
        m_currentScissorHeight = height;
    }
}

void Renderer::SetTextureParameter(GLenum pname, GLint param) {
    if (pname == GL_TEXTURE_MAG_FILTER) {
        if (m_currentTextureMagFilter != param) {
            glTexParameteri(GL_TEXTURE_2D, pname, param);
            m_currentTextureMagFilter = param;
        }
    } else if (pname == GL_TEXTURE_MIN_FILTER) {
        if (m_currentTextureMinFilter != param) {
            glTexParameteri(GL_TEXTURE_2D, pname, param);
            m_currentTextureMinFilter = param;
        }
    } else {
        glTexParameteri(GL_TEXTURE_2D, pname, param);
    }
}

void Renderer::SetClip(int x, int y, int w, int h) {
    
    //
    // flush all eclipse and text batches before changing scissor state
    // create a seperate draw call for each rendering operation with
    // scissor testing enabled. as you cannot have multiple scissor regions
    // active in the same draw call. other options like shader clipping was
    // explored but due to being ass at programming i just caused lots of
    // rendering artifacts.
    //
    // we increase draw calls by around 10 percent with this method :(
    
    EndRectFillBatch();
    EndTriangleFillBatch();
    EndStaticSpriteBatch(); 
    EndLineBatch();
    EndRectBatch();
    EndTextBatch();

    //
    // calculate scale factors between logical and physical resolution

#if !defined(TARGET_OS_MACOSX) && (!defined(TARGET_OS_LINUX) || !defined(TARGET_EMSCRIPTEN))
    float scaleX = (float)g_windowManager->PhysicalWindowW() / (float)g_windowManager->WindowW();
    float scaleY = (float)g_windowManager->PhysicalWindowH() / (float)g_windowManager->WindowH();
    
    //
    // apply scaling to convert logical coordinates to physical clipping coordinates  

    int physicalX = (int)(x * scaleX);
    int physicalY = (int)((g_windowManager->WindowH() - h - y) * scaleY);
    int physicalW = (int)(w * scaleX);
    int physicalH = (int)(h * scaleY);
    
    SetScissor(physicalX, physicalY, physicalW, physicalH);
#else
    float scaleX = g_windowManager->GetHighDPIScaleX();
    float scaleY = g_windowManager->GetHighDPIScaleY();

    SetScissor(scaleX * x, scaleY * (g_windowManager->WindowH() - h - y), scaleX * w, scaleY * h);
#endif
    SetScissorTest(true);
}

void Renderer::ResetClip() {

    EndRectFillBatch();
    EndTriangleFillBatch();
    EndStaticSpriteBatch(); 
    EndLineBatch();
    EndRectBatch();
    EndTextBatch();
    
    SetScissorTest(false);
}

void Renderer::SaveScreenshot() {
    float timeNow = GetHighResTime();
    char *screenshotsDir = ScreenshotsDirectory();

#if !defined(TARGET_OS_LINUX) || !defined(TARGET_EMSCRIPTEN)
    // Screenshot should capture the actual physical resolution, not logical
    int screenW = g_windowManager->PhysicalWindowW();
    int screenH = g_windowManager->PhysicalWindowH();
#else
    int screenW = g_windowManager->GetHighDPIScaleX() * g_windowManager->WindowW();
    int screenH = g_windowManager->GetHighDPIScaleY() * g_windowManager->WindowH();
#endif
    
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

void Renderer::CacheUniformLocations() {

    //
    // cache color shader uniforms

    m_colorShaderUniforms.projectionLoc = glGetUniformLocation(m_colorShaderProgram, "uProjection");
    m_colorShaderUniforms.modelViewLoc = glGetUniformLocation(m_colorShaderProgram, "uModelView");
    m_colorShaderUniforms.textureLoc = -1; // not used in color shader
    
    //
    // cache texture shader uniforms  

    m_textureShaderUniforms.projectionLoc = glGetUniformLocation(m_textureShaderProgram, "uProjection");
    m_textureShaderUniforms.modelViewLoc = glGetUniformLocation(m_textureShaderProgram, "uModelView");
    m_textureShaderUniforms.textureLoc = glGetUniformLocation(m_textureShaderProgram, "ourTexture");
    
}

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
    
#ifdef TARGET_OS_MACOSX
    glBindAttribLocation(shaderProgram, 0, "aPos");
    glBindAttribLocation(shaderProgram, 1, "aColor");
    glBindAttribLocation(shaderProgram, 2, "aTexCoord");
#endif
    
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

//
// create shader programs using shader sources from .glsl.h headers

void Renderer::InitializeShaders() {
    m_colorShaderProgram = CreateShader(VERTEX_2D_SHADER_SOURCE, COLOR_FRAGMENT_2D_SHADER_SOURCE);
    m_textureShaderProgram = CreateShader(VERTEX_2D_SHADER_SOURCE, TEXTURE_FRAGMENT_2D_SHADER_SOURCE);
    m_shaderProgram = m_colorShaderProgram;
}

void Renderer::SetupVertexArrays() {

    //
    // set up vertex attributes for Vertex2D format

    auto setupVertexAttributes = []() {

        //
        // position attribute (x, y)

        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), (void*)0);
        glEnableVertexAttribArray(0);

        //
        // color attribute (r, g, b, a)
            
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);

        //
        // texture coordinate attribute (u, v)

        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);
    };
    
    // Create text VAO/VBO pair  

    glGenVertexArrays(1, &m_textVAO);
    glGenBuffers(1, &m_textVBO);
    glBindVertexArray(m_textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_textVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex2D) * MAX_TEXT_VERTICES, NULL, GL_DYNAMIC_DRAW);
    setupVertexAttributes();
    
    //
    // Create sprite VAO/VBO pair
    
    glGenVertexArrays(1, &m_spriteVAO);
    glGenBuffers(1, &m_spriteVBO);
    glBindVertexArray(m_spriteVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_spriteVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex2D) * (MAX_STATIC_SPRITE_VERTICES + MAX_ROTATING_SPRITE_VERTICES), NULL, GL_DYNAMIC_DRAW);
    setupVertexAttributes();
    
    //
    // Create Line VAO/VBO pair

    glGenVertexArrays(1, &m_lineVAO);
    glGenBuffers(1, &m_lineVBO);
    glBindVertexArray(m_lineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_lineVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex2D) * (MAX_LINE_VERTICES * 2), NULL, GL_DYNAMIC_DRAW);
    setupVertexAttributes();
    
    //
    // Create circle VAO/VBO pair

    glGenVertexArrays(1, &m_circleVAO);
    glGenBuffers(1, &m_circleVBO);
    glBindVertexArray(m_circleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_circleVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex2D) * MAX_CIRCLE_VERTICES, NULL, GL_DYNAMIC_DRAW);
    setupVertexAttributes();

    //
    // Create circle fill VAO/VBO pair

    glGenVertexArrays(1, &m_circleFillVAO);
    glGenBuffers(1, &m_circleFillVBO);
    glBindVertexArray(m_circleFillVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_circleFillVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex2D) * MAX_CIRCLE_FILL_VERTICES, NULL, GL_DYNAMIC_DRAW);
    setupVertexAttributes();
    
    //
    // Create rect VAO/VBO pair

    glGenVertexArrays(1, &m_rectVAO);
    glGenBuffers(1, &m_rectVBO);
    glBindVertexArray(m_rectVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_rectVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex2D) * MAX_RECT_VERTICES, NULL, GL_DYNAMIC_DRAW);
    setupVertexAttributes();

    //
    // Create rect fill VAO/VBO pair

    glGenVertexArrays(1, &m_rectFillVAO);
    glGenBuffers(1, &m_rectFillVBO);
    glBindVertexArray(m_rectFillVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_rectFillVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex2D) * MAX_RECT_FILL_VERTICES, NULL, GL_DYNAMIC_DRAW);
    setupVertexAttributes();
    
    //
    // Create triangle fill VAO/VBO pair

    glGenVertexArrays(1, &m_triangleFillVAO);
    glGenBuffers(1, &m_triangleFillVBO);
    glBindVertexArray(m_triangleFillVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_triangleFillVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex2D) * MAX_TRIANGLE_FILL_VERTICES, NULL, GL_DYNAMIC_DRAW);
    setupVertexAttributes();
    
    //
    // Create legacy VAO/VBO pair

    glGenVertexArrays(1, &m_legacyVAO);
    glGenBuffers(1, &m_legacyVBO);
    glBindVertexArray(m_legacyVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_legacyVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex2D) * MAX_VERTICES, NULL, GL_DYNAMIC_DRAW);
    setupVertexAttributes();
    
    //
    // Old VAO/VBO

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex2D) * MAX_VERTICES, NULL, GL_DYNAMIC_DRAW);
    setupVertexAttributes();
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

// ============================================================================
// UNIFORM MANAGEMENT
// ============================================================================

void Renderer::SetColorShaderUniforms() {
    glUniformMatrix4fv(m_colorShaderUniforms.projectionLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(m_colorShaderUniforms.modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
}

void Renderer::SetTextureShaderUniforms() {
    glUniformMatrix4fv(m_textureShaderUniforms.projectionLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(m_textureShaderUniforms.modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    glUniform1i(m_textureShaderUniforms.textureLoc, 0);
}

void Renderer::UploadVertexData(const Vertex2D* vertices, int vertexCount) {
    if (vertexCount == 0) return;
    
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertexCount * sizeof(Vertex2D), vertices);
}

void Renderer::UploadVertexDataToVBO(unsigned int vbo,
                                     const Vertex2D* vertices,
                                     int vertexCount,
                                     unsigned int usageHint)
{
    if (vertexCount <= 0 || !vertices) return;

    const GLsizeiptr bytes =
        static_cast<GLsizeiptr>(vertexCount) * sizeof(Vertex2D);

    if (usageHint == GL_STATIC_DRAW) {
        glBufferData(GL_ARRAY_BUFFER, bytes, vertices, usageHint);
    } else {
        glBufferData(GL_ARRAY_BUFFER, bytes, NULL, usageHint);
        glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, vertices);
    }
}

// ============================================================================
// DEBUG MENU FUNCTIONS
// ============================================================================

void Renderer::StartFlushTiming(const char* name) {

    //
    // store start time
    
    m_currentFlushStartTime = GetHighResTime();
    
    //
    // find timing entry

    for (int i = 0; i < m_flushTimingCount; i++) {
        if (strcmp(m_flushTimings[i].name, name) == 0) {

            //
            // start GPU timing

            if (m_flushTimings[i].queryObject == 0) {
#ifndef TARGET_EMSCRIPTEN
                glGenQueries(1, &m_flushTimings[i].queryObject);
#endif
            }
            
            //
            // only start new query if previous one is complete
            
            if (!m_flushTimings[i].queryPending) {
#ifndef TARGET_EMSCRIPTEN
                glBeginQuery(GL_TIME_ELAPSED, m_flushTimings[i].queryObject);
#endif
                m_flushTimings[i].queryPending = true;
            }
            return;
        }
    }
    
    //
    // create new timing entry

    if (m_flushTimingCount < MAX_FLUSH_TYPES) {
        FlushTiming* timing = &m_flushTimings[m_flushTimingCount];
        timing->name = name;
        timing->totalTime = 0.0;
        timing->totalGpuTime = 0.0;
        timing->callCount = 0;
        timing->queryPending = false;
        
#ifndef TARGET_EMSCRIPTEN
        glGenQueries(1, &timing->queryObject);
        glBeginQuery(GL_TIME_ELAPSED, timing->queryObject);
#endif
        timing->queryPending = true;
        m_flushTimingCount++;
    }
}

void Renderer::EndFlushTiming(const char* name) {
    double cpuTime = GetHighResTime() - m_currentFlushStartTime;
    
    for (int i = 0; i < m_flushTimingCount; i++) {
        if (strcmp(m_flushTimings[i].name, name) == 0) {
            if (m_flushTimings[i].queryPending) {
#ifndef TARGET_EMSCRIPTEN
                glEndQuery(GL_TIME_ELAPSED);
#endif
                m_flushTimings[i].queryPending = false;
            }
            
            //
            // update CPU timing

            m_flushTimings[i].totalTime += cpuTime;
            m_flushTimings[i].callCount++;
            return;
        }
    }
}

void Renderer::UpdateGpuTimings() {

    //
    // call this once per frame to collect GPU timing results

    for (int i = 0; i < m_flushTimingCount; i++) {
        int available = 0;
#ifndef TARGET_EMSCRIPTEN
        glGetQueryObjectiv(m_flushTimings[i].queryObject, GL_QUERY_RESULT_AVAILABLE, &available);
#endif
        if (available) {
            uint64_t gpuTime;
#ifndef TARGET_EMSCRIPTEN
            glGetQueryObjectui64v(m_flushTimings[i].queryObject, GL_QUERY_RESULT, &gpuTime);
#endif
            m_flushTimings[i].totalGpuTime += gpuTime / 1000000.0; // Convert to milliseconds
        }
    }
}

void Renderer::ResetFlushTimings() {
    for (int i = 0; i < m_flushTimingCount; i++) {
        m_flushTimings[i].totalTime = 0.0;
        m_flushTimings[i].totalGpuTime = 0.0;
        m_flushTimings[i].callCount = 0;
    }
}

const Renderer::FlushTiming* Renderer::GetFlushTimings(int& count) const {
    count = m_flushTimingCount;
    return m_flushTimings;
}

void Renderer::ResetFrameCounters() {    
    
    //
    // store previous frames data

    m_prevDrawCallsPerFrame = m_drawCallsPerFrame;
    m_prevLegacyTriangleCalls = m_legacyTriangleCalls;
    m_prevLegacyLineCalls = m_legacyLineCalls;
    m_prevTextCalls = m_textCalls;
    m_prevLineCalls = m_lineCalls;
    m_prevStaticSpriteCalls = m_staticSpriteCalls;
    m_prevRotatingSpriteCalls = m_rotatingSpriteCalls;
    m_prevCircleCalls = m_circleCalls;
    m_prevCircleFillCalls = m_circleFillCalls;
    m_prevRectCalls = m_rectCalls;
    m_prevRectFillCalls = m_rectFillCalls;
    m_prevTriangleFillCalls = m_triangleFillCalls;
    
    //
    // reset current frame counters

    m_drawCallsPerFrame = 0;
    m_legacyTriangleCalls = 0;
    m_legacyLineCalls = 0;
    m_textCalls = 0;
    m_lineCalls = 0;
    m_staticSpriteCalls = 0;
    m_rotatingSpriteCalls = 0;
    m_circleCalls = 0;
    m_circleFillCalls = 0;
    m_rectCalls = 0;
    m_rectFillCalls = 0;
    m_triangleFillCalls = 0;
}

//
// increment draw calls for the 2d renderer

void Renderer::IncrementDrawCall(const char* bufferType) {
    m_drawCallsPerFrame++;
    
    constexpr auto hash = [](const char* str) constexpr {
        uint32_t hash = 5381;
        while (*str) {
            hash = ((hash << 5) + hash) + *str++;
        }
        return hash;
    };
    
    switch (hash(bufferType)) {
        case hash("legacy_triangles"): m_legacyTriangleCalls++; break;
        case hash("legacy_lines"): m_legacyLineCalls++; break;
        case hash("text"): m_textCalls++; break;
        case hash("lines"): m_lineCalls++; break;
        case hash("static_sprites"): m_staticSpriteCalls++; break;
        case hash("rotating_sprites"): m_rotatingSpriteCalls++; break;
        case hash("circles"): m_circleCalls++; break;
        case hash("circle_fills"): m_circleFillCalls++; break;
        case hash("rects"): m_rectCalls++; break;
        case hash("rect_fills"): m_rectFillCalls++; break;
        case hash("triangle_fills"): m_triangleFillCalls++; break;
    }
}
