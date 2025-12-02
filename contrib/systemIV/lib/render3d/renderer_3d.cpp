#include "lib/resource/image.h"
#include "lib/universal_include.h"

#include <math.h>
#include <string.h>

#include "lib/debug_utils.h"
#include "lib/hi_res_time.h"
#include "lib/render2d/renderer.h"
#include "lib/resource/sprite_atlas.h"
#include "lib/resource/image.h"
#include "renderer/map_renderer.h"
#include "shaders/vertex.glsl.h"
#include "shaders/fragment.glsl.h"
#include "shaders/textured_vertex.glsl.h"
#include "shaders/textured_fragment.glsl.h"

#include "renderer_3d.h"

Renderer3D *g_renderer3d = NULL;

// ================================
// Matrix4f3D Implementation
// ================================

constexpr void Matrix4f3D::LoadIdentity() {
    m[0] = 1.0f; m[1] = 0.0f; m[2] = 0.0f; m[3] = 0.0f;
    m[4] = 0.0f; m[5] = 1.0f; m[6] = 0.0f; m[7] = 0.0f;
    m[8] = 0.0f; m[9] = 0.0f; m[10] = 1.0f; m[11] = 0.0f;
    m[12] = 0.0f; m[13] = 0.0f; m[14] = 0.0f; m[15] = 1.0f;
}

void Matrix4f3D::Perspective(float fovy, float aspect, float nearZ, float farZ) {
    float rad = fovy * M_PI / 180.0f;
    float f = 1.0f / tanf(rad / 2.0f);
    
    LoadIdentity();
    
    m[0] = f / aspect;
    m[5] = f;
    m[10] = (farZ + nearZ) / (nearZ - farZ);
    m[11] = -1.0f;
    m[14] = (2.0f * farZ * nearZ) / (nearZ - farZ);
    m[15] = 0.0f;
}

void Matrix4f3D::LookAt(float eyeX, float eyeY, float eyeZ,
                        float centerX, float centerY, float centerZ,
                        float upX, float upY, float upZ) {
    
    //
    // Calculate look direction (from eye to center)
    
    float fx = centerX - eyeX;
    float fy = centerY - eyeY;
    float fz = centerZ - eyeZ;
    Normalize(fx, fy, fz);
    
    //
    // Calculate right vector (forward cross up)
    
    float sx, sy, sz;
    Cross(fx, fy, fz, upX, upY, upZ, sx, sy, sz);
    Normalize(sx, sy, sz);
    
    //
    // Calculate up vector (right cross forward)
    
    float ux, uy, uz;
    Cross(sx, sy, sz, fx, fy, fz, ux, uy, uz);
    
    //
    // Build view matrix exactly like gluLookAt (column-major order)
    
    LoadIdentity();
    
    // Column 0
    m[0] = sx;
    m[1] = ux; 
    m[2] = -fx;
    m[3] = 0.0f;
    
    // Column 1
    m[4] = sy;
    m[5] = uy;
    m[6] = -fy;
    m[7] = 0.0f;
    
    // Column 2
    m[8] = sz;
    m[9] = uz;
    m[10] = -fz;
    m[11] = 0.0f;
    
    // Column 3 (translation)
    m[12] = -(sx * eyeX + sy * eyeY + sz * eyeZ);
    m[13] = -(ux * eyeX + uy * eyeY + uz * eyeZ);
    m[14] = -(-fx * eyeX + -fy * eyeY + -fz * eyeZ);
    m[15] = 1.0f;
}

constexpr void Matrix4f3D::Multiply(const Matrix4f3D& other) {
    Matrix4f3D result;
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

void Matrix4f3D::Copy(float* dest) const {
    memcpy(dest, m, sizeof(m));
}

void Matrix4f3D::Normalize(float& x, float& y, float& z) {
    float length = sqrtf(x * x + y * y + z * z);
    if (length > 0.0f) {
        x /= length;
        y /= length;
        z /= length;
    }
}

constexpr void Matrix4f3D::Cross(float ax, float ay, float az, float bx, float by, float bz, float& cx, float& cy, float& cz) {
    cx = ay * bz - az * by;
    cy = az * bx - ax * bz;
    cz = ax * by - ay * bx;
}

// ================================
// Renderer3D Implementation
// ================================

Renderer3D::Renderer3D(Renderer* renderer)
:   m_renderer(renderer),
    m_shader3DProgram(0),
    m_shader3DTexturedProgram(0),
    m_VAO3D(0), m_VBO3D(0),
    m_VAO3DTextured(0), m_VBO3DTextured(0),
    m_spriteVAO3D(0), m_spriteVBO3D(0),
    m_lineVAO3D(0), m_lineVBO3D(0),
    m_nukeVAO3D(0), m_nukeVBO3D(0),
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
    m_texturedQuad3DActive(false),
    m_currentTexture3D(0),
    m_maxMegaVertices3D(100),
    m_maxMegaIndices3D(100),
    m_megaVBO3DActive(false),
    m_currentMegaVBO3DKey(NULL),
    m_megaVertices3D(NULL),
    m_megaVertex3DCount(0),
    m_megaIndices3D(NULL),
    m_megaIndex3DCount(0),
    m_megaVBO3DTexturedActive(false),
    m_currentMegaVBO3DTexturedKey(NULL),
    m_currentMegaVBO3DTextureID(0),
    m_maxMegaTexturedVertices3D(100),
    m_maxMegaTexturedIndices3D(100),
    m_megaTexturedVertices3D(NULL),
    m_megaTexturedVertex3DCount(0),
    m_megaTexturedIndices3D(NULL),
    m_megaTexturedIndex3DCount(0),
    m_lineConversionBuffer3D(NULL),
    m_lineConversionBufferSize3D(0),
    m_lineVertexCount3D(0),
    m_staticSpriteVertexCount3D(0),
    m_currentStaticSpriteTexture3D(0),
    m_rotatingSpriteVertexCount3D(0),
    m_currentRotatingSpriteTexture3D(0),
    m_textVertexCount3D(0),
    m_currentTextTexture3D(0),
    m_nuke3DModelVertexCount3D(0),
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
    m_nuke3DModelCalls3D = 0;
    m_circleCalls3D = 0;
    m_circleFillCalls3D = 0;
    m_rectCalls3D = 0;
    m_rectFillCalls3D = 0;
    m_triangleFillCalls3D = 0;
    m_prevDrawCallsPerFrame3D = 0;
    m_prevImmediateLineCalls3D = 0;
    m_prevImmediateTriangleCalls3D = 0;
    m_prevLineCalls3D = 0;
    m_prevStaticSpriteCalls3D = 0;
    m_prevRotatingSpriteCalls3D = 0;
    m_prevTextCalls3D = 0;
    m_prevMegaVBOCalls3D = 0;
    m_prevNuke3DModelCalls3D = 0;
    m_prevCircleCalls3D = 0;
    m_prevCircleFillCalls3D = 0;
    m_prevRectCalls3D = 0;
    m_prevRectFillCalls3D = 0;
    m_prevTriangleFillCalls3D = 0;
    m_flushTimingCount3D = 0;
    m_currentFlushStartTime3D = 0.0;
    
    Initialize3DShaders();
    Cache3DUniformLocations();
    Setup3DVertexArrays();
    Setup3DTexturedVertexArrays();
    
    //
    // Allocate mega vertex and index buffers

    if (m_maxMegaVertices3D > 0) {
        m_megaVertices3D = new Vertex3D[m_maxMegaVertices3D];
        m_megaIndices3D = new unsigned int[m_maxMegaIndices3D];
    }

    //
    // Allocate textured mega vertex and index buffers

    if (m_maxMegaTexturedVertices3D > 0) {
        m_megaTexturedVertices3D = new Vertex3DTextured[m_maxMegaTexturedVertices3D];
        m_megaTexturedIndices3D = new unsigned int[m_maxMegaTexturedIndices3D];
    }
    
    m_lineConversionBufferSize3D = MAX_3D_VERTICES * 2;
    m_lineConversionBuffer3D = new Vertex3D[m_lineConversionBufferSize3D];
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
    if (m_nukeVAO3D) glDeleteVertexArrays(1, &m_nukeVAO3D);
    if (m_nukeVBO3D) glDeleteBuffers(1, &m_nukeVBO3D);
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
    
    //
    // Clean up mega vertex and index buffers
    
    if (m_megaVertices3D) {
        delete[] m_megaVertices3D;
        m_megaVertices3D = NULL;
    }
    if (m_megaIndices3D) {
        delete[] m_megaIndices3D;
        m_megaIndices3D = NULL;
    }
    if (m_currentMegaVBO3DKey) {
        delete[] m_currentMegaVBO3DKey;
        m_currentMegaVBO3DKey = NULL;
    }
    
    if (m_megaTexturedVertices3D) {
        delete[] m_megaTexturedVertices3D;
        m_megaTexturedVertices3D = NULL;
    }
    if (m_megaTexturedIndices3D) {
        delete[] m_megaTexturedIndices3D;
        m_megaTexturedIndices3D = NULL;
    }
    if (m_currentMegaVBO3DTexturedKey) {
        delete[] m_currentMegaVBO3DTexturedKey;
        m_currentMegaVBO3DTexturedKey = NULL;
    }
    
    if (m_lineConversionBuffer3D) {
        delete[] m_lineConversionBuffer3D;
        m_lineConversionBuffer3D = NULL;
    }
    
    //
    // Clean up cached VBOs

    DArray<char*> *keys = m_cached3DVBOs.ConvertIndexToDArray();
    for (int i = 0; i < keys->Size(); i++) {
        Cached3DVBO* cachedVBO = m_cached3DVBOs.GetData(keys->GetData(i));
        if (cachedVBO) {
            if (cachedVBO->VBO) glDeleteBuffers(1, &cachedVBO->VBO);
            if (cachedVBO->VAO) glDeleteVertexArrays(1, &cachedVBO->VAO);
            delete cachedVBO;
        }
    }
    delete keys;
    m_cached3DVBOs.Empty();
}

//
// Create 3D shader programs using shader sources from .glsl.h headers

void Renderer3D::Initialize3DShaders() {
    m_shader3DProgram = m_renderer->CreateShader(VERTEX_3D_SHADER_SOURCE, FRAGMENT_3D_SHADER_SOURCE);
    
    //
    // Create 3D shader program

    if (m_shader3DProgram == 0) {
        AppDebugOut("Renderer3D: Failed to create 3D shader program\n");
    }
    
    //
    // Create textured 3D shader program

    m_shader3DTexturedProgram = m_renderer->CreateShader(TEXTURED_VERTEX_3D_SHADER_SOURCE, TEXTURED_FRAGMENT_3D_SHADER_SOURCE);
    
    if (m_shader3DTexturedProgram == 0) {
        AppDebugOut("Renderer3D: Failed to create textured 3D shader program\n");
    }
}

//
// Get UV coordinates for an image from the atlas

void Renderer3D::GetImageUVCoords(Image* image, float& u1, float& v1, float& u2, float& v2) {
    AtlasImage* atlasImage = dynamic_cast<AtlasImage*>(image);
    if (atlasImage) {

        //
        // Use atlas coordinates

        const AtlasCoord* coord = atlasImage->GetAtlasCoord();
        if (coord) {
            u1 = coord->u1;
            v1 = coord->v1;
            u2 = coord->u2;
            v2 = coord->v2;
            return;
        }
    }
    
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
// Get texture ID for batching

unsigned int Renderer3D::GetEffectiveTextureID(Image* image) {
    AtlasImage* atlasImage = dynamic_cast<AtlasImage*>(image);
    if (atlasImage) {
        return atlasImage->GetAtlasTextureID();
    }
    return image->m_textureID;
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
    // Create Nuke 3D Models VAO/VBO pair

    glGenVertexArrays(1, &m_nukeVAO3D);
    glGenBuffers(1, &m_nukeVBO3D);
    glBindVertexArray(m_nukeVAO3D);
    glBindBuffer(GL_ARRAY_BUFFER, m_nukeVBO3D);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex3D) * MAX_NUKE_3D_MODEL_VERTICES_3D, NULL, GL_DYNAMIC_DRAW);
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

        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex3DTextured), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex3DTextured), (void*)(7 * sizeof(float)));
        glEnableVertexAttribArray(2);
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

void Renderer3D::BeginLineStrip3D(Colour const &col) {
    m_lineStrip3DActive = true;
    m_lineStrip3DColor = col;
    m_vertex3DCount = 0;
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

    Flush3DVertices(GL_LINES);
    
    m_lineStrip3DActive = false;
}

void Renderer3D::BeginLineLoop3D(Colour const &col) {
    BeginLineStrip3D(col);
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
    
    m_vertices3DTextured[m_vertex3DTexturedCount] = Vertex3DTextured(x, y, z, r, g, b, a, u, v);
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

    //
    // Only upload matrices if they changed or we switched shaders

    if (m_matrices3DNeedUpdate || m_currentShaderProgram3D != m_shader3DTexturedProgram) {
        glUniformMatrix4fv(m_shader3DTexturedUniforms.projectionLoc, 1, GL_FALSE, m_projectionMatrix3D.m);
        glUniformMatrix4fv(m_shader3DTexturedUniforms.modelViewLoc, 1, GL_FALSE, m_modelViewMatrix3D.m);
        glUniform1i(m_shader3DTexturedUniforms.textureLoc, 0);
    }
    
    //
    // Only upload fog uniforms if they changed or we switched shaders

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

    glBindBuffer(GL_ARRAY_BUFFER, vbo);

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

    glBindBuffer(GL_ARRAY_BUFFER, vbo);

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
    vertices[0] = {position.x - tangent1.x * halfWidth + tangent2.x * halfHeight,
                   position.y - tangent1.y * halfWidth + tangent2.y * halfHeight,
                   position.z - tangent1.z * halfWidth + tangent2.z * halfHeight,
                   r, g, b, a, u1, v2};
    vertices[1] = {position.x + tangent1.x * halfWidth + tangent2.x * halfHeight,
                   position.y + tangent1.y * halfWidth + tangent2.y * halfHeight,
                   position.z + tangent1.z * halfWidth + tangent2.z * halfHeight,
                   r, g, b, a, u2, v2};
    vertices[2] = {position.x + tangent1.x * halfWidth - tangent2.x * halfHeight,
                   position.y + tangent1.y * halfWidth - tangent2.y * halfHeight,
                   position.z + tangent1.z * halfWidth - tangent2.z * halfHeight,
                   r, g, b, a, u2, v1};
    
    // Second triangle: TL, BR, BL
    vertices[3] = vertices[0]; // TL
    vertices[4] = vertices[2]; // BR
    vertices[5] = {position.x - tangent1.x * halfWidth - tangent2.x * halfHeight,
                   position.y - tangent1.y * halfWidth - tangent2.y * halfHeight,
                   position.z - tangent1.z * halfWidth - tangent2.z * halfHeight,
                   r, g, b, a, u1, v1};
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
    vertices[0] = {position.x - right.x * halfWidth + up.x * halfHeight,
                   position.y - right.y * halfWidth + up.y * halfHeight,
                   position.z - right.z * halfWidth + up.z * halfHeight,
                   r, g, b, a, u1, v2};
    vertices[1] = {position.x + right.x * halfWidth + up.x * halfHeight,
                   position.y + right.y * halfWidth + up.y * halfHeight,
                   position.z + right.z * halfWidth + up.z * halfHeight,
                   r, g, b, a, u2, v2};
    vertices[2] = {position.x + right.x * halfWidth - up.x * halfHeight,
                   position.y + right.y * halfWidth - up.y * halfHeight,
                   position.z + right.z * halfWidth - up.z * halfHeight,
                   r, g, b, a, u2, v1};
    
    // Second triangle: TL, BR, BL
    vertices[3] = vertices[0]; // TL
    vertices[4] = vertices[2]; // BR
    vertices[5] = {position.x - right.x * halfWidth - up.x * halfHeight,
                   position.y - right.y * halfWidth - up.y * halfHeight,
                   position.z - right.z * halfWidth - up.z * halfHeight,
                   r, g, b, a, u1, v1};
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
        case hash("nuke_3d_models"): m_nuke3DModelCalls3D++; break;
        case hash("circles"): m_circleCalls3D++; break;
        case hash("circle_fills"): m_circleFillCalls3D++; break;
        case hash("rects"): m_rectCalls3D++; break;
        case hash("rect_fills"): m_rectFillCalls3D++; break;
        case hash("triangle_fills"): m_triangleFillCalls3D++; break;
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
    m_prevNuke3DModelCalls3D = m_nuke3DModelCalls3D;
    m_prevCircleCalls3D = m_circleCalls3D;
    m_prevCircleFillCalls3D = m_circleFillCalls3D;
    m_prevRectCalls3D = m_rectCalls3D;
    m_prevRectFillCalls3D = m_rectFillCalls3D;
    m_prevTriangleFillCalls3D = m_triangleFillCalls3D;
    
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
    m_nuke3DModelCalls3D = 0;
    m_circleCalls3D = 0;
    m_circleFillCalls3D = 0;
    m_rectCalls3D = 0;
    m_rectFillCalls3D = 0;
    m_triangleFillCalls3D = 0;
}

void Renderer3D::BeginFrame3D() {
    ResetFrameCounters3D();
    ResetFlushTimings3D();
}

void Renderer3D::EndFrame3D() {
    g_renderer->UpdateGpuTimings();
}

// ============================================================================
// DEBUG MENU FUNCTIONS
// ============================================================================

void Renderer3D::StartFlushTiming3D(const char* name) {
    
    m_currentFlushStartTime3D = GetHighResTime();
    
    for (int i = 0; i < m_flushTimingCount3D; i++) {
        if (strcmp(m_flushTimings3D[i].name, name) == 0) {
            return;
        }
    }
    
    if (m_flushTimingCount3D < MAX_FLUSH_TYPES_3D) {
        m_flushTimings3D[m_flushTimingCount3D].name = name;
        m_flushTimings3D[m_flushTimingCount3D].totalTime = 0.0;
        m_flushTimings3D[m_flushTimingCount3D].callCount = 0;
        m_flushTimingCount3D++;
    }
}

void Renderer3D::EndFlushTiming3D(const char* name) {
    double endTime = GetHighResTime();
    double elapsed = endTime - m_currentFlushStartTime3D;
    
    for (int i = 0; i < m_flushTimingCount3D; i++) {
        if (strcmp(m_flushTimings3D[i].name, name) == 0) {
            m_flushTimings3D[i].totalTime += elapsed;
            m_flushTimings3D[i].callCount++;
            return;
        }
    }
}

void Renderer3D::ResetFlushTimings3D() {
    for (int i = 0; i < m_flushTimingCount3D; i++) {
        m_flushTimings3D[i].totalTime = 0.0;
        m_flushTimings3D[i].callCount = 0;
    }
}

const Renderer3D::FlushTiming3D* Renderer3D::GetFlushTimings3D(int& count) const {
    count = m_flushTimingCount3D;
    return m_flushTimings3D;
} 