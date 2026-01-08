#include "systemiv.h"

#ifdef RENDERER_OPENGL
#include "renderer_2d_opengl.h"
#include "lib/render/renderer.h"
#include "lib/render/renderer_opengl.h"
#include "lib/gucci/window_manager.h"
#include "lib/preferences.h"
#include "shaders/vertex.glsl.h"
#include "shaders/color_fragment.glsl.h"
#include "shaders/texture_fragment.glsl.h"

// ============================================================================
// CONSTRUCTOR & DESTRUCTOR
// ============================================================================

Renderer2DOpenGL::Renderer2DOpenGL()
{
    InitializeShaders();
    CacheUniformLocations();
    SetupVertexArrays();
    g_renderer->ResetFlushTimings();
    
    int msaaSamples = g_preferences ? g_preferences->GetInt(PREFS_SCREEN_ANTIALIAS, 0) : 0;
    
    int screenW = g_windowManager->DrawableWidth();
    int screenH = g_windowManager->DrawableHeight();

    g_renderer->InitializeMSAAFramebuffer(screenW, screenH, msaaSamples);
}

Renderer2DOpenGL::~Renderer2DOpenGL()
{
    CleanupBuffers();
}

// ============================================================================
// SHADER MANAGEMENT
// ============================================================================

void Renderer2DOpenGL::InitializeShaders()
{
    m_colorShaderProgram = g_renderer->CreateShader(VERTEX_2D_SHADER_SOURCE, COLOR_FRAGMENT_2D_SHADER_SOURCE);
    m_textureShaderProgram = g_renderer->CreateShader(VERTEX_2D_SHADER_SOURCE, TEXTURE_FRAGMENT_2D_SHADER_SOURCE);
    m_lineShaderProgram = m_colorShaderProgram; // OpenGL uses same shader, handles line width via glLineWidth
    m_shaderProgram = m_colorShaderProgram;
}

void Renderer2DOpenGL::CacheUniformLocations()
{
    //
    // Cache color shader uniforms

    m_colorShaderUniforms.projectionLoc = glGetUniformLocation(m_colorShaderProgram, "uProjection");
    m_colorShaderUniforms.modelViewLoc = glGetUniformLocation(m_colorShaderProgram, "uModelView");
    m_colorShaderUniforms.textureLoc = -1; // not used in color shader
    
    //
    // Cache texture shader uniforms  

    m_textureShaderUniforms.projectionLoc = glGetUniformLocation(m_textureShaderProgram, "uProjection");
    m_textureShaderUniforms.modelViewLoc = glGetUniformLocation(m_textureShaderProgram, "uModelView");
    m_textureShaderUniforms.textureLoc = glGetUniformLocation(m_textureShaderProgram, "ourTexture");
}

void Renderer2DOpenGL::SetColorShaderUniforms()
{
    glUniformMatrix4fv(m_colorShaderUniforms.projectionLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(m_colorShaderUniforms.modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
}

void Renderer2DOpenGL::SetLineShaderUniforms(float lineWidth)
{
    glUniformMatrix4fv(m_colorShaderUniforms.projectionLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(m_colorShaderUniforms.modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    
#ifndef TARGET_EMSCRIPTEN
    g_renderer->SetLineWidth(lineWidth);
#endif
}

void Renderer2DOpenGL::SetTextureShaderUniforms()
{
    glUniformMatrix4fv(m_textureShaderUniforms.projectionLoc, 1, GL_FALSE, m_projectionMatrix.m);
    glUniformMatrix4fv(m_textureShaderUniforms.modelViewLoc, 1, GL_FALSE, m_modelViewMatrix.m);
    glUniform1i(m_textureShaderUniforms.textureLoc, 0);
}

// ============================================================================
// BUFFER MANAGEMENT
// ============================================================================

void Renderer2DOpenGL::SetupVertexArrays()
{
    //
    // Set up vertex attributes for Vertex2D format

    auto setupVertexAttributes = []() 
    {
        //
        // Position attribute (x, y)

        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), (void*)0);
        glEnableVertexAttribArray(0);

        //
        // Color attribute (r, g, b, a)

        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);

        //
        // Texture coordinate attribute (u, v)

        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);
    };
    
    //
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

void Renderer2DOpenGL::UploadVertexData(const Vertex2D* vertices, int vertexCount)
{
    if (vertexCount == 0) return;
    
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertexCount * sizeof(Vertex2D), vertices);
}

void Renderer2DOpenGL::UploadVertexDataToVBO(unsigned int vbo,
                                              const Vertex2D* vertices,
                                              int vertexCount,
                                              unsigned int usageHint)
{
    if (vertexCount <= 0 || !vertices) return;

    const GLsizeiptr bytes = static_cast<GLsizeiptr>(vertexCount) * sizeof(Vertex2D);

    if (usageHint == GL_STATIC_DRAW) 
    {
        glBufferData(GL_ARRAY_BUFFER, bytes, vertices, usageHint);
    } 
    else 
    {
        glBufferData(GL_ARRAY_BUFFER, bytes, NULL, usageHint);
        glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, vertices);
    }
}

// ============================================================================
// CORE FLUSH FUNCTIONS
// ============================================================================

void Renderer2DOpenGL::FlushTriangles(bool useTexture)
{
    if (m_triangleVertexCount == 0) return;
    
    g_renderer->StartFlushTiming("Immediate_Triangles");
    IncrementDrawCall("immediate_triangles");
    
    if (useTexture) 
    {
        g_renderer->SetShaderProgram(m_textureShaderProgram);
        SetTextureShaderUniforms();
    } 
    else 
    {
        g_renderer->SetShaderProgram(m_colorShaderProgram);
        SetColorShaderUniforms();
    }
    
    g_renderer->SetVertexArray(m_immediateVAO);
    g_renderer->SetArrayBuffer(m_immediateVBO);
    UploadVertexDataToVBO(m_immediateVBO, m_triangleVertices, m_triangleVertexCount, GL_STREAM_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_triangleVertexCount);
    
    m_triangleVertexCount = 0;
    
    g_renderer->EndFlushTiming("Immediate_Triangles");
}

void Renderer2DOpenGL::FlushTextBuffer()
{
    if (m_textVertexCount == 0) return;
    
    g_renderer->StartFlushTiming("Text");
    IncrementDrawCall("text");
    
    int currentBlendSrc = g_renderer->GetCurrentBlendSrcFactor();
    int currentBlendDst = g_renderer->GetCurrentBlendDstFactor();
    
    if (g_renderer->GetBlendMode() != Renderer::BlendModeSubtractive) 
    {
        g_renderer->SetBlendMode(Renderer::BlendModeAdditive);
    }
    
    g_renderer->SetShaderProgram(m_textureShaderProgram);
    SetTextureShaderUniforms();
    
    if (m_currentTextTexture != 0) 
    {
        g_renderer->SetActiveTexture(GL_TEXTURE0);
        g_renderer->SetBoundTexture(m_currentTextTexture);
    }
    
    g_renderer->SetVertexArray(m_textVAO);
    g_renderer->SetArrayBuffer(m_textVBO);
    UploadVertexDataToVBO(m_textVBO, m_textVertices, m_textVertexCount, GL_DYNAMIC_DRAW);  
    
    glDrawArrays(GL_TRIANGLES, 0, m_textVertexCount);
    
    g_renderer->SetBlendFunc(currentBlendSrc, currentBlendDst);
    
    m_textVertexCount = 0;
    m_currentTextTexture = 0;
    
    g_renderer->EndFlushTiming("Text");
}

void Renderer2DOpenGL::FlushLines()
{
    if (m_lineVertexCount == 0) return;
    
    g_renderer->StartFlushTiming("Lines");
    IncrementDrawCall("lines");
    
#ifndef TARGET_EMSCRIPTEN
    g_renderer->SetLineWidth(m_currentLineWidth);
#endif
    
    g_renderer->SetShaderProgram(m_colorShaderProgram);
    SetColorShaderUniforms();
    
    g_renderer->SetVertexArray(m_lineVAO);
    g_renderer->SetArrayBuffer(m_lineVBO);
    UploadVertexDataToVBO(m_lineVBO, m_lineVertices, m_lineVertexCount, GL_DYNAMIC_DRAW); 
    
    glDrawArrays(GL_LINES, 0, m_lineVertexCount);
    
    m_lineVertexCount = 0;
    
    g_renderer->EndFlushTiming("Lines"); 
}

void Renderer2DOpenGL::FlushStaticSprites()
{
    if (m_staticSpriteVertexCount == 0) return;
    
    g_renderer->StartFlushTiming("Static_Sprite");
    IncrementDrawCall("static_sprites");
    
    g_renderer->SetShaderProgram(m_textureShaderProgram);
    SetTextureShaderUniforms();
    
    if (m_currentStaticSpriteTexture != 0) 
    {
        g_renderer->SetActiveTexture(GL_TEXTURE0);
        g_renderer->SetBoundTexture(m_currentStaticSpriteTexture);
    }
    
    g_renderer->SetVertexArray(m_spriteVAO);
    g_renderer->SetArrayBuffer(m_spriteVBO);
    UploadVertexDataToVBO(m_spriteVBO, m_staticSpriteVertices, m_staticSpriteVertexCount, GL_STREAM_DRAW);   
    
    glDrawArrays(GL_TRIANGLES, 0, m_staticSpriteVertexCount);
    
    m_staticSpriteVertexCount = 0;
    
    g_renderer->EndFlushTiming("Static_Sprite");
}

void Renderer2DOpenGL::FlushRotatingSprite()
{
    if (m_rotatingSpriteVertexCount == 0) return;
    
    g_renderer->StartFlushTiming("Rotating_Sprite");
    IncrementDrawCall("rotating_sprites");
    
    g_renderer->SetShaderProgram(m_textureShaderProgram);
    SetTextureShaderUniforms();
    
    if (m_currentRotatingSpriteTexture != 0) 
    {
        g_renderer->SetActiveTexture(GL_TEXTURE0);
        g_renderer->SetBoundTexture(m_currentRotatingSpriteTexture);
    }
    
    g_renderer->SetVertexArray(m_rotatingSpriteVAO);
    g_renderer->SetArrayBuffer(m_rotatingSpriteVBO);
    UploadVertexDataToVBO(m_rotatingSpriteVBO, m_rotatingSpriteVertices, m_rotatingSpriteVertexCount, GL_STREAM_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_rotatingSpriteVertexCount);
    
    m_rotatingSpriteVertexCount = 0;
    
    g_renderer->EndFlushTiming("Rotating_Sprite");
}

void Renderer2DOpenGL::FlushCircles()
{
    if (m_circleVertexCount == 0) return;
    
    g_renderer->StartFlushTiming("Circles");
    IncrementDrawCall("circles");
    
#ifndef TARGET_EMSCRIPTEN
    g_renderer->SetLineWidth(m_currentCircleWidth);
#endif
    
    g_renderer->SetShaderProgram(m_colorShaderProgram);
    SetColorShaderUniforms();
    
    g_renderer->SetVertexArray(m_circleVAO);
    g_renderer->SetArrayBuffer(m_circleVBO);
    UploadVertexDataToVBO(m_circleVBO, m_circleVertices, m_circleVertexCount, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_LINES, 0, m_circleVertexCount);
    
    m_circleVertexCount = 0;
    
    g_renderer->EndFlushTiming("Circles");
}

void Renderer2DOpenGL::FlushCircleFills()
{
    if (m_circleFillVertexCount == 0) return;
    
    g_renderer->StartFlushTiming("Circle_Fills");
    IncrementDrawCall("circle_fills");
    
    g_renderer->SetShaderProgram(m_colorShaderProgram);
    SetColorShaderUniforms();
    
    g_renderer->SetVertexArray(m_circleFillVAO);
    g_renderer->SetArrayBuffer(m_circleFillVBO);
    UploadVertexDataToVBO(m_circleFillVBO, m_circleFillVertices, m_circleFillVertexCount, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_circleFillVertexCount);
    
    m_circleFillVertexCount = 0;
    
    g_renderer->EndFlushTiming("Circle_Fills");
}

void Renderer2DOpenGL::FlushRects()
{
    if (m_rectVertexCount == 0) return;
    
    g_renderer->StartFlushTiming("Rects");
    IncrementDrawCall("rects");
    
#ifndef TARGET_EMSCRIPTEN
    g_renderer->SetLineWidth(m_currentRectWidth);
#endif
    
    g_renderer->SetShaderProgram(m_colorShaderProgram);
    SetColorShaderUniforms();
    
    g_renderer->SetVertexArray(m_rectVAO);
    g_renderer->SetArrayBuffer(m_rectVBO);
    UploadVertexDataToVBO(m_rectVBO, m_rectVertices, m_rectVertexCount, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_LINES, 0, m_rectVertexCount);
    
    m_rectVertexCount = 0;
    
    g_renderer->EndFlushTiming("Rects");
}

void Renderer2DOpenGL::FlushRectFills()
{
    if (m_rectFillVertexCount == 0) return;
    
    g_renderer->StartFlushTiming("Rect_Fills");
    IncrementDrawCall("rect_fills");
    
    g_renderer->SetShaderProgram(m_colorShaderProgram);
    SetColorShaderUniforms();
    
    g_renderer->SetVertexArray(m_rectFillVAO);
    g_renderer->SetArrayBuffer(m_rectFillVBO);
    UploadVertexDataToVBO(m_rectFillVBO, m_rectFillVertices, m_rectFillVertexCount, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_rectFillVertexCount);
    
    m_rectFillVertexCount = 0;
    
    g_renderer->EndFlushTiming("Rect_Fills");
}

void Renderer2DOpenGL::FlushTriangleFills()
{
    if (m_triangleFillVertexCount == 0) return;
    
    g_renderer->StartFlushTiming("Triangle_Fills");
    IncrementDrawCall("triangle_fills");
    
    g_renderer->SetShaderProgram(m_colorShaderProgram);
    SetColorShaderUniforms();
    
    g_renderer->SetVertexArray(m_triangleFillVAO);
    g_renderer->SetArrayBuffer(m_triangleFillVBO);
    UploadVertexDataToVBO(m_triangleFillVBO, m_triangleFillVertices, m_triangleFillVertexCount, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_triangleFillVertexCount);
    
    m_triangleFillVertexCount = 0;
    
    g_renderer->EndFlushTiming("Triangle_Fills");
}

// ============================================================================
// MEGAVBO BUFFER MANAGEMENT
// ============================================================================

unsigned int Renderer2DOpenGL::CreateMegaVBOVertexBuffer(size_t size, BufferUsageHint usageHint)
{
    GLenum glUsage;

    switch (usageHint) 
    {
        case BUFFER_USAGE_STATIC_DRAW:  glUsage = GL_STATIC_DRAW; break;
        case BUFFER_USAGE_DYNAMIC_DRAW: glUsage = GL_DYNAMIC_DRAW; break;
        case BUFFER_USAGE_STREAM_DRAW:  glUsage = GL_STREAM_DRAW; break;
        default: glUsage = GL_STATIC_DRAW; break;
    }
    
    unsigned int vbo;
    glGenBuffers(1, &vbo);
    g_renderer->SetArrayBuffer(vbo);
    glBufferData(GL_ARRAY_BUFFER, size, NULL, glUsage);
    g_renderer->SetArrayBuffer(0);
    return vbo;
}

unsigned int Renderer2DOpenGL::CreateMegaVBOIndexBuffer(size_t size, BufferUsageHint usageHint)
{
    GLenum glUsage;
    switch (usageHint) 
    {
        case BUFFER_USAGE_STATIC_DRAW:  glUsage = GL_STATIC_DRAW; break;
        case BUFFER_USAGE_DYNAMIC_DRAW: glUsage = GL_DYNAMIC_DRAW; break;
        case BUFFER_USAGE_STREAM_DRAW:  glUsage = GL_STREAM_DRAW; break;
        default: glUsage = GL_STATIC_DRAW; break;
    }
    
    unsigned int ibo;
    glGenBuffers(1, &ibo);
    return ibo;
}

unsigned int Renderer2DOpenGL::CreateMegaVBOVertexArray()
{
    unsigned int vao;
    glGenVertexArrays(1, &vao);
    return vao;
}

void Renderer2DOpenGL::DeleteMegaVBOVertexBuffer(unsigned int buffer)
{
    if (buffer != 0) 
    {
        glDeleteBuffers(1, &buffer);
    }
}

void Renderer2DOpenGL::DeleteMegaVBOIndexBuffer(unsigned int buffer)
{
    if (buffer != 0) 
    {
        glDeleteBuffers(1, &buffer);
    }
}

void Renderer2DOpenGL::DeleteMegaVBOVertexArray(unsigned int vao)
{
    if (vao != 0) 
    {
        glDeleteVertexArrays(1, &vao);
    }
}

void Renderer2DOpenGL::SetupMegaVBOVertexAttributes2D(unsigned int vao, unsigned int vbo, unsigned int ibo)
{
    g_renderer->SetVertexArray(vao);
    g_renderer->SetArrayBuffer(vbo);
    
    // Position attribute (x, y)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Color attribute (r, g, b, a)
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    // Texture coordinate attribute (u, v)
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    // Bind index buffer to VAO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
}

void Renderer2DOpenGL::UploadMegaVBOIndexData(unsigned int ibo, const unsigned int* indices,
                                             int indexCount, BufferUsageHint usageHint)
{
    GLenum glUsage;
    switch (usageHint) 
    {
        case BUFFER_USAGE_STATIC_DRAW:  glUsage = GL_STATIC_DRAW; break;
        case BUFFER_USAGE_DYNAMIC_DRAW: glUsage = GL_DYNAMIC_DRAW; break;
        case BUFFER_USAGE_STREAM_DRAW:  glUsage = GL_STREAM_DRAW; break;
        default: glUsage = GL_STATIC_DRAW; break;
    }
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexCount * sizeof(unsigned int), indices, glUsage);
}

void Renderer2DOpenGL::UploadMegaVBOVertexData(unsigned int vbo, const Vertex2D* vertices,
                                               int vertexCount, BufferUsageHint usageHint)
{
    GLenum glUsage;
    switch (usageHint) 
    {
        case BUFFER_USAGE_STATIC_DRAW:  glUsage = GL_STATIC_DRAW; break;
        case BUFFER_USAGE_DYNAMIC_DRAW: glUsage = GL_DYNAMIC_DRAW; break;
        case BUFFER_USAGE_STREAM_DRAW:  glUsage = GL_STREAM_DRAW; break;
        default: glUsage = GL_STATIC_DRAW; break;
    }
    
    g_renderer->SetArrayBuffer(vbo);
    glBufferData(GL_ARRAY_BUFFER, vertexCount * sizeof(Vertex2D), vertices, glUsage);
}

void Renderer2DOpenGL::DrawMegaVBOIndexed(PrimitiveType primitiveType, unsigned int indexCount)
{
    GLenum glPrimitive;
    switch (primitiveType) 
    {
        case PRIMITIVE_TRIANGLES:   glPrimitive = GL_TRIANGLES; break;
        case PRIMITIVE_LINE_STRIP:  glPrimitive = GL_LINE_STRIP; break;
        case PRIMITIVE_LINES:       glPrimitive = GL_LINES; break;
        default: glPrimitive = GL_TRIANGLES; break;
    }
    
    glDrawElements(glPrimitive, indexCount, GL_UNSIGNED_INT, 0);
}

void Renderer2DOpenGL::EnableMegaVBOPrimitiveRestart(unsigned int restartIndex)
{
#ifndef TARGET_EMSCRIPTEN
    glEnable(GL_PRIMITIVE_RESTART);
    glPrimitiveRestartIndex(restartIndex);
#endif
}

void Renderer2DOpenGL::DisableMegaVBOPrimitiveRestart()
{
#ifndef TARGET_EMSCRIPTEN
    glDisable(GL_PRIMITIVE_RESTART);
#endif
}

// ============================================================================
// STATE CACHE INVALIDATION
// ============================================================================

void Renderer2DOpenGL::InvalidateStateCache()
{
    RendererOpenGL* rendererOpenGL = dynamic_cast<RendererOpenGL*>(g_renderer);
    if (rendererOpenGL) 
    {
        g_renderer->SetShaderProgram(0);
        g_renderer->SetVertexArray(0);
        
        g_renderer->SetArrayBuffer(0);
        g_renderer->SetElementBuffer(0);
        
        g_renderer->SetBoundTexture(0);
    }
}

// ============================================================================
// CLEANUP
// ============================================================================

void Renderer2DOpenGL::CleanupBuffers()
{
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
}

#endif // RENDERER_OPENGL

