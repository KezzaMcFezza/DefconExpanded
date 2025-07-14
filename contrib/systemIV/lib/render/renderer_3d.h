/*
 * ===========
 * RENDERER 3D
 * ===========
 *
 * Modern OpenGL 3.3 Core Profile 3D rendering system
 * Specifically designed for DEFCON's 3D globe rendering
 */

#ifndef _included_renderer3d_h
#define _included_renderer3d_h

#include "lib/tosser/btree.h"
#include "lib/math/vector3.h"
#include "colour.h"

// Forward declarations
class Renderer;

// 3D vertex structure for modern OpenGL
struct Vertex3D {
    float x, y, z;      // 3D position
    float r, g, b, a;   // Color
    
    Vertex3D() : x(0), y(0), z(0), r(1), g(1), b(1), a(1) {}
    Vertex3D(float px, float py, float pz, float pr, float pg, float pb, float pa) 
        : x(px), y(py), z(pz), r(pr), g(pg), b(pb), a(pa) {}
};

// 3D textured vertex structure for sprites/quads on globe surface
struct Vertex3DTextured {
    float x, y, z;      // 3D position
    float r, g, b, a;   // Color
    float u, v;         // Texture coordinates
    
    Vertex3DTextured() : x(0), y(0), z(0), r(1), g(1), b(1), a(1), u(0), v(0) {}
    Vertex3DTextured(float px, float py, float pz, float pr, float pg, float pb, float pa, float pu, float pv) 
        : x(px), y(py), z(pz), r(pr), g(pg), b(pb), a(pa), u(pu), v(pv) {}
};

// Extended Matrix4f with 3D operations
class Matrix4f3D {
public:
    float m[16];
    
    Matrix4f3D();
    void LoadIdentity();
    void Perspective(float fovy, float aspect, float nearZ, float farZ);
    void LookAt(float eyeX, float eyeY, float eyeZ,
                float centerX, float centerY, float centerZ,
                float upX, float upY, float upZ);
    void Multiply(const Matrix4f3D& other);
    void Copy(float* dest) const;
    
private:
    void Normalize(float& x, float& y, float& z);
    void Cross(float ax, float ay, float az, float bx, float by, float bz, float& cx, float& cy, float& cz);
};

class Renderer3D
{
private:
    // Reference to main renderer for shader access
    Renderer* m_renderer;
    
    // 3D-specific OpenGL objects
    unsigned int m_shader3DProgram;
    unsigned int m_shader3DTexturedProgram;  // Textured shader for quads
    unsigned int m_VAO3D;
    unsigned int m_VBO3D;
    unsigned int m_VAO3DTextured;            // VAO for textured quads
    unsigned int m_VBO3DTextured;            // VBO for textured quads
    
    // 3D matrices
    Matrix4f3D m_projectionMatrix3D;
    Matrix4f3D m_modelViewMatrix3D;
    
    // Dynamic vertex buffer for 3D rendering
    static const int MAX_3D_VERTICES = 50000;
    Vertex3D m_vertices3D[MAX_3D_VERTICES];
    int m_vertex3DCount;
    
    // Dynamic vertex buffer for 3D textured rendering
    static const int MAX_3D_TEXTURED_VERTICES = 10000;
    Vertex3DTextured m_vertices3DTextured[MAX_3D_TEXTURED_VERTICES];
    int m_vertex3DTexturedCount;
    
    // 3D Line strip rendering state
    bool m_lineStrip3DActive;
    Colour m_lineStrip3DColor;
    
    // 3D Textured quad rendering state
    bool m_texturedQuad3DActive;
    Colour m_texturedQuad3DColor;
    unsigned int m_currentTexture3D;
    
    // Mega-VBO state for 3D rendering (like 2D system)
    bool m_megaVBO3DActive;
    char* m_currentMegaVBO3DKey;
    Colour m_megaVBO3DColor;
    static const int MAX_MEGA_3D_VERTICES = 200000;  // Large buffer for 3D globe
    Vertex3D* m_megaVertices3D;
    int m_megaVertex3DCount;
    
    // VBO caching system for 3D geometry (replaces display lists)
    struct Cached3DVBO {
        unsigned int VBO;
        unsigned int VAO;
        int vertexCount;
        Colour color;
        bool isValid;
    };
    
    BTree<Cached3DVBO*> m_cached3DVBOs;
    
    // Fog parameters
    bool m_fogEnabled;
    bool m_fogOrientationBased;  // true = orientation-based, false = distance-based
    float m_fogStart;           // Distance-based fog start
    float m_fogEnd;             // Distance-based fog end
    float m_fogDensity;         // Distance-based fog density
    float m_fogColor[4];        // R, G, B, A
    float m_cameraPos[3];       // X, Y, Z for orientation-based fog
    
    // Helper methods
    void Initialize3DShaders();
    void Setup3DVertexArrays();
    void Setup3DTexturedVertexArrays();
    void Flush3DVertices(unsigned int primitiveType);
    void Flush3DTexturedVertices();

public:
    Renderer3D(Renderer* renderer);
    ~Renderer3D();
    
    void Shutdown();
    
    // 3D viewport and camera setup
    void SetPerspective(float fovy, float aspect, float nearZ, float farZ);
    void SetLookAt(float eyeX, float eyeY, float eyeZ,
                   float centerX, float centerY, float centerZ,
                   float upX, float upY, float upZ);
    
    // 3D immediate mode rendering replacements
    void BeginLineStrip3D(Colour const &col);
    void LineStripVertex3D(float x, float y, float z);
    void LineStripVertex3D(const Vector3<float>& vertex);
    void EndLineStrip3D();
    
    void BeginLineLoop3D(Colour const &col);
    void LineLoopVertex3D(float x, float y, float z);
    void LineLoopVertex3D(const Vector3<float>& vertex);
    void EndLineLoop3D();
    
    // 3D textured quad rendering (for city sprites on globe surface)
    void BeginTexturedQuad3D(unsigned int textureID, Colour const &col);
    void TexturedQuadVertex3D(float x, float y, float z, float u, float v);
    void TexturedQuadVertex3D(const Vector3<float>& vertex, float u, float v);
    void EndTexturedQuad3D();
    
    // VBO caching system for 3D geometry (replaces display lists)
    void BeginCachedGeometry3D(const char* cacheKey, Colour const &col);
    void CachedLineStrip3D(const Vector3<float>* vertices, int vertexCount);
    void EndCachedGeometry3D();
    void RenderCachedGeometry3D(const char* cacheKey);
    void InvalidateCached3DVBO(const char* cacheKey);
    
    // Check if cached geometry exists and is valid
    bool IsCachedGeometry3DValid(const char* cacheKey);
    
    // Mega-VBO system for maximum performance (like 2D map renderer)
    void BeginMegaVBO3D(const char* megaVBOKey, Colour const &col);
    void AddLineStripToMegaVBO3D(const Vector3<float>* vertices, int vertexCount);
    void EndMegaVBO3D();
    void RenderMegaVBO3D(const char* megaVBOKey);
    bool IsMegaVBO3DValid(const char* megaVBOKey);
    
    // Utility functions
    void SetColor(const Colour& col);
    void Clear3DState();
    
    // Fog control
    void EnableDistanceFog(float start, float end, float density, float r, float g, float b, float a);
    void EnableOrientationFog(float r, float g, float b, float a, float camX, float camY, float camZ);
    void DisableFog();
};

// Global 3D renderer instance
extern Renderer3D *g_renderer3d;

#endif 