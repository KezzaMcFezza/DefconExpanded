#include "systemiv.h"

#include <time.h>
#include <stdarg.h>
#include <algorithm>

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
#include "lib/math/matrix4f.h"
#include "lib/render/colour.h"
#include "shaders/vertex.glsl.h"
#include "shaders/color_fragment.glsl.h"
#include "shaders/texture_fragment.glsl.h"

#include "lib/render/renderer.h"
#include "renderer_2d.h"
#include "megavbo/megavbo_2d.h"

Renderer2D *g_renderer2d = NULL;

// ============================================================================
// CONSTRUCTOR & DESTRUCTOR
// ============================================================================

Renderer2D::Renderer2D()
    : m_alpha(1.0f),
      m_colourDepth(32),
      m_mouseMode(0),
      m_shaderProgram(0),
      m_colorShaderProgram(0),
      m_textureShaderProgram(0),
      m_VAO(0), 
      m_VBO(0),
      m_textVAO(0), 
      m_textVBO(0), 
      m_spriteVAO(0), 
      m_spriteVBO(0),
      m_rotatingSpriteVAO(0),
      m_rotatingSpriteVBO(0),
      m_lineVAO(0),
      m_lineVBO(0),
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
      m_immediateVAO(0), 
      m_immediateVBO(0),
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
      m_lineConversionBuffer(NULL),
      m_lineConversionBufferSize(0),
      m_currentLineColor(0, 0, 0, 0),
      m_currentLineWidth(1.0f),
      m_lineStripActive(false),
      m_lineStripColor(0, 0, 0, 0),
      m_lineStripWidth(1.0f),
      m_currentCircleWidth(1.0f),
      m_currentRectWidth(1.0f),
      m_activeBuffer(BUFFER_IMMEDIATE),
      m_batchingTextures(true)
{
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
      m_immediateTriangleCalls = 0;
      m_immediateLineCalls = 0;
      m_textCalls = 0;
      m_lineCalls = 0;
      m_staticSpriteCalls = 0;
      m_rotatingSpriteCalls = 0;
      m_circleCalls = 0;
      m_circleFillCalls = 0;
      m_rectCalls = 0;
      m_rectFillCalls = 0;
      m_triangleFillCalls = 0;
      m_lineVBOCalls = 0;
      m_quadVBOCalls = 0;
      m_triangleVBOCalls = 0;
      m_prevDrawCallsPerFrame = 0;
      m_prevImmediateTriangleCalls = 0;
      m_prevImmediateLineCalls = 0;
      m_prevTextCalls = 0;
      m_prevLineCalls = 0;
      m_prevStaticSpriteCalls = 0;
      m_prevRotatingSpriteCalls = 0;
      m_prevCircleCalls = 0;
      m_prevCircleFillCalls = 0;
      m_prevRectCalls = 0;
      m_prevRectFillCalls = 0;
      m_prevTriangleFillCalls = 0;
      m_prevLineVBOCalls = 0;
      m_prevQuadVBOCalls = 0;
      m_prevTriangleVBOCalls = 0;
      m_activeFontBatches = 0;
      m_prevActiveFontBatches = 0;

      //
      // Initialize font-aware batching system

      for (int i = 0; i < Renderer::MAX_FONT_ATLASES; i++) {
          m_fontBatches[i].vertexCount = 0;
          m_fontBatches[i].textureID = 0;
      }
    
      //
      // Allocate line conversion buffer
      
      m_lineConversionBufferSize = std::max(MAX_VERTICES * 2, MAX_LINE_VERTICES);
      m_lineConversionBuffer = new Vertex2D[m_lineConversionBufferSize];
    
      //
      // Initialize OpenGL components

      InitializeShaders();
      CacheUniformLocations();
      SetupVertexArrays();
      g_renderer->ResetFlushTimings();
    
    int msaaSamples = g_preferences ? g_preferences->GetInt(PREFS_SCREEN_ANTIALIAS, 0) : 0;
    
    //
    // Ensure the FBO is applied to the correct screen dimensions
    //
    // Use physical dimensions to prevent logical resolution scaling
    // from being applied to the framebuffer and causing clipping
    
    int screenW = g_windowManager->DrawableWidth();
    int screenH = g_windowManager->DrawableHeight();

    g_renderer->InitializeMSAAFramebuffer(screenW, screenH, msaaSamples);
    
}

Renderer2D::~Renderer2D() {
    if (m_colorShaderProgram) glDeleteProgram(m_colorShaderProgram);
    if (m_textureShaderProgram) glDeleteProgram(m_textureShaderProgram);
    if (m_VAO) glDeleteVertexArrays(1, &m_VAO);
    if (m_VBO) glDeleteBuffers(1, &m_VBO);
    if (m_textVAO) glDeleteVertexArrays(1, &m_textVAO);
    if (m_textVBO) glDeleteBuffers(1, &m_textVBO);
    if (m_spriteVAO) glDeleteVertexArrays(1, &m_spriteVAO);
    if (m_spriteVBO) glDeleteBuffers(1, &m_spriteVBO);
    if (m_rotatingSpriteVAO) glDeleteVertexArrays(1, &m_rotatingSpriteVAO);
    if (m_rotatingSpriteVBO) glDeleteBuffers(1, &m_rotatingSpriteVBO);
    if (m_lineVAO) glDeleteVertexArrays(1, &m_lineVAO);
    if (m_lineVBO) glDeleteBuffers(1, &m_lineVBO);
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
    if (m_immediateVAO) glDeleteVertexArrays(1, &m_immediateVAO);
    if (m_immediateVBO) glDeleteBuffers(1, &m_immediateVBO);
    
    if (m_lineConversionBuffer) {
        delete[] m_lineConversionBuffer;
        m_lineConversionBuffer = NULL;
    }
}

// ============================================================================
// BLIT
// ============================================================================

void Renderer2D::Blit(Image *src, float x, float y, Colour const &col) {
    float w = src->Width();
    float h = src->Height();
    Blit(src, x, y, w, h, col);
}

void Renderer2D::Blit(Image *src, float x, float y, float w, float h, Colour const &col) {    
    if (m_triangleVertexCount + 6 > MAX_VERTICES) {
        FlushTriangles(true);
    }
    
    float r = col.GetRFloat(), g = col.GetGFloat(), b = col.GetBFloat(), a = col.GetAFloat();
    
    g_renderer->SetActiveTexture(GL_TEXTURE0);
    g_renderer->SetBoundTexture(src->m_textureID);
    g_renderer->SetTextureParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    if (src->m_mipmapping) {
        g_renderer->SetTextureParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        g_renderer->SetTextureParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    
    // Get UV coordinates - atlas sprites use specific regions, others use full texture
    float u1, v1, u2, v2;
    g_renderer->GetImageUVCoords(src, u1, v1, u2, v2);
    
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

void Renderer2D::Blit(Image *src, float x, float y, float w, float h, Colour const &col, float angle) {    
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
    
    g_renderer->SetActiveTexture(GL_TEXTURE0);
    g_renderer->SetBoundTexture(src->m_textureID);
    g_renderer->SetTextureParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    if (src->m_mipmapping) {
        g_renderer->SetTextureParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        g_renderer->SetTextureParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    
    // get UV coordinates, atlas sprites use specific regions, others use full texture
    float u1, v1, u2, v2;
    g_renderer->GetImageUVCoords(src, u1, v1, u2, v2);
    
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

void Renderer2D::Set2DViewport(float l, float r, float b, float t, int x, int y, int w, int h) {
    m_modelViewMatrix.LoadIdentity();
    m_projectionMatrix.Ortho(l - 0.325f, r - 0.325f, b - 0.325f, t - 0.325f, -1.0f, 1.0f);

    //
    // Calculate scale factors between logical and physical resolution

    float scaleX = (float)g_windowManager->DrawableWidth()  / (float)g_windowManager->WindowW();
    float scaleY = (float)g_windowManager->DrawableHeight() / (float)g_windowManager->WindowH();
    
    //
    // Apply scaling to convert logical coordinates to physical viewport coordinates
    
    int physicalX = (int)(x * scaleX);
    int physicalY = (int)((g_windowManager->WindowH() - h - y) * scaleY);
    int physicalW = (int)(w * scaleX);
    int physicalH = (int)(h * scaleY);

    g_renderer->SetViewport(physicalX, physicalY, physicalW, physicalH);
}

void Renderer2D::Reset2DViewport() {
    Set2DViewport(0, g_windowManager->WindowW(), g_windowManager->WindowH(), 0, 
                  0, 0, g_windowManager->WindowW(), g_windowManager->WindowH());
    ResetClip();
}


void Renderer2D::BeginFrame2D() {
    ResetFrameCounters();
}

void Renderer2D::EndFrame2D() {

}

void Renderer2D::Shutdown() {
    // Intentionally empty
}


void Renderer2D::SetClip(int x, int y, int w, int h) {
    
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

    float scaleX = (float)g_windowManager->DrawableWidth()  / (float)g_windowManager->WindowW();
    float scaleY = (float)g_windowManager->DrawableHeight() / (float)g_windowManager->WindowH();

    int sx = int(x * scaleX);
    int sy = int((g_windowManager->WindowH() - h - y) * scaleY);
    int sw = int(w * scaleX);
    int sh = int(h * scaleY);

    g_renderer->SetScissor(sx, sy, sw, sh);
    g_renderer->SetScissorTest(true);
}

void Renderer2D::ResetClip() {

    EndRectFillBatch();
    EndTriangleFillBatch();
    EndStaticSpriteBatch(); 
    EndLineBatch();
    EndRectBatch();
    EndTextBatch();
    
    g_renderer->SetScissorTest(false);
}


//
// shader and buffer managment
//

void Renderer2D::CacheUniformLocations() {

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


//
// create shader programs using shader sources from .glsl.h headers

void Renderer2D::InitializeShaders() {
    m_colorShaderProgram = g_renderer->CreateShader(VERTEX_2D_SHADER_SOURCE, COLOR_FRAGMENT_2D_SHADER_SOURCE);
    m_textureShaderProgram = g_renderer->CreateShader(VERTEX_2D_SHADER_SOURCE, TEXTURE_FRAGMENT_2D_SHADER_SOURCE);
    m_shaderProgram = m_colorShaderProgram;
}

void Renderer2D::SetupVertexArrays() {

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
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex2D) * (MAX_STATIC_SPRITE_VERTICES), NULL, GL_STREAM_DRAW);
    setupVertexAttributes();

    //
    // Rotating sprites

    glGenVertexArrays(1, &m_rotatingSpriteVAO);
    glGenBuffers(1, &m_rotatingSpriteVBO);
    glBindVertexArray(m_rotatingSpriteVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_rotatingSpriteVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex2D) * MAX_ROTATING_SPRITE_VERTICES, NULL, GL_STREAM_DRAW);
    setupVertexAttributes();
    
    //
    // Create Line VAO/VBO pair

    glGenVertexArrays(1, &m_lineVAO);
    glGenBuffers(1, &m_lineVBO);
    glBindVertexArray(m_lineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_lineVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex2D) * (MAX_LINE_VERTICES), NULL, GL_STREAM_DRAW);
    setupVertexAttributes();
    
    //
    // Create circle VAO/VBO pair

    glGenVertexArrays(1, &m_circleVAO);
    glGenBuffers(1, &m_circleVBO);
    glBindVertexArray(m_circleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_circleVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex2D) * MAX_CIRCLE_VERTICES, NULL, GL_STREAM_DRAW);
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
    // Create immediate VAO/VBO pair

    glGenVertexArrays(1, &m_immediateVAO);
    glGenBuffers(1, &m_immediateVBO);
    glBindVertexArray(m_immediateVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_immediateVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex2D) * MAX_VERTICES, NULL, GL_STREAM_DRAW);
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

void Renderer2D::SetColorShaderUniforms() {
    glUniformMatrix4fv(m_colorShaderUniforms.projectionLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(m_colorShaderUniforms.modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
}

void Renderer2D::SetTextureShaderUniforms() {
    glUniformMatrix4fv(m_textureShaderUniforms.projectionLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(m_textureShaderUniforms.modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    glUniform1i(m_textureShaderUniforms.textureLoc, 0);
}

void Renderer2D::UploadVertexData(const Vertex2D* vertices, int vertexCount) {
    if (vertexCount == 0) return;
    
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertexCount * sizeof(Vertex2D), vertices);
}

void Renderer2D::UploadVertexDataToVBO(unsigned int vbo,
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


void Renderer2D::ResetFrameCounters() {    
    
    //
    // store previous frames data

    m_prevDrawCallsPerFrame = m_drawCallsPerFrame;
    m_prevImmediateTriangleCalls = m_immediateTriangleCalls;
    m_prevImmediateLineCalls = m_immediateLineCalls;
    m_prevTextCalls = m_textCalls;
    m_prevLineCalls = m_lineCalls;
    m_prevStaticSpriteCalls = m_staticSpriteCalls;
    m_prevRotatingSpriteCalls = m_rotatingSpriteCalls;
    m_prevCircleCalls = m_circleCalls;
    m_prevCircleFillCalls = m_circleFillCalls;
    m_prevRectCalls = m_rectCalls;
    m_prevRectFillCalls = m_rectFillCalls;
    m_prevTriangleFillCalls = m_triangleFillCalls;
    m_prevLineVBOCalls = m_lineVBOCalls;
    m_prevQuadVBOCalls = m_quadVBOCalls;
    m_prevTriangleVBOCalls = m_triangleVBOCalls;
    m_prevActiveFontBatches = m_activeFontBatches;
    
    //
    // reset current frame counters

    m_drawCallsPerFrame = 0;
    m_immediateTriangleCalls = 0;
    m_immediateLineCalls = 0;
    m_textCalls = 0;
    m_lineCalls = 0;
    m_staticSpriteCalls = 0;
    m_rotatingSpriteCalls = 0;
    m_circleCalls = 0;
    m_circleFillCalls = 0;
    m_rectCalls = 0;
    m_rectFillCalls = 0;
    m_triangleFillCalls = 0;
    m_lineVBOCalls = 0;
    m_quadVBOCalls = 0;
    m_triangleVBOCalls = 0;
    m_activeFontBatches = 0;
}

//
// increment draw calls for the 2d renderer

void Renderer2D::IncrementDrawCall(const char* bufferType) {
    m_drawCallsPerFrame++;
    
    constexpr auto hash = [](const char* str) constexpr {
        uint32_t hash = 5381;
        while (*str) {
            hash = ((hash << 5) + hash) + *str++;
        }
        return hash;
    };
    
    switch (hash(bufferType)) {
        case hash("immediate_triangles"): m_immediateTriangleCalls++; break;
        case hash("immediate_lines"): m_immediateLineCalls++; break;
        case hash("text"): m_textCalls++; break;
        case hash("lines"): m_lineCalls++; break;
        case hash("static_sprites"): m_staticSpriteCalls++; break;
        case hash("rotating_sprites"): m_rotatingSpriteCalls++; break;
        case hash("circles"): m_circleCalls++; break;
        case hash("circle_fills"): m_circleFillCalls++; break;
        case hash("rects"): m_rectCalls++; break;
        case hash("rect_fills"): m_rectFillCalls++; break;
        case hash("triangle_fills"): m_triangleFillCalls++; break;
        case hash("line_vbo"): m_lineVBOCalls++; break;
        case hash("quad_vbo"): m_quadVBOCalls++; break;
        case hash("triangle_vbo"): m_triangleVBOCalls++; break;
    }
}
