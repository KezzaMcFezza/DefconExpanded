/*
 * ========
 * RENDERER
 * ========
 */

#ifndef _included_renderer_h
#define _included_renderer_h

#include "lib/math/vector3.h"
#include "lib/render/colour.h"
#include "lib/tosser/btree.h"

class Image;
class BitmapFont;

static const Colour White{255, 255, 255};
static const Colour Black{0, 0, 0};
static const Colour LightGray{200, 200, 200};
static const Colour DarkGray{100, 100, 100};

#ifndef TARGET_EMSCRIPTEN
#define PREFS_GRAPHICS_SMOOTHLINES "RenderSmoothLines"
#endif

//
// modern OpenGL support structures

struct Matrix4f {
  float m[16];

  constexpr Matrix4f();
  constexpr void LoadIdentity();
  void Ortho(float left, float right, float bottom, float top, float nearZ,
             float farZ);
  constexpr void Multiply(const Matrix4f &other);
};

struct Vertex2D {
  float x, y;
  float r, g, b, a;
  float u, v; // texture coordinates
  
  constexpr Vertex2D() : x(0), y(0), r(1), g(1), b(1), a(1), u(0), v(0) {}
  constexpr Vertex2D(float px, float py, float pr, float pg, float pb, float pa, float pu, float pv) 
    : x(px), y(py), r(pr), g(pg), b(pb), a(pa), u(pu), v(pv) {}
  constexpr Vertex2D(float px, float py, const Colour& color, float pu, float pv)
    : x(px), y(py), r(color.GetRFloat()), g(color.GetGFloat()), b(color.GetBFloat()), a(color.GetAFloat()), u(pu), v(pv) {}
};

class Renderer {
private:

  struct ShaderUniforms {
    int projectionLoc;
    int modelViewLoc;
    int textureLoc; 
  };

  ShaderUniforms m_colorShaderUniforms;
  ShaderUniforms m_textureShaderUniforms;


  //
  // micro batching system - allows the debug menu to track the number of draw
  // calls per frame this system could be abolished, but realistically to
  // maintain mod compatiblity its bet we keep it as mods do not contain texture
  // atlases for the units. this ensures backwards compatibility without using
  // the old blit system. to be honest claude done a good job with the micro
  // batching system so i see no reason to optimise it anymore.

  enum BufferType {
    BUFFER_UI_TRIANGLES,       // UI rectangles, buttons, panels, not yet implemented with batching
    BUFFER_UI_LINES,           // UI borders, grids, wireframes, not yet implemented with batching
    BUFFER_TEXT,               // all text rendering, not yet implemented with batching
    BUFFER_SPRITES,            // general sprites/images
    BUFFER_UNIT_TRAILS,        // movement history lines, 4000 draw call reductin when implemented with batching
    BUFFER_UNIT_MAIN_SPRITES,  // main static unit sprites (ground/sea units)
    BUFFER_UNIT_ROTATING,      // rotating sprites (aircraft/nukes)
    BUFFER_UNIT_HIGHLIGHTS,    // selection/highlight
    BUFFER_UNIT_STATE_ICONS,   // fighter/bomber/nuke state indicators
    BUFFER_UNIT_COUNTERS,      // unit counter text
    BUFFER_UNIT_NUKE_ICONS,    // small nuke icons
    BUFFER_EFFECTS_LINES,      // gunfire trails, radar ranges
    BUFFER_EFFECTS_SPRITES,    // explosions
    BUFFER_WHITEBOARD,         // whiteboard drawings
    BUFFER_LEGACY              // immediate mode rendering
  };

  static constexpr int MAX_VERTICES                 = 1000;
  static constexpr int MAX_UI_VERTICES              = 5000;
  static constexpr int MAX_TEXT_VERTICES            = 28000;
  static constexpr int MAX_UNIT_TRAIL_VERTICES      = 30000;
  static constexpr int MAX_UNIT_MAIN_VERTICES       = 30000;
  static constexpr int MAX_UNIT_ROTATING_VERTICES   = 10000;
  static constexpr int MAX_UNIT_HIGHLIGHT_VERTICES  = 500;
  static constexpr int MAX_UNIT_STATE_VERTICES      = 7500;
  static constexpr int MAX_UNIT_COUNTER_VERTICES    = 100;
  static constexpr int MAX_UNIT_NUKE_VERTICES       = 7500;
  static constexpr int MAX_EFFECTS_LINE_VERTICES    = 50000;
  static constexpr int MAX_EFFECTS_SPRITE_VERTICES  = 7500;
  static constexpr int MAX_HEALTH_BAR_VERTICES      = 500;
  static constexpr int MAX_WHITEBOARD_VERTICES      = 2000;
  static constexpr int MAX_ECLIPSE_RECT_VERTICES    = 2000;
  static constexpr int MAX_ECLIPSE_RECTFILL_VERTICES= 3000;
  static constexpr int MAX_ECLIPSE_TRIANGLEFILL_VERTICES = 3000;
  static constexpr int MAX_ECLIPSE_LINE_VERTICES    = 2000;
  static constexpr int MAX_ECLIPSE_SPRITE_VERTICES  = 1500; 

protected:
  char *m_defaultFontName;
  char *m_defaultFontFilename;
  bool m_defaultFontLanguageSpecific;
  char *m_currentFontName;
  char *m_currentFontFilename;
  bool m_currentFontLanguageSpecific;
  bool m_horizFlip;
  bool m_fixedWidth;
  bool m_negative;
  BTree<float> m_fontSpacings;

  //
  // OpenGL 3.3 Core profile support

  unsigned int m_shaderProgram;
  unsigned int m_colorShaderProgram;
  unsigned int m_textureShaderProgram;
  unsigned int m_VAO;
  unsigned int m_VBO; 

  unsigned int m_eclipseLinesVAO, m_eclipseLinesVBO;      // eclipse rects + lines (line primitives)
  unsigned int m_eclipseFillsVAO, m_eclipseFillsVBO;      // eclipse rectfills + trianglefills (filled primitives)
  unsigned int m_eclipseSpritesVAO, m_eclipseSpritesVBO;  // eclipse sprites (textured, high texture switching)
  unsigned int m_uiVAO, m_uiVBO;                          // general UI triangles and lines (non-Eclipse)
  unsigned int m_textVAO, m_textVBO;                      // text rendering
  unsigned int m_unitVAO, m_unitVBO;                      // unit sprites and highlights
  unsigned int m_effectsVAO, m_effectsVBO;                // effects and trails
  unsigned int m_healthVAO, m_healthVBO;                  // health bars
  unsigned int m_legacyVAO, m_legacyVBO;                  // triangle/line rendering
  
  bool m_bufferNeedsUpload;

  Matrix4f m_projectionMatrix;
  Matrix4f m_modelViewMatrix;

  int m_projMatrixLocation;
  int m_modelViewMatrixLocation;
  int m_colorLocation;
  int m_textureLocation;

  //
  // legacy vertex buffers

  Vertex2D m_triangleVertices       [MAX_VERTICES];
  int m_triangleVertexCount;
  Vertex2D m_lineVertices           [MAX_VERTICES];
  int m_lineVertexCount;

  //
  // ui/primitive buffers (lines, rects, circles - non-textured)

  Vertex2D m_uiTriangleVertices     [MAX_UI_VERTICES];
  int m_uiTriangleVertexCount;
  Vertex2D m_uiLineVertices         [MAX_UI_VERTICES];
  int m_uiLineVertexCount;

  //
  // text/font buffer, textured with font-aware batching for multiple atlases

  static const int MAX_FONT_ATLASES = 4;  // bitlow, kremlin, lucon, zerothre
  
  struct FontBatch {
    Vertex2D vertices[MAX_TEXT_VERTICES];
    int vertexCount;
    unsigned int textureID;
  };
  
  FontBatch m_fontBatches[MAX_FONT_ATLASES];
  int m_currentFontBatchIndex;
  
  Vertex2D *m_textVertices;           // points to current font batch vertices
  int m_textVertexCount;              // current font batch vertex count  
  unsigned int m_currentTextTexture;  // current font batch texture ID

  //
  // unit rendering buffers

  Vertex2D m_unitTrailVertices      [MAX_UNIT_TRAIL_VERTICES];
  int m_unitTrailVertexCount;

  Vertex2D m_unitMainVertices       [MAX_UNIT_MAIN_VERTICES];
  int m_unitMainVertexCount;
  unsigned int m_currentUnitMainTexture;

  Vertex2D m_unitRotatingVertices   [MAX_UNIT_ROTATING_VERTICES];
  int m_unitRotatingVertexCount;
  unsigned int m_currentUnitRotatingTexture;

  Vertex2D m_radarFillVertices      [MAX_EFFECTS_SPRITE_VERTICES];
  int m_effectsCircleFillVertexCount;

  Vertex2D m_effectsCircleOutlineVertices   [MAX_EFFECTS_LINE_VERTICES];
  int m_effectsCircleOutlineVertexCount;

  Vertex2D m_effectsCircleOutlineThickVertices[MAX_EFFECTS_LINE_VERTICES];
  int m_effectsCircleOutlineThickVertexCount;

  Vertex2D m_unitHighlightVertices  [MAX_UNIT_HIGHLIGHT_VERTICES];
  int m_unitHighlightVertexCount;
  unsigned int m_currentUnitHighlightTexture;

  Vertex2D m_unitStateVertices      [MAX_UNIT_STATE_VERTICES];
  int m_unitStateVertexCount;
  unsigned int m_currentUnitStateTexture;

  Vertex2D m_unitCounterVertices    [MAX_UNIT_COUNTER_VERTICES];
  int m_unitCounterVertexCount;
  unsigned int m_currentUnitCounterTexture;

  Vertex2D m_unitNukeVertices       [MAX_UNIT_NUKE_VERTICES];
  int m_unitNukeVertexCount;
  unsigned int m_currentUnitNukeTexture;

  //
  // effect rendering buffers

  Vertex2D m_effectsLineVertices    [MAX_EFFECTS_LINE_VERTICES];
  int m_effectsLineVertexCount;

  Vertex2D m_effectsRectVertices    [MAX_EFFECTS_LINE_VERTICES];
  int m_effectsRectVertexCount;

  Vertex2D m_effectsSpriteVertices  [MAX_EFFECTS_SPRITE_VERTICES];
  int m_effectsSpriteVertexCount;
  unsigned int m_currentEffectsSpriteTexture;

  //
  // health bar rendering buffer

  Vertex2D m_healthBarVertices      [MAX_HEALTH_BAR_VERTICES];
  int m_healthBarVertexCount;

  //
  // whiteboard rendering buffer

  Vertex2D m_whiteboardVertices     [MAX_WHITEBOARD_VERTICES];
  int m_whiteboardVertexCount;

  //
  // eclipse UI primitive rendering buffers 

  Vertex2D m_eclipseRectVertices    [MAX_ECLIPSE_RECT_VERTICES];
  int m_eclipseRectVertexCount;

  Vertex2D m_eclipseRectFillVertices[MAX_ECLIPSE_RECTFILL_VERTICES];
  int m_eclipseRectFillVertexCount;

  Vertex2D m_eclipseTriangleFillVertices[MAX_ECLIPSE_TRIANGLEFILL_VERTICES];
  int m_eclipseTriangleFillVertexCount;

  Vertex2D m_eclipseLineVertices    [MAX_ECLIPSE_LINE_VERTICES];
  int m_eclipseLineVertexCount;

  Vertex2D m_eclipseSpriteVertices  [MAX_ECLIPSE_SPRITE_VERTICES];
  int m_eclipseSpriteVertexCount;
  unsigned int m_currentEclipseSpriteTexture;

  //
  // flush control, more of a global way to disable batching

  bool m_allowImmedateFlush;

  Vertex2D* m_lineConversionBuffer;
  int m_lineConversionBufferSize;

  //
  // line rendering state

  Colour m_currentLineColor;
  Colour m_currentEclipseLineColor;

  //
  // line strip rendering state

  bool m_lineStripActive;
  Colour m_lineStripColor;
  float m_lineStripWidth;

  //
  // cached line strip state

  bool m_cachedLineStripActive;
  char *m_currentCacheKey;
  Colour m_cachedLineStripColor;
  float m_cachedLineStripWidth;

  //
  // megavbo state for large geometry

  int m_maxMegaVertices;
  int m_maxMegaIndices;

  bool m_megaVBOActive;
  char *m_currentMegaVBOKey;
  Colour m_megaVBOColor;
  float m_megaVBOWidth;
  Vertex2D *m_megaVertices;
  int m_megaVertexCount;
  unsigned int *m_megaIndices;
  int m_megaIndexCount;

  //
  // vbo caching system for coastlines and borders, replaced display lists

  struct CachedVBO {
    unsigned int VBO;
    unsigned int VAO;
    unsigned int IBO;           // Index buffer for indexed drawing
    int vertexCount;            // Number of actual vertices (no duplication)
    int indexCount;             // Number of indices
    Colour color;
    float lineWidth;
    bool isValid;
  };

  BTree<CachedVBO *> m_cachedVBOs;

  //
  // font/texture batching state

  unsigned int m_currentBoundTexture;
  bool m_batchingTextures;

  //
  // methods for modern OpenGL

  void InitializeShaders();
  void CacheUniformLocations();
  void SetupVertexArrays();
  void SetColorShaderUniforms();
  void SetTextureShaderUniforms();
  void UploadVertexData(const Vertex2D* vertices, int vertexCount);
  void UploadVertexDataToVBO(unsigned int vbo, const Vertex2D* vertices, int vertexCount, unsigned int usageHint);
  void FlushIfTextureChanged(unsigned int newTextureID, bool useTexture);
  bool ShouldFlushThisFrame();
  void FlushTriangles(bool useTexture);
  void FlushLines();
  void FlushAllBuffers();

  //
  // flush methods

  void FlushUITriangles();
  void FlushUILines();
  void FlushTextBuffer();
  void FlushAllSpecializedBuffers();

  //
  // Unit rendering flush methods

  void FlushUnitTrails();
  void FlushUnitMainSprites();
  void FlushUnitRotating();
  void FlushUnitHighlights();
  void FlushUnitStateIcons();
  void FlushUnitCounters();
  void FlushUnitNukeIcons();

  //
  // effect rendering flush methods

  void FlushEffectsLines();
  void FlushEffectsRects();
  void FlushEffectsSprites();
  void FlushHealthBars();
  void FlushEffectsCircleFills();
  void FlushEffectsCircleOutlines();
  void FlushEffectsCircleOutlinesThick();
  void FlushWhiteboard();

  //
  // eclipse UI primitive flush methods
  
  void FlushEclipseRects();
  void FlushEclipseRectFills();
  void FlushEclipseTriangleFills();
  void FlushEclipseLines();
  void FlushEclipseSprites();

  //
  // buffer overflow management

  void FlushEffectsLinesIfFull      (int segmentsNeeded);
  void FlushEffectsRectsIfFull      (int segmentsNeeded);
  void FlushEclipseRectsIfFull      (int segmentsNeeded);
  void FlushEclipseRectFillsIfFull  (int verticesNeeded);
  void FlushEclipseTriangleFillsIfFull(int verticesNeeded);
  void FlushEclipseLinesIfFull      (int segmentsNeeded);
  void FlushEclipseSpritesIfFull    (int verticesNeeded);
  void FlushUnitStateIconsIfFull    (int verticesNeeded);
  void FlushUnitNukeIconsIfFull     (int verticesNeeded);
  void FlushUnitRotatingIfFull      (int verticesNeeded);

  //
  // main flush methods

  void FlushAllUnitBuffers();
  void FlushAllEffectBuffers();

  // Buffer selection for drawing operations
  BufferType m_activeBuffer;

public:

  //
  // shader creation for 3D renderer

  unsigned int CreateShader         (const char *vertexSource,
                                     const char *fragmentSource);

  void SetViewport                  (int x, int y, int width, int height);
  void SetActiveTexture             (GLenum texture);
  void SetShaderProgram             (GLuint program);
  void SetVertexArray               (GLuint vao);
  void SetArrayBuffer               (GLuint buffer);
  void SetElementBuffer             (GLuint buffer);
  void SetLineWidth                 (GLfloat width);
  void SetBoundTexture              (GLuint texture);
  void SetScissorTest               (bool enabled);
  void SetScissor                   (int x, int y, int width, int height);
  void SetTextureParameter          (GLenum pname, GLint param);

  //
  // atlas sprite support helpers

  void GetImageUVCoords             (Image *image, float &u1, float &v1, float &u2,float &v2);
  unsigned int GetEffectiveTextureID(Image *image);

  //
  // flush method for font batching optimization

  void FlushVertices                (unsigned int primitiveType, bool useTexture = false);

  //
  // batching system controls

  void SetTextureBatching           (bool enabled) { m_batchingTextures = enabled; }
  bool IsTextureBatchingEnabled     () const { return m_batchingTextures; }

  //
  // Frame management

  void SetImmediateFlush            (bool enabled) { m_allowImmedateFlush = enabled; }
  bool IsImmediateFlushEnabled      () const { return m_allowImmedateFlush; }
  void BeginFrame();
  void EndFrame();

  //
  // context switching, flush specific buffers when switching render contexts

  void FlushUIContext();
  void FlushTextContext();

  //
  // UI batching system - following map_renderer pattern
  //
  
  void BeginUITriangleBatch();
  void EndUITriangleBatch();
  void BeginUILineBatch();
  void EndUILineBatch();
  void BeginUIBatch();
  void EndUIBatch();
  
  //
  // eclipse UI primitive batching system
  
  void BeginEclipseRectBatch();
  void EndEclipseRectBatch();
  void BeginEclipseRectFillBatch();
  void EndEclipseRectFillBatch();
  void BeginEclipseTriangleFillBatch();
  void EndEclipseTriangleFillBatch();
  void BeginEclipseLineBatch();
  void EndEclipseLineBatch();
  void BeginEclipseSpriteBatch();
  void EndEclipseSpriteBatch();
  
  //
  // eclipse primitive rendering (non-textured)

  void EclipseRect(float x, float y, float w, float h, Colour const &col, float lineWidth = 1.0f);
  void EclipseRectFill(float x, float y, float w, float h, Colour const &col);
  void EclipseRectFill(float x, float y, float w, float h, Colour const &colTL, Colour const &colTR, 
                       Colour const &colBR, Colour const &colBL);
  void EclipseRectFill(float x, float y, float w, float h, Colour const &col1, 
                       Colour const &col2, bool horizontal);

  void EclipseTriangleFill(float x1, float y1, float x2, float y2, float x3, float y3, Colour const &col);
  void EclipseTriangleFill(float x1, float y1, float x2, float y2, float x3, float y3, 
                          Colour const &col1, Colour const &col2, Colour const &col3);

  void EclipseLine(float x1, float y1, float x2, float y2, Colour const &col, float lineWidth = 1.0f);
  
  void BeginEclipseLines(Colour const &col, float lineWidth);
  void EclipseLine(float x, float y);
  void EndEclipseLines();
  
  //
  // Eclipse sprite rendering (textured)
  
  void EclipseSprite(Image *src, float x, float y, Colour const &col);
  void EclipseSprite(Image *src, float x, float y, float w, float h, Colour const &col);
  void EclipseSprite(Image *src, float x, float y, float w, float h, Colour const &col, float angle);

  //
  // performance tracking, draw call counters per frame

  int m_drawCallsPerFrame;
  int m_legacyTriangleCalls;
  int m_legacyLineCalls;
  int m_uiTriangleCalls;
  int m_uiLineCalls;
  int m_textCalls;
  int m_unitTrailCalls;
  int m_unitMainSpriteCalls;
  int m_unitRotatingCalls;
  int m_unitHighlightCalls;
  int m_unitStateIconCalls;
  int m_unitCounterCalls;
  int m_unitNukeIconCalls;
  int m_effectsLineCalls;
  int m_effectsRectCalls;
  int m_effectsSpriteCalls;
  int m_effectsCircleFillCalls;
  int m_effectsCircleOutlineCalls;
  int m_effectsCircleOutlineThickCalls;
  int m_healthBarCalls;
  int m_whiteboardCalls;
  int m_eclipseRectCalls;
  int m_eclipseRectFillCalls;
  int m_eclipseTriangleFillCalls;
  int m_eclipseLineCalls;
  int m_eclipseSpriteCalls;
  int m_prevDrawCallsPerFrame;
  int m_prevLegacyTriangleCalls;
  int m_prevLegacyLineCalls;
  int m_prevUiTriangleCalls;
  int m_prevUiLineCalls;
  int m_prevTextCalls;
  int m_prevUnitTrailCalls;
  int m_prevUnitMainSpriteCalls;
  int m_prevUnitRotatingCalls;
  int m_prevUnitHighlightCalls;
  int m_prevUnitStateIconCalls;
  int m_prevUnitCounterCalls;
  int m_prevUnitNukeIconCalls;
  int m_prevEffectsLineCalls;
  int m_prevEffectsRectCalls;
  int m_prevEffectsSpriteCalls;
  int m_prevEffectsCircleFillCalls;
  int m_prevEffectsCircleOutlineCalls;
  int m_prevEffectsCircleOutlineThickCalls;
  int m_prevHealthBarCalls;
  int m_prevWhiteboardCalls;
  int m_prevEclipseRectCalls;
  int m_prevEclipseRectFillCalls;
  int m_prevEclipseTriangleFillCalls;
  int m_prevEclipseLineCalls;
  int m_prevEclipseSpriteCalls;

  //
  // start over

  void ResetFrameCounters();
  void IncrementDrawCall(const char *bufferType);

  //
  // performance monitoring getters

  int GetTotalDrawCalls         () const { return m_prevDrawCallsPerFrame; }
  int GetLegacyTriangleCalls    () const { return m_prevLegacyTriangleCalls; }
  int GetLegacyLineCalls        () const { return m_prevLegacyLineCalls; }
  int GetUITriangleCalls        () const { return m_prevUiTriangleCalls; }
  int GetUILineCalls            () const { return m_prevUiLineCalls; }
  int GetTextCalls              () const { return m_prevTextCalls; }
  int GetUnitTrailCalls         () const { return m_prevUnitTrailCalls; }
  int GetUnitMainSpriteCalls    () const { return m_prevUnitMainSpriteCalls; }
  int GetUnitRotatingCalls      () const { return m_prevUnitRotatingCalls; }
  int GetUnitHighlightCalls     () const { return m_prevUnitHighlightCalls; }
  int GetUnitStateIconCalls     () const { return m_prevUnitStateIconCalls; }
  int GetUnitCounterCalls       () const { return m_prevUnitCounterCalls; }
  int GetUnitNukeIconCalls      () const { return m_prevUnitNukeIconCalls; }
  int GetEffectsLineCalls       () const { return m_prevEffectsLineCalls; }
  int GetEffectsRectCalls       () const { return m_prevEffectsRectCalls; }
  int GetEffectsSpriteCalls     () const { return m_prevEffectsSpriteCalls; }
  int GetEffectsCircleFillCalls () const { return m_prevEffectsCircleFillCalls; }
  int GetEffectsCircleOutlineCalls () const { return m_prevEffectsCircleOutlineCalls; }
  int GetEffectsCircleOutlineThickCalls () const { return m_prevEffectsCircleOutlineThickCalls; }
  int GetHealthBarCalls         () const { return m_prevHealthBarCalls; }
  int GetWhiteboardCalls        () const { return m_prevWhiteboardCalls; }
  int GetEclipseRectCalls       () const { return m_prevEclipseRectCalls; }
  int GetEclipseRectFillCalls   () const { return m_prevEclipseRectFillCalls; }
  int GetEclipseTriangleFillCalls() const { return m_prevEclipseTriangleFillCalls; }
  int GetEclipseLineCalls       () const { return m_prevEclipseLineCalls; }
  int GetEclipseSpriteCalls     () const { return m_prevEclipseSpriteCalls; }

  //
  // combined draw call counters

  int GetTotalUnitCalls() const {
    return m_prevUnitTrailCalls + m_prevUnitMainSpriteCalls +
           m_prevUnitRotatingCalls + m_prevUnitHighlightCalls +
           m_prevUnitStateIconCalls + m_prevUnitCounterCalls +
           m_prevUnitNukeIconCalls + m_prevHealthBarCalls;
  }
  int GetTotalEffectCalls() const {
    return m_prevEffectsLineCalls + m_prevEffectsRectCalls + m_prevEffectsSpriteCalls + 
           m_prevEffectsCircleFillCalls + m_prevEffectsCircleOutlineCalls + 
           m_prevEffectsCircleOutlineThickCalls + m_prevWhiteboardCalls;
  }
  int GetTotalSpecializedCalls() const {
    return GetTotalUnitCalls() + GetTotalEffectCalls() + m_prevUiTriangleCalls +
           m_prevUiLineCalls + m_prevTextCalls;
  }

  //
  // get vertex counts

  int GetCurrentTextVertexCount() const { return m_textVertexCount; }
  int GetCurrentUnitTrailVertexCount() const { return m_unitTrailVertexCount; }
  int GetCurrentUnitMainVertexCount() const { return m_unitMainVertexCount; }
  int GetCurrentUnitRotatingVertexCount() const { return m_unitRotatingVertexCount; }
  int GetCurrentUnitHighlightVertexCount() const { return m_unitHighlightVertexCount; }
  int GetCurrentUnitStateVertexCount() const { return m_unitStateVertexCount; }
  int GetCurrentUnitCounterVertexCount() const { return m_unitCounterVertexCount; }
  int GetCurrentUnitNukeVertexCount() const { return m_unitNukeVertexCount; }
  int GetCurrentEffectsLineVertexCount() const { return m_effectsLineVertexCount; }
  int GetCurrentEffectsRectVertexCount() const { return m_effectsRectVertexCount; }
  int GetCurrentEffectsSpriteVertexCount() const { return m_effectsSpriteVertexCount; }
  int GetCurrentEffectsCircleFillVertexCount() const { return m_effectsCircleFillVertexCount; }
  int GetCurrentEffectsCircleOutlineVertexCount() const { return m_effectsCircleOutlineVertexCount; }
  int GetCurrentEffectsCircleOutlineThickVertexCount() const { return m_effectsCircleOutlineThickVertexCount; }
  int GetCurrentHealthBarVertexCount() const { return m_healthBarVertexCount; }
  int GetCurrentWhiteboardVertexCount() const { return m_whiteboardVertexCount; }
  int GetCurrentEclipseRectVertexCount() const { return m_eclipseRectVertexCount; }
  int GetCurrentEclipseRectFillVertexCount() const { return m_eclipseRectFillVertexCount; }
  int GetCurrentEclipseTriangleFillVertexCount() const { return m_eclipseTriangleFillVertexCount; }
  int GetCurrentEclipseLineVertexCount() const { return m_eclipseLineVertexCount; }
  int GetCurrentEclipseSpriteVertexCount() const { return m_eclipseSpriteVertexCount; }
  int GetCurrentUITriangleVertexCount() const { return m_uiTriangleVertexCount; }
  int GetCurrentUILineVertexCount() const { return m_uiLineVertexCount; }
  int GetCurrentLegacyTriangleVertexCount() const { return m_triangleVertexCount; }
  int GetCurrentLegacyLineVertexCount() const { return m_lineVertexCount; }
  
  //
  // now get total current vertex count across all buffers
  
  int GetTotalCurrentVertexCount() const {
    return m_textVertexCount + m_unitTrailVertexCount + m_unitMainVertexCount + 
           m_unitRotatingVertexCount + m_unitHighlightVertexCount + m_unitStateVertexCount +
           m_unitCounterVertexCount + m_unitNukeVertexCount + m_effectsLineVertexCount +
           m_effectsRectVertexCount + m_effectsSpriteVertexCount + m_effectsCircleFillVertexCount +
           m_effectsCircleOutlineVertexCount + m_effectsCircleOutlineThickVertexCount +
           m_healthBarVertexCount + m_whiteboardVertexCount + m_eclipseRectVertexCount +
           m_eclipseRectFillVertexCount + m_eclipseTriangleFillVertexCount + m_eclipseLineVertexCount +
           m_eclipseSpriteVertexCount + m_uiTriangleVertexCount + m_uiLineVertexCount +
           m_triangleVertexCount + m_lineVertexCount;
  }

  int GetMegaBufferVertexCount() const { return m_maxMegaVertices; }
  int GetMegaBufferIndexCount() const { return m_maxMegaIndices; }
  int GetLineConversionBufferSize() const { return m_lineConversionBufferSize; }
  
  //
  // now get total allocated memory

  size_t GetTotalAllocatedBufferMemory() const {
    size_t total = 0;
    
    total += GetTotalCurrentVertexCount() * sizeof(Vertex2D);
    total += m_maxMegaVertices * sizeof(Vertex2D);
    total += m_maxMegaIndices * sizeof(unsigned int);
    total += m_lineConversionBufferSize * sizeof(Vertex2D);
    
    return total;
  }

protected:
  char *ScreenshotsDirectory();

public:
  float m_alpha;
  int m_colourDepth;
  int m_mouseMode;
  int m_blendMode;
  int m_blendSrcFactor;
  int m_blendDstFactor;
  
  bool m_blendEnabled;
  bool m_depthTestEnabled;
  bool m_depthMaskEnabled;
  bool m_scissorTestEnabled;

  int m_currentBlendSrcFactor;
  int m_currentBlendDstFactor;
  int m_currentViewportX;
  int m_currentViewportY;
  int m_currentViewportWidth;
  int m_currentViewportHeight;
  int m_currentScissorX;
  int m_currentScissorY;
  int m_currentScissorWidth;
  int m_currentScissorHeight;

  GLenum m_currentActiveTexture;
  GLfloat m_currentLineWidth;
  GLuint m_currentShaderProgram;
  GLuint m_currentVAO;
  GLuint m_currentArrayBuffer;
  GLuint m_currentElementBuffer;
  GLint m_currentTextureMagFilter;
  GLint m_currentTextureMinFilter;
  
  GLuint m_msaaFBO;
  GLuint m_msaaColorRBO;
  GLuint m_msaaDepthRBO;
  
  bool m_msaaEnabled;
  int m_msaaSamples;
  int m_msaaWidth;
  int m_msaaHeight;
  
  void InitializeMSAAFramebuffer(int width, int height, int samples);
  void ResizeMSAAFramebuffer(int width, int height);
  void DestroyMSAAFramebuffer();
  void BeginMSAARendering();
  void EndMSAARendering();

  enum {
    BlendModeDisabled,
    BlendModeNormal,
    BlendModeAdditive,
    BlendModeSubtractive
  };

public:
  Renderer();
  virtual ~Renderer();

  void Shutdown();

  void Set2DViewport  (float l, float r, float b, float t, int x, int y, int w,int h);
  void Reset2DViewport();

  void BeginScene     ();
  void ClearScreen    (bool _colour, bool _depth);

  void SaveScreenshot ();

  //
  // rendering modes

  void SetBlendMode   (int _blendMode);
  void SetBlendFunc   (int srcFactor, int dstFactor);
  void SetDepthBuffer (bool _enabled, bool _clearNow);

  //
  // text output

  void SetDefaultFont           (const char *font, const char *_langName = NULL);
  void SetFontSpacing           (const char *font, float _spacing);
  float GetFontSpacing          (const char *font);
  void SetFont                  (const char *font = NULL, bool horizFlip = false,
                                 bool negative = false, bool fixedWidth = false,
                                 const char *_langName = NULL);

  void SetFont                  (const char *font, const char *_langName);
  bool IsFontLanguageSpecific   ();
  BitmapFont *GetBitmapFont     ();

  void Text                     (float x, float y, Colour const &col, float size, 
                                 const char *text, ...);
  void TextCentre               (float x, float y, Colour const &col, float size,
                                 const char *text, ...);
  void TextRight                (float x, float y, Colour const &col, float size,
                                 const char *text, ...);
  void TextSimple               (float x, float y, Colour const &col, float size,
                                 const char *text);
  void TextCentreSimple         (float x, float y, Colour const &col, float size,
                                 const char *text);
  void TextRightSimple          (float x, float y, Colour const &col, float size,
                                 const char *text);

  float TextWidth               (const char *text, float size);
  float TextWidth               (const char *text, unsigned int textLen, float size,
                                 BitmapFont *bitmapFont);

  //
  // drawing primitives

  void Rect             (float x, float y, float w, float h, Colour const &col,
                         float lineWidth = 1.0f);
  void RectFill         (float x, float y, float w, float h, Colour const &col);
  void RectFill         (float x, float y, float w, float h, Colour const &colTL,
                         Colour const &colTR, Colour const &colBR, Colour const &colBL);
  void RectFill         (float x, float y, float w, float h, Colour const &col1,
                         Colour const &col2, bool horizontal);

  void Circle           (float x, float y, float radius, int numPoints, Colour const &col,
                         float lineWidth = 1.0f);
  void CircleFill       (float x, float y, float radius, int numPoints,
                         Colour const &col);
  void TriangleFill     (float x1, float y1, float x2, float y2, float x3, float y3,
                         Colour const &col);
  void Line             (float x1, float y1, float x2, float y2, Colour const &col,
                         float lineWidth = 1.0f);

  void BeginLines       (Colour const &col, float lineWidth);
  void Line             (float x, float y);
  void EndLines         ();

  //
  // line strip rendering for continuous lines

  void BeginLineStrip2D         (Colour const &col, float lineWidth);
  void LineStripVertex2D        (float x, float y);
  void EndLineStrip2D           ();

  //
  // VBO caching system for coastlines and borders

  void BeginCachedLineStrip     (const char *cacheKey, Colour const &col,
                                 float lineWidth);
  void CachedLineStripVertex    (float x, float y);
  void EndCachedLineStrip       ();
  void RenderCachedLineStrip    (const char *cacheKey);
  void InvalidateCachedVBO      (const char *cacheKey);

  //
  // megavbo batching system that combines borders and coastlines into a single draw call

  void BeginMegaVBO             (const char *megaVBOKey, Colour const &col);
  void AddLineStripToMegaVBO    (float *vertices, int vertexCount);
  void EndMegaVBO               ();
  void RenderMegaVBO            (const char *megaVBOKey);
  bool IsMegaVBOValid           (const char *megaVBOKey);
  void SetMegaVBOBufferSizes    (int vertexCount, int indexCount);
  void InvalidateAllVBOs        ();

  //
  // whiteboard batching system 

  void BeginWhiteboardBatch     ();
  void WhiteboardLine           (float x1, float y1, float x2, float y2, Colour const &col);
  void EndWhiteboardBatch       ();

  void SetClip                  (int x, int y, int w, int h);
  void ResetClip                ();

  void Blit                     (Image *src, float x, float y, Colour const &col);
  void Blit                     (Image *src, float x, float y, float w, float h, Colour const &col);
  void Blit                     (Image *src, float x, float y, float w, float h, Colour const &col,
                                 float angle);
  void BlitChar                 (unsigned int textureID, float x, float y, float w, float h,
                                 float texX, float texY, float texW, float texH,
                                 Colour const &col);

  void BeginTextBatch();
  void EndTextBatch();
  void FlushTextBufferIfFull    (int charactersNeeded);

  //
  // map-specific text batching methods

  void BeginMapTextBatch();
  void EndMapTextBatch();

  //
  // frame-level text batching for entire scene

  void BeginFrameTextBatch();
  void EndFrameTextBatch();

  //
  // unit trail/movement history rendering

  void BeginUnitTrailBatch        ();
  void UnitTrailLine              (float x1, float y1, float x2, float y2, Colour const &col);
  void EndUnitTrailBatch          ();
  void FlushUnitTrailsIfFull      (int segmentsNeeded);

  //
  // main unit sprite rendering

  void BeginUnitMainBatch         ();
  void UnitMainSprite             (Image *src, float x, float y, float w, float h,
                                   Colour const &col);
  void EndUnitMainBatch           ();
  void FlushUnitMainSpritesIfFull (int verticesNeeded);

  //
  // circle batched rendering

  void    BeginEffectsCircleBatch ();
  void    EndEffectsCircleBatch   ();
  void    EffectsCircleFill       ( float x, float y, float radius, int segments, Colour const &col );
  void    EffectsCircleOutline    ( float x, float y, float radius, int segments, Colour const &col, float lineWidth );

  //
  // rotating sprite rendering (aircraft/nukes with rotation)

  void BeginUnitRotatingBatch     ();
  void UnitRotating               (Image *src, float x, float y, float w, float h,
                                   Colour const &col, float angle);
  void EndUnitRotatingBatch       ();

  //
  // unit selection/highlight rendering

  void BeginUnitHighlightBatch    ();
  void UnitHighlight              (Image *blurSrc, float x, float y, float w, float h,
                                   Colour const &col);
  void EndUnitHighlightBatch      ();

  //
  // unit state indicator rendering
  
  void BeginUnitStateBatch        ();
  void UnitStateIcon              (Image *stateSrc, float x, float y, float w, float h,
                                   Colour const &col);
  void EndUnitStateBatch          ();

  //
  // unit counter/text rendering
  
  void BeginUnitCounterBatch      ();
  void UnitCounterText            (float x, float y, Colour const &col, float size,
                                   const char *text);
  void EndUnitCounterBatch        ();

  //
  // unit nuke supply icon rendering
  
  void BeginUnitNukeBatch         ();
  void UnitNukeIcon               (float x, float y, float w, float h, Colour const &col);
  void UnitNukeIcon               (float x, float y, float w, float h, Colour const &col,
                                   float angle);
  void EndUnitNukeBatch           ();

  //
  // effect lines (gunfire trails, radar ranges)
  
  void BeginEffectsLineBatch      ();
  void EffectsLine                (float x1, float y1, float x2, float y2, Colour const &col);
  void EffectsRect                (float x, float y, float w, float h, Colour const &col, float lineWidth);
  void EndEffectsLineBatch        ();

  //
  // effect sprites (explosions)
  
  void BeginEffectsSpriteBatch    ();
  void EffectsSprite              (Image *src, float x, float y, float w, float h,
                                   Colour const &col);
  void EndEffectsSpriteBatch      ();

  //
  // health bar rendering
  
  void BeginHealthBarBatch        ();
  void HealthBarRect              (float x, float y, float w, float h, Colour const &col);
  void EndHealthBarBatch          ();

  //
  // flush timing system for performance monitoring

  static const int MAX_FLUSH_TYPES = 50;

  struct FlushTiming {
    const char* name;
    double totalTime;
    double totalGpuTime;
    int callCount;
    unsigned int queryObject;
    bool queryPending;
  };

  FlushTiming m_flushTimings[MAX_FLUSH_TYPES];
  int m_flushTimingCount;
  double m_currentFlushStartTime;

  void StartFlushTiming(const char* name);
  void EndFlushTiming(const char* name);
  void UpdateGpuTimings();
  void ResetFlushTimings();
  const FlushTiming* GetFlushTimings(int& count) const;

protected:
};

extern Renderer *g_renderer;

#endif