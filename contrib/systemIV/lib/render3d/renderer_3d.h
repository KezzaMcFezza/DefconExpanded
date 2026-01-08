/*
 * ========
 * RENDERER 3D
 * ========
 */

#ifndef _included_renderer3d_h
#define _included_renderer3d_h

//
// Set to 1 to disable batching and flush after every 3D draw call
// Set to 0 to enable batching

#define IMMEDIATE_MODE_3D 0

#include "lib/render/renderer.h"
#include "lib/tosser/btree.h"
#include "lib/tosser/llist.h"
#include "lib/math/vector3.h"
#include "lib/math/vector2.h"
#include "lib/math/matrix4f.h"
#include "lib/render/colour.h"

class Renderer;
class Image;
class BitmapFont;

//
// Billboard modes for different sprite types

enum BillboardMode3D {
    BILLBOARD_SURFACE_ALIGNED,  // Lay flat on surface
    BILLBOARD_CAMERA_FACING,    // Face camera
    BILLBOARD_NONE              // Use exact coordinates
};

struct Vertex3D {
    float x, y, z;      // 3D position
    float r, g, b, a;   // Color
    
    constexpr Vertex3D     () : x(0), y(0), z(0), r(1), g(1), b(1), a(1) {}
    constexpr Vertex3D     (float px, float py, float pz, float pr, float pg, float pb, float pa) 
                            : x(px), y(py), z(pz), r(pr), g(pg), b(pb), a(pa) {}
    constexpr Vertex3D     (float px, float py, float pz, const Colour& color)
                            : x(px), y(py), z(pz), r(color.GetRFloat()), g(color.GetGFloat()), 
                             b(color.GetBFloat()), a(color.GetAFloat()) {}
};

struct Vertex3DTextured {
    float x, y, z;      // 3D position
    float nx, ny, nz;   // Normal (NEW!)
    float r, g, b, a;   // Color
    float u, v;         // Texture coordinates
    
    constexpr Vertex3DTextured() 
        : x(0), y(0), z(0), 
          nx(0), ny(1), nz(0),  // Default up normal
          r(1), g(1), b(1), a(1), 
          u(0), v(0) {}
    
    constexpr Vertex3DTextured(float px, float py, float pz, 
                               float pnx, float pny, float pnz,
                               float pr, float pg, float pb, float pa, 
                               float pu, float pv) 
        : x(px), y(py), z(pz), 
          nx(pnx), ny(pny), nz(pnz),
          r(pr), g(pg), b(pb), a(pa), 
          u(pu), v(pv) {}
    
    constexpr Vertex3DTextured(float px, float py, float pz, 
                               float pnx, float pny, float pnz,
                               const Colour& color, 
                               float pu, float pv)
        : x(px), y(py), z(pz), 
          nx(pnx), ny(pny), nz(pnz),
          r(color.GetRFloat()), g(color.GetGFloat()), 
          b(color.GetBFloat()), a(color.GetAFloat()), 
          u(pu), v(pv) {}
};

class MegaVBO3D;

class Renderer3D
{
    friend class Renderer;
    friend class MegaVBO3D;
    
protected:

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
    
    struct Shader3DModelUniforms {
        int projectionLoc;
        int modelViewLoc;
        int modelMatrixLoc;
        int modelColorLoc;
        int modelMatricesLoc;
        int modelColorsLoc;
        int instanceCountLoc;
        int useInstancingLoc;
        int fogEnabledLoc;
        int fogOrientationLoc;
        int fogStartLoc;
        int fogEndLoc;
        int fogColorLoc;
        int cameraPosLoc;
    };
    
    Shader3DUniforms m_shader3DUniforms;
    Shader3DUniforms m_shader3DTexturedUniforms;
    Shader3DModelUniforms m_shader3DModelUniforms;
    
    unsigned int m_currentShaderProgram3D;
    bool m_matrices3DNeedUpdate;
    bool m_fog3DNeedsUpdate;

    unsigned int m_shader3DProgram;
    unsigned int m_shader3DTexturedProgram;                 // Textured shader for quads
    unsigned int m_shader3DModelProgram;                    // Model shader with per-instance transforms
    unsigned int m_VAO3D;
    unsigned int m_VBO3D;
    unsigned int m_VAO3DTextured;                           // VAO for textured quads
    unsigned int m_VBO3DTextured;                           // VBO for textured quads
    unsigned int m_spriteVAO3D, m_spriteVBO3D;              // Unit sprites and highlights
    unsigned int m_lineVAO3D, m_lineVBO3D;                  // Effects lines and trails (non-textured)
    unsigned int m_textVAO3D, m_textVBO3D;                  // Text rendering
    unsigned int m_circleVAO3D, m_circleVBO3D;              // Circle outlines
    unsigned int m_circleFillVAO3D, m_circleFillVBO3D;      // Circle fills
    unsigned int m_rectVAO3D, m_rectVBO3D;                  // Rect outlines
    unsigned int m_rectFillVAO3D, m_rectFillVBO3D;          // Rect fills
    unsigned int m_triangleFillVAO3D, m_triangleFillVBO3D;  // Triangle fills
    unsigned int m_immediateVAO3D, m_immediateVBO3D;        // Immediate rendering
    
    Matrix4f m_projectionMatrix3D;
    Matrix4f m_modelViewMatrix3D;

    static constexpr int MAX_3D_VERTICES                   = 4200;
    static constexpr int MAX_3D_TEXTURED_VERTICES          = 3000;
    static constexpr int MAX_LINE_VERTICES_3D              = 30000; 
    static constexpr int MAX_STATIC_SPRITE_VERTICES_3D     = 30000;
    static constexpr int MAX_ROTATING_SPRITE_VERTICES_3D   = 10000;
    static constexpr int MAX_TEXT_VERTICES_3D              = 20000;
    static constexpr int MAX_CIRCLE_VERTICES_3D            = 5000;
    static constexpr int MAX_CIRCLE_FILL_VERTICES_3D       = 5000;
    static constexpr int MAX_RECT_VERTICES_3D              = 3000;
    static constexpr int MAX_RECT_FILL_VERTICES_3D         = 3000;
    static constexpr int MAX_TRIANGLE_FILL_VERTICES_3D     = 10000;
    static constexpr int MAX_INSTANCES                     = 64;
    
    bool m_fogEnabled;
    bool m_fogOrientationBased;  // true = orientation-based, false = distance-based
    float m_fogStart;            // Distance-based fog start
    float m_fogEnd;              // Distance-based fog end
    float m_fogDensity;          // Distance-based fog density
    float m_fogColor[4];         // R, G, B, A
    float m_cameraPos[3];        // X, Y, Z for orientation-based fog
    
    Vertex3DTextured m_staticSpriteVertices3D     [MAX_STATIC_SPRITE_VERTICES_3D];
    int m_staticSpriteVertexCount3D;
    unsigned int m_currentStaticSpriteTexture3D;
    
    Vertex3DTextured m_rotatingSpriteVertices3D   [MAX_ROTATING_SPRITE_VERTICES_3D];
    int m_rotatingSpriteVertexCount3D;
    unsigned int m_currentRotatingSpriteTexture3D;
    
    struct FontBatch3D {
        Vertex3DTextured vertices[MAX_TEXT_VERTICES_3D];
        int vertexCount;
        unsigned int textureID;
    };
    
    FontBatch3D m_fontBatches3D[Renderer::MAX_FONT_ATLASES];
    int m_currentFontBatchIndex3D;
    
    Vertex3DTextured *m_textVertices3D;
    int m_textVertexCount3D;
    unsigned int m_currentTextTexture3D;

    Vertex3D m_vertices3D                         [MAX_3D_VERTICES];
    int m_vertex3DCount;
    
    Vertex3DTextured m_vertices3DTextured         [MAX_3D_TEXTURED_VERTICES];
    int m_vertex3DTexturedCount;

    Vertex3D m_lineVertices3D                     [MAX_LINE_VERTICES_3D];
    int m_lineVertexCount3D;

    Vertex3D m_circleVertices3D                   [MAX_CIRCLE_VERTICES_3D];
    int m_circleVertexCount3D;

    Vertex3D m_circleFillVertices3D               [MAX_CIRCLE_FILL_VERTICES_3D];
    int m_circleFillVertexCount3D;
    
    Vertex3D m_rectVertices3D                     [MAX_RECT_VERTICES_3D];
    int m_rectVertexCount3D;
    
    Vertex3D m_rectFillVertices3D                 [MAX_RECT_FILL_VERTICES_3D];
    int m_rectFillVertexCount3D;
    
    Vertex3D m_triangleFillVertices3D             [MAX_TRIANGLE_FILL_VERTICES_3D];
    int m_triangleFillVertexCount3D;
    
    void IncrementDrawCall3D         (const char* bufferType);
    void ResetFrameCounters3D        ();
    
    float m_currentLineWidth3D;
    float m_currentCircleWidth3D;
    float m_currentRectWidth3D;
    unsigned int m_currentTexture3D;

private:
    
    bool m_lineStrip3DActive;
    Colour m_lineStrip3DColor;
    float m_lineStrip3DWidth;
    
    bool m_texturedQuad3DActive;
    Colour m_texturedQuad3DColor;
    
    Vertex3D* m_lineConversionBuffer3D;
    int m_lineConversionBufferSize3D;
    
    //
    // Draw call tracking
    
    int m_drawCallsPerFrame3D;
    int m_immediateVertexCalls3D;        
    int m_immediateTriangleCalls3D;      
    int m_lineCalls3D;
    int m_staticSpriteCalls3D;
    int m_rotatingSpriteCalls3D;
    int m_textCalls3D;
    int m_megaVBOCalls3D;
    int m_circleCalls3D;
    int m_circleFillCalls3D;
    int m_rectCalls3D;
    int m_rectFillCalls3D;
    int m_triangleFillCalls3D;
    int m_lineVBOCalls3D;
    int m_quadVBOCalls3D;
    int m_triangleVBOCalls3D;
    int m_prevDrawCallsPerFrame3D;
    int m_prevImmediateVertexCalls3D;
    int m_prevImmediateTriangleCalls3D;
    int m_prevImmediateLineCalls3D;
    int m_prevLineCalls3D;
    int m_prevStaticSpriteCalls3D;
    int m_prevRotatingSpriteCalls3D;
    int m_prevTextCalls3D;
    int m_prevMegaVBOCalls3D;
    int m_prevCircleCalls3D;
    int m_prevCircleFillCalls3D;
    int m_prevRectCalls3D;
    int m_prevRectFillCalls3D;
    int m_prevTriangleFillCalls3D;
    int m_prevLineVBOCalls3D;
    int m_prevQuadVBOCalls3D;
    int m_prevTriangleVBOCalls3D;
    int m_activeFontBatches3D;
    int m_prevActiveFontBatches3D;
    
    virtual void Initialize3DShaders          () = 0;
    virtual void Cache3DUniformLocations      () = 0;
    virtual void Setup3DVertexArrays          () = 0;
    virtual void Setup3DTexturedVertexArrays  () = 0;
    virtual void Set3DShaderUniforms          () = 0;
    virtual void SetTextured3DShaderUniforms  () = 0;

    virtual void SetFogUniforms3D                 (unsigned int shaderProgram) = 0;
    virtual void Set3DModelShaderUniforms         (const Matrix4f& modelMatrix, const Colour& modelColor) = 0;
    virtual void Set3DModelShaderUniformsInstanced(const Matrix4f* modelMatrices, const Colour* modelColors, 
                                                   int instanceCount) = 0;
    virtual void UploadVertexDataTo3DVBO          (unsigned int vbo, const Vertex3D* vertices, 
                                                   int vertexCount, unsigned int usageHint) = 0;
    virtual void UploadVertexDataTo3DVBO          (unsigned int vbo, const Vertex3DTextured* vertices, 
                                                   int vertexCount, unsigned int usageHint) = 0;
    
    virtual void Flush3DVertices              (unsigned int primitiveType) = 0;
    virtual void Flush3DTexturedVertices      () = 0;
    virtual void FlushLine3D                  () = 0;
    virtual void FlushStaticSprites3D         () = 0;
    virtual void FlushRotatingSprite3D        () = 0;
    virtual void FlushTextBuffer3D            () = 0;
    virtual void FlushCircles3D               () = 0;
    virtual void FlushCircleFills3D           () = 0;
    virtual void FlushRects3D                 () = 0;
    virtual void FlushRectFills3D             () = 0;
    virtual void FlushTriangleFills3D         () = 0;
    
    virtual void CleanupBuffers3D             () = 0;
    
    virtual unsigned int CreateMegaVBOVertexBuffer3D (size_t size, BufferUsageHint usageHint) = 0;
    virtual unsigned int CreateMegaVBOIndexBuffer3D  (size_t size, BufferUsageHint usageHint) = 0;
    virtual unsigned int CreateMegaVBOVertexArray3D  () = 0;
    
    virtual void DeleteMegaVBOVertexBuffer3D  (unsigned int buffer) = 0;
    virtual void DeleteMegaVBOIndexBuffer3D   (unsigned int buffer) = 0;
    virtual void DeleteMegaVBOVertexArray3D   (unsigned int vao) = 0;
    
    virtual void SetupMegaVBOVertexAttributes3D        (unsigned int vao, unsigned int vbo, unsigned int ibo) = 0;
    virtual void SetupMegaVBOVertexAttributes3DTextured(unsigned int vao, unsigned int vbo, unsigned int ibo) = 0;
    
    virtual void UploadMegaVBOIndexData3D (unsigned int ibo, const unsigned int* indices, 
                                           int indexCount, BufferUsageHint usageHint) = 0;
    virtual void UploadMegaVBOVertexData3D(unsigned int vbo, const Vertex3D* vertices,
                                           int vertexCount, BufferUsageHint usageHint) = 0;

    virtual void UploadMegaVBOVertexData3DTextured(unsigned int vbo, const Vertex3DTextured* vertices,
                                                   int vertexCount, BufferUsageHint usageHint) = 0;
    
    virtual void DrawMegaVBOIndexed3D          (PrimitiveType primitiveType, unsigned int indexCount) = 0;
    virtual void DrawMegaVBOIndexedInstanced3D (PrimitiveType primitiveType, unsigned int indexCount, 
                                                unsigned int instanceCount) = 0;
    
    virtual void SetupMegaVBOInstancedVertexAttributes3DTextured(unsigned int vao, unsigned int vbo, 
                                                                 unsigned int ibo) = 0;
    
    virtual void EnableMegaVBOPrimitiveRestart3D (unsigned int restartIndex) = 0;
    virtual void DisableMegaVBOPrimitiveRestart3D() = 0;

public:
    Renderer3D();
    virtual ~Renderer3D();
    
    void CreateSurfaceAlignedBillboard    (const Vector3<float>& position, float width, float height, 
                                           Vertex3DTextured* vertices, float u1, float v1, float u2, float v2, 
                                           float r, float g, float b, float a, float rotation = 0.0f);
    void CreateCameraFacingBillboard      (const Vector3<float>& position, float width, float height,
                                           Vertex3DTextured* vertices, float u1, float v1, float u2, float v2,
                                           float r, float g, float b, float a, float rotation = 0.0f);
    static void CalculateSphericalTangents(const Vector3<float>& position, Vector3<float>& outEast, 
                                           Vector3<float>& outNorth);
    
    void Shutdown();
    
    void SetPerspective      (float fovy, float aspect, float nearZ, float farZ);
    void SetCameraPosition   (float x, float y, float z);
    void SetLookAt           (float eyeX, float eyeY, float eyeZ,
                              float centerX, float centerY, float centerZ,
                              float upX, float upY, float upZ);
    
    void BeginLineStrip3D    (Colour const &col, float lineWidth = 1.0f);
    void LineStripVertex3D   (float x, float y, float z);
    void LineStripVertex3D   (const Vector3<float>& vertex);
    void EndLineStrip3D      ();

    void BeginLineLoop3D     (Colour const &col, float lineWidth = 1.0f);
    void LineLoopVertex3D    (float x, float y, float z);
    void LineLoopVertex3D    (const Vector3<float>& vertex);
    void EndLineLoop3D       ();
    
    void BeginTexturedQuad3D     (unsigned int textureID, Colour const &col);
    void TexturedQuadVertex3D    (float x, float y, float z, float u, float v);
    void TexturedQuadVertex3D    (const Vector3<float>& vertex, float u, float v);
    void EndTexturedQuad3D       ();

    
    void SetColor      (const Colour& col);
    void Clear3DState  ();
    
    void EnableDistanceFog     (float start, float end, float density, float r, float g, float b, float a);
    void EnableOrientationFog  (float r, float g, float b, float a, float camX, float camY, float camZ);
    void DisableFog            ();
    
    void InvalidateMatrices3D  () { m_matrices3DNeedUpdate = true; }
    void InvalidateFog3D       () { m_fog3DNeedsUpdate = true; }
    
    void BeginFrame3D();
    void EndFrame3D();
    
    int GetTotalDrawCalls           () const { return m_prevDrawCallsPerFrame3D; }
    int GetImmediateTriangleCalls   () const { return m_prevImmediateTriangleCalls3D; }
    int GetImmediateLineCalls       () const { return m_prevImmediateLineCalls3D; }         
    int GetTextCalls                () const { return m_prevTextCalls3D; }
    int GetLineCalls                () const { return m_prevLineCalls3D; }
    int GetStaticSpriteCalls        () const { return m_prevStaticSpriteCalls3D; }
    int GetRotatingSpriteCalls      () const { return m_prevRotatingSpriteCalls3D; }
    int GetCircleCalls              () const { return m_prevCircleCalls3D; }
    int GetCircleFillCalls          () const { return m_prevCircleFillCalls3D; }
    int GetRectCalls                () const { return m_prevRectCalls3D; }
    int GetRectFillCalls            () const { return m_prevRectFillCalls3D; }
    int GetTriangleFillCalls        () const { return m_prevTriangleFillCalls3D; }
    int GetLineVBOCalls             () const { return m_prevLineVBOCalls3D; }
    int GetQuadVBOCalls             () const { return m_prevQuadVBOCalls3D; }
    int GetTriangleVBOCalls         () const { return m_prevTriangleVBOCalls3D; }
    int GetActiveFontBatches        () const { return m_prevActiveFontBatches3D; }
    
    int GetTotalUnitCalls() const { 
        return m_prevLineCalls3D + m_prevStaticSpriteCalls3D + 
        m_prevRotatingSpriteCalls3D; 
    }
    int GetTotalSpecializedCalls() const {
        return GetTotalUnitCalls() + m_prevTextCalls3D + m_prevMegaVBOCalls3D;
    }

    int GetCurrentTextVertexCount             () const { return m_textVertexCount3D; }
    int GetCurrentLineVertexCount             () const { return m_lineVertexCount3D; }
    int GetCurrentStaticSpriteVertexCount     () const { return m_staticSpriteVertexCount3D; }
    int GetCurrentRotatingSpriteVertexCount   () const { return m_rotatingSpriteVertexCount3D; }
    int GetCurrentCircleVertexCount           () const { return m_circleVertexCount3D; }
    int GetCurrentCircleFillVertexCount       () const { return m_circleFillVertexCount3D; }
    int GetCurrentRectVertexCount             () const { return m_rectVertexCount3D; }
    int GetCurrentRectFillVertexCount         () const { return m_rectFillVertexCount3D; }
    int GetCurrentTriangleFillVertexCount     () const { return m_triangleFillVertexCount3D; }
    
    int GetTotalCurrentVertexCount() const {
        return m_textVertexCount3D + m_lineVertexCount3D + m_staticSpriteVertexCount3D + 
               m_rotatingSpriteVertexCount3D + m_circleVertexCount3D + m_circleFillVertexCount3D + 
               m_rectVertexCount3D + m_rectFillVertexCount3D + m_triangleFillVertexCount3D;
    }
    
    void Line3D           (float x1, float y1, float z1, float x2, float y2, float z2, Colour const &col, float lineWidth = 1.0f, bool immediateFlush = false);
    void BeginLineBatch3D ();
    void EndLineBatch3D   ();
    void FlushLine3DIfFull(int segmentsNeeded);
    
    void StaticSprite3D            (Image *src, float x, float y, float z, float w, float h, Colour const &col, bool immediateFlush = false);
    void StaticSprite3D            (Image *src, float x, float y, float z, float w, float h, Colour const &col, BillboardMode3D mode, bool immediateFlush = false);
    void BeginStaticSpriteBatch3D  ();
    void EndStaticSpriteBatch3D    ();
    void FlushStaticSprites3DIfFull(int verticesNeeded);
    
    void RotatingSprite3D           (Image *src, float x, float y, float z, float w, float h, Colour const &col, float angle, bool immediateFlush = false);
    void RotatingSprite3D           (Image *src, float x, float y, float z, float w, float h, Colour const &col, float angle, BillboardMode3D mode, bool immediateFlush = false);
    void BeginRotatingSpriteBatch3D ();
    void EndRotatingSpriteBatch3D   ();
    void FlushRotatingSprite3DIfFull(int verticesNeeded);

    void BlitChar3D                 (unsigned int textureID, const Vector3<float>& position, float width, float height,
                                     float texX, float texY, float texW, float texH, Colour const &col, 
                                     BillboardMode3D mode, bool immediateFlush = false);
    
    void Text3D                      (float x, float y, float z, Colour const &col, float size, const char *text, ...);
    void TextCentre3D                (float x, float y, float z, Colour const &col, float size, const char *text, ...);
    void TextRight3D                 (float x, float y, float z, Colour const &col, float size, const char *text, ...);
    void TextSimple3D                (float x, float y, float z, Colour const &col, float size, const char *text, 
                                     BillboardMode3D mode = BILLBOARD_CAMERA_FACING, bool immediateFlush = false);
    void TextCentreSimple3D          (float x, float y, float z, Colour const &col, float size, const char *text,
                                     BillboardMode3D mode = BILLBOARD_CAMERA_FACING, bool immediateFlush = false);
    void TextRightSimple3D           (float x, float y, float z, Colour const &col, float size, const char *text,
                                     BillboardMode3D mode = BILLBOARD_CAMERA_FACING, bool immediateFlush = false);
    
    float TextWidth3D                (const char *text, float size);
    float TextWidth3D                (const char *text, unsigned int textLen, float size, BitmapFont *bitmapFont);
    
    void BeginTextBatch3D();
    void EndTextBatch3D();
    void FlushTextBuffer3DIfFull     (int charactersNeeded);
    
    void Circle3D                  (float x, float y, float z, float radius, int numPoints, Colour const &col, float lineWidth = 1.0f, bool immediateFlush = false);
    void Circle3D                  (const Vector3<float>& pos, const Vector3<float>& tangent1, const Vector3<float>& tangent2, float radius, int numPoints, Colour const &col, float lineWidth = 1.0f, bool immediateFlush = false);
    void BeginCircleBatch3D        ();
    void EndCircleBatch3D          ();
    void FlushCircles3DIfFull      (int verticesNeeded);
    
    void CircleFill3D              (float x, float y, float z, float radius, int numPoints, Colour const &col, bool immediateFlush = false);
    void CircleFill3D              (const Vector3<float>& pos, const Vector3<float>& tangent1, const Vector3<float>& tangent2, float radius, int numPoints, Colour const &col, bool immediateFlush = false);
    void BeginCircleFillBatch3D    ();
    void EndCircleFillBatch3D      ();
    void FlushCircleFills3DIfFull  (int verticesNeeded);
    
    void Rect3D                    (float x, float y, float z, float w, float h, Colour const &col, float lineWidth = 1.0f, bool immediateFlush = false);
    void Rect3D                    (const Vector3<float>& pos, const Vector3<float>& tangent1, const Vector3<float>& tangent2, float w, float h, Colour const &col, float lineWidth = 1.0f, bool immediateFlush = false);
    void BeginRectBatch3D          ();
    void EndRectBatch3D            ();
    void FlushRects3DIfFull        (int verticesNeeded);
    
    void RectFill3D                (float x, float y, float z, float w, float h, Colour const &col, bool immediateFlush = false);
    void RectFill3D                (const Vector3<float>& pos, const Vector3<float>& tangent1, const Vector3<float>& tangent2, float w, float h, Colour const &col, bool immediateFlush = false);
    void BeginRectFillBatch3D      ();
    void EndRectFillBatch3D        ();
    void FlushRectFills3DIfFull    (int verticesNeeded);
    
    void TriangleFill3D            (float x1, float y1, float z1, float x2, float y2, float z2, float x3, float y3, float z3, Colour const &col, bool immediateFlush = false);
    void TriangleFill3D            (const Vector3<float>& pos, const Vector3<float>& tangent1, const Vector3<float>& tangent2, const Vector2& v1Offset, const Vector2& v2Offset, const Vector2& v3Offset, Colour const &col, bool immediateFlush = false);
    void BeginTriangleFillBatch3D  ();
    void EndTriangleFillBatch3D    ();
    void FlushTriangleFills3DIfFull(int verticesNeeded);
    
    void FlushAllBatches3D();
};

extern Renderer3D *g_renderer3d;

#endif 
