#include "lib/resource/image.h"
#include "systemiv.h"

#include <math.h>
#include <string.h>

#include "lib/debug_utils.h"
#include "lib/hi_res_time.h"
#include "lib/render/renderer.h"
#include "lib/render2d/renderer_2d.h"
#include "lib/resource/sprite_atlas.h"
#include "lib/resource/image.h"
#include "lib/math/matrix4f.h"
#include "shaders/vertex.glsl.h"
#include "shaders/fragment.glsl.h"
#include "shaders/textured_vertex.glsl.h"
#include "shaders/textured_fragment.glsl.h"
#include "shaders/model_vertex.glsl.h"

#include "renderer_3d.h"

Renderer3D *g_renderer3d = NULL;

// ============================================================================
// CONSTRUCTOR & DESTRUCTOR
// ============================================================================

Renderer3D::Renderer3D()
:   m_shader3DProgram(0),
    m_shader3DTexturedProgram(0),
    m_shader3DModelProgram(0),
    m_VAO3D(0), m_VBO3D(0),
    m_VAO3DTextured(0), m_VBO3DTextured(0),
    m_spriteVAO3D(0), m_spriteVBO3D(0),
    m_lineVAO3D(0), m_lineVBO3D(0),
    m_circleVAO3D(0), m_circleVBO3D(0),
    m_circleFillVAO3D(0), m_circleFillVBO3D(0),
    m_rectVAO3D(0), m_rectVBO3D(0),
    m_rectFillVAO3D(0), m_rectFillVBO3D(0),
    m_triangleFillVAO3D(0), m_triangleFillVBO3D(0),
    m_immediateVAO3D(0), m_immediateVBO3D(0),
    m_currentShaderProgram3D(0),
    m_matrices3DNeedUpdate(true),
    m_fog3DNeedsUpdate(true),
    m_vertex3DCount(0),
    m_vertex3DTexturedCount(0),
    m_lineStrip3DActive(false),
    m_lineStrip3DWidth(1.0f),
    m_currentLineWidth3D(1.0f),
    m_currentCircleWidth3D(1.0f),
    m_currentRectWidth3D(1.0f),
    m_texturedQuad3DActive(false),
    m_currentTexture3D(0),
    m_lineConversionBuffer3D(NULL),
    m_lineConversionBufferSize3D(0),
    m_lineVertexCount3D(0),
    m_staticSpriteVertexCount3D(0),
    m_currentStaticSpriteTexture3D(0),
    m_rotatingSpriteVertexCount3D(0),
    m_currentRotatingSpriteTexture3D(0),
    m_textVertexCount3D(0),
    m_currentTextTexture3D(0),
    m_currentFontBatchIndex3D(0),
    m_textVertices3D(nullptr),
    m_circleVertexCount3D(0),
    m_circleFillVertexCount3D(0),
    m_rectVertexCount3D(0),
    m_rectFillVertexCount3D(0),
    m_triangleFillVertexCount3D(0)
{
    // Initialize fog parameters
    m_fogEnabled = false;
    m_fogOrientationBased = false;
    m_fogStart = 1.0f;
    m_fogEnd = 10.0f;
    m_fogDensity = 1.0f;
    m_fogColor[0] = 0.0f; // R
    m_fogColor[1] = 0.0f; // G
    m_fogColor[2] = 0.0f; // B
    m_fogColor[3] = 1.0f; // A
    m_cameraPos[0] = 0.0f; // X
    m_cameraPos[1] = 0.0f; // Y
    m_cameraPos[2] = 0.0f; // Z

    m_drawCallsPerFrame3D = 0;
    m_immediateVertexCalls3D = 0;
    m_immediateTriangleCalls3D = 0;
    m_lineCalls3D = 0;
    m_staticSpriteCalls3D = 0;
    m_rotatingSpriteCalls3D = 0;
    m_textCalls3D = 0;
    m_megaVBOCalls3D = 0;
    m_circleCalls3D = 0;
    m_circleFillCalls3D = 0;
    m_rectCalls3D = 0;
    m_rectFillCalls3D = 0;
    m_triangleFillCalls3D = 0;
    m_lineVBOCalls3D = 0;
    m_quadVBOCalls3D = 0;
    m_triangleVBOCalls3D = 0;
    m_prevDrawCallsPerFrame3D = 0;
    m_prevImmediateLineCalls3D = 0;
    m_prevImmediateTriangleCalls3D = 0;
    m_prevLineCalls3D = 0;
    m_prevStaticSpriteCalls3D = 0;
    m_prevRotatingSpriteCalls3D = 0;
    m_prevTextCalls3D = 0;
    m_prevMegaVBOCalls3D = 0;
    m_prevCircleCalls3D = 0;
    m_prevCircleFillCalls3D = 0;
    m_prevRectCalls3D = 0;
    m_prevRectFillCalls3D = 0;
    m_prevTriangleFillCalls3D = 0;
    m_prevLineVBOCalls3D = 0;
    m_prevQuadVBOCalls3D = 0;
    m_prevTriangleVBOCalls3D = 0;
    m_activeFontBatches3D = 0;
    m_prevActiveFontBatches3D = 0;
    
    Initialize3DShaders();
    Cache3DUniformLocations();
    Setup3DVertexArrays();
    Setup3DTexturedVertexArrays();
    
    m_lineConversionBufferSize3D = MAX_3D_VERTICES * 2;
    m_lineConversionBuffer3D = new Vertex3D[m_lineConversionBufferSize3D];
    
    //
    // Initialize font-aware batching system

    for (int i = 0; i < Renderer::MAX_FONT_ATLASES; i++) {
        m_fontBatches3D[i].vertexCount = 0;
        m_fontBatches3D[i].textureID = 0;
    }
    
    m_currentFontBatchIndex3D = 0;
    m_textVertices3D = m_fontBatches3D[0].vertices;
    m_textVertexCount3D = 0;
    m_currentTextTexture3D = 0;
}

Renderer3D::~Renderer3D() {
    Shutdown();
}

void Renderer3D::Shutdown() {
    if (m_VBO3D) {
        glDeleteBuffers(1, &m_VBO3D);
        m_VBO3D = 0;
    }
    if (m_VAO3D) {
        glDeleteVertexArrays(1, &m_VAO3D);
        m_VAO3D = 0;
    }
    if (m_VBO3DTextured) {
        glDeleteBuffers(1, &m_VBO3DTextured);
        m_VBO3DTextured = 0;
    }
    if (m_VAO3DTextured) {
        glDeleteVertexArrays(1, &m_VAO3DTextured);
        m_VAO3DTextured = 0;
    }
    
    //
    // Clean up 3D VAOs and VBOs

    if (m_spriteVAO3D) glDeleteVertexArrays(1, &m_spriteVAO3D);
    if (m_spriteVBO3D) glDeleteBuffers(1, &m_spriteVBO3D);
    if (m_lineVAO3D) glDeleteVertexArrays(1, &m_lineVAO3D);
    if (m_lineVBO3D) glDeleteBuffers(1, &m_lineVBO3D);
    if (m_textVAO3D) glDeleteVertexArrays(1, &m_textVAO3D);
    if (m_textVBO3D) glDeleteBuffers(1, &m_textVBO3D);
    if (m_circleVAO3D) glDeleteVertexArrays(1, &m_circleVAO3D);
    if (m_circleVBO3D) glDeleteBuffers(1, &m_circleVBO3D);
    if (m_circleFillVAO3D) glDeleteVertexArrays(1, &m_circleFillVAO3D);
    if (m_circleFillVBO3D) glDeleteBuffers(1, &m_circleFillVBO3D);
    if (m_rectVAO3D) glDeleteVertexArrays(1, &m_rectVAO3D);
    if (m_rectVBO3D) glDeleteBuffers(1, &m_rectVBO3D);
    if (m_rectFillVAO3D) glDeleteVertexArrays(1, &m_rectFillVAO3D);
    if (m_rectFillVBO3D) glDeleteBuffers(1, &m_rectFillVBO3D);
    if (m_triangleFillVAO3D) glDeleteVertexArrays(1, &m_triangleFillVAO3D);
    if (m_triangleFillVBO3D) glDeleteBuffers(1, &m_triangleFillVBO3D);
    if (m_immediateVAO3D) glDeleteVertexArrays(1, &m_immediateVAO3D);
    if (m_immediateVBO3D) glDeleteBuffers(1, &m_immediateVBO3D);
    
    if (m_shader3DProgram) {
        glDeleteProgram(m_shader3DProgram);
        m_shader3DProgram = 0;
    }
    if (m_shader3DTexturedProgram) {
        glDeleteProgram(m_shader3DTexturedProgram);
        m_shader3DTexturedProgram = 0;
    }
    
    if (m_shader3DModelProgram) {
        glDeleteProgram(m_shader3DModelProgram);
        m_shader3DModelProgram = 0;
    }
    
    if (m_lineConversionBuffer3D) {
        delete[] m_lineConversionBuffer3D;
        m_lineConversionBuffer3D = NULL;
    }
}

//
// Create 3D shader programs using shader sources from .glsl.h headers

void Renderer3D::Initialize3DShaders() {
    m_shader3DProgram = g_renderer->CreateShader(VERTEX_3D_SHADER_SOURCE, FRAGMENT_3D_SHADER_SOURCE);
    
    //
    // Create 3D shader program

    if (m_shader3DProgram == 0) {
        AppDebugOut("Renderer3D: Failed to create 3D shader program\n");
    }
    
    //
    // Create textured 3D shader program

    m_shader3DTexturedProgram = g_renderer->CreateShader(TEXTURED_VERTEX_3D_SHADER_SOURCE, TEXTURED_FRAGMENT_3D_SHADER_SOURCE);
    
    if (m_shader3DTexturedProgram == 0) {
        AppDebugOut("Renderer3D: Failed to create textured 3D shader program\n");
    }
    
    //
    // Create model 3D shader program with per-instance transforms

    m_shader3DModelProgram = g_renderer->CreateShader(MODEL_VERTEX_3D_SHADER_SOURCE, FRAGMENT_3D_SHADER_SOURCE);
    
    if (m_shader3DModelProgram == 0) {
        AppDebugOut("Renderer3D: Failed to create model 3D shader program\n");
    }
}

void Renderer3D::Setup3DVertexArrays() {
    auto setup3DVertexAttributes = []() {

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)0);
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
    };
    
    //
    // Generate VAO and VBO for immediate 3D rendering

    glGenVertexArrays(1, &m_VAO3D);
    glGenBuffers(1, &m_VBO3D);
    glBindVertexArray(m_VAO3D);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO3D);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex3D) * MAX_3D_VERTICES, NULL, GL_DYNAMIC_DRAW);
    setup3DVertexAttributes();
    
    //
    // Create Line VAO/VBO pair

    glGenVertexArrays(1, &m_lineVAO3D);
    glGenBuffers(1, &m_lineVBO3D);
    glBindVertexArray(m_lineVAO3D);
    glBindBuffer(GL_ARRAY_BUFFER, m_lineVBO3D);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex3D) * (MAX_LINE_VERTICES_3D), NULL, GL_STREAM_DRAW);
    setup3DVertexAttributes(); 
    
    //
    // Create Circle VAO/VBO pair

    glGenVertexArrays(1, &m_circleVAO3D);
    glGenBuffers(1, &m_circleVBO3D);
    glBindVertexArray(m_circleVAO3D);
    glBindBuffer(GL_ARRAY_BUFFER, m_circleVBO3D);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex3D) * MAX_CIRCLE_VERTICES_3D, NULL, GL_DYNAMIC_DRAW);
    setup3DVertexAttributes();
    
    //
    // Create Circle Fill VAO/VBO pair

    glGenVertexArrays(1, &m_circleFillVAO3D);
    glGenBuffers(1, &m_circleFillVBO3D);
    glBindVertexArray(m_circleFillVAO3D);
    glBindBuffer(GL_ARRAY_BUFFER, m_circleFillVBO3D);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex3D) * MAX_CIRCLE_FILL_VERTICES_3D, NULL, GL_DYNAMIC_DRAW);
    setup3DVertexAttributes();
    
    //
    // Create Rect VAO/VBO pair

    glGenVertexArrays(1, &m_rectVAO3D);
    glGenBuffers(1, &m_rectVBO3D);
    glBindVertexArray(m_rectVAO3D);
    glBindBuffer(GL_ARRAY_BUFFER, m_rectVBO3D);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex3D) * MAX_RECT_VERTICES_3D, NULL, GL_DYNAMIC_DRAW);
    setup3DVertexAttributes();
    
    //
    // Create Rect Fill VAO/VBO pair

    glGenVertexArrays(1, &m_rectFillVAO3D);
    glGenBuffers(1, &m_rectFillVBO3D);
    glBindVertexArray(m_rectFillVAO3D);
    glBindBuffer(GL_ARRAY_BUFFER, m_rectFillVBO3D);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex3D) * MAX_RECT_FILL_VERTICES_3D, NULL, GL_DYNAMIC_DRAW);
    setup3DVertexAttributes();
    
    //
    // Create Triangle Fill VAO/VBO pair

    glGenVertexArrays(1, &m_triangleFillVAO3D);
    glGenBuffers(1, &m_triangleFillVBO3D);
    glBindVertexArray(m_triangleFillVAO3D);
    glBindBuffer(GL_ARRAY_BUFFER, m_triangleFillVBO3D);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex3D) * MAX_TRIANGLE_FILL_VERTICES_3D, NULL, GL_DYNAMIC_DRAW);
    setup3DVertexAttributes();
    
    //
    // Create Immediate VAO/VBO pair

    glGenVertexArrays(1, &m_immediateVAO3D);
    glGenBuffers(1, &m_immediateVBO3D);
    glBindVertexArray(m_immediateVAO3D);
    glBindBuffer(GL_ARRAY_BUFFER, m_immediateVBO3D);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex3D) * MAX_3D_VERTICES, NULL, GL_DYNAMIC_DRAW);
    setup3DVertexAttributes();
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    
    AppDebugOut("Renderer3D: 3D vertex arrays setup complete\n");
}

void Renderer3D::Setup3DTexturedVertexArrays() {
    auto setup3DTexturedVertexAttributes = []() {

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3DTextured), (void*)0);
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3DTextured), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex3DTextured), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);

        glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex3DTextured), (void*)(10 * sizeof(float)));
        glEnableVertexAttribArray(3);
    };
    
    //
    // Generate VAO and VBO for immediate textured 3D rendering

    glGenVertexArrays(1, &m_VAO3DTextured);
    glGenBuffers(1, &m_VBO3DTextured);
    glBindVertexArray(m_VAO3DTextured);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO3DTextured);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex3DTextured) * MAX_3D_TEXTURED_VERTICES, NULL, GL_DYNAMIC_DRAW);
    setup3DTexturedVertexAttributes();
    
    //
    // Create Sprite VAO/VBO pair

    glGenVertexArrays(1, &m_spriteVAO3D);
    glGenBuffers(1, &m_spriteVBO3D);
    glBindVertexArray(m_spriteVAO3D);
    glBindBuffer(GL_ARRAY_BUFFER, m_spriteVBO3D);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex3DTextured) * (MAX_STATIC_SPRITE_VERTICES_3D + MAX_ROTATING_SPRITE_VERTICES_3D), NULL, GL_DYNAMIC_DRAW);
    setup3DTexturedVertexAttributes();
    
    // Create Text VAO/VBO pair
    glGenVertexArrays(1, &m_textVAO3D);
    glGenBuffers(1, &m_textVBO3D);
    glBindVertexArray(m_textVAO3D);
    glBindBuffer(GL_ARRAY_BUFFER, m_textVBO3D);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex3DTextured) * MAX_TEXT_VERTICES_3D, NULL, GL_DYNAMIC_DRAW);
    setup3DTexturedVertexAttributes();
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    
    AppDebugOut("Renderer3D: Textured 3D vertex arrays setup complete\n");
}

void Renderer3D::SetPerspective(float fovy, float aspect, float nearZ, float farZ) {
    m_projectionMatrix3D.Perspective(fovy, aspect, nearZ, farZ);
    InvalidateMatrices3D();
}

void Renderer3D::SetLookAt(float eyeX, float eyeY, float eyeZ,
                           float centerX, float centerY, float centerZ,
                           float upX, float upY, float upZ) {
    m_modelViewMatrix3D.LookAt(eyeX, eyeY, eyeZ, centerX, centerY, centerZ, upX, upY, upZ);
    InvalidateMatrices3D();
}

void Renderer3D::BeginLineStrip3D(Colour const &col, float lineWidth) {
    m_lineStrip3DActive = true;
    m_lineStrip3DColor = col;
    m_lineStrip3DWidth = lineWidth;
    m_vertex3DCount = 0;

#ifndef TARGET_EMSCRIPTEN
    g_renderer->SetLineWidth(lineWidth);
#endif
}

void Renderer3D::LineStripVertex3D(float x, float y, float z) {
    if (!m_lineStrip3DActive) return;
    
    //
    // Check buffer overflow

    if (m_vertex3DCount >= MAX_3D_VERTICES) {
        return;
    }
    
    //
    // Convert color to float

    float r = m_lineStrip3DColor.GetRFloat();
    float g = m_lineStrip3DColor.GetGFloat();
    float b = m_lineStrip3DColor.GetBFloat();
    float a = m_lineStrip3DColor.GetAFloat();
    
    m_vertices3D[m_vertex3DCount] = Vertex3D(x, y, z, r, g, b, a);
    m_vertex3DCount++;
}

void Renderer3D::LineStripVertex3D(const Vector3<float>& vertex) {
    LineStripVertex3D(vertex.x, vertex.y, vertex.z);
}

void Renderer3D::EndLineStrip3D() {
    if (!m_lineStrip3DActive || m_vertex3DCount < 2) {
        m_lineStrip3DActive = false;
        m_vertex3DCount = 0;
        return;
    }
    
    int lineVertexCount = (m_vertex3DCount - 1) * 2;
    Vertex3D* lineVertices = m_lineConversionBuffer3D;
    
    int lineIndex = 0;
    for (int i = 0; i < m_vertex3DCount - 1; i++) {

        //
        // Line from vertex i to vertex i+1

        lineVertices[lineIndex++] = m_vertices3D[i];
        lineVertices[lineIndex++] = m_vertices3D[i + 1];
    }
    
    //
    // Clear current buffer and add line segments

    m_vertex3DCount = 0;
    for (int i = 0; i < lineVertexCount; i++) {
        if (m_vertex3DCount >= MAX_3D_VERTICES) break;
        m_vertices3D[m_vertex3DCount++] = lineVertices[i];
    }  

#ifndef TARGET_EMSCRIPTEN
    g_renderer->SetLineWidth(m_lineStrip3DWidth);
#endif

    Flush3DVertices(GL_LINES);
    
    m_lineStrip3DActive = false;
}

void Renderer3D::BeginLineLoop3D(Colour const &col, float lineWidth) {
    BeginLineStrip3D(col, lineWidth);
}

void Renderer3D::LineLoopVertex3D(float x, float y, float z) {
    LineStripVertex3D(x, y, z);
}

void Renderer3D::LineLoopVertex3D(const Vector3<float>& vertex) {
    LineStripVertex3D(vertex);
}

void Renderer3D::EndLineLoop3D() {
    if (!m_lineStrip3DActive || m_vertex3DCount < 3) {
        m_lineStrip3DActive = false;
        m_vertex3DCount = 0;
        return;
    }
    
    //
    // For line loop, add a final line from last vertex back to first

    Vertex3D firstVertex = m_vertices3D[0];
    if (m_vertex3DCount < MAX_3D_VERTICES) {
        m_vertices3D[m_vertex3DCount++] = firstVertex;
    }
    
    EndLineStrip3D();
}

void Renderer3D::BeginTexturedQuad3D(unsigned int textureID, Colour const &col) {
    m_texturedQuad3DActive = true;
    m_texturedQuad3DColor = col;
    m_currentTexture3D = textureID;
    m_vertex3DTexturedCount = 0;
}

void Renderer3D::TexturedQuadVertex3D(float x, float y, float z, float u, float v) {
    if (!m_texturedQuad3DActive) return;
    
    if (m_vertex3DTexturedCount >= MAX_3D_TEXTURED_VERTICES) {
        return;
    }

    float r = m_texturedQuad3DColor.GetRFloat();
    float g = m_texturedQuad3DColor.GetGFloat();
    float b = m_texturedQuad3DColor.GetBFloat();
    float a = m_texturedQuad3DColor.GetAFloat();
    
    m_vertices3DTextured[m_vertex3DTexturedCount] = Vertex3DTextured(x, y, z, 0.0f, 1.0f, 0.0f, r, g, b, a, u, v);
    m_vertex3DTexturedCount++;
}

void Renderer3D::TexturedQuadVertex3D(const Vector3<float>& vertex, float u, float v) {
    TexturedQuadVertex3D(vertex.x, vertex.y, vertex.z, u, v);
}

void Renderer3D::EndTexturedQuad3D() {
    if (!m_texturedQuad3DActive || m_vertex3DTexturedCount < 4) {
        m_texturedQuad3DActive = false;
        m_vertex3DTexturedCount = 0;
        return;
    }
    
    Flush3DTexturedVertices();
    m_texturedQuad3DActive = false;
}

void Renderer3D::SetColor(const Colour& col) {
    m_lineStrip3DColor = col;
}

void Renderer3D::Clear3DState() {
    m_vertex3DCount = 0;
    m_lineStrip3DActive = false;
}

void Renderer3D::Cache3DUniformLocations() {
    m_shader3DUniforms.projectionLoc = glGetUniformLocation(m_shader3DProgram, "uProjection");
    m_shader3DUniforms.modelViewLoc = glGetUniformLocation(m_shader3DProgram, "uModelView");
    m_shader3DUniforms.textureLoc = -1; // not used in basic 3D shader
    m_shader3DUniforms.fogEnabledLoc = glGetUniformLocation(m_shader3DProgram, "uFogEnabled");
    m_shader3DUniforms.fogOrientationLoc = glGetUniformLocation(m_shader3DProgram, "uFogOrientationBased");
    m_shader3DUniforms.fogStartLoc = glGetUniformLocation(m_shader3DProgram, "uFogStart");
    m_shader3DUniforms.fogEndLoc = glGetUniformLocation(m_shader3DProgram, "uFogEnd");
    m_shader3DUniforms.fogColorLoc = glGetUniformLocation(m_shader3DProgram, "uFogColor");
    m_shader3DUniforms.cameraPosLoc = glGetUniformLocation(m_shader3DProgram, "uCameraPos");
    
    //
    // cache uniform locations for 3D textured shader

    m_shader3DTexturedUniforms.projectionLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uProjection");
    m_shader3DTexturedUniforms.modelViewLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uModelView");
    m_shader3DTexturedUniforms.textureLoc = glGetUniformLocation(m_shader3DTexturedProgram, "ourTexture");
    m_shader3DTexturedUniforms.fogEnabledLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uFogEnabled");
    m_shader3DTexturedUniforms.fogOrientationLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uFogOrientationBased");
    m_shader3DTexturedUniforms.fogStartLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uFogStart");
    m_shader3DTexturedUniforms.fogEndLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uFogEnd");
    m_shader3DTexturedUniforms.fogColorLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uFogColor");
    m_shader3DTexturedUniforms.cameraPosLoc = glGetUniformLocation(m_shader3DTexturedProgram, "uCameraPos");
    
    //
    // cache uniform locations for 3D model shader

    m_shader3DModelUniforms.projectionLoc = glGetUniformLocation(m_shader3DModelProgram, "uProjection");
    m_shader3DModelUniforms.modelViewLoc = glGetUniformLocation(m_shader3DModelProgram, "uModelView");
    m_shader3DModelUniforms.modelMatrixLoc = glGetUniformLocation(m_shader3DModelProgram, "uModelMatrix");
    m_shader3DModelUniforms.modelColorLoc = glGetUniformLocation(m_shader3DModelProgram, "uModelColor");
    m_shader3DModelUniforms.modelMatricesLoc = glGetUniformLocation(m_shader3DModelProgram, "uModelMatrices");
    m_shader3DModelUniforms.modelColorsLoc = glGetUniformLocation(m_shader3DModelProgram, "uModelColors");
    m_shader3DModelUniforms.instanceCountLoc = glGetUniformLocation(m_shader3DModelProgram, "uInstanceCount");
    m_shader3DModelUniforms.useInstancingLoc = glGetUniformLocation(m_shader3DModelProgram, "uUseInstancing");
    m_shader3DModelUniforms.fogEnabledLoc = glGetUniformLocation(m_shader3DModelProgram, "uFogEnabled");
    m_shader3DModelUniforms.fogOrientationLoc = glGetUniformLocation(m_shader3DModelProgram, "uFogOrientationBased");
    m_shader3DModelUniforms.fogStartLoc = glGetUniformLocation(m_shader3DModelProgram, "uFogStart");
    m_shader3DModelUniforms.fogEndLoc = glGetUniformLocation(m_shader3DModelProgram, "uFogEnd");
    m_shader3DModelUniforms.fogColorLoc = glGetUniformLocation(m_shader3DModelProgram, "uFogColor");
    m_shader3DModelUniforms.cameraPosLoc = glGetUniformLocation(m_shader3DModelProgram, "uCameraPos");
}

void Renderer3D::Set3DShaderUniforms() {

    //
    // Only upload matrices if they changed or we switched shaders

    if (m_matrices3DNeedUpdate || m_currentShaderProgram3D != m_shader3DProgram) {
        glUniformMatrix4fv(m_shader3DUniforms.projectionLoc, 1, GL_FALSE, m_projectionMatrix3D.m);
        glUniformMatrix4fv(m_shader3DUniforms.modelViewLoc, 1, GL_FALSE, m_modelViewMatrix3D.m);
    }
    
    //
    // Only upload fog uniforms if they changed or we switched shaders  

    if (m_fog3DNeedsUpdate || m_currentShaderProgram3D != m_shader3DProgram) {
        glUniform1i(m_shader3DUniforms.fogEnabledLoc, m_fogEnabled ? 1 : 0);
        glUniform1i(m_shader3DUniforms.fogOrientationLoc, m_fogOrientationBased ? 1 : 0);
        glUniform1f(m_shader3DUniforms.fogStartLoc, m_fogStart);
        glUniform1f(m_shader3DUniforms.fogEndLoc, m_fogEnd);
        glUniform4f(m_shader3DUniforms.fogColorLoc, m_fogColor[0], m_fogColor[1], m_fogColor[2], m_fogColor[3]);
        glUniform3f(m_shader3DUniforms.cameraPosLoc, m_cameraPos[0], m_cameraPos[1], m_cameraPos[2]);
    }
    
    m_currentShaderProgram3D = m_shader3DProgram;
    m_matrices3DNeedUpdate = false;
    m_fog3DNeedsUpdate = false;
}

void Renderer3D::SetTextured3DShaderUniforms() {

    if (m_matrices3DNeedUpdate || m_currentShaderProgram3D != m_shader3DTexturedProgram) {
        glUniformMatrix4fv(m_shader3DTexturedUniforms.projectionLoc, 1, GL_FALSE, m_projectionMatrix3D.m);
        glUniformMatrix4fv(m_shader3DTexturedUniforms.modelViewLoc, 1, GL_FALSE, m_modelViewMatrix3D.m);
        glUniform1i(m_shader3DTexturedUniforms.textureLoc, 0);
    }

    if (m_fog3DNeedsUpdate || m_currentShaderProgram3D != m_shader3DTexturedProgram) {
        glUniform1i(m_shader3DTexturedUniforms.fogEnabledLoc, m_fogEnabled ? 1 : 0);
        glUniform1i(m_shader3DTexturedUniforms.fogOrientationLoc, m_fogOrientationBased ? 1 : 0);
        glUniform1f(m_shader3DTexturedUniforms.fogStartLoc, m_fogStart);
        glUniform1f(m_shader3DTexturedUniforms.fogEndLoc, m_fogEnd);
        glUniform4f(m_shader3DTexturedUniforms.fogColorLoc, m_fogColor[0], m_fogColor[1], m_fogColor[2], m_fogColor[3]);
        glUniform3f(m_shader3DTexturedUniforms.cameraPosLoc, m_cameraPos[0], m_cameraPos[1], m_cameraPos[2]);
    }
    
    m_currentShaderProgram3D = m_shader3DTexturedProgram;
    m_matrices3DNeedUpdate = false;
    m_fog3DNeedsUpdate = false;
}

void Renderer3D::Set3DModelShaderUniforms(const Matrix4f& modelMatrix, const Colour& modelColor) {

    if (m_matrices3DNeedUpdate || m_currentShaderProgram3D != m_shader3DModelProgram) {
        glUniformMatrix4fv(m_shader3DModelUniforms.projectionLoc, 1, GL_FALSE, m_projectionMatrix3D.m);
        glUniformMatrix4fv(m_shader3DModelUniforms.modelViewLoc, 1, GL_FALSE, m_modelViewMatrix3D.m);
    }
    
    glUniform1i(m_shader3DModelUniforms.useInstancingLoc, 0);
    glUniform1i(m_shader3DModelUniforms.instanceCountLoc, 0);

    glUniformMatrix4fv(m_shader3DModelUniforms.modelMatrixLoc, 1, GL_FALSE, modelMatrix.m);
    
    glUniform4f(m_shader3DModelUniforms.modelColorLoc, 
                modelColor.GetRFloat(), 
                modelColor.GetGFloat(), 
                modelColor.GetBFloat(), 
                modelColor.GetAFloat());

    if (m_fog3DNeedsUpdate || m_currentShaderProgram3D != m_shader3DModelProgram) {
        glUniform1i(m_shader3DModelUniforms.fogEnabledLoc, m_fogEnabled ? 1 : 0);
        glUniform1i(m_shader3DModelUniforms.fogOrientationLoc, m_fogOrientationBased ? 1 : 0);
        glUniform1f(m_shader3DModelUniforms.fogStartLoc, m_fogStart);
        glUniform1f(m_shader3DModelUniforms.fogEndLoc, m_fogEnd);
        glUniform4f(m_shader3DModelUniforms.fogColorLoc, m_fogColor[0], m_fogColor[1], m_fogColor[2], m_fogColor[3]);
        glUniform3f(m_shader3DModelUniforms.cameraPosLoc, m_cameraPos[0], m_cameraPos[1], m_cameraPos[2]);
    }
    
    m_currentShaderProgram3D = m_shader3DModelProgram;
    m_matrices3DNeedUpdate = false;
    m_fog3DNeedsUpdate = false;
}

void Renderer3D::Set3DModelShaderUniformsInstanced(const Matrix4f* modelMatrices, const Colour* modelColors, int instanceCount) {
    
    int maxInstances = MAX_INSTANCES;
    
    if (instanceCount > maxInstances) {
        instanceCount = maxInstances;
    }
    
    if (m_matrices3DNeedUpdate || m_currentShaderProgram3D != m_shader3DModelProgram) {
        glUniformMatrix4fv(m_shader3DModelUniforms.projectionLoc, 1, GL_FALSE, m_projectionMatrix3D.m);
        glUniformMatrix4fv(m_shader3DModelUniforms.modelViewLoc, 1, GL_FALSE, m_modelViewMatrix3D.m);
    }
    
    glUniform1i(m_shader3DModelUniforms.useInstancingLoc, 1);
    glUniform1i(m_shader3DModelUniforms.instanceCountLoc, instanceCount);
    
    glUniformMatrix4fv(m_shader3DModelUniforms.modelMatricesLoc, instanceCount, GL_FALSE, (const float*)modelMatrices);
    
    float* colorArray = new float[instanceCount * 4];
    for (int i = 0; i < instanceCount; i++) {
        colorArray[i * 4 + 0] = modelColors[i].GetRFloat();
        colorArray[i * 4 + 1] = modelColors[i].GetGFloat();
        colorArray[i * 4 + 2] = modelColors[i].GetBFloat();
        colorArray[i * 4 + 3] = modelColors[i].GetAFloat();
    }
    glUniform4fv(m_shader3DModelUniforms.modelColorsLoc, instanceCount, colorArray);
    delete[] colorArray;
    
    if (m_fog3DNeedsUpdate || m_currentShaderProgram3D != m_shader3DModelProgram) {
        glUniform1i(m_shader3DModelUniforms.fogEnabledLoc, m_fogEnabled ? 1 : 0);
        glUniform1i(m_shader3DModelUniforms.fogOrientationLoc, m_fogOrientationBased ? 1 : 0);
        glUniform1f(m_shader3DModelUniforms.fogStartLoc, m_fogStart);
        glUniform1f(m_shader3DModelUniforms.fogEndLoc, m_fogEnd);
        glUniform4f(m_shader3DModelUniforms.fogColorLoc, m_fogColor[0], m_fogColor[1], m_fogColor[2], m_fogColor[3]);
        glUniform3f(m_shader3DModelUniforms.cameraPosLoc, m_cameraPos[0], m_cameraPos[1], m_cameraPos[2]);
    }
    
    m_currentShaderProgram3D = m_shader3DModelProgram;
    m_matrices3DNeedUpdate = false;
    m_fog3DNeedsUpdate = false;
}

void Renderer3D::SetFogUniforms3D(unsigned int shaderProgram) {
    int fogEnabledLoc = glGetUniformLocation(shaderProgram, "uFogEnabled");
    int fogOrientationLoc = glGetUniformLocation(shaderProgram, "uFogOrientationBased");
    int fogStartLoc = glGetUniformLocation(shaderProgram, "uFogStart");
    int fogEndLoc = glGetUniformLocation(shaderProgram, "uFogEnd");
    int fogColorLoc = glGetUniformLocation(shaderProgram, "uFogColor");
    int cameraPosLoc = glGetUniformLocation(shaderProgram, "uCameraPos");
    
    glUniform1i(fogEnabledLoc, m_fogEnabled ? 1 : 0);
    glUniform1i(fogOrientationLoc, m_fogOrientationBased ? 1 : 0);
    glUniform1f(fogStartLoc, m_fogStart);
    glUniform1f(fogEndLoc, m_fogEnd);
    glUniform4f(fogColorLoc, m_fogColor[0], m_fogColor[1], m_fogColor[2], m_fogColor[3]);
    glUniform3f(cameraPosLoc, m_cameraPos[0], m_cameraPos[1], m_cameraPos[2]);
}

void Renderer3D::UploadVertexDataTo3DVBO(unsigned int vbo,
                                         const Vertex3D* vertices,
                                         int vertexCount,
                                         unsigned int usageHint)
{
    if (vertexCount <= 0 || !vertices) return;

    g_renderer->SetArrayBuffer(vbo);

    const GLsizeiptr bytes =
        static_cast<GLsizeiptr>(vertexCount) * sizeof(Vertex3D);

    if (usageHint == GL_STATIC_DRAW)
    {
        glBufferData(GL_ARRAY_BUFFER, bytes, vertices, usageHint);
    }
    else
    {
        glBufferData(GL_ARRAY_BUFFER, bytes, nullptr, usageHint);
        glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, vertices);
    }
}

void Renderer3D::UploadVertexDataTo3DVBO(unsigned int vbo,
                                         const Vertex3DTextured* vertices,
                                         int vertexCount,
                                         unsigned int usageHint)
{
    if (vertexCount <= 0 || !vertices) return;

    g_renderer->SetArrayBuffer(vbo);

    const GLsizeiptr bytes =
        static_cast<GLsizeiptr>(vertexCount) * sizeof(Vertex3DTextured);

    if (usageHint == GL_STATIC_DRAW)
    {
        glBufferData(GL_ARRAY_BUFFER, bytes, vertices, usageHint);
    }
    else
    {
        glBufferData(GL_ARRAY_BUFFER, bytes, nullptr, usageHint);
        glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, vertices);
    }
}

void Renderer3D::EnableDistanceFog(float start, float end, float density, float r, float g, float b, float a) {
    m_fogEnabled = true;
    m_fogOrientationBased = false;
    m_fogStart = start;
    m_fogEnd = end;
    m_fogDensity = density;
    m_fogColor[0] = r;
    m_fogColor[1] = g;
    m_fogColor[2] = b;
    m_fogColor[3] = a;
    InvalidateFog3D();
}

void Renderer3D::EnableOrientationFog(float r, float g, float b, float a, float camX, float camY, float camZ) {
    m_fogEnabled = true;
    m_fogOrientationBased = true;
    m_fogColor[0] = r;
    m_fogColor[1] = g;
    m_fogColor[2] = b;
    m_fogColor[3] = a;
    m_cameraPos[0] = camX;
    m_cameraPos[1] = camY;
    m_cameraPos[2] = camZ;
    InvalidateFog3D();
}

void Renderer3D::DisableFog() {
    m_fogEnabled = false;
    InvalidateFog3D();
} 

void Renderer3D::SetCameraPosition(float x, float y, float z) {
    m_cameraPos[0] = x;
    m_cameraPos[1] = y;
    m_cameraPos[2] = z;
    InvalidateFog3D();
}

void Renderer3D::CreateSurfaceAlignedBillboard(const Vector3<float>& position, float width, float height, 
                                               Vertex3DTextured* vertices, float u1, float v1, float u2, float v2, 
                                               float r, float g, float b, float a, float rotation) {

    //
    // Create billboard that lays flat

    Vector3<float> normal = position;
    normal.Normalise();
    
    //
    // Create consistent up direction relative surface

    Vector3<float> up = Vector3<float>(0.0f, 1.0f, 0.0f);

    Vector3<float> tangent1;
    if (fabsf(normal.y) > 0.98f) {
        tangent1 = Vector3<float>(1.0f, 0.0f, 0.0f);
    } else {
        tangent1 = up ^ normal;
        tangent1.Normalise();
    }
    
    //
    // Create tangent pointing north

    Vector3<float> tangent2 = normal ^ tangent1;
    tangent2.Normalise();
    
    //
    // Apply rotation if specified
    
    if (rotation != 0.0f) {
        Vector3<float> rotatedTangent1 = tangent1 * cosf(rotation) + tangent2 * sinf(rotation);
        Vector3<float> rotatedTangent2 = tangent2 * cosf(rotation) - tangent1 * sinf(rotation);
        tangent1 = rotatedTangent1;
        tangent2 = rotatedTangent2;
    }
    
    float halfWidth = width * 0.5f;
    float halfHeight = height * 0.5f;
    
    // First triangle: TL, TR, BR
    vertices[0] = Vertex3DTextured(
        position.x - tangent1.x * halfWidth + tangent2.x * halfHeight,
        position.y - tangent1.y * halfWidth + tangent2.y * halfHeight,
        position.z - tangent1.z * halfWidth + tangent2.z * halfHeight,
        normal.x, normal.y, normal.z,
        r, g, b, a, u1, v2);
    vertices[1] = Vertex3DTextured(
        position.x + tangent1.x * halfWidth + tangent2.x * halfHeight,
        position.y + tangent1.y * halfWidth + tangent2.y * halfHeight,
        position.z + tangent1.z * halfWidth + tangent2.z * halfHeight,
        normal.x, normal.y, normal.z,
        r, g, b, a, u2, v2);
    vertices[2] = Vertex3DTextured(
        position.x + tangent1.x * halfWidth - tangent2.x * halfHeight,
        position.y + tangent1.y * halfWidth - tangent2.y * halfHeight,
        position.z + tangent1.z * halfWidth - tangent2.z * halfHeight,
        normal.x, normal.y, normal.z,
        r, g, b, a, u2, v1);
    
    // Second triangle: TL, BR, BL
    vertices[3] = vertices[0]; // TL
    vertices[4] = vertices[2]; // BR
    vertices[5] = Vertex3DTextured(
        position.x - tangent1.x * halfWidth - tangent2.x * halfHeight,
        position.y - tangent1.y * halfWidth - tangent2.y * halfHeight,
        position.z - tangent1.z * halfWidth - tangent2.z * halfHeight,
        normal.x, normal.y, normal.z,
        r, g, b, a, u1, v1);
}

void Renderer3D::CreateCameraFacingBillboard(const Vector3<float>& position, float width, float height,
                                             Vertex3DTextured* vertices, float u1, float v1, float u2, float v2,
                                             float r, float g, float b, float a, float rotation) {

    //
    // Create billboard that faces the camera

    Vector3<float> cameraPos(m_cameraPos[0], m_cameraPos[1], m_cameraPos[2]);
    Vector3<float> cameraDir = cameraPos - position;
    cameraDir.Normalise();
    
    //
    // Create billboard facing camera

    Vector3<float> up = Vector3<float>(0.0f, 1.0f, 0.0f);
    Vector3<float> right = up ^ cameraDir;
    right.Normalise();
    up = cameraDir ^ right;
    up.Normalise();
    
    if (rotation != 0.0f) {
        float cosRot = cosf(rotation);
        float sinRot = sinf(rotation);
        Vector3<float> rotatedRight = right * cosRot + up * sinRot;
        Vector3<float> rotatedUp = -right * sinRot + up * cosRot;
        right = rotatedRight;
        up = rotatedUp;
    }
    
    float halfWidth = width * 0.5f;
    float halfHeight = height * 0.5f;
    
    // First triangle: TL, TR, BR
    vertices[0] = Vertex3DTextured(
        position.x - right.x * halfWidth + up.x * halfHeight,
        position.y - right.y * halfWidth + up.y * halfHeight,
        position.z - right.z * halfWidth + up.z * halfHeight,
        cameraDir.x, cameraDir.y, cameraDir.z,
        r, g, b, a, u1, v2);
    vertices[1] = Vertex3DTextured(
        position.x + right.x * halfWidth + up.x * halfHeight,
        position.y + right.y * halfWidth + up.y * halfHeight,
        position.z + right.z * halfWidth + up.z * halfHeight,
        cameraDir.x, cameraDir.y, cameraDir.z,
        r, g, b, a, u2, v2);
    vertices[2] = Vertex3DTextured(
        position.x + right.x * halfWidth - up.x * halfHeight,
        position.y + right.y * halfWidth - up.y * halfHeight,
        position.z + right.z * halfWidth - up.z * halfHeight,
        cameraDir.x, cameraDir.y, cameraDir.z,
        r, g, b, a, u2, v1);
    
    // Second triangle: TL, BR, BL
    vertices[3] = vertices[0]; // TL
    vertices[4] = vertices[2]; // BR
    vertices[5] = Vertex3DTextured(
        position.x - right.x * halfWidth - up.x * halfHeight,
        position.y - right.y * halfWidth - up.y * halfHeight,
        position.z - right.z * halfWidth - up.z * halfHeight,
        cameraDir.x, cameraDir.y, cameraDir.z,
        r, g, b, a, u1, v1);
}

void Renderer3D::CalculateSphericalTangents(const Vector3<float>& position, Vector3<float>& outEast, Vector3<float>& outNorth) {
    Vector3<float> normal = position;
    normal.Normalise();
    
    Vector3<float> worldNorth(0.0f, 1.0f, 0.0f);
    
    if (fabsf(normal.y) > 0.999f) {
        outEast = Vector3<float>(1.0f, 0.0f, 0.0f);
    } else {
        outEast = worldNorth ^ normal;
        outEast.Normalise();
    }
    
    outNorth = normal ^ outEast;
    outNorth.Normalise();
} 


//
// Increment draw calls for the 3d renderer

void Renderer3D::IncrementDrawCall3D(const char* bufferType) {
    m_drawCallsPerFrame3D++;
    
    constexpr auto hash = [](const char* str) constexpr {
        uint32_t hash = 5381;
        while (*str) {
            hash = ((hash << 5) + hash) + *str++;
        }
        return hash;
    };
    
    switch (hash(bufferType)) {
        case hash("immediate_vertices"): m_immediateVertexCalls3D++; break;
        case hash("immediate_triangles"): m_immediateTriangleCalls3D++; break;
        case hash("text"): m_textCalls3D++; break;
        case hash("mega_vbo"): m_megaVBOCalls3D++; break;
        case hash("lines"): m_lineCalls3D++; break;
        case hash("batched_lines"): m_lineCalls3D++; break;
        case hash("static_sprites"): m_staticSpriteCalls3D++; break;
        case hash("rotating_sprites"): m_rotatingSpriteCalls3D++; break;
        case hash("circles"): m_circleCalls3D++; break;
        case hash("circle_fills"): m_circleFillCalls3D++; break;
        case hash("rects"): m_rectCalls3D++; break;
        case hash("rect_fills"): m_rectFillCalls3D++; break;
        case hash("triangle_fills"): m_triangleFillCalls3D++; break;
        case hash("line_vbo"): m_lineVBOCalls3D++; break;
        case hash("quad_vbo"): m_quadVBOCalls3D++; break;
        case hash("triangle_vbo"): m_triangleVBOCalls3D++; break;
    }
}

void Renderer3D::ResetFrameCounters3D() {
    m_prevDrawCallsPerFrame3D = m_drawCallsPerFrame3D;
    m_prevImmediateVertexCalls3D = m_immediateVertexCalls3D;
    m_prevImmediateTriangleCalls3D = m_immediateTriangleCalls3D;
    m_prevImmediateLineCalls3D = m_lineCalls3D;
    m_prevLineCalls3D = m_lineCalls3D;
    m_prevStaticSpriteCalls3D = m_staticSpriteCalls3D;
    m_prevRotatingSpriteCalls3D = m_rotatingSpriteCalls3D;
    m_prevTextCalls3D = m_textCalls3D;
    m_prevMegaVBOCalls3D = m_megaVBOCalls3D;
    m_prevCircleCalls3D = m_circleCalls3D;
    m_prevCircleFillCalls3D = m_circleFillCalls3D;
    m_prevRectCalls3D = m_rectCalls3D;
    m_prevRectFillCalls3D = m_rectFillCalls3D;
    m_prevTriangleFillCalls3D = m_triangleFillCalls3D;
    m_prevLineVBOCalls3D = m_lineVBOCalls3D;
    m_prevQuadVBOCalls3D = m_quadVBOCalls3D;
    m_prevTriangleVBOCalls3D = m_triangleVBOCalls3D;
    m_prevActiveFontBatches3D = m_activeFontBatches3D;
    
    //
    // Reset current frame counters

    m_drawCallsPerFrame3D = 0;
    m_immediateVertexCalls3D = 0;
    m_immediateTriangleCalls3D = 0;
    m_lineCalls3D = 0;
    m_staticSpriteCalls3D = 0;
    m_rotatingSpriteCalls3D = 0;
    m_textCalls3D = 0;
    m_megaVBOCalls3D = 0;
    m_circleCalls3D = 0;
    m_circleFillCalls3D = 0;
    m_rectCalls3D = 0;
    m_rectFillCalls3D = 0;
    m_triangleFillCalls3D = 0;
    m_lineVBOCalls3D = 0;
    m_quadVBOCalls3D = 0;
    m_triangleVBOCalls3D = 0;
    m_activeFontBatches3D = 0;
}

void Renderer3D::BeginFrame3D() {
    ResetFrameCounters3D();
}

void Renderer3D::EndFrame3D() {

}
