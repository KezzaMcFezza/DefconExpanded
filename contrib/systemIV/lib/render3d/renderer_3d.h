#ifndef _included_renderer3d_h
#define _included_renderer3d_h

#include "lib/tosser/btree.h"
#include "lib/math/vector3.h"
#include "lib/render/colour.h"

// Forward declarations
class Renderer;
class Image;
class BitmapFont;

// Billboard modes for different sprite types
enum BillboardMode3D {
    BILLBOARD_SURFACE_ALIGNED,  // Lay flat on globe surface (units, cities)
    BILLBOARD_CAMERA_FACING,    // Face camera (nukes, air units)  
    BILLBOARD_NONE              // Use exact coordinates (lines, etc.)
};

// 3D vertex structure for modern OpenGL
struct Vertex3D {
    float x, y, z;      // 3D position
    float r, g, b, a;   // Color
    
    constexpr Vertex3D() : x(0), y(0), z(0), r(1), g(1), b(1), a(1) {}
    constexpr Vertex3D(float px, float py, float pz, float pr, float pg, float pb, float pa) 
        : x(px), y(py), z(pz), r(pr), g(pg), b(pb), a(pa) {}
    constexpr Vertex3D(float px, float py, float pz, const Colour& color)
        : x(px), y(py), z(pz), r(color.GetRFloat()), g(color.GetGFloat()), b(color.GetBFloat()), a(color.GetAFloat()) {}
};

// 3D textured vertex structure for sprites/quads on globe surface
struct Vertex3DTextured {
    float x, y, z;      // 3D position
    float r, g, b, a;   // Color
    float u, v;         // Texture coordinates
    
    constexpr Vertex3DTextured() : x(0), y(0), z(0), r(1), g(1), b(1), a(1), u(0), v(0) {}
    constexpr Vertex3DTextured(float px, float py, float pz, float pr, float pg, float pb, float pa, float pu, float pv) 
        : x(px), y(py), z(pz), r(pr), g(pg), b(pb), a(pa), u(pu), v(pv) {}
    constexpr Vertex3DTextured(float px, float py, float pz, const Colour& color, float pu, float pv)
        : x(px), y(py), z(pz), r(color.GetRFloat()), g(color.GetGFloat()), b(color.GetBFloat()), a(color.GetAFloat()), u(pu), v(pv) {}
};

// Extended Matrix4f with 3D operations
class Matrix4f3D {
public:
    float m[16];
    
    constexpr Matrix4f3D() : m{1,0,0,0,  0,1,0,0,  0,0,1,0,  0,0,0,1} {}
    constexpr void LoadIdentity();
    void Perspective(float fovy, float aspect, float nearZ, float farZ);
    void LookAt(float eyeX, float eyeY, float eyeZ,
                float centerX, float centerY, float centerZ,
                float upX, float upY, float upZ);
    constexpr void Multiply(const Matrix4f3D& other);
    void Copy(float* dest) const;
    
private:
    void Normalize(float& x, float& y, float& z);
    constexpr void Cross(float ax, float ay, float az, float bx, float by, float bz, float& cx, float& cy, float& cz);
};

class Renderer3D
{
private:
    // Reference to main renderer for shader access
    Renderer* m_renderer;
    
    // Uniform location caching for 3D shaders
    struct Shader3DUniforms {
        int projectionLoc;
        int modelViewLoc;
        int textureLoc;
        int fogEnabledLoc;
        int fogOrientationLoc;
        int fogStartLoc;
        int fogEndLoc;
        int fogColorLoc;
        int cameraPosLoc;
    };
    
    Shader3DUniforms m_shader3DUniforms;
    Shader3DUniforms m_shader3DTexturedUniforms;
    
    // Uniform state caching to eliminate redundant uploads
    unsigned int m_currentShaderProgram3D;
    bool m_matrices3DNeedUpdate;
    bool m_fog3DNeedsUpdate;
    
    // 3D-specific OpenGL objects
    unsigned int m_shader3DProgram;
    unsigned int m_shader3DTexturedProgram;  // Textured shader for quads
    unsigned int m_VAO3D;
    unsigned int m_VBO3D;
    unsigned int m_VAO3DTextured;            // VAO for textured quads
    unsigned int m_VBO3DTextured;            // VBO for textured quads
    
    // Specialized 3D VAOs and VBOs for different rendering groups
    unsigned int m_unitVAO3D, m_unitVBO3D;           // Unit sprites and highlights
    unsigned int m_effectsVAO3D, m_effectsVBO3D;     // Effects lines and trails (non-textured)
    unsigned int m_globeVAO3D, m_globeVBO3D;         // Globe surface (non-textured)
    unsigned int m_starsVAO3D, m_starsVBO3D;         // Stars and effects sprites (textured)
    unsigned int m_healthVAO3D, m_healthVBO3D;       // Health bars
    unsigned int m_textVAO3D, m_textVBO3D;           // Text rendering
    unsigned int m_nukeVAO3D, m_nukeVBO3D;           // 3D nuke models
    unsigned int m_legacyVAO3D, m_legacyVBO3D;       // Legacy rendering
    
    // 3D matrices
    Matrix4f3D m_projectionMatrix3D;
    Matrix4f3D m_modelViewMatrix3D;
    
    // Dynamic vertex buffer for 3D rendering
    static constexpr int MAX_3D_VERTICES = 4200;
    Vertex3D m_vertices3D[MAX_3D_VERTICES];
    int m_vertex3DCount;
    
    // Dynamic vertex buffer for 3D textured rendering
    static constexpr int MAX_3D_TEXTURED_VERTICES = 3000;
    Vertex3DTextured m_vertices3DTextured[MAX_3D_TEXTURED_VERTICES];
    int m_vertex3DTexturedCount;
    
    // 3D Line strip rendering state
    bool m_lineStrip3DActive;
    Colour m_lineStrip3DColor;
    
    // 3D Textured quad rendering state
    bool m_texturedQuad3DActive;
    Colour m_texturedQuad3DColor;
    unsigned int m_currentTexture3D;
    
    // Mega VBO for the 3D coastlines and borders
    bool m_megaVBO3DActive;
    char* m_currentMegaVBO3DKey;
    Colour m_megaVBO3DColor;
    int m_maxMegaVertices3D;
    int m_maxMegaIndices3D;
    Vertex3D* m_megaVertices3D;
    int m_megaVertex3DCount;
    unsigned int* m_megaIndices3D;
    int m_megaIndex3DCount;
    
    // VBO caching system for 3D geometry (replaces display lists)
    struct Cached3DVBO {
        unsigned int VBO;
        unsigned int VAO;
        unsigned int IBO;           // Index buffer for indexed drawing
        int vertexCount;            // Number of actual vertices (no duplication)
        int indexCount;             // Number of indices
        Colour color;
        bool isValid;
    };
    
    BTree<Cached3DVBO*> m_cached3DVBOs;
    
    Vertex3D* m_lineConversionBuffer3D;
    int m_lineConversionBufferSize3D;
    
    // Fog parameters
    bool m_fogEnabled;
    bool m_fogOrientationBased;  // true = orientation-based, false = distance-based
    float m_fogStart;           // Distance-based fog start
    float m_fogEnd;             // Distance-based fog end
    float m_fogDensity;         // Distance-based fog density
    float m_fogColor[4];        // R, G, B, A
    float m_cameraPos[3];       // X, Y, Z for orientation-based fog

    // ============================================
    // 3D BATCHING SYSTEM - Member Variables
    // ============================================
    
    // Unit trail rendering buffers
    static constexpr int MAX_BATCHED_LINES_VERTICES_3D = 30000; 
    Vertex3D m_lineBatchedVertices3D[MAX_BATCHED_LINES_VERTICES_3D];
    int m_lineBatchedVertexCount3D;
    
    // Static Sprite sprite rendering buffers
    static constexpr int MAX_STATIC_SPRITE_VERTICES_3D = 30000;
    Vertex3DTextured m_staticSpriteVertices3D[MAX_STATIC_SPRITE_VERTICES_3D];
    int m_staticSpriteVertexCount3D;
    unsigned int m_currentStaticSpriteTexture3D;
    
    // Rotating Sprite sprite rendering buffers  
    static constexpr int MAX_ROTATING_SPRITE_VERTICES_3D = 10000;
    Vertex3DTextured m_rotatingSpriteVertices3D[MAX_ROTATING_SPRITE_VERTICES_3D];
    int m_rotatingSpriteVertexCount3D;
    unsigned int m_currentRotatingSpriteTexture3D;
    
    // Health bar rendering buffers
    static constexpr int MAX_HEALTH_BAR_VERTICES_3D = 500;
    Vertex3DTextured m_healthBarVertices3D[MAX_HEALTH_BAR_VERTICES_3D];
    int m_healthBarVertexCount3D;
    
    // Text rendering buffers
    static constexpr int MAX_TEXT_VERTICES_3D = 1000;
    Vertex3DTextured m_textVertices3D[MAX_TEXT_VERTICES_3D];
    int m_textVertexCount3D;
    unsigned int m_currentTextTexture3D;
    
    // 3D Nuke model rendering buffers  
    static constexpr int MAX_NUKE_3D_MODEL_VERTICES_3D = 7500;
    Vertex3D m_nuke3DModelVertices3D[MAX_NUKE_3D_MODEL_VERTICES_3D];
    int m_nuke3DModelVertexCount3D;
    
    // Star field rendering buffers
    static constexpr int MAX_STAR_FIELD_VERTICES_3D = 5000; 
    Vertex3DTextured m_starFieldVertices3D[MAX_STAR_FIELD_VERTICES_3D];
    int m_starFieldVertexCount3D;
    unsigned int m_currentStarFieldTexture3D;
    
    // Globe surface rendering buffers  
    static constexpr int MAX_GLOBE_SURFACE_VERTICES_3D = 2500; 
    Vertex3D m_globeSurfaceVertices3D[MAX_GLOBE_SURFACE_VERTICES_3D];
    int m_globeSurfaceVertexCount3D;
    
    //
    // draw call tracking
    
    int m_drawCallsPerFrame3D;
    int m_legacyVertexCalls3D;        
    int m_legacyTriangleCalls3D;      
    int m_lineBatchedCalls3D;
    int m_staticSpriteCalls3D;
    int m_rotatingSpriteCalls3D;
    int m_healthBarCalls3D;
    int m_textCalls3D;
    int m_megaVBOCalls3D;
    int m_nuke3DModelCalls3D;
    int m_starFieldCalls3D;
    int m_globeSurfaceCalls3D;
    int m_prevDrawCallsPerFrame3D;
    int m_prevLegacyVertexCalls3D;
    int m_prevLegacyTriangleCalls3D;
    int m_prevlineBatchedCalls3D;
    int m_prevStaticSpriteCalls3D;
    int m_prevRotatingSpriteCalls3D;
    int m_prevHealthBarCalls3D;
    int m_prevTextCalls3D;
    int m_prevMegaVBOCalls3D;
    int m_prevNuke3DModelCalls3D;
    int m_prevStarFieldCalls3D;
    int m_prevGlobeSurfaceCalls3D;
    
    static constexpr int MAX_FLUSH_TYPES_3D = 50;
    
    struct FlushTiming3D {
        const char* name;
        double totalTime;
        int callCount;
    };
    
    FlushTiming3D m_flushTimings3D[MAX_FLUSH_TYPES_3D];
    int m_flushTimingCount3D;
    double m_currentFlushStartTime3D;
    
    void IncrementDrawCall3D(const char* bufferType);
    void ResetFrameCounters3D();
    void StartFlushTiming3D(const char* name);
    void EndFlushTiming3D(const char* name);
    void ResetFlushTimings3D();
    const FlushTiming3D* GetFlushTimings3D(int& count) const;
    void Initialize3DShaders();
    void Cache3DUniformLocations();
    void Setup3DVertexArrays();
    void Setup3DTexturedVertexArrays();
    void Flush3DVertices(unsigned int primitiveType);
    void Flush3DTexturedVertices();
    void SetFogUniforms3D(unsigned int shaderProgram);
    void Set3DShaderUniforms();
    void SetTextured3DShaderUniforms();
    void UploadVertexDataTo3DVBO(unsigned int vbo, const Vertex3D* vertices, int vertexCount, unsigned int usageHint);
    void UploadVertexDataTo3DVBO(unsigned int vbo, const Vertex3DTextured* vertices, int vertexCount, unsigned int usageHint);
    
    // Uniform invalidation functions
    void InvalidateMatrices3D() { m_matrices3DNeedUpdate = true; }
    void InvalidateFog3D() { m_fog3DNeedsUpdate = true; }
    
    // Billboard helper functions
    void CreateSurfaceAlignedBillboard(const Vector3<float>& position, float width, float height, 
                                     Vertex3DTextured* vertices, float u1, float v1, float u2, float v2, 
                                     float r, float g, float b, float a, float rotation = 0.0f);
    void CreateCameraFacingBillboard(const Vector3<float>& position, float width, float height,
                                   Vertex3DTextured* vertices, float u1, float v1, float u2, float v2,
                                   float r, float g, float b, float a, float rotation = 0.0f);
    
    // 3D Nuke model helper functions
    void CreateNukeModel3D(const Vector3<float>& position, const Vector3<float>& direction, 
                          float length, float radius, Colour const &col, Vertex3D* vertices, int& vertexCount);

public:
    Renderer3D(Renderer* renderer);
    ~Renderer3D();
    
    void Shutdown();
    
    // 3D viewport and camera setup
    void SetPerspective(float fovy, float aspect, float nearZ, float farZ);
    void SetLookAt(float eyeX, float eyeY, float eyeZ,
                   float centerX, float centerY, float centerZ,
                   float upX, float upY, float upZ);
    
    // Camera position tracking for billboards
    void SetCameraPosition(float x, float y, float z);
    
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
    void SetMegaVBO3DBufferSizes(int vertexCount, int indexCount);
    void InvalidateAll3DVBOs();
    
    // Utility functions
    void SetColor(const Colour& col);
    void Clear3DState();
    
    // Fog control
    void EnableDistanceFog(float start, float end, float density, float r, float g, float b, float a);
    void EnableOrientationFog(float r, float g, float b, float a, float camX, float camY, float camZ);
    void DisableFog();
    
    void BeginFrame3D();
    void EndFrame3D();
    
    int GetTotalDrawCalls() const { return m_prevDrawCallsPerFrame3D; }
    int GetLegacyTriangleCalls() const { return m_prevLegacyTriangleCalls3D; }
    int GetLegacyLineCalls() const { return m_prevLegacyVertexCalls3D; }  
    int GetUITriangleCalls() const { return 0; }    
    int GetUILineCalls() const { return 0; }        
    int GetTextCalls() const { return m_prevTextCalls3D; }
    int GetlineBatchedCalls() const { return m_prevlineBatchedCalls3D; }
    int GetStaticSpriteCalls() const { return m_prevStaticSpriteCalls3D; }
    int GetRotatingSpriteCalls() const { return m_prevRotatingSpriteCalls3D; }
    int GetHealthBarCalls() const { return m_prevHealthBarCalls3D; }
    int GetNuke3DModelCalls() const { return m_prevNuke3DModelCalls3D; }
    int GetStarFieldCalls() const { return m_prevStarFieldCalls3D; }
    int GetGlobeSurfaceCalls() const { return m_prevGlobeSurfaceCalls3D; }
    
    int GetTotalUnitCalls() const { 
        return m_prevlineBatchedCalls3D + m_prevStaticSpriteCalls3D + 
        m_prevRotatingSpriteCalls3D + m_prevHealthBarCalls3D; 
    }
    int GetTotalSpecializedCalls() const {
        return GetTotalUnitCalls() + m_prevTextCalls3D + m_prevMegaVBOCalls3D;
    }

    int GetCurrentTextVertexCount() const { return m_textVertexCount3D; }
    int GetCurrentLineBatchedVertexCount() const { return m_lineBatchedVertexCount3D; }
    int GetCurrentUnitMainVertexCount() const { return m_staticSpriteVertexCount3D; }
    int GetCurrentRotatingSpriteVertexCount() const { return m_rotatingSpriteVertexCount3D; }
    int GetCurrentHealthBarVertexCount() const { return m_healthBarVertexCount3D; }
    int GetCurrentNuke3DModelVertexCount() const { return m_nuke3DModelVertexCount3D; }
    int GetCurrentStarFieldVertexCount() const { return m_starFieldVertexCount3D; }
    int GetCurrentGlobeSurfaceVertexCount() const { return m_globeSurfaceVertexCount3D; }
    
    int GetTotalCurrentVertexCount() const {
        return m_textVertexCount3D + m_lineBatchedVertexCount3D + m_staticSpriteVertexCount3D + 
               m_rotatingSpriteVertexCount3D +
               m_healthBarVertexCount3D + m_nuke3DModelVertexCount3D +
               m_starFieldVertexCount3D + m_globeSurfaceVertexCount3D;
    }

    size_t GetTotalAllocatedBufferMemory() const {
        size_t total = 0;
        
        int nonTexturedVertices = m_lineBatchedVertexCount3D + m_nuke3DModelVertexCount3D + m_globeSurfaceVertexCount3D;
        total += nonTexturedVertices * sizeof(Vertex3D);
        
        int texturedVertices = m_staticSpriteVertexCount3D + m_rotatingSpriteVertexCount3D +  
                              m_healthBarVertexCount3D + m_textVertexCount3D + m_starFieldVertexCount3D;
        total += texturedVertices * sizeof(Vertex3DTextured);
        
        return total;
    }

    void GetImageUVCoords(Image* image, float& u1, float& v1, float& u2, float& v2);
    unsigned int GetEffectiveTextureID(Image* image);
    
    // Unit trail rendering
    void LineBatched3D(float x1, float y1, float z1, float x2, float y2, float z2, Colour const &col);
    void BeginLineBatch3D();
    void EndLineBatch3D();
    void FlushLineBatched3D();
    void FlushLineBatched3DIfFull(int segmentsNeeded);
    
    // Static Sprite sprite rendering
    void StaticSprite3D(Image *src, float x, float y, float z, float w, float h, Colour const &col);
    void StaticSprite3D(Image *src, float x, float y, float z, float w, float h, Colour const &col, BillboardMode3D mode);
    void BeginStaticSpriteBatch3D();
    void EndStaticSpriteBatch3D();
    void FlushStaticSprites3D();
    void FlushStaticSprites3DIfFull(int verticesNeeded);
    
    // Rotating Sprite sprite rendering
    void RotatingSprite3D(Image *src, float x, float y, float z, float w, float h, Colour const &col, float angle);
    void RotatingSprite3D(Image *src, float x, float y, float z, float w, float h, Colour const &col, float angle, BillboardMode3D mode);
    void BeginRotatingSpriteBatch3D();
    void EndRotatingSpriteBatch3D();
    void FlushRotatingSprite3D();
    void FlushRotatingSprite3DIfFull(int verticesNeeded);
    
    // Health bar rendering
    void HealthBarRect3D(float x, float y, float z, float w, float h, Colour const &col);
    void BeginHealthBarBatch3D();
    void EndHealthBarBatch3D();
    void FlushHealthBars3D();
    
    // Text batching convenience methods
    void BeginMapTextBatch3D();
    void EndMapTextBatch3D();
    void BeginFrameTextBatch3D();
    void EndFrameTextBatch3D();
    void BeginTextBatch3D();
    void EndTextBatch3D();
    void FlushTextContext3D();
    void FlushTextBuffer3D();
    
    // 3D Nuke model rendering
    void Nuke3DModel(const Vector3<float>& position, const Vector3<float>& direction, 
                     float length, float radius, Colour const &col);
    void BeginNuke3DModelBatch3D();
    void EndNuke3DModelBatch3D();
    void FlushNuke3DModels3D();
    void FlushNuke3DModels3DIfFull(int verticesNeeded);
    void FlushAllNuke3DModelBuffers3D();
    
    // Star field rendering
    void StarFieldSprite3D(Image *src, float x, float y, float z, float w, float h, Colour const &col);
    void BeginStarFieldBatch3D();
    void EndStarFieldBatch3D();
    void FlushStarField3D();
    void FlushStarField3DIfFull(int verticesNeeded);
    
    // Globe surface rendering
    void GlobeSurfaceTriangle3D(float x1, float y1, float z1, float x2, float y2, float z2, float x3, float y3, float z3, Colour const &col);
    void BeginGlobeSurfaceBatch3D();
    void EndGlobeSurfaceBatch3D();
    void FlushGlobeSurface3D();
    void FlushGlobeSurface3DIfFull(int verticesNeeded);
    
    // Buffer management
    void FlushAllSpecializedBuffers3D();
    void FlushAllUnitBuffers3D();
    void FlushAllEffectBuffers3D();
};

// Global 3D renderer instance
extern Renderer3D *g_renderer3d;

#endif 