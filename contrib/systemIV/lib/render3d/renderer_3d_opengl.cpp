#include "systemiv.h"

#ifdef RENDERER_OPENGL
#include "renderer_3d_opengl.h"
#include "lib/render/renderer.h"
#include "shaders/vertex.glsl.h"
#include "shaders/fragment.glsl.h"
#include "shaders/textured_vertex.glsl.h"
#include "shaders/textured_fragment.glsl.h"
#include "shaders/model_vertex.glsl.h"

// ============================================================================
// CONSTRUCTOR & DESTRUCTOR
// ============================================================================

Renderer3DOpenGL::Renderer3DOpenGL()
{
    Initialize3DShaders();
    Cache3DUniformLocations();
    Setup3DVertexArrays();
    Setup3DTexturedVertexArrays();
}

Renderer3DOpenGL::~Renderer3DOpenGL()
{
    CleanupBuffers3D();
}

// ============================================================================
// SHADER MANAGEMENT
// ============================================================================

void Renderer3DOpenGL::Initialize3DShaders()
{
    m_shader3DProgram = g_renderer->CreateShader(VERTEX_3D_SHADER_SOURCE, FRAGMENT_3D_SHADER_SOURCE);
    
    if (m_shader3DProgram == 0) {
        AppDebugOut("Renderer3D: Failed to create 3D shader program\n");
    }
    
    m_shader3DTexturedProgram = g_renderer->CreateShader(TEXTURED_VERTEX_3D_SHADER_SOURCE, TEXTURED_FRAGMENT_3D_SHADER_SOURCE);
    
    if (m_shader3DTexturedProgram == 0) {
        AppDebugOut("Renderer3D: Failed to create textured 3D shader program\n");
    }
    
    m_shader3DModelProgram = g_renderer->CreateShader(MODEL_VERTEX_3D_SHADER_SOURCE, FRAGMENT_3D_SHADER_SOURCE);
    
    if (m_shader3DModelProgram == 0) {
        AppDebugOut("Renderer3D: Failed to create model 3D shader program\n");
    }
}

void Renderer3DOpenGL::Cache3DUniformLocations()
{
    //
    // Cache 3D shader uniforms

    m_shader3DUniforms.projectionLoc = glGetUniformLocation(m_shader3DProgram, "uProjection");
    m_shader3DUniforms.modelViewLoc = glGetUniformLocation(m_shader3DProgram, "uModelView");
    m_shader3DUniforms.textureLoc = -1; // not used in 3D shader
    m_shader3DUniforms.fogEnabledLoc = glGetUniformLocation(m_shader3DProgram, "uFogEnabled");
    m_shader3DUniforms.fogOrientationLoc = glGetUniformLocation(m_shader3DProgram, "uFogOrientationBased");
    m_shader3DUniforms.fogStartLoc = glGetUniformLocation(m_shader3DProgram, "uFogStart");
    m_shader3DUniforms.fogEndLoc = glGetUniformLocation(m_shader3DProgram, "uFogEnd");
    m_shader3DUniforms.fogColorLoc = glGetUniformLocation(m_shader3DProgram, "uFogColor");
    m_shader3DUniforms.cameraPosLoc = glGetUniformLocation(m_shader3DProgram, "uCameraPos");
    
    //
    // Cache textured 3D shader uniforms

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
    // Cache model 3D shader uniforms

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

void Renderer3DOpenGL::Set3DShaderUniforms()
{
    if (m_matrices3DNeedUpdate || m_currentShaderProgram3D != m_shader3DProgram) 
    {
        glUniformMatrix4fv(m_shader3DUniforms.projectionLoc, 1, GL_FALSE, m_projectionMatrix3D.m);
        glUniformMatrix4fv(m_shader3DUniforms.modelViewLoc, 1, GL_FALSE, m_modelViewMatrix3D.m);
    }
    
    if (m_fog3DNeedsUpdate || m_currentShaderProgram3D != m_shader3DProgram) 
    {
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

void Renderer3DOpenGL::SetTextured3DShaderUniforms()
{
    if (m_matrices3DNeedUpdate || m_currentShaderProgram3D != m_shader3DTexturedProgram) 
    {
        glUniformMatrix4fv(m_shader3DTexturedUniforms.projectionLoc, 1, GL_FALSE, m_projectionMatrix3D.m);
        glUniformMatrix4fv(m_shader3DTexturedUniforms.modelViewLoc, 1, GL_FALSE, m_modelViewMatrix3D.m);
        glUniform1i(m_shader3DTexturedUniforms.textureLoc, 0);
    }

    if (m_fog3DNeedsUpdate || m_currentShaderProgram3D != m_shader3DTexturedProgram) 
    {
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

void Renderer3DOpenGL::SetFogUniforms3D(unsigned int shaderProgram)
{
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

void Renderer3DOpenGL::Set3DModelShaderUniforms(const Matrix4f& modelMatrix, const Colour& modelColor)
{
    if (m_matrices3DNeedUpdate || m_currentShaderProgram3D != m_shader3DModelProgram) 
    {
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

    if (m_fog3DNeedsUpdate || m_currentShaderProgram3D != m_shader3DModelProgram) 
    {
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

void Renderer3DOpenGL::Set3DModelShaderUniformsInstanced(const Matrix4f* modelMatrices, const Colour* modelColors, int instanceCount)
{
    int maxInstances = MAX_INSTANCES;
    
    if (instanceCount > maxInstances) 
    {
        instanceCount = maxInstances;
    }
    
    if (m_matrices3DNeedUpdate || m_currentShaderProgram3D != m_shader3DModelProgram) 
    {
        glUniformMatrix4fv(m_shader3DModelUniforms.projectionLoc, 1, GL_FALSE, m_projectionMatrix3D.m);
        glUniformMatrix4fv(m_shader3DModelUniforms.modelViewLoc, 1, GL_FALSE, m_modelViewMatrix3D.m);
    }
    
    glUniform1i(m_shader3DModelUniforms.useInstancingLoc, 1);
    glUniform1i(m_shader3DModelUniforms.instanceCountLoc, instanceCount);
    
    glUniformMatrix4fv(m_shader3DModelUniforms.modelMatricesLoc, instanceCount, GL_FALSE, (const float*)modelMatrices);
    
    float* colorArray = new float[instanceCount * 4];
    for (int i = 0; i < instanceCount; i++) 
    {
        colorArray[i * 4 + 0] = modelColors[i].GetRFloat();
        colorArray[i * 4 + 1] = modelColors[i].GetGFloat();
        colorArray[i * 4 + 2] = modelColors[i].GetBFloat();
        colorArray[i * 4 + 3] = modelColors[i].GetAFloat();
    }
    glUniform4fv(m_shader3DModelUniforms.modelColorsLoc, instanceCount, colorArray);
    delete[] colorArray;
    
    if (m_fog3DNeedsUpdate || m_currentShaderProgram3D != m_shader3DModelProgram) 
    {
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

// ============================================================================
// BUFFER MANAGEMENT
// ============================================================================

void Renderer3DOpenGL::Setup3DVertexArrays()
{
    //
    // Set up vertex attributes for Vertex3D format

    auto setup3DVertexAttributes = []() 
    {
        //
        // Position attribute (x, y, z)

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)0);
        glEnableVertexAttribArray(0);

        //
        // Color attribute (r, g, b, a)

        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
    };
    
    //
    // Create main 3D VAO/VBO pair

    glGenVertexArrays(1, &m_VAO3D);
    glGenBuffers(1, &m_VBO3D);
    glBindVertexArray(m_VAO3D);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO3D);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex3D) * MAX_3D_VERTICES, NULL, GL_DYNAMIC_DRAW);
    setup3DVertexAttributes();
    
    //
    // Create line VAO/VBO pair

    glGenVertexArrays(1, &m_lineVAO3D);
    glGenBuffers(1, &m_lineVBO3D);
    glBindVertexArray(m_lineVAO3D);
    glBindBuffer(GL_ARRAY_BUFFER, m_lineVBO3D);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex3D) * (MAX_LINE_VERTICES_3D), NULL, GL_STREAM_DRAW);
    setup3DVertexAttributes(); 
    
    //
    // Create circle VAO/VBO pair

    glGenVertexArrays(1, &m_circleVAO3D);
    glGenBuffers(1, &m_circleVBO3D);
    glBindVertexArray(m_circleVAO3D);
    glBindBuffer(GL_ARRAY_BUFFER, m_circleVBO3D);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex3D) * MAX_CIRCLE_VERTICES_3D, NULL, GL_DYNAMIC_DRAW);
    setup3DVertexAttributes();
    
    //
    // Create circle fill VAO/VBO pair

    glGenVertexArrays(1, &m_circleFillVAO3D);
    glGenBuffers(1, &m_circleFillVBO3D);
    glBindVertexArray(m_circleFillVAO3D);
    glBindBuffer(GL_ARRAY_BUFFER, m_circleFillVBO3D);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex3D) * MAX_CIRCLE_FILL_VERTICES_3D, NULL, GL_DYNAMIC_DRAW);
    setup3DVertexAttributes();
    
    //
    // Create rect VAO/VBO pair

    glGenVertexArrays(1, &m_rectVAO3D);
    glGenBuffers(1, &m_rectVBO3D);
    glBindVertexArray(m_rectVAO3D);
    glBindBuffer(GL_ARRAY_BUFFER, m_rectVBO3D);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex3D) * MAX_RECT_VERTICES_3D, NULL, GL_DYNAMIC_DRAW);
    setup3DVertexAttributes();
    
    //
    // Create rect fill VAO/VBO pair

    glGenVertexArrays(1, &m_rectFillVAO3D);
    glGenBuffers(1, &m_rectFillVBO3D);
    glBindVertexArray(m_rectFillVAO3D);
    glBindBuffer(GL_ARRAY_BUFFER, m_rectFillVBO3D);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex3D) * MAX_RECT_FILL_VERTICES_3D, NULL, GL_DYNAMIC_DRAW);
    setup3DVertexAttributes();
    
    //
    // Create triangle fill VAO/VBO pair

    glGenVertexArrays(1, &m_triangleFillVAO3D);
    glGenBuffers(1, &m_triangleFillVBO3D);
    glBindVertexArray(m_triangleFillVAO3D);
    glBindBuffer(GL_ARRAY_BUFFER, m_triangleFillVBO3D);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex3D) * MAX_TRIANGLE_FILL_VERTICES_3D, NULL, GL_DYNAMIC_DRAW);
    setup3DVertexAttributes();
    
    //
    // Create immediate VAO/VBO pair

    glGenVertexArrays(1, &m_immediateVAO3D);
    glGenBuffers(1, &m_immediateVBO3D);
    glBindVertexArray(m_immediateVAO3D);
    glBindBuffer(GL_ARRAY_BUFFER, m_immediateVBO3D);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex3D) * MAX_3D_VERTICES, NULL, GL_DYNAMIC_DRAW);
    setup3DVertexAttributes();
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    #ifdef _DEBUG
    AppDebugOut("Renderer3D: 3D vertex arrays setup complete\n");
    #endif
}

void Renderer3DOpenGL::Setup3DTexturedVertexArrays()
{
    //
    // Set up vertex attributes for Vertex3DTextured format

    auto setup3DTexturedVertexAttributes = []() 
    {
        //
        // Position attribute (x, y, z)

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3DTextured), (void*)0);
        glEnableVertexAttribArray(0);

        //
        // Normal attribute (nx, ny, nz)

        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3DTextured), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        //
        // Color attribute (r, g, b, a)

        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex3DTextured), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);

        //
        // Texture coordinate attribute (u, v)

        glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex3DTextured), (void*)(10 * sizeof(float)));
        glEnableVertexAttribArray(3);
    };
    
    //
    // Create main textured 3D VAO/VBO pair

    glGenVertexArrays(1, &m_VAO3DTextured);
    glGenBuffers(1, &m_VBO3DTextured);
    glBindVertexArray(m_VAO3DTextured);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO3DTextured);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex3DTextured) * MAX_3D_TEXTURED_VERTICES, NULL, GL_DYNAMIC_DRAW);
    setup3DTexturedVertexAttributes();
    
    //
    // Create sprite VAO/VBO pair

    glGenVertexArrays(1, &m_spriteVAO3D);
    glGenBuffers(1, &m_spriteVBO3D);
    glBindVertexArray(m_spriteVAO3D);
    glBindBuffer(GL_ARRAY_BUFFER, m_spriteVBO3D);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex3DTextured) * (MAX_STATIC_SPRITE_VERTICES_3D + MAX_ROTATING_SPRITE_VERTICES_3D), NULL, GL_DYNAMIC_DRAW);
    setup3DTexturedVertexAttributes();
    
    //
    // Create text VAO/VBO pair

    glGenVertexArrays(1, &m_textVAO3D);
    glGenBuffers(1, &m_textVBO3D);
    glBindVertexArray(m_textVAO3D);
    glBindBuffer(GL_ARRAY_BUFFER, m_textVBO3D);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex3DTextured) * MAX_TEXT_VERTICES_3D, NULL, GL_DYNAMIC_DRAW);
    setup3DTexturedVertexAttributes();
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    
    #ifdef _DEBUG
    AppDebugOut("Renderer3D: Textured 3D vertex arrays setup complete\n");
    #endif
}

void Renderer3DOpenGL::UploadVertexDataTo3DVBO(unsigned int vbo,
                                               const Vertex3D* vertices,
                                               int vertexCount,
                                               unsigned int usageHint)
{
    if (vertexCount <= 0 || !vertices) return;

    g_renderer->SetArrayBuffer(vbo);

    const GLsizeiptr bytes = static_cast<GLsizeiptr>(vertexCount) * sizeof(Vertex3D);

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

void Renderer3DOpenGL::UploadVertexDataTo3DVBO(unsigned int vbo,
                                               const Vertex3DTextured* vertices,
                                               int vertexCount,
                                               unsigned int usageHint)
{
    if (vertexCount <= 0 || !vertices) return;

    g_renderer->SetArrayBuffer(vbo);

    const GLsizeiptr bytes = static_cast<GLsizeiptr>(vertexCount) * sizeof(Vertex3DTextured);

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

// ============================================================================
// CORE FLUSH FUNCTIONS
// ============================================================================

void Renderer3DOpenGL::Flush3DVertices(unsigned int primitiveType)
{
    if (m_vertex3DCount == 0) return;
    
    g_renderer->StartFlushTiming("Immediate_Vertices_3D");
    IncrementDrawCall3D("immediate_vertices");
    
    g_renderer->SetShaderProgram(m_shader3DProgram);
    Set3DShaderUniforms();
    
    g_renderer->SetVertexArray(m_immediateVAO3D);
    UploadVertexDataTo3DVBO(m_immediateVBO3D, m_vertices3D, m_vertex3DCount, GL_DYNAMIC_DRAW);
    
    glDrawArrays(primitiveType, 0, m_vertex3DCount);
    
    m_vertex3DCount = 0;
    
    g_renderer->EndFlushTiming("Immediate_Vertices_3D");
}

void Renderer3DOpenGL::Flush3DTexturedVertices()
{
    if (m_vertex3DTexturedCount == 0) return;
    
    g_renderer->StartFlushTiming("Immediate_Triangles_3D");
    IncrementDrawCall3D("immediate_triangles");
    
    g_renderer->SetShaderProgram(m_shader3DTexturedProgram);
    SetTextured3DShaderUniforms();
    
    g_renderer->SetActiveTexture(GL_TEXTURE0);
    g_renderer->SetBoundTexture(m_currentTexture3D);
    
    g_renderer->SetVertexArray(m_VAO3DTextured);
    UploadVertexDataTo3DVBO(m_VBO3DTextured, m_vertices3DTextured, m_vertex3DTexturedCount, GL_DYNAMIC_DRAW);
    
    if (m_vertex3DTexturedCount == 4) 
    {
        Vertex3DTextured triangleVertices[6];
        
        triangleVertices[0] = m_vertices3DTextured[0];
        triangleVertices[1] = m_vertices3DTextured[1];  
        triangleVertices[2] = m_vertices3DTextured[2];
        
        triangleVertices[3] = m_vertices3DTextured[0];
        triangleVertices[4] = m_vertices3DTextured[2];
        triangleVertices[5] = m_vertices3DTextured[3];

        UploadVertexDataTo3DVBO(m_VBO3DTextured, triangleVertices, 6, GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    } 
    else 
    {
        glDrawArrays(GL_TRIANGLE_FAN, 0, m_vertex3DTexturedCount);
    }
    
    m_vertex3DTexturedCount = 0;
    
    g_renderer->EndFlushTiming("Immediate_Triangles_3D");
}

void Renderer3DOpenGL::FlushLine3D()
{
    if (m_lineVertexCount3D == 0) return;

    g_renderer->StartFlushTiming("Batched_Lines_3D");
    IncrementDrawCall3D("batched_lines");
    
#ifndef TARGET_EMSCRIPTEN
    g_renderer->SetLineWidth(m_currentLineWidth3D);
#endif
    
    g_renderer->SetShaderProgram(m_shader3DProgram);
    Set3DShaderUniforms();
    
    g_renderer->SetVertexArray(m_lineVAO3D);
    UploadVertexDataTo3DVBO(m_lineVBO3D, m_lineVertices3D, m_lineVertexCount3D, GL_STREAM_DRAW);
    
    glDrawArrays(GL_LINES, 0, m_lineVertexCount3D);
    
    m_lineVertexCount3D = 0;
    
    g_renderer->EndFlushTiming("Batched_Lines_3D");
}

void Renderer3DOpenGL::FlushStaticSprites3D()
{
    if (m_staticSpriteVertexCount3D == 0) return;
    
    g_renderer->StartFlushTiming("Static_Sprite_3D");
    IncrementDrawCall3D("static_sprites");
    
    g_renderer->SetDepthMask(false);
    
    g_renderer->SetShaderProgram(m_shader3DTexturedProgram);
    SetTextured3DShaderUniforms();
    
    if (m_currentStaticSpriteTexture3D != 0) 
    {
        g_renderer->SetActiveTexture(GL_TEXTURE0);
        g_renderer->SetBoundTexture(m_currentStaticSpriteTexture3D);
    }
    
    g_renderer->SetVertexArray(m_spriteVAO3D);
    UploadVertexDataTo3DVBO(m_spriteVBO3D, m_staticSpriteVertices3D, m_staticSpriteVertexCount3D, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_staticSpriteVertexCount3D);
    g_renderer->SetDepthMask(true);
    
    m_staticSpriteVertexCount3D = 0;
    
    g_renderer->EndFlushTiming("Static_Sprite_3D");
}

void Renderer3DOpenGL::FlushRotatingSprite3D()
{
    if (m_rotatingSpriteVertexCount3D == 0) return;
    
    g_renderer->StartFlushTiming("Unit_Rotating_3D");
    IncrementDrawCall3D("rotating_sprites");
    
    g_renderer->SetDepthMask(false);
    
    g_renderer->SetShaderProgram(m_shader3DTexturedProgram);
    SetTextured3DShaderUniforms();
    
    if (m_currentRotatingSpriteTexture3D != 0) 
    {
        g_renderer->SetActiveTexture(GL_TEXTURE0);
        g_renderer->SetBoundTexture(m_currentRotatingSpriteTexture3D);
    }
    
    g_renderer->SetVertexArray(m_spriteVAO3D);
    UploadVertexDataTo3DVBO(m_spriteVBO3D, m_rotatingSpriteVertices3D, m_rotatingSpriteVertexCount3D, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_rotatingSpriteVertexCount3D);
    g_renderer->SetDepthMask(true);
    
    m_rotatingSpriteVertexCount3D = 0;
    
    g_renderer->EndFlushTiming("Unit_Rotating_3D");
}

void Renderer3DOpenGL::FlushTextBuffer3D()
{
    if (m_textVertexCount3D == 0) return;
    
    g_renderer->StartFlushTiming("Text_3D");
    IncrementDrawCall3D("text");
    
    int currentBlendSrc = g_renderer->GetCurrentBlendSrcFactor();
    int currentBlendDst = g_renderer->GetCurrentBlendDstFactor();
    
    g_renderer->SetDepthMask(false);
    
    if (g_renderer->GetBlendMode() != Renderer::BlendModeSubtractive) 
    {
        g_renderer->SetBlendMode(Renderer::BlendModeAdditive);
    }
    
    g_renderer->SetShaderProgram(m_shader3DTexturedProgram);
    SetTextured3DShaderUniforms();
    
    if (m_currentTextTexture3D != 0) 
    {
        g_renderer->SetActiveTexture(GL_TEXTURE0);
        g_renderer->SetBoundTexture(m_currentTextTexture3D);
    }
    
    g_renderer->SetVertexArray(m_textVAO3D);
    UploadVertexDataTo3DVBO(m_textVBO3D, m_textVertices3D, m_textVertexCount3D, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_textVertexCount3D);
    
    g_renderer->SetBlendFunc(currentBlendSrc, currentBlendDst);
    
    m_textVertexCount3D = 0;
    m_currentTextTexture3D = 0;
    
    g_renderer->EndFlushTiming("Text_3D");
}

void Renderer3DOpenGL::FlushCircles3D()
{
    if (m_circleVertexCount3D == 0) return;
    
    g_renderer->StartFlushTiming("Circles_3D");
    IncrementDrawCall3D("circles");
    
#ifndef TARGET_EMSCRIPTEN
    g_renderer->SetLineWidth(m_currentCircleWidth3D);
#endif
    
    g_renderer->SetShaderProgram(m_shader3DProgram);
    Set3DShaderUniforms();
    
    g_renderer->SetVertexArray(m_circleVAO3D);
    UploadVertexDataTo3DVBO(m_circleVBO3D, m_circleVertices3D, m_circleVertexCount3D, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_LINES, 0, m_circleVertexCount3D);
    
    m_circleVertexCount3D = 0;
    
    g_renderer->EndFlushTiming("Circles_3D");
}

void Renderer3DOpenGL::FlushCircleFills3D()
{
    if (m_circleFillVertexCount3D == 0) return;
    
    g_renderer->StartFlushTiming("Circle_Fills_3D");
    IncrementDrawCall3D("circle_fills");
    
    g_renderer->SetShaderProgram(m_shader3DProgram);
    Set3DShaderUniforms();
    
    g_renderer->SetVertexArray(m_circleFillVAO3D);
    UploadVertexDataTo3DVBO(m_circleFillVBO3D, m_circleFillVertices3D, m_circleFillVertexCount3D, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_circleFillVertexCount3D);
    
    m_circleFillVertexCount3D = 0;
    
    g_renderer->EndFlushTiming("Circle_Fills_3D");
}

void Renderer3DOpenGL::FlushRects3D()
{
    if (m_rectVertexCount3D == 0) return;
    
    g_renderer->StartFlushTiming("Rects_3D");
    IncrementDrawCall3D("rects");
    
#ifndef TARGET_EMSCRIPTEN
    g_renderer->SetLineWidth(m_currentRectWidth3D);
#endif
    
    g_renderer->SetShaderProgram(m_shader3DProgram);
    Set3DShaderUniforms();
    
    g_renderer->SetVertexArray(m_rectVAO3D);
    UploadVertexDataTo3DVBO(m_rectVBO3D, m_rectVertices3D, m_rectVertexCount3D, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_LINES, 0, m_rectVertexCount3D);
    
    m_rectVertexCount3D = 0;
    
    g_renderer->EndFlushTiming("Rects_3D");
}

void Renderer3DOpenGL::FlushRectFills3D()
{
    if (m_rectFillVertexCount3D == 0) return;
    
    g_renderer->StartFlushTiming("Rect_Fills_3D");
    IncrementDrawCall3D("rect_fills");
    
    g_renderer->SetShaderProgram(m_shader3DProgram);
    Set3DShaderUniforms();
    
    g_renderer->SetVertexArray(m_rectFillVAO3D);
    UploadVertexDataTo3DVBO(m_rectFillVBO3D, m_rectFillVertices3D, m_rectFillVertexCount3D, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_rectFillVertexCount3D);
    
    m_rectFillVertexCount3D = 0;
    
    g_renderer->EndFlushTiming("Rect_Fills_3D");
}

void Renderer3DOpenGL::FlushTriangleFills3D()
{
    if (m_triangleFillVertexCount3D == 0) return;
    
    g_renderer->StartFlushTiming("Triangle_Fills_3D");
    IncrementDrawCall3D("triangle_fills");
    
    g_renderer->SetShaderProgram(m_shader3DProgram);
    Set3DShaderUniforms();
    
    g_renderer->SetVertexArray(m_triangleFillVAO3D);
    UploadVertexDataTo3DVBO(m_triangleFillVBO3D, m_triangleFillVertices3D, m_triangleFillVertexCount3D, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_TRIANGLES, 0, m_triangleFillVertexCount3D);
    
    m_triangleFillVertexCount3D = 0;
    
    g_renderer->EndFlushTiming("Triangle_Fills_3D");
}

// ============================================================================
// CLEANUP
// ============================================================================

void Renderer3DOpenGL::CleanupBuffers3D()
{
    if (m_VBO3D) glDeleteBuffers(1, &m_VBO3D);
    if (m_VAO3D) glDeleteVertexArrays(1, &m_VAO3D);
    if (m_VBO3DTextured) glDeleteBuffers(1, &m_VBO3DTextured);
    if (m_VAO3DTextured) glDeleteVertexArrays(1, &m_VAO3DTextured);
    
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
    
    if (m_shader3DProgram) glDeleteProgram(m_shader3DProgram);
    if (m_shader3DTexturedProgram) glDeleteProgram(m_shader3DTexturedProgram);
    if (m_shader3DModelProgram) glDeleteProgram(m_shader3DModelProgram);
}

// ============================================================================
// MEGAVBO BUFFER MANAGEMENT
// ============================================================================

unsigned int Renderer3DOpenGL::CreateMegaVBOVertexBuffer3D(size_t size, BufferUsageHint usageHint)
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

unsigned int Renderer3DOpenGL::CreateMegaVBOIndexBuffer3D(size_t size, BufferUsageHint usageHint)
{
    (void)usageHint;
    (void)size;
    
    unsigned int ibo;
    glGenBuffers(1, &ibo);
    return ibo;
}

unsigned int Renderer3DOpenGL::CreateMegaVBOVertexArray3D()
{
    unsigned int vao;
    glGenVertexArrays(1, &vao);
    return vao;
}

void Renderer3DOpenGL::DeleteMegaVBOVertexBuffer3D(unsigned int buffer)
{
    if (buffer != 0) 
    {
        glDeleteBuffers(1, &buffer);
    }
}

void Renderer3DOpenGL::DeleteMegaVBOIndexBuffer3D(unsigned int buffer)
{
    if (buffer != 0) 
    {
        glDeleteBuffers(1, &buffer);
    }
}

void Renderer3DOpenGL::DeleteMegaVBOVertexArray3D(unsigned int vao)
{
    if (vao != 0) 
    {
        glDeleteVertexArrays(1, &vao);
    }
}

void Renderer3DOpenGL::SetupMegaVBOVertexAttributes3D(unsigned int vao, unsigned int vbo, unsigned int ibo)
{
    g_renderer->SetVertexArray(vao);
    g_renderer->SetArrayBuffer(vbo);
    
    // Position attribute (x, y, z)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Color attribute (r, g, b, a)
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    // Bind index buffer to VAO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
}

void Renderer3DOpenGL::SetupMegaVBOVertexAttributes3DTextured(unsigned int vao, unsigned int vbo, unsigned int ibo)
{
    g_renderer->SetVertexArray(vao);
    g_renderer->SetArrayBuffer(vbo);
    
    // Position attribute (x, y, z)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3DTextured), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Normal attribute (nx, ny, nz)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3DTextured), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    // Color attribute (r, g, b, a)
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex3DTextured), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    // Texture coordinate attribute (u, v)
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex3DTextured), (void*)(10 * sizeof(float)));
    glEnableVertexAttribArray(3);
    
    // Bind index buffer to VAO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
}

void Renderer3DOpenGL::UploadMegaVBOIndexData3D(unsigned int ibo, const unsigned int* indices,
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

void Renderer3DOpenGL::UploadMegaVBOVertexData3D(unsigned int vbo, const Vertex3D* vertices,
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
    glBufferData(GL_ARRAY_BUFFER, vertexCount * sizeof(Vertex3D), vertices, glUsage);
}

void Renderer3DOpenGL::UploadMegaVBOVertexData3DTextured(unsigned int vbo, const Vertex3DTextured* vertices,
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
    glBufferData(GL_ARRAY_BUFFER, vertexCount * sizeof(Vertex3DTextured), vertices, glUsage);
}

void Renderer3DOpenGL::DrawMegaVBOIndexed3D(PrimitiveType primitiveType, unsigned int indexCount)
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

void Renderer3DOpenGL::DrawMegaVBOIndexedInstanced3D(PrimitiveType primitiveType, unsigned int indexCount, unsigned int instanceCount)
{
    GLenum glPrimitive;
    switch (primitiveType) 
    {
        case PRIMITIVE_TRIANGLES:   glPrimitive = GL_TRIANGLES; break;
        case PRIMITIVE_LINE_STRIP:  glPrimitive = GL_LINE_STRIP; break;
        case PRIMITIVE_LINES:       glPrimitive = GL_LINES; break;
        default: glPrimitive = GL_TRIANGLES; break;
    }
    
    glDrawElementsInstanced(glPrimitive, indexCount, GL_UNSIGNED_INT, 0, instanceCount);
}

void Renderer3DOpenGL::SetupMegaVBOInstancedVertexAttributes3DTextured(unsigned int vao, unsigned int vbo, unsigned int ibo)
{
    g_renderer->SetVertexArray(vao);
    g_renderer->SetArrayBuffer(vbo);
    
    // For instanced rendering with textured VBOs, we need to remap:
    // location 0 = position, location 1 = color (from location 2)
    
    // Position attribute (x, y, z) - location 0
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3DTextured), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Disable normal attribute (location 1)
    glDisableVertexAttribArray(1);
    
    // Color attribute (r, g, b, a) - location 1 (remapped from location 2)
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex3DTextured), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    // Disable texture coordinate attribute (location 3)
    glDisableVertexAttribArray(3);
    
    // Bind index buffer to VAO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
}

void Renderer3DOpenGL::EnableMegaVBOPrimitiveRestart3D(unsigned int restartIndex)
{
#ifndef TARGET_EMSCRIPTEN
    glEnable(GL_PRIMITIVE_RESTART);
    glPrimitiveRestartIndex(restartIndex);
#endif
}

void Renderer3DOpenGL::DisableMegaVBOPrimitiveRestart3D()
{
#ifndef TARGET_EMSCRIPTEN
    glDisable(GL_PRIMITIVE_RESTART);
#endif
}

#endif // RENDERER_OPENGL

