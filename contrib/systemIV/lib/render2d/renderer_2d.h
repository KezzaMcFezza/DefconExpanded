#ifndef _included_renderer2d_h
#define _included_renderer2d_h

//
// More for testing, we get 20k draw calls with immediate mode enabled
// Set to 1 to disable batching and flush after every draw call
// Set to 0 to enable batching

#define IMMEDIATE_MODE_2D 0

#include "lib/render/renderer.h"
#include "lib/math/vector3.h"
#include "lib/math/matrix4f.h"
#include "lib/render/colour.h"
#include "lib/tosser/btree.h"
#include "lib/tosser/llist.h"

class Image;
class BitmapFont;

static const Colour White       {255, 255, 255};
static const Colour Black       {0, 0, 0};
static const Colour LightGray   {200, 200, 200};
static const Colour DarkGray    {100, 100, 100};

struct Vertex2D {
  float x, y;
  float r, g, b, a;
  float u, v; // texture coordinates
  
  constexpr Vertex2D     () : x(0), y(0), r(1), g(1), b(1), a(1), u(0), v(0) {}
  constexpr Vertex2D     (float px, float py, float pr, float pg, float pb, float pa, float pu, float pv) 
                         : x(px), y(py), r(pr), g(pg), b(pb), a(pa), u(pu), v(pv) {}
  constexpr Vertex2D     (float px, float py, const Colour& color, float pu, float pv)
                         : x(px), y(py), r(color.GetRFloat()), g(color.GetGFloat()), 
                          b(color.GetBFloat()), a(color.GetAFloat()), u(pu), v(pv) {}
};

class MegaVBO2D;

class Renderer2D {

  friend class Renderer;
  friend class MegaVBO2D;

protected:

  struct ShaderUniforms {
    int projectionLoc;
    int modelViewLoc;
    int textureLoc; 
  };

  ShaderUniforms m_colorShaderUniforms;
  ShaderUniforms m_textureShaderUniforms;

private:

  enum BufferType {
    BUFFER_TEXT,
    BUFFER_SPRITES,
    BUFFER_LINES,
    BUFFER_STATIC_SPRITES,
    BUFFER_ROTATING_SPRITES,
    BUFFER_IMMEDIATE              
  };

protected:

  static constexpr int MAX_VERTICES                 = 5000;
  static constexpr int MAX_TEXT_VERTICES            = 20000;
  static constexpr int MAX_LINE_VERTICES            = 20000;
  static constexpr int MAX_STATIC_SPRITE_VERTICES   = 15000;
  static constexpr int MAX_ROTATING_SPRITE_VERTICES = 20000;
  static constexpr int MAX_CIRCLE_VERTICES          = 50000;
  static constexpr int MAX_CIRCLE_FILL_VERTICES     = 20000;
  static constexpr int MAX_RECT_VERTICES            = 5000;
  static constexpr int MAX_RECT_FILL_VERTICES       = 5000;
  static constexpr int MAX_TRIANGLE_FILL_VERTICES   = 3000;

  unsigned int m_shaderProgram;
  unsigned int m_colorShaderProgram;
  unsigned int m_textureShaderProgram;
  unsigned int m_lineShaderProgram;
  unsigned int m_VAO;
  unsigned int m_VBO; 
  unsigned int m_spriteVAO, m_spriteVBO;
  unsigned int m_rotatingSpriteVAO, m_rotatingSpriteVBO;
  unsigned int m_lineVAO, m_lineVBO;
  unsigned int m_textVAO, m_textVBO;
  unsigned int m_circleVAO, m_circleVBO;
  unsigned int m_circleFillVAO, m_circleFillVBO;
  unsigned int m_rectVAO, m_rectVBO;
  unsigned int m_rectFillVAO, m_rectFillVBO;
  unsigned int m_triangleFillVAO, m_triangleFillVBO;
  unsigned int m_immediateVAO, m_immediateVBO;
  
  bool m_bufferNeedsUpload;

  Matrix4f m_projectionMatrix;
  Matrix4f m_modelViewMatrix;

  int m_projMatrixLocation;
  int m_modelViewMatrixLocation;
  int m_colorLocation;
  int m_textureLocation;

  //
  // Text/Font buffer, font-aware batching for multiple font atlases
  
  struct FontBatch {
    Vertex2D vertices[MAX_TEXT_VERTICES];
    int vertexCount;
    unsigned int textureID;
  };
  
  FontBatch m_fontBatches[Renderer::MAX_FONT_ATLASES];
  int m_currentFontBatchIndex;
  
  Vertex2D *m_textVertices;                  // points to current font batch vertices
  int m_textVertexCount;                     // current font batch vertex count  
  unsigned int m_currentTextTexture;         // current font batch texture ID

  Vertex2D m_staticSpriteVertices            [MAX_STATIC_SPRITE_VERTICES];
  int m_staticSpriteVertexCount;
  unsigned int m_currentStaticSpriteTexture;

  Vertex2D m_rotatingSpriteVertices          [MAX_ROTATING_SPRITE_VERTICES];
  int m_rotatingSpriteVertexCount;
  unsigned int m_currentRotatingSpriteTexture;

  Vertex2D m_triangleVertices                [MAX_VERTICES];
  int m_triangleVertexCount;

  Vertex2D m_lineVertices                    [MAX_LINE_VERTICES];
  int m_lineVertexCount;

  Vertex2D m_circleVertices                  [MAX_CIRCLE_VERTICES];
  int m_circleVertexCount;

  Vertex2D m_circleFillVertices              [MAX_CIRCLE_FILL_VERTICES];
  int m_circleFillVertexCount;

  Vertex2D m_rectVertices                    [MAX_RECT_VERTICES];
  int m_rectVertexCount;

  Vertex2D m_rectFillVertices                [MAX_RECT_FILL_VERTICES];
  int m_rectFillVertexCount;

  Vertex2D m_triangleFillVertices            [MAX_TRIANGLE_FILL_VERTICES];
  int m_triangleFillVertexCount;

  Vertex2D* m_lineConversionBuffer;
  int m_lineConversionBufferSize;

  Colour m_currentLineColor;
  float m_currentLineWidth;

  bool m_lineStripActive;
  Colour m_lineStripColor;
  float m_lineStripWidth;
  
  float m_currentCircleWidth;
  float m_currentRectWidth;

  BufferType m_activeBuffer;

  bool m_batchingTextures;

  virtual void InitializeShaders       () = 0;
  virtual void CacheUniformLocations   () = 0;
  virtual void SetupVertexArrays       () = 0;
  virtual void SetColorShaderUniforms  () = 0;
  virtual void SetLineShaderUniforms   (float lineWidth) = 0;
  virtual void SetTextureShaderUniforms() = 0;
  virtual void UploadVertexData        (const Vertex2D* vertices, int vertexCount) = 0;
  virtual void UploadVertexDataToVBO   (unsigned int vbo, const Vertex2D* vertices, 
                                        int vertexCount, unsigned int usageHint) = 0;
  
  virtual void FlushTriangles          (bool useTexture) = 0;
  virtual void FlushTextBuffer         () = 0;
  virtual void FlushLines              () = 0;
  virtual void FlushStaticSprites      () = 0;
  virtual void FlushRotatingSprite     () = 0;
  virtual void FlushCircles            () = 0;
  virtual void FlushCircleFills        () = 0;
  virtual void FlushRects              () = 0;
  virtual void FlushRectFills          () = 0;
  virtual void FlushTriangleFills      () = 0;
  
  virtual void CleanupBuffers          () = 0;
  
  virtual unsigned int CreateMegaVBOVertexBuffer (size_t size, BufferUsageHint usageHint) = 0;
  virtual unsigned int CreateMegaVBOIndexBuffer  (size_t size, BufferUsageHint usageHint) = 0;
  virtual unsigned int CreateMegaVBOVertexArray  () = 0;
 
  virtual void DeleteMegaVBOVertexBuffer  (unsigned int buffer) = 0;
  virtual void DeleteMegaVBOIndexBuffer   (unsigned int buffer) = 0;
  virtual void DeleteMegaVBOVertexArray   (unsigned int vao) = 0;
  
  virtual void SetupMegaVBOVertexAttributes2D(unsigned int vao, unsigned int vbo, unsigned int ibo) = 0; 
 
  virtual void UploadMegaVBOIndexData (unsigned int ibo, const unsigned int* indices, 
                                        int indexCount, BufferUsageHint usageHint) = 0;
  virtual void UploadMegaVBOVertexData(unsigned int vbo, const Vertex2D* vertices,
                                        int vertexCount, BufferUsageHint usageHint) = 0;
  virtual void DrawMegaVBOIndexed     (PrimitiveType primitiveType, unsigned int indexCount) = 0;
  
  virtual void EnableMegaVBOPrimitiveRestart (unsigned int restartIndex) = 0;
  virtual void DisableMegaVBOPrimitiveRestart() = 0;
  
  void FlushIfTextureChanged         (unsigned int newTextureID, bool useTexture);
  void FlushRotatingSpritesIfFull    (int verticesNeeded);
  void FlushCirclesIfFull            (int verticesNeeded);
  void FlushCircleFillsIfFull        (int verticesNeeded);
  void FlushRectsIfFull              (int verticesNeeded);
  void FlushRectFillsIfFull          (int verticesNeeded);
  void FlushTriangleFillsIfFull      (int verticesNeeded);

public:

  void SetTextureBatching           (bool enabled) { m_batchingTextures = enabled; }
  bool IsTextureBatchingEnabled     () const { return m_batchingTextures; }

  //
  // Track draw calls, used in app::render()
  
  void BeginFrame2D();
  void EndFrame2D();

  int m_drawCallsPerFrame;
  int m_immediateTriangleCalls;
  int m_immediateLineCalls;
  int m_textCalls;
  int m_lineCalls;
  int m_staticSpriteCalls;
  int m_rotatingSpriteCalls;
  int m_circleCalls;
  int m_circleFillCalls;
  int m_rectCalls;
  int m_rectFillCalls;
  int m_triangleFillCalls;
  int m_lineVBOCalls;
  int m_quadVBOCalls;
  int m_triangleVBOCalls;
  int m_prevDrawCallsPerFrame;
  int m_prevImmediateTriangleCalls;
  int m_prevImmediateLineCalls;
  int m_prevTextCalls;
  int m_prevLineCalls;
  int m_prevStaticSpriteCalls;
  int m_prevRotatingSpriteCalls;
  int m_prevCircleCalls;
  int m_prevCircleFillCalls;
  int m_prevRectCalls;
  int m_prevRectFillCalls;
  int m_prevTriangleFillCalls;
  int m_prevLineVBOCalls;
  int m_prevQuadVBOCalls;
  int m_prevTriangleVBOCalls;
  int m_activeFontBatches;
  int m_prevActiveFontBatches;

  void ResetFrameCounters       ();
  void IncrementDrawCall        (const char *bufferType);

  int GetTotalDrawCalls         () const { return m_prevDrawCallsPerFrame; }
  int GetImmediateTriangleCalls () const { return m_prevImmediateTriangleCalls; }
  int GetImmediateLineCalls     () const { return m_prevImmediateLineCalls; }
  int GetTextCalls              () const { return m_prevTextCalls; }
  int GetLineCalls              () const { return m_prevLineCalls; }
  int GetStaticSpriteCalls      () const { return m_prevStaticSpriteCalls; }
  int GetRotatingSpriteCalls    () const { return m_prevRotatingSpriteCalls; }
  int GetCircleCalls            () const { return m_prevCircleCalls; }
  int GetCircleFillCalls        () const { return m_prevCircleFillCalls; }
  int GetRectCalls              () const { return m_prevRectCalls; }
  int GetRectFillCalls          () const { return m_prevRectFillCalls; }
  int GetTriangleFillCalls      () const { return m_prevTriangleFillCalls; }
  int GetLineVBOCalls           () const { return m_prevLineVBOCalls; }
  int GetQuadVBOCalls           () const { return m_prevQuadVBOCalls; }
  int GetTriangleVBOCalls       () const { return m_prevTriangleVBOCalls; }
  int GetActiveFontBatches      () const { return m_prevActiveFontBatches; }


  int GetCurrentTextVertexCount             () const { return m_textVertexCount; }
  int GetCurrentLineVertexCount             () const { return m_lineVertexCount; }
  int GetCurrentStaticSpriteVertexCount     () const { return m_staticSpriteVertexCount; }
  int GetCurrentRotatingSpriteVertexCount   () const { return m_rotatingSpriteVertexCount; }
  int GetCurrentCircleVertexCount           () const { return m_circleVertexCount; }
  int GetCurrentCircleFillVertexCount       () const { return m_circleFillVertexCount; }
  int GetCurrentRectVertexCount             () const { return m_rectVertexCount; }
  int GetCurrentRectFillVertexCount         () const { return m_rectFillVertexCount; }
  int GetCurrentTriangleFillVertexCount     () const { return m_triangleFillVertexCount; }
  int GetCurrentImmediateTriangleVertexCount() const { return m_triangleVertexCount; }
  int GetCurrentImmediateLineVertexCount    () const { return m_lineVertexCount; } 
  int GetLineConversionBufferSize           () const { return m_lineConversionBufferSize; }
  
  int GetTotalCurrentVertexCount() const {
    return m_textVertexCount + m_lineVertexCount + m_staticSpriteVertexCount + 
           m_rotatingSpriteVertexCount + m_circleVertexCount + m_circleFillVertexCount +
           m_rectVertexCount + m_rectFillVertexCount + m_triangleFillVertexCount + 
           m_triangleVertexCount + m_lineVertexCount;
  }

public:
  float m_alpha;
  int m_colourDepth;
  int m_mouseMode;

public:
  Renderer2D();
  virtual ~Renderer2D();

  void Shutdown();

  void Set2DViewport  (float l, float r, float b, float t, int x, int y, int w,int h);
  void Reset2DViewport();


  void BeginTextBatch();
  void Text                     (float x, float y, Colour const &col, float size, 
                                 const char *text, ...);
  void TextCentre               (float x, float y, Colour const &col, float size,
                                 const char *text, ...);
  void TextRight                (float x, float y, Colour const &col, float size,
                                 const char *text, ...);
  void TextSimple               (float x, float y, Colour const &col, float size,
                                 const char *text, bool immediateFlush = false);
  void TextCentreSimple         (float x, float y, Colour const &col, float size,
                                 const char *text, bool immediateFlush = false);
  void TextRightSimple          (float x, float y, Colour const &col, float size,
                                 const char *text, bool immediateFlush = false);

  float TextWidth               (const char *text, float size);
  float TextWidth               (const char *text, unsigned int textLen, float size,
                                 BitmapFont *bitmapFont);
  void EndTextBatch();
  void FlushTextBufferIfFull     (int charactersNeeded);
  
  void BeginRectBatch();
  void Rect             (float x, float y, float w, float h, Colour const &col,
                         float lineWidth = 1.0f, bool immediateFlush = false);
  void EndRectBatch();

  void BeginRectFillBatch();
  void RectFill         (float x, float y, float w, float h, Colour const &col, 
                         bool immediateFlush = false);
  void RectFill         (float x, float y, float w, float h, Colour const &colTL,
                         Colour const &colTR, Colour const &colBR, Colour const &colBL,
                         bool immediateFlush = false);
  void RectFill         (float x, float y, float w, float h, Colour const &col1,
                         Colour const &col2, bool horizontal, bool immediateFlush = false);
  void EndRectFillBatch ();

  void BeginCircleBatch();
  void Circle           (float x, float y, float radius, int numPoints, Colour const &col,
                         float lineWidth = 1.0f, bool immediateFlush = false);
  void EndCircleBatch();

  void BeginCircleFillBatch();
  void CircleFill       (float x, float y, float radius, int numPoints,
                         Colour const &col, bool immediateFlush = false);
  void EndCircleFillBatch();

  void BeginTriangleFillBatch();
  void TriangleFill     (float x1, float y1, float x2, float y2, float x3, float y3,
                         Colour const &col, bool immediateFlush = false);
  void EndTriangleFillBatch();

  void BeginLines       (Colour const &col, float lineWidth);
  void EndLines         ();
  void BeginLineBatch   ();
  void Line             (float x1, float y1, float x2, float y2, Colour const &col,
                         float lineWidth = 1.0f, bool immediateFlush = false);
  void Line             (float x, float y);
  void EndLineBatch     ();
  void FlushLinesIfFull (int segmentsNeeded);

  void BeginLineStrip2D         (Colour const &col, float lineWidth);
  void LineStripVertex2D        (float x, float y);
  void EndLineStrip2D           ();

  void SetClip                  (int x, int y, int w, int h);
  void ResetClip                ();

  void BlitChar                 (unsigned int textureID, float x, float y, float w, float h,
                                 float texX, float texY, float texW, float texH,
                                 Colour const &col, bool immediateFlush = false);

  void BeginStaticSpriteBatch    ();
  void StaticSprite              (Image *src, float x, float y, float w, float h,
                                  Colour const &col, bool immediateFlush = false);
  void StaticSprite              (Image *src, float x, float y, Colour const &col, 
                                  bool immediateFlush = false);
  void EndStaticSpriteBatch      ();
  void FlushStaticSpritesIfFull  (int verticesNeeded);

  void BeginRotatingSpriteBatch  ();
  void RotatingSprite            (Image *src, float x, float y, float w, float h,
                                  Colour const &col, float angle, bool immediateFlush = false);
  void EndRotatingSpriteBatch    ();

  void FlushAllBatches();


protected:
};

class Renderer2D;

extern Renderer2D *g_renderer2d;

#endif
