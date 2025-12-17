#include "systemiv.h"

#include <string.h>
#include <time.h>

#include "lib/debug_utils.h"
#include "lib/hi_res_time.h"
#include "lib/resource/image.h"
#include "lib/resource/sprite_atlas.h"
#include "lib/resource/bitmap.h"
#include "lib/resource/bitmapfont.h"
#include "lib/resource/resource.h"
#include "lib/language_table.h"
#include "lib/gucci/window_manager.h"
#include "lib/string_utils.h"
#include "lib/filesys/filesys_utils.h"
#include "lib/render/colour.h"
#include "lib/gucci/window_manager.h"
#include "lib/render2d/renderer_2d.h"
#include "lib/render3d/renderer_3d.h"
#include "lib/render2d/megavbo/megavbo_2d.h"
#include "lib/render3d/megavbo/megavbo_3d.h"

#include "renderer.h"

Renderer* g_renderer = NULL;

// ============================================================================
// CONSTRUCTOR & DESTRUCTOR
// ============================================================================

Renderer::Renderer()
    : m_defaultFontName(NULL),
      m_defaultFontFilename(NULL),
      m_defaultFontLanguageSpecific(false),
      m_currentFontName(NULL),
      m_currentFontFilename(NULL),
      m_currentFontLanguageSpecific(false),
      m_horizFlip(false),
      m_fixedWidth(false),
      m_negative(false),
      m_currentViewportX(-1),
      m_currentViewportY(-1),
      m_currentViewportWidth(-1),
      m_currentViewportHeight(-1),
      m_currentScissorX(-1),
      m_currentScissorY(-1),
      m_currentScissorWidth(-1),
      m_currentScissorHeight(-1),
      m_currentActiveTexture(0),
      m_currentLineWidth(-1.0f),
      m_currentShaderProgram(0),
      m_currentVAO(0),
      m_currentArrayBuffer(0),
      m_currentElementBuffer(0),
      m_currentTextureMagFilter(-1),
      m_currentTextureMinFilter(-1),
      m_currentBoundTexture(0),
      m_scissorTestEnabled(false),
      m_textureSwitches(0),
      m_prevTextureSwitches(0),
      m_flushTimingCount(0),
      m_currentFlushStartTime(0.0),
      m_blendMode(BlendModeNormal),
      m_blendSrcFactor(GL_ONE),
      m_blendDstFactor(GL_ZERO),
      m_blendEnabled(false),
      m_depthTestEnabled(false),
      m_depthMaskEnabled(false),
      m_currentBlendSrcFactor(-1),
      m_currentBlendDstFactor(-1),
      m_msaaFBO(0),
      m_msaaColorRBO(0),
      m_msaaDepthRBO(0),
      m_msaaEnabled(false),
      m_msaaSamples(0),
      m_msaaWidth(0),
      m_msaaHeight(0)
{
    //
    // Set global pointer immediately so subrenderers can use g_renderer
    // app.cpp will also set it, but this ensures its available during construction
    
    g_renderer = this;
    
    g_renderer2d = new Renderer2D();
    g_renderer2dvbo = new Renderer2DVBO();
    g_renderer3d = new Renderer3D();
    g_renderer3dvbo = new Renderer3DVBO();
    
    //
    // Initialize flush timings
    
    for (int i = 0; i < MAX_FLUSH_TYPES; i++) {
        m_flushTimings[i].name = NULL;
        m_flushTimings[i].totalTime = 0.0;
        m_flushTimings[i].totalGpuTime = 0.0;
        m_flushTimings[i].callCount = 0;
        m_flushTimings[i].queryObject = 0;
        m_flushTimings[i].queryPending = false;
    }
}

Renderer::~Renderer()
{
    if (m_currentFontName) delete[] m_currentFontName;
    if (m_currentFontFilename) delete[] m_currentFontFilename;
    if (m_defaultFontName) delete[] m_defaultFontName;
    if (m_defaultFontFilename) delete[] m_defaultFontFilename;
    
    DestroyMSAAFramebuffer();
    
    for (int i = 0; i < m_flushTimingCount; i++) {
        if (m_flushTimings[i].queryObject != 0) {
            glDeleteQueries(1, &m_flushTimings[i].queryObject);
        }
    }
    
    if (g_renderer3d) {
        delete g_renderer3d;
        g_renderer3d = NULL;
    }
    
    if (g_renderer3dvbo) {
        delete g_renderer3dvbo;
        g_renderer3dvbo = NULL;
    }
    
    if (g_renderer2d) {
        delete g_renderer2d;
        g_renderer2d = NULL;
    }
    
    if (g_renderer2dvbo) {
        delete g_renderer2dvbo;
        g_renderer2dvbo = NULL;
    }
}

// ============================================================================
// OPENGL STATE MANAGEMENT
// ============================================================================

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
        m_textureSwitches++;
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

// ============================================================================
// BLEND MODES & DEPTH
// ============================================================================

void Renderer::SetBlendMode(int _blendMode) {

    //
    // flush any existing sprites if the blend mode changes
    
    if (m_blendMode != _blendMode && g_renderer2d->GetCurrentStaticSpriteVertexCount() > 0) {
        g_renderer2d->EndStaticSpriteBatch();
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
    // Only call glBlendFunc if the parameters have actually changed

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

void Renderer::SetDepthMask(bool enabled) {
    if (m_depthMaskEnabled != enabled) {
        glDepthMask(enabled ? GL_TRUE : GL_FALSE);
        m_depthMaskEnabled = enabled;
    }
}

void Renderer::SetCullFace(bool enabled, GLenum mode) {
    if (enabled) {
        if (!m_cullFaceEnabled) {
            glEnable(GL_CULL_FACE);
            m_cullFaceEnabled = true;
        }
        if (m_cullFaceMode != mode) {
            glCullFace(mode);
            m_cullFaceMode = mode;
        }
    } else {
        if (m_cullFaceEnabled) {
            glDisable(GL_CULL_FACE);
            m_cullFaceEnabled = false;
        }
    }
}

// ============================================================================
// SHADER SETUP
// ============================================================================

unsigned int Renderer::CreateShader(const char* vertexSource, const char* fragmentSource) {

    //
    // Create vertex shader
    
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
    
    //
    // Create fragment shader
    
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
    glCompileShader(fragmentShader);
    
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        AppDebugOut("Fragment shader compilation failed: %s\n", infoLog);
    }
    
    //
    // Create shader program
    
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

// ============================================================================
// ATLAS COORDINATES
// ============================================================================

void Renderer::GetImageUVCoords(Image* image, float& u1, float& v1, float& u2, float& v2) {
    AtlasImage* atlasImage = dynamic_cast<AtlasImage*>(image);
    if (atlasImage) 
    {

        //
        // Use atlas coordinates
        
        const AtlasCoord* coord = atlasImage->GetAtlasCoord();
        if (coord) 
        {
            u1 = coord->u1;
            v1 = coord->v1;
            u2 = coord->u2;
            v2 = coord->v2;
        } 
        else 
        {
            return;
        }
    } 
    else 
    {
        //
        // Regular image, use full texture with edge padding
        
        float onePixelW = 1.0f / (float) image->Width();
        float onePixelH = 1.0f / (float) image->Height();
        u1 = onePixelW;
        v1 = onePixelH;
        u2 = 1.0f - onePixelW;
        v2 = 1.0f - onePixelH;
    }
    
    //
    // Apply UV adjustments if set
    //
    // Positive values shrink  (reduce UV range)
    // Negative values stretch (increase UV range)
    
    if (image->m_uvAdjustX != 0.0f) 
    {
        float shrinkX = (u2 - u1) * image->m_uvAdjustX;
        u1 += shrinkX;
        u2 -= shrinkX;
    }
    
    if (image->m_uvAdjustY != 0.0f) 
    {
        float shrinkY = (v2 - v1) * image->m_uvAdjustY;
        v1 += shrinkY;
        v2 -= shrinkY;
    }
}

unsigned int Renderer::GetEffectiveTextureID(Image* image) {
    AtlasImage* atlasImage = dynamic_cast<AtlasImage*>(image);
    if (atlasImage) {
        return atlasImage->GetAtlasTextureID();
    }
    return image->m_textureID;
}

// ============================================================================
// FRAME COORDINATION
// ============================================================================

void Renderer::BeginFrame() {

    g_renderer2d->BeginFrame2D();
    g_renderer3d->BeginFrame3D();
    
    ResetFlushTimings();
    m_textureSwitches = 0;
}

void Renderer::EndFrame() {
    
    g_renderer2d->EndFrame2D();
    g_renderer3d->EndFrame3D();
    
    UpdateGpuTimings();
    
    m_prevTextureSwitches = m_textureSwitches;
}

// ============================================================================
// WINDOW MANAGEMENT
// ============================================================================

void Renderer::HandleWindowResize() {
    int screenW = g_windowManager->DrawableWidth();
    int screenH = g_windowManager->DrawableHeight();

    ResizeMSAAFramebuffer(screenW, screenH);
    
    if (g_renderer2d) {
        g_renderer2d->Reset2DViewport();
    }
}

// ============================================================================
// GPU TIMING
// ============================================================================

void Renderer::StartFlushTiming(const char* name) {

    m_currentFlushStartTime = GetHighResTime();
    
    //
    // Find timing entry
    
    for (int i = 0; i < m_flushTimingCount; i++) {
        if (strcmp(m_flushTimings[i].name, name) == 0) {
            
            if (m_flushTimings[i].queryObject == 0) {
#ifndef TARGET_EMSCRIPTEN
                glGenQueries(1, &m_flushTimings[i].queryObject);
#endif
            }
            
            //
            // Only start new query if previous one is complete
            
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
    // Create new timing entry
    
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
            // Update CPU timing
            
            m_flushTimings[i].totalTime += cpuTime;
            m_flushTimings[i].callCount++;
            return;
        }
    }
}

void Renderer::UpdateGpuTimings() {

    //
    // Call this once per frame to collect GPU timing results
    
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

// ============================================================================
// BITMAP 
// ============================================================================

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

void Renderer::SaveScreenshot() {
    float timeNow = GetHighResTime();
    char *screenshotsDir = ScreenshotsDirectory();

    int screenW = g_windowManager->DrawableWidth();
    int screenH = g_windowManager->DrawableHeight();
    
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

// ============================================================================
// SCENE MANAGEMENT
// ============================================================================

void Renderer::BeginScene() {
    SetBoundTexture(0);
    SetBlendMode(BlendModeNormal);
}

void Renderer::ClearScreen(bool _colour, bool _depth) {
    if (_colour) glClear(GL_COLOR_BUFFER_BIT);
    if (_depth) glClear(GL_DEPTH_BUFFER_BIT);
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
    // Query the maximum supported samples
    
    GLint maxSamples = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
    
    //
    // Generate and bind the MSAA framebuffer
    
    glGenFramebuffers(1, &m_msaaFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_msaaFBO);
    
    //
    // Create the multisampled color renderbuffer
    
    glGenRenderbuffers(1, &m_msaaColorRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, m_msaaColorRBO);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_msaaSamples, 
                                     GL_RGBA8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 
                             GL_RENDERBUFFER, m_msaaColorRBO);
    
    //
    // Create the multisampled depth renderbuffer
    
    glGenRenderbuffers(1, &m_msaaDepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, m_msaaDepthRBO);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_msaaSamples, 
                                     GL_DEPTH_COMPONENT24, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, 
                             GL_RENDERBUFFER, m_msaaDepthRBO);
    
    //
    // Check the framebuffer for completeness
    
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
    // Unbind the framebuffer

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

void Renderer::ResizeMSAAFramebuffer(int width, int height) {
    if (!m_msaaEnabled || m_msaaSamples <= 0) {
        return;
    }
    
    if (width == m_msaaWidth && height == m_msaaHeight) {
        return;
    }

    //
    // Destroy and reincarnate the FBO with new dimensions
    
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
// FONT MANAGEMENT
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
