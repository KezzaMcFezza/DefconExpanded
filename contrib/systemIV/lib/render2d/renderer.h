/*
 * ========
 * RENDERER
 * ========
 */

 #ifndef _included_renderer_h
 #define _included_renderer_h
 
 #include "lib/tosser/btree.h"
 #include "lib/math/vector3.h"
 #include "lib/render/colour.h"
 
 class Image;
 class BitmapFont;
 
 #define     White           Colour(255,255,255)
 #define     Black           Colour(0,0,0)
 #define     LightGray       Colour(200,200,200)
 #define     DarkGray        Colour(100,100,100)
 
 // modern OpenGL support structures
 struct Matrix4f {
     float m[16];
     
     Matrix4f();
     void LoadIdentity();
     void Ortho              ( float left, float right, float bottom, float top, float nearZ, float farZ );
     void Multiply           ( const Matrix4f& other );
 };
 
 struct Vertex2D {
     float x, y;
     float r, g, b, a;
     float u, v;  // texture coordinates
 };
 
 class Renderer
 {
 private:
     // micro batching system - allows the debug menu to track the number of draw calls per frame
     // this system could be abolished, but realistically to maintain mod compatiblity its bet we keep it
     // as mods do not contain texture atlases for the units. this ensures backwards compatibility
     // without using the old blit system. to be honest claude done a good job with the micro
     // batching system so i see no reason to optimise it anymore.
     
     enum BufferType {
         BUFFER_UI_TRIANGLES,           // UI rectangles, buttons, panels, not yet implemented with batching
         BUFFER_UI_LINES,               // UI borders, grids, wireframes, not yet implemented with batching
         BUFFER_TEXT,                   // all text rendering, not yet implemented with batching
         BUFFER_SPRITES,                // general sprites/images
         BUFFER_UNIT_TRAILS,            // movement history lines, 4000 draw call reductin when implemented with batching
         BUFFER_UNIT_MAIN_SPRITES,      // main static unit sprites (ground/sea units)
         BUFFER_UNIT_ROTATING,          // rotating sprites (aircraft/nukes)
         BUFFER_UNIT_HIGHLIGHTS,        // selection/highlight 
         BUFFER_UNIT_STATE_ICONS,       // fighter/bomber/nuke state indicators
         BUFFER_UNIT_COUNTERS,          // unit counter text
         BUFFER_UNIT_NUKE_ICONS,        // small nuke icons
         BUFFER_EFFECTS_LINES,          // gunfire trails, radar ranges
         BUFFER_EFFECTS_SPRITES,        // explosions
         BUFFER_LEGACY                  // immediate mode rendering
     };
     
     static const int MAX_VERTICES = 100000;
     static const int MAX_UI_VERTICES = 500000;
     static const int MAX_TEXT_VERTICES = 300000;
     static const int MAX_SPRITE_VERTICES = 200000;
     static const int MAX_UNIT_TRAIL_VERTICES = 250000;
     static const int MAX_UNIT_MAIN_VERTICES = 50000;
     static const int MAX_UNIT_ROTATING_VERTICES = 30000;
     static const int MAX_UNIT_HIGHLIGHT_VERTICES = 20000;
     static const int MAX_UNIT_STATE_VERTICES = 40000;
     static const int MAX_UNIT_COUNTER_VERTICES = 350000;
     static const int MAX_UNIT_NUKE_VERTICES = 20000;
     static const int MAX_EFFECTS_LINE_VERTICES = 150000;
     static const int MAX_EFFECTS_SPRITE_VERTICES = 100000;
     static const int MAX_HEALTH_BAR_VERTICES = 50000;
 
 protected:
     char    *m_defaultFontName;
     char    *m_defaultFontFilename;
     bool    m_defaultFontLanguageSpecific;
     char    *m_currentFontName;
     char    *m_currentFontFilename;
     bool    m_currentFontLanguageSpecific;
     bool    m_horizFlip;
     bool    m_fixedWidth;
     bool    m_negative;
     BTree <float> m_fontSpacings;
 
     // OpenGL 3.3 Core profile support
     unsigned int m_shaderProgram;
     unsigned int m_colorShaderProgram;
     unsigned int m_textureShaderProgram;
     unsigned int m_VAO;
     unsigned int m_VBO;
     
     Matrix4f m_projectionMatrix;
     Matrix4f m_modelViewMatrix;
     
     int m_projMatrixLocation;
     int m_modelViewMatrixLocation;
     int m_colorLocation;
     int m_textureLocation;
     
     // legacy vertex buffers
     Vertex2D m_triangleVertices[MAX_VERTICES];
     int m_triangleVertexCount;
     Vertex2D m_lineVertices[MAX_VERTICES];
     int m_lineVertexCount;
     
     // ui/primitive buffers (lines, rects, circles - non-textured)
     Vertex2D m_uiTriangleVertices[MAX_UI_VERTICES];
     int m_uiTriangleVertexCount;
     Vertex2D m_uiLineVertices[MAX_UI_VERTICES];
     int m_uiLineVertexCount;
     
     // text/font buffer (textured with smart batching by font texture)
     Vertex2D m_textVertices[MAX_TEXT_VERTICES];
     int m_textVertexCount;
     unsigned int m_currentTextTexture;
     
     // sprite/image buffer (textured with smart batching by image texture)
     Vertex2D m_spriteVertices[MAX_SPRITE_VERTICES];
     int m_spriteVertexCount;
     unsigned int m_currentSpriteTexture;
     
     // unit rendering buffers
     Vertex2D m_unitTrailVertices[MAX_UNIT_TRAIL_VERTICES];
     int m_unitTrailVertexCount;
     
     Vertex2D m_unitMainVertices[MAX_UNIT_MAIN_VERTICES];
     int m_unitMainVertexCount;
     unsigned int m_currentUnitMainTexture;
     
     Vertex2D m_unitRotatingVertices[MAX_UNIT_ROTATING_VERTICES];
     int m_unitRotatingVertexCount;
     unsigned int m_currentUnitRotatingTexture;
     
     Vertex2D m_unitHighlightVertices[MAX_UNIT_HIGHLIGHT_VERTICES];
     int m_unitHighlightVertexCount;
     unsigned int m_currentUnitHighlightTexture;
     
     Vertex2D m_unitStateVertices[MAX_UNIT_STATE_VERTICES];
     int m_unitStateVertexCount;
     unsigned int m_currentUnitStateTexture;
     
     Vertex2D m_unitCounterVertices[MAX_UNIT_COUNTER_VERTICES];
     int m_unitCounterVertexCount;
     unsigned int m_currentUnitCounterTexture;
     
     Vertex2D m_unitNukeVertices[MAX_UNIT_NUKE_VERTICES];
     int m_unitNukeVertexCount;
     unsigned int m_currentUnitNukeTexture;
     
     // effect rendering buffers
     Vertex2D m_effectsLineVertices[MAX_EFFECTS_LINE_VERTICES];
     int m_effectsLineVertexCount;
     
     Vertex2D m_effectsSpriteVertices[MAX_EFFECTS_SPRITE_VERTICES];
     int m_effectsSpriteVertexCount;
     unsigned int m_currentEffectsSpriteTexture;
     
     // health bar rendering buffer
     Vertex2D m_healthBarVertices[MAX_HEALTH_BAR_VERTICES];
     int m_healthBarVertexCount;
     
     // flush control, more of a global way to disable batching
     bool m_allowImmedateFlush;
     
     // line rendering state
     Colour m_currentLineColor;
     
     // line strip rendering state
     bool m_lineStripActive;
     Colour m_lineStripColor;
     float m_lineStripWidth;
     
     // cached line strip state
     bool m_cachedLineStripActive;
     char* m_currentCacheKey;
     Colour m_cachedLineStripColor;
     float m_cachedLineStripWidth;
     
     // megavbo state for large geometry
     bool m_megaVBOActive;
     char* m_currentMegaVBOKey;
     Colour m_megaVBOColor;
     float m_megaVBOWidth;
     static const int MAX_MEGA_VERTICES = 4200000; // any higher and she crashes
     Vertex2D* m_megaVertices;
     int m_megaVertexCount;
     
     // vbo caching system for coastlines and borders, replaced display lists
     struct CachedVBO {
         unsigned int VBO;
         unsigned int VAO;
         int vertexCount;
         Colour color;
         float lineWidth;
         bool isValid;
     };
     
     BTree<CachedVBO*> m_cachedVBOs;
     
     // font/texture batching state
     unsigned int m_currentBoundTexture;
     bool m_batchingTextures;
     
     // methods for modern OpenGL
     void InitializeShaders();
     void SetupVertexArrays();
     void FlushIfTextureChanged  ( unsigned int newTextureID, bool useTexture );
     bool ShouldFlushThisFrame();
     void FlushTriangles         ( bool useTexture );
     void FlushLines();
     void FlushAllBuffers();
 
     // flush methods
     void FlushUITriangles();
     void FlushUILines();
     void FlushTextBuffer();
     void FlushSpriteBuffer();
     void FlushAllSpecializedBuffers();
     
     // Unit rendering flush methods
     void FlushUnitTrails();
     void FlushUnitMainSprites();
     void FlushUnitRotating();
     void FlushUnitHighlights();
     void FlushUnitStateIcons();
     void FlushUnitCounters();
     void FlushUnitNukeIcons();
     
     // effect rendering flush methods
     void FlushEffectsLines();
     void FlushEffectsSprites();
     void FlushHealthBars();
     void FlushEffectsLinesIfFull( int segmentsNeeded );
     
     // buffer overflow management
     void FlushUnitStateIconsIfFull( int verticesNeeded );
     void FlushUnitNukeIconsIfFull( int verticesNeeded );
     void FlushUnitRotatingIfFull( int verticesNeeded );
     
     // main flush methods
     void FlushAllUnitBuffers();
     void FlushAllEffectBuffers();
     
     // Buffer selection for drawing operations
     BufferType m_activeBuffer;
 
 public:
     // shader creation for 3D renderer
         unsigned int CreateShader   ( const char* vertexSource, const char* fragmentSource );
    
    // atlas sprite support helpers
    void GetImageUVCoords       ( Image* image, float& u1, float& v1, float& u2, float& v2 );
    unsigned int GetEffectiveTextureID( Image* image );
    
    // flush method for font batching optimization
    void FlushVertices          ( unsigned int primitiveType, bool useTexture = false );
     
     // batching system controls
     void SetTextureBatching     ( bool enabled ) { m_batchingTextures = enabled; }
     bool IsTextureBatchingEnabled() const { return m_batchingTextures; }
     
     // Frame management
     void SetImmediateFlush      ( bool enabled ) { m_allowImmedateFlush = enabled; }
     bool IsImmediateFlushEnabled() const { return m_allowImmedateFlush; }
     void BeginFrame();
     void EndFrame();
     
     // context switching, flush specific buffers when switching render contexts
     void FlushUIContext();
     void FlushTextContext();
     void FlushSpriteContext();
 
     // performance tracking, draw call counters per frame
     int m_drawCallsPerFrame;
     int m_legacyTriangleCalls;
     int m_legacyLineCalls;
     int m_uiTriangleCalls;
     int m_uiLineCalls;
     int m_textCalls;
     int m_spriteCalls;
     int m_unitTrailCalls;
     int m_unitMainSpriteCalls;
     int m_unitRotatingCalls;
     int m_unitHighlightCalls;
     int m_unitStateIconCalls;
     int m_unitCounterCalls;
     int m_unitNukeIconCalls;
     int m_effectsLineCalls;
     int m_effectsSpriteCalls;
     int m_healthBarCalls;
     int m_prevDrawCallsPerFrame;
     int m_prevLegacyTriangleCalls;
     int m_prevLegacyLineCalls;
     int m_prevUiTriangleCalls;
     int m_prevUiLineCalls;
     int m_prevTextCalls;
     int m_prevSpriteCalls;
     int m_prevUnitTrailCalls;
     int m_prevUnitMainSpriteCalls;
     int m_prevUnitRotatingCalls;
     int m_prevUnitHighlightCalls;
     int m_prevUnitStateIconCalls;
     int m_prevUnitCounterCalls;
     int m_prevUnitNukeIconCalls;
     int m_prevEffectsLineCalls;
     int m_prevEffectsSpriteCalls;
     int m_prevHealthBarCalls;
     
     // start over
     void ResetFrameCounters();
     void IncrementDrawCall      ( const char* bufferType );
     
     // performance monitoring getters
     int GetTotalDrawCalls() const { return m_prevDrawCallsPerFrame; }
     int GetLegacyTriangleCalls() const { return m_prevLegacyTriangleCalls; }
     int GetLegacyLineCalls() const { return m_prevLegacyLineCalls; }
     int GetUITriangleCalls() const { return m_prevUiTriangleCalls; }
     int GetUILineCalls() const { return m_prevUiLineCalls; }
     int GetTextCalls() const { return m_prevTextCalls; }
     int GetSpriteCalls() const { return m_prevSpriteCalls; }
     int GetUnitTrailCalls() const { return m_prevUnitTrailCalls; }
     int GetUnitMainSpriteCalls() const { return m_prevUnitMainSpriteCalls; }
     int GetUnitRotatingCalls() const { return m_prevUnitRotatingCalls; }
     int GetUnitHighlightCalls() const { return m_prevUnitHighlightCalls; }
     int GetUnitStateIconCalls() const { return m_prevUnitStateIconCalls; }
     int GetUnitCounterCalls() const { return m_prevUnitCounterCalls; }
     int GetUnitNukeIconCalls() const { return m_prevUnitNukeIconCalls; }
     int GetEffectsLineCalls() const { return m_prevEffectsLineCalls; }
     int GetEffectsSpriteCalls() const { return m_prevEffectsSpriteCalls; }
     int GetHealthBarCalls() const { return m_prevHealthBarCalls; }
     
     // combined draw call counters
     int GetTotalUnitCalls() const { 
         return m_prevUnitTrailCalls + m_prevUnitMainSpriteCalls + m_prevUnitRotatingCalls + m_prevUnitHighlightCalls + 
                m_prevUnitStateIconCalls + m_prevUnitCounterCalls + m_prevUnitNukeIconCalls + m_prevHealthBarCalls; 
     }
     int GetTotalEffectCalls() const { 
         return m_prevEffectsLineCalls + m_prevEffectsSpriteCalls; 
     }
     int GetTotalSpecializedCalls() const {
         return GetTotalUnitCalls() + GetTotalEffectCalls() + m_prevUiTriangleCalls + 
                m_prevUiLineCalls + m_prevTextCalls + m_prevSpriteCalls;
     }
 
 protected:
     char *ScreenshotsDirectory();
 
 public:
     float   m_alpha;
     int     m_colourDepth;
     int     m_mouseMode;
     int     m_blendMode;
     int     m_blendSrcFactor;
     int     m_blendDstFactor;
     
     enum
     {
         BlendModeDisabled,
         BlendModeNormal,
         BlendModeAdditive,
         BlendModeSubtractive
     };
 
 public:
     Renderer();
     virtual ~Renderer();
 
     void    Shutdown            ();
     
     void    Set2DViewport       ( float l, float r, float b, float t, int x, int y, int w, int h );
     void    Reset2DViewport     ();
 
     void    BeginScene          ();
     void    ClearScreen         ( bool _colour, bool _depth );
 
     void    SaveScreenshot      ();
 
     // rendering modes
     void    SetBlendMode        ( int _blendMode );
     void	SetBlendFunc        ( int srcFactor, int dstFactor );
     void    SetDepthBuffer      ( bool _enabled, bool _clearNow );
 
     // text output
     void    SetDefaultFont      ( const char *font, const char *_langName = NULL );
     void    SetFontSpacing      ( const char *font, float _spacing );
     float   GetFontSpacing      ( const char *font );
     void    SetFont             ( const char *font = NULL, bool horizFlip = false, bool negative = false,
                                   bool fixedWidth = false, const char *_langName = NULL );
     void    SetFont             ( const char *font, const char *_langName );
     bool    IsFontLanguageSpecific();
     BitmapFont *GetBitmapFont   ();
 
     void    Text                ( float x, float y, Colour const &col, float size, const char *text, ... );
     void    TextCentre          ( float x, float y, Colour const &col, float size, const char *text, ... );
     void    TextRight           ( float x, float y, Colour const &col, float size, const char *text, ... );
     void    TextSimple          ( float x, float y, Colour const &col, float size, const char *text );
     void    TextCentreSimple    ( float x, float y, Colour const &col, float size, const char *text );
     void    TextRightSimple     ( float x, float y, Colour const &col, float size, const char *text );
     void    TextSimpleBatch     ( float x, float y, Colour const &col, float size, const char *text );
     void    TextCentreSimpleBatch( float x, float y, Colour const &col, float size, const char *text );
     void    TextRightSimpleBatch ( float x, float y, Colour const &col, float size, const char *text );
     
     float   TextWidth           ( const char *text, float size );
     float   TextWidth           ( const char *text, unsigned int textLen, float size, BitmapFont *bitmapFont );
 
     // drawing primitives
     void    Rect                ( float x, float y, float w, float h, Colour const &col, float lineWidth=1.0f );
     void    RectFill            ( float x, float y, float w, float h, Colour const &col );    
     void    RectFill            ( float x, float y, float w, float h, Colour const &colTL, Colour const &colTR, Colour const &colBR, Colour const &colBL );    
     void    RectFill            ( float x, float y, float w, float h, Colour const &col1, Colour const &col2, bool horizontal );    
     
     void    Circle              ( float x, float y, float radius, int numPoints, Colour const &col, float lineWidth=1.0f );        
     void    CircleFill          ( float x, float y, float radius, int numPoints, Colour const &col );
     void    TriangleFill        ( float x1, float y1, float x2, float y2, float x3, float y3, Colour const &col );
     void    Line                ( float x1, float y1, float x2, float y2, Colour const &col, float lineWidth=1.0f );
     
     void    BeginLines          ( Colour const &col, float lineWidth );    
     void    Line                ( float x, float y );
     void    EndLines            ();
 
     // line strip rendering for continuous lines
     void    BeginLineStrip2D    ( Colour const &col, float lineWidth );
     void    LineStripVertex2D   ( float x, float y );
     void    EndLineStrip2D      ();
 
     // vbo caching system for coastlines and borders
     void    BeginCachedLineStrip( const char* cacheKey, Colour const &col, float lineWidth );
     void    CachedLineStripVertex( float x, float y );
     void    EndCachedLineStrip  ();
     void    RenderCachedLineStrip( const char* cacheKey );
     void    InvalidateCachedVBO ( const char* cacheKey );
 
     // megavbo batching system that combines borders and coastlines into a single draw call
     void    BeginMegaVBO        ( const char* megaVBOKey, Colour const &col, float lineWidth );
     void    AddLineStripToMegaVBO( float* vertices, int vertexCount );
     void    EndMegaVBO          ();
          void    RenderMegaVBO       ( const char* megaVBOKey );
     bool    IsMegaVBOValid      ( const char* megaVBOKey );
     void    InvalidateAllVBOs   ();

     void    SetClip             ( int x, int y, int w, int h );      
     void    ResetClip           ();
 
     void    Blit                ( Image *src, float x, float y, Colour const &col );
     void    Blit                ( Image *src, float x, float y, float w, float h, Colour const &col );
     void    Blit                ( Image *src, float x, float y, float w, float h, Colour const &col, float angle );
     void    BlitChar            ( unsigned int textureID, float x, float y, float w, float h, 
                                   float texX, float texY, float texW, float texH, Colour const &col );
     void    BatchBlitChar       ( unsigned int textureID, float x, float y, float w, float h, 
                                   float texX, float texY, float texW, float texH, Colour const &col );
 
     void    BeginUIBatch        ();
     void    EndUIBatch          ();
     void    BeginTextBatch      ();
     void    EndTextBatch        ();
     void    BeginSpriteBatch    ();
     void    EndSpriteBatch      ();
     
     // map-specific text batching methods
     void    BeginMapTextBatch   ();
     void    EndMapTextBatch     ();
     
     // frame-level text batching for entire scene
     void    BeginFrameTextBatch ();
     void    EndFrameTextBatch   ();
     
     // unit trail/movement history rendering
     void    BeginUnitTrailBatch ();
     void    UnitTrailLine       ( float x1, float y1, float x2, float y2, Colour const &col );
     void    EndUnitTrailBatch   ();
     void    FlushUnitTrailsIfFull( int segmentsNeeded );
     
     // main unit sprite rendering
     void    BeginUnitMainBatch  ();  
     void    UnitMainSprite      ( Image *src, float x, float y, float w, float h, Colour const &col );
     void    EndUnitMainBatch    ();
     void    FlushUnitMainSpritesIfFull( int verticesNeeded );
     
     // rotating sprite rendering (aircraft/nukes with rotation)
     void    BeginUnitRotatingBatch();
     void    UnitRotating        ( Image *src, float x, float y, float w, float h, Colour const &col, float angle );
     void    EndUnitRotatingBatch();
     
     // unit selection/highlight rendering
     void    BeginUnitHighlightBatch();
     void    UnitHighlight       ( Image *blurSrc, float x, float y, float w, float h, Colour const &col );
     void    EndUnitHighlightBatch();
     
     // unit state indicator rendering
     void    BeginUnitStateBatch ();
     void    UnitStateIcon       ( Image *stateSrc, float x, float y, float w, float h, Colour const &col );
     void    EndUnitStateBatch   ();
     
     // unit counter/text rendering
     void    BeginUnitCounterBatch();
     void    UnitCounterText     ( float x, float y, Colour const &col, float size, const char *text );
     void    EndUnitCounterBatch ();
     
     // unit nuke supply icon rendering
     void    BeginUnitNukeBatch  ();
     void    UnitNukeIcon        ( float x, float y, float w, float h, Colour const &col );
     void    UnitNukeIcon        ( float x, float y, float w, float h, Colour const &col, float angle );
     void    EndUnitNukeBatch    ();
     
     // effect lines (gunfire trails, radar ranges)
     void    BeginEffectsLineBatch();
     void    EffectsLine         ( float x1, float y1, float x2, float y2, Colour const &col );
     void    EndEffectsLineBatch ();
     
     // effect sprites (explosions)  
     void    BeginEffectsSpriteBatch();
     void    EffectsSprite       ( Image *src, float x, float y, float w, float h, Colour const &col );
     void    EndEffectsSpriteBatch();
     
     // health bar rendering
     void    BeginHealthBarBatch ();
     void    HealthBarRect       ( float x, float y, float w, float h, Colour const &col );
     void    EndHealthBarBatch   ();
 
 protected:
 };
 
 extern Renderer *g_renderer;
 
 #endif