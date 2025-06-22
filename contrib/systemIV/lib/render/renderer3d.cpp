#include "lib/universal_include.h"

#include <math.h>
#include <string.h>

#include "lib/debug_utils.h"
#include "lib/string_utils.h"
#include "renderer3d.h"
#include "renderer.h"

Renderer3D *g_renderer3d = NULL;

// ================================
// Matrix4f3D Implementation
// ================================

Matrix4f3D::Matrix4f3D() {
    LoadIdentity();
}

void Matrix4f3D::LoadIdentity() {
    memset(m, 0, sizeof(m));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
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
    // Calculate look direction (from eye to center)
    float fx = centerX - eyeX;
    float fy = centerY - eyeY;
    float fz = centerZ - eyeZ;
    Normalize(fx, fy, fz);
    
    // Calculate right vector (forward cross up)
    float sx, sy, sz;
    Cross(fx, fy, fz, upX, upY, upZ, sx, sy, sz);
    Normalize(sx, sy, sz);
    
    // Calculate up vector (right cross forward)
    float ux, uy, uz;
    Cross(sx, sy, sz, fx, fy, fz, ux, uy, uz);
    
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

void Matrix4f3D::Multiply(const Matrix4f3D& other) {
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

void Matrix4f3D::Cross(float ax, float ay, float az, float bx, float by, float bz, float& cx, float& cy, float& cz) {
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
    m_VAO3D(0),
    m_VBO3D(0),
    m_vertex3DCount(0),
    m_lineStrip3DActive(false),
    m_megaVBO3DActive(false),
    m_currentMegaVBO3DKey(NULL),
    m_megaVertices3D(NULL),
    m_megaVertex3DCount(0)
{
    // Initialize fog parameters
    m_fogEnabled = false;
    m_fogStart = 1.0f;
    m_fogEnd = 10.0f;
    m_fogDensity = 1.0f;
    m_fogColor[0] = 0.0f; // R
    m_fogColor[1] = 0.0f; // G
    m_fogColor[2] = 0.0f; // B
    m_fogColor[3] = 1.0f; // A
    
    Initialize3DShaders();
    Setup3DVertexArrays();
    
    // Allocate mega vertex buffer for 3D
    m_megaVertices3D = new Vertex3D[MAX_MEGA_3D_VERTICES];
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
    if (m_shader3DProgram) {
        glDeleteProgram(m_shader3DProgram);
        m_shader3DProgram = 0;
    }
    
    // Clean up mega vertex buffer
    if (m_megaVertices3D) {
        delete[] m_megaVertices3D;
        m_megaVertices3D = NULL;
    }
    if (m_currentMegaVBO3DKey) {
        delete[] m_currentMegaVBO3DKey;
        m_currentMegaVBO3DKey = NULL;
    }
    
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

void Renderer3D::Initialize3DShaders() {
    // Create separate 3D shader sources for WebGL vs Desktop
#ifdef TARGET_EMSCRIPTEN
    // WebGL 2.0 (OpenGL ES 3.0) 3D shaders
    const char* vertex3DShaderSource = R"(#version 300 es
precision highp float;

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec4 aColor;

uniform mat4 uProjection;
uniform mat4 uModelView;

out vec4 vertexColor;
out float fogDistance;

void main() {
    vec4 viewPos = uModelView * vec4(aPos, 1.0);
    gl_Position = uProjection * viewPos;
    vertexColor = aColor;
    
    // Calculate distance from camera for fog (negative Z in view space)
    fogDistance = length(viewPos.xyz);
}
)";

    const char* fragment3DShaderSource = R"(#version 300 es
precision mediump float;

in vec4 vertexColor;
in float fogDistance;

uniform bool uFogEnabled;
uniform float uFogStart;
uniform float uFogEnd;
uniform vec4 uFogColor;

out vec4 FragColor;

void main() {
    vec4 finalColor = vertexColor;
    
    if (uFogEnabled) {
        // Linear fog calculation matching OpenGL fixed-function fog
        float fogFactor = clamp((uFogEnd - fogDistance) / (uFogEnd - uFogStart), 0.0, 1.0);
        finalColor = mix(uFogColor, vertexColor, fogFactor);
    }
    
    FragColor = finalColor;
}
)";
#else
    // Desktop OpenGL 3.3 Core 3D shaders
    const char* vertex3DShaderSource = R"(#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec4 aColor;

uniform mat4 uProjection;
uniform mat4 uModelView;

out vec4 vertexColor;
out float fogDistance;

void main() {
    vec4 viewPos = uModelView * vec4(aPos, 1.0);
    gl_Position = uProjection * viewPos;
    vertexColor = aColor;
    
    // Calculate distance from camera for fog (negative Z in view space)
    fogDistance = length(viewPos.xyz);
}
)";

    const char* fragment3DShaderSource = R"(#version 330 core

in vec4 vertexColor;
in float fogDistance;

uniform bool uFogEnabled;
uniform float uFogStart;
uniform float uFogEnd;
uniform vec4 uFogColor;

out vec4 FragColor;

void main() {
    vec4 finalColor = vertexColor;
    
    if (uFogEnabled) {
        // Linear fog calculation matching OpenGL fixed-function fog
        float fogFactor = clamp((uFogEnd - fogDistance) / (uFogEnd - uFogStart), 0.0, 1.0);
        finalColor = mix(uFogColor, vertexColor, fogFactor);
    }
    
    FragColor = finalColor;
}
)";
#endif
    
    // Create 3D shader program
    m_shader3DProgram = m_renderer->CreateShader(vertex3DShaderSource, fragment3DShaderSource);
    
    if (m_shader3DProgram == 0) {
        AppDebugOut("Renderer3D: Failed to create 3D shader program\n");
    }
}

void Renderer3D::Setup3DVertexArrays() {
    // Generate VAO and VBO for 3D rendering
    glGenVertexArrays(1, &m_VAO3D);
    glGenBuffers(1, &m_VBO3D);
    
    glBindVertexArray(m_VAO3D);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO3D);
    
    // Allocate buffer for maximum 3D vertices
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex3D) * MAX_3D_VERTICES, NULL, GL_DYNAMIC_DRAW);
    
    // Position attribute (location 0) - now 3 components
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Color attribute (location 1)
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    
    AppDebugOut("Renderer3D: 3D vertex arrays setup complete\n");
}

void Renderer3D::SetPerspective(float fovy, float aspect, float nearZ, float farZ) {
    m_projectionMatrix3D.Perspective(fovy, aspect, nearZ, farZ);
}

void Renderer3D::SetLookAt(float eyeX, float eyeY, float eyeZ,
                           float centerX, float centerY, float centerZ,
                           float upX, float upY, float upZ) {
    m_modelViewMatrix3D.LookAt(eyeX, eyeY, eyeZ, centerX, centerY, centerZ, upX, upY, upZ);
}

void Renderer3D::BeginLineStrip3D(Colour const &col) {
    m_lineStrip3DActive = true;
    m_lineStrip3DColor = col;
    m_vertex3DCount = 0;
}

void Renderer3D::LineStripVertex3D(float x, float y, float z) {
    if (!m_lineStrip3DActive) return;
    
    // Check buffer overflow
    if (m_vertex3DCount >= MAX_3D_VERTICES) {
        AppDebugOut("Renderer3D: 3D vertex buffer overflow\n");
        return;
    }
    
    // Convert color to float
    float r = m_lineStrip3DColor.m_r / 255.0f;
    float g = m_lineStrip3DColor.m_g / 255.0f;
    float b = m_lineStrip3DColor.m_b / 255.0f;
    float a = m_lineStrip3DColor.m_a / 255.0f;
    
    // Add vertex to buffer
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
    
    // Convert line strip to line segments for GL_LINES
    int lineVertexCount = (m_vertex3DCount - 1) * 2;
    Vertex3D* lineVertices = new Vertex3D[lineVertexCount];
    
    int lineIndex = 0;
    for (int i = 0; i < m_vertex3DCount - 1; i++) {
        // Line from vertex i to vertex i+1
        lineVertices[lineIndex++] = m_vertices3D[i];
        lineVertices[lineIndex++] = m_vertices3D[i + 1];
    }
    
    // Clear current buffer and add line segments
    m_vertex3DCount = 0;
    for (int i = 0; i < lineVertexCount; i++) {
        if (m_vertex3DCount >= MAX_3D_VERTICES) break;
        m_vertices3D[m_vertex3DCount++] = lineVertices[i];
    }
    
    delete[] lineVertices;
    
    // Render the lines
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
    
    // For line loop, add a final line from last vertex back to first
    Vertex3D firstVertex = m_vertices3D[0];
    if (m_vertex3DCount < MAX_3D_VERTICES) {
        m_vertices3D[m_vertex3DCount++] = firstVertex;
    }
    
    // Now render as line strip
    EndLineStrip3D();
}

void Renderer3D::BeginCachedGeometry3D(const char* cacheKey, Colour const &col) {
    // Check if this VBO already exists and is valid
    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(cacheKey);
    if (tree && tree->data && tree->data->isValid) {
        // VBO already exists, no need to rebuild
        return;
    }
    
    // Start building cached geometry
    BeginLineStrip3D(col);
}

void Renderer3D::CachedLineStrip3D(const Vector3<float>* vertices, int vertexCount) {
    for (int i = 0; i < vertexCount; i++) {
        LineStripVertex3D(vertices[i]);
    }
}

void Renderer3D::EndCachedGeometry3D() {
    // Implementation will be completed when we convert the actual globe rendering
    // For now, this is a placeholder
    EndLineStrip3D();
}

void Renderer3D::RenderCachedGeometry3D(const char* cacheKey) {
    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(cacheKey);
    if (!tree || !tree->data || !tree->data->isValid) {
        return; // VBO doesn't exist or is invalid
    }
    
    Cached3DVBO* cachedVBO = tree->data;
    
    // Use 3D shader
    glUseProgram(m_shader3DProgram);
    
    // Set uniforms
    int projLoc = glGetUniformLocation(m_shader3DProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_shader3DProgram, "uModelView");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix3D.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix3D.m);
    
    // Render the cached VBO
    glBindVertexArray(cachedVBO->VAO);
    glDrawArrays(GL_LINES, 0, cachedVBO->vertexCount);
    glBindVertexArray(0);
    glUseProgram(0);
}

void Renderer3D::InvalidateCached3DVBO(const char* cacheKey) {
    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(cacheKey);
    if (tree && tree->data) {
        Cached3DVBO* cachedVBO = tree->data;
        cachedVBO->isValid = false;
        if (cachedVBO->VBO != 0) {
            glDeleteBuffers(1, &cachedVBO->VBO);
            cachedVBO->VBO = 0;
        }
        if (cachedVBO->VAO != 0) {
            glDeleteVertexArrays(1, &cachedVBO->VAO);
            cachedVBO->VAO = 0;
        }
    }
}

bool Renderer3D::IsCachedGeometry3DValid(const char* cacheKey) {
    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(cacheKey);
    return (tree && tree->data && tree->data->isValid);
}

void Renderer3D::SetColor(const Colour& col) {
    m_lineStrip3DColor = col;
}

void Renderer3D::Clear3DState() {
    m_vertex3DCount = 0;
    m_lineStrip3DActive = false;
}

void Renderer3D::Flush3DVertices(unsigned int primitiveType) {
    if (m_vertex3DCount == 0) return;
    
    // Use 3D shader program
    glUseProgram(m_shader3DProgram);
    
    // Set matrix uniforms
    int projLoc = glGetUniformLocation(m_shader3DProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_shader3DProgram, "uModelView");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix3D.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix3D.m);
    
    // Set fog uniforms
    int fogEnabledLoc = glGetUniformLocation(m_shader3DProgram, "uFogEnabled");
    int fogStartLoc = glGetUniformLocation(m_shader3DProgram, "uFogStart");
    int fogEndLoc = glGetUniformLocation(m_shader3DProgram, "uFogEnd");
    int fogColorLoc = glGetUniformLocation(m_shader3DProgram, "uFogColor");
    
    glUniform1i(fogEnabledLoc, m_fogEnabled ? 1 : 0);
    glUniform1f(fogStartLoc, m_fogStart);
    glUniform1f(fogEndLoc, m_fogEnd);
    glUniform4f(fogColorLoc, m_fogColor[0], m_fogColor[1], m_fogColor[2], m_fogColor[3]);
    
    // Upload vertex data
    glBindVertexArray(m_VAO3D);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO3D);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_vertex3DCount * sizeof(Vertex3D), m_vertices3D);
    
    // Draw
    glDrawArrays(primitiveType, 0, m_vertex3DCount);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    // Reset vertex count
    m_vertex3DCount = 0;
}

// ================================
// Mega-VBO System for 3D (Performance)
// ================================

void Renderer3D::BeginMegaVBO3D(const char* megaVBOKey, Colour const &col) {
    // Check if this mega-VBO already exists and is valid
    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(megaVBOKey);
    if (tree && tree->data && tree->data->isValid) {
        // Mega-VBO already exists, no need to rebuild
        return;
    }
    
    // Store mega-VBO key for building
    if (m_currentMegaVBO3DKey) {
        delete[] m_currentMegaVBO3DKey;
    }
    m_currentMegaVBO3DKey = newStr(megaVBOKey);
    
    // Store mega-VBO state
    m_megaVBO3DActive = true;
    m_megaVBO3DColor = col;
    
    // Clear mega vertex buffer
    m_megaVertex3DCount = 0;
}

void Renderer3D::AddLineStripToMegaVBO3D(const Vector3<float>* vertices, int vertexCount) {
    if (!m_megaVBO3DActive || vertexCount < 2) return;
    
    // Convert color to float
    float r = m_megaVBO3DColor.m_r / 255.0f, g = m_megaVBO3DColor.m_g / 255.0f;
    float b = m_megaVBO3DColor.m_b / 255.0f, a = m_megaVBO3DColor.m_a / 255.0f;
    
    // Convert line strip to line segments and add to mega buffer
    for (int i = 0; i < vertexCount - 1; i++) {
        if (m_megaVertex3DCount + 2 >= MAX_MEGA_3D_VERTICES) {
            return;
        }
        
        // Line from vertex i to vertex i+1
        // First vertex of line segment
        m_megaVertices3D[m_megaVertex3DCount] = Vertex3D(
            vertices[i].x, vertices[i].y, vertices[i].z, r, g, b, a
        );
        m_megaVertex3DCount++;
        
        // Second vertex of line segment
        m_megaVertices3D[m_megaVertex3DCount] = Vertex3D(
            vertices[i + 1].x, vertices[i + 1].y, vertices[i + 1].z, r, g, b, a
        );
        m_megaVertex3DCount++;
    }
}

void Renderer3D::EndMegaVBO3D() {
    if (!m_megaVBO3DActive || !m_currentMegaVBO3DKey) return;
    
    if (m_megaVertex3DCount < 2) {
        // Not enough vertices for lines
        m_megaVBO3DActive = false;
        return;
    }
    
    // Create or get cached mega-VBO
    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(m_currentMegaVBO3DKey);
    Cached3DVBO* cachedVBO = NULL;
    if (!tree || !tree->data) {
        cachedVBO = new Cached3DVBO();
        cachedVBO->VBO = 0;
        cachedVBO->VAO = 0;
        cachedVBO->vertexCount = 0;
        cachedVBO->isValid = false;
        m_cached3DVBOs.PutData(m_currentMegaVBO3DKey, cachedVBO);
    } else {
        cachedVBO = tree->data;
    }
    
    // Create VBO if it doesn't exist
    if (cachedVBO->VBO == 0) {
        glGenVertexArrays(1, &cachedVBO->VAO);
        glGenBuffers(1, &cachedVBO->VBO);
    }
    
    // Upload mega data to VBO
    glBindVertexArray(cachedVBO->VAO);
    glBindBuffer(GL_ARRAY_BUFFER, cachedVBO->VBO);
    glBufferData(GL_ARRAY_BUFFER, m_megaVertex3DCount * sizeof(Vertex3D), m_megaVertices3D, GL_STATIC_DRAW);
    
    // Set up vertex attributes (3D version)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    
    // Store mega-VBO info
    cachedVBO->vertexCount = m_megaVertex3DCount;
    cachedVBO->color = m_megaVBO3DColor;
    cachedVBO->isValid = true;
    
    // Reset state
    m_megaVertex3DCount = 0;
    m_megaVBO3DActive = false;
}

void Renderer3D::RenderMegaVBO3D(const char* megaVBOKey) {
    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(megaVBOKey);
    if (!tree || !tree->data || !tree->data->isValid) {
        return; // Mega-VBO doesn't exist or is invalid
    }
    
    Cached3DVBO* cachedVBO = tree->data;
    
    // Use 3D shader
    glUseProgram(m_shader3DProgram);
    
    // Set uniforms
    int projLoc = glGetUniformLocation(m_shader3DProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_shader3DProgram, "uModelView");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m_projectionMatrix3D.m);
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, m_modelViewMatrix3D.m);
    
    // Set fog uniforms
    int fogEnabledLoc = glGetUniformLocation(m_shader3DProgram, "uFogEnabled");
    int fogStartLoc = glGetUniformLocation(m_shader3DProgram, "uFogStart");
    int fogEndLoc = glGetUniformLocation(m_shader3DProgram, "uFogEnd");
    int fogColorLoc = glGetUniformLocation(m_shader3DProgram, "uFogColor");
    
    glUniform1i(fogEnabledLoc, m_fogEnabled ? 1 : 0);
    glUniform1f(fogStartLoc, m_fogStart);
    glUniform1f(fogEndLoc, m_fogEnd);
    glUniform4f(fogColorLoc, m_fogColor[0], m_fogColor[1], m_fogColor[2], m_fogColor[3]);
    
    // Render the mega-VBO with single draw call
    glBindVertexArray(cachedVBO->VAO);
    glDrawArrays(GL_LINES, 0, cachedVBO->vertexCount);
    glBindVertexArray(0);
    glUseProgram(0);
}

bool Renderer3D::IsMegaVBO3DValid(const char* megaVBOKey) {
    BTree<Cached3DVBO*>* tree = m_cached3DVBOs.LookupTree(megaVBOKey);
    return (tree && tree->data && tree->data->isValid);
}

void Renderer3D::EnableFog(float start, float end, float density, float r, float g, float b, float a) {
    m_fogEnabled = true;
    m_fogStart = start;
    m_fogEnd = end;
    m_fogDensity = density;
    m_fogColor[0] = r;
    m_fogColor[1] = g;
    m_fogColor[2] = b;
    m_fogColor[3] = a;
}

void Renderer3D::DisableFog() {
    m_fogEnabled = false;
} 