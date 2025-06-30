/*
 * ========
 * RENDERER
 * ========
 *
 */

#ifndef _included_renderer_h
#define _included_renderer_h

#include "lib/tosser/btree.h"
#include "lib/math/vector3.h"

#include "colour.h"

class Image;
class BitmapFont;

#define     White           Colour(255,255,255)
#define     Black           Colour(0,0,0)

#define     LightGray       Colour(200,200,200)
#define     DarkGray        Colour(100,100,100)

// Modern OpenGL support structures
struct Matrix4f {
    float m[16];
    
    Matrix4f();
    void LoadIdentity();
    void Ortho(float left, float right, float bottom, float top, float nearZ, float farZ);
    void Multiply(const Matrix4f& other);
};

struct Vertex2D {
    float x, y;
    float r, g, b, a;
    float u, v;  // texture coordinates
};

class Renderer
{
private:
    // SPECIALIZED BUFFER SYSTEM: Separate buffers by rendering type to prevent overflow
    // and maintain optimal batching for WebAssembly performance
    enum BufferType {
        // Core UI rendering (existing)
        BUFFER_UI_TRIANGLES,           // UI rectangles, buttons, panels (untextured)
        BUFFER_UI_LINES,               // UI borders, grids, wireframes (untextured)
        BUFFER_TEXT,                   // All text rendering (font texture batched)
        BUFFER_SPRITES,                // General sprites/images (texture batched)
        
        // Unit rendering system (specialized for complex unit states)
        BUFFER_UNIT_TRAILS,            // Movement history lines (untextured lines)
        BUFFER_UNIT_MAIN_SPRITES,      // Main unit sprites (texture batched by unit type) - GROUND/SEA UNITS ONLY
        BUFFER_UNIT_ROTATING,          // Rotating sprites (aircraft/nukes) - SEPARATE BUFFER
        BUFFER_UNIT_HIGHLIGHTS,        // Selection/highlight effects (blur textures)
        BUFFER_UNIT_STATE_ICONS,       // Fighter/bomber/nuke state indicators (small sprites)
        BUFFER_UNIT_COUNTERS,          // Unit counter text/numbers (font texture)
        BUFFER_UNIT_NUKE_ICONS,        // Nuke supply symbols (smallnuke.bmp texture)
        
        // World effects rendering
        BUFFER_EFFECTS_LINES,          // Gunfire trails, radar sweeps (untextured lines)
        BUFFER_EFFECTS_SPRITES,        // Explosions, particles (texture batched)
        
        // Legacy compatibility
        BUFFER_LEGACY                  // Backward compatibility for immediate mode
    };
    
    // Buffer size constants (WebAssembly optimized)
    static const int MAX_VERTICES = 10000;        // Legacy buffer size
    static const int MAX_UI_VERTICES = 50000;     // UI elements (existing)
    static const int MAX_TEXT_VERTICES = 30000;   // Text rendering (existing)
    static const int MAX_SPRITE_VERTICES = 20000; // General sprites (existing)
    
    // Unit rendering buffer sizes (calculated from worst-case scenarios)
    static const int MAX_UNIT_TRAIL_VERTICES = 25000;      // 100 units × 80 trail segments × 2 verts = 16K + safety
    static const int MAX_UNIT_MAIN_VERTICES = 5000;        // 400 ground/sea units × 6 vertices × 2 passes = 4.8K + safety  
    static const int MAX_UNIT_ROTATING_VERTICES = 3000;    // 50 rotating sprites × 6 vertices × 10 rotations = 3K + safety
    static const int MAX_UNIT_HIGHLIGHT_VERTICES = 2000;   // 100 selected units × 6 vertices × 2 passes = 1.2K + safety
    static const int MAX_UNIT_STATE_VERTICES = 4000;       // 200 units × 3 states × 6 vertices = 3.6K + safety
    static const int MAX_UNIT_COUNTER_VERTICES = 35000;    // 200 text renders × 20 chars × 6 vertices = 24K + safety  
    static const int MAX_UNIT_NUKE_VERTICES = 2000;        // 100 units × 6 vertices × 2 icons = 1.2K + safety
    
    // Effect rendering buffer sizes
    static const int MAX_EFFECTS_LINE_VERTICES = 15000;    // Gunfire trails, radar sweeps
    static const int MAX_EFFECTS_SPRITE_VERTICES = 10000;  // Explosions, particles

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

    // Modern OpenGL 3.3 Core Profile support
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
    
    // Dynamic vertex buffers for different primitive types
    Vertex2D m_triangleVertices[MAX_VERTICES];
    int m_triangleVertexCount;
    Vertex2D m_lineVertices[MAX_VERTICES];
    int m_lineVertexCount;
    
    // PERFORMANCE FIX: Separate specialized buffer systems
    // Each system can batch independently without conflicts
    
    // 1. UI/Primitive Buffer (lines, rects, circles - non-textured)
    Vertex2D m_uiTriangleVertices[MAX_UI_VERTICES];
    int m_uiTriangleVertexCount;
    Vertex2D m_uiLineVertices[MAX_UI_VERTICES];
    int m_uiLineVertexCount;
    
    // 2. Text/Font Buffer (textured with smart batching by font texture)
    Vertex2D m_textVertices[MAX_TEXT_VERTICES];
    int m_textVertexCount;
    unsigned int m_currentTextTexture;
    
    // 3. Sprite/Image Buffer (textured with smart batching by image texture)
    Vertex2D m_spriteVertices[MAX_SPRITE_VERTICES];
    int m_spriteVertexCount;
    unsigned int m_currentSpriteTexture;
    
    // 4. Unit rendering specialized buffers (WebAssembly optimized)
    // Trail buffer (movement history - untextured lines)
    Vertex2D m_unitTrailVertices[MAX_UNIT_TRAIL_VERTICES];
    int m_unitTrailVertexCount;
    
    // Main unit sprite buffer (textured triangles - batched by unit type)
    Vertex2D m_unitMainVertices[MAX_UNIT_MAIN_VERTICES];
    int m_unitMainVertexCount;
    unsigned int m_currentUnitMainTexture;
    
    // Rotating sprite buffer (textured triangles with rotation - aircraft/nukes)
    Vertex2D m_unitRotatingVertices[MAX_UNIT_ROTATING_VERTICES];
    int m_unitRotatingVertexCount;
    unsigned int m_currentUnitRotatingTexture;
    
    // Unit highlight buffer (selection/highlight effects - blur textures)
    Vertex2D m_unitHighlightVertices[MAX_UNIT_HIGHLIGHT_VERTICES];
    int m_unitHighlightVertexCount;
    unsigned int m_currentUnitHighlightTexture;
    
    // Unit state icon buffer (fighter/bomber/nuke indicators - small sprites)
    Vertex2D m_unitStateVertices[MAX_UNIT_STATE_VERTICES];
    int m_unitStateVertexCount;
    unsigned int m_currentUnitStateTexture;
    
    // Unit counter buffer (text/numbers - font texture)
    Vertex2D m_unitCounterVertices[MAX_UNIT_COUNTER_VERTICES];
    int m_unitCounterVertexCount;
    unsigned int m_currentUnitCounterTexture;
    
    // Unit nuke icon buffer (nuke supply symbols - smallnuke.bmp)
    Vertex2D m_unitNukeVertices[MAX_UNIT_NUKE_VERTICES];
    int m_unitNukeVertexCount;
    unsigned int m_currentUnitNukeTexture;
    
    // 5. Effect rendering buffers
    // Effect lines buffer (gunfire trails, radar sweeps - untextured lines)
    Vertex2D m_effectsLineVertices[MAX_EFFECTS_LINE_VERTICES];
    int m_effectsLineVertexCount;
    
    // Effect sprite buffer (explosions, particles - textured triangles)
    Vertex2D m_effectsSpriteVertices[MAX_EFFECTS_SPRITE_VERTICES];
    int m_effectsSpriteVertexCount;
    unsigned int m_currentEffectsSpriteTexture;
    
    // 6. World Geometry Buffer (large static geometry like coastlines)
    // This uses the existing mega-VBO system
    
    // Flush control - only flush when switching contexts or at frame end
    bool m_allowImmedateFlush;  // Debug flag to disable immediate flushing
    
    // Line rendering state
    Colour m_currentLineColor;
    
    // Line strip rendering state
    bool m_lineStripActive;
    Colour m_lineStripColor;
    float m_lineStripWidth;
    
    // Cached line strip state
    bool m_cachedLineStripActive;
    char* m_currentCacheKey;
    Colour m_cachedLineStripColor;
    float m_cachedLineStripWidth;
    
    // Mega-VBO state
    bool m_megaVBOActive;
    char* m_currentMegaVBOKey;
    Colour m_megaVBOColor;
    float m_megaVBOWidth;
    static const int MAX_MEGA_VERTICES = 500000;  // Much larger buffer
    Vertex2D* m_megaVertices;
    int m_megaVertexCount;
    
    // VBO caching system for performance (replaces display lists)
    struct CachedVBO {
        unsigned int VBO;
        unsigned int VAO;
        int vertexCount;
        Colour color;
        float lineWidth;
        bool isValid;
    };
    
    BTree<CachedVBO*> m_cachedVBOs;
    
    // Font/texture batching state - for Phase 5.1 performance optimization
    unsigned int m_currentBoundTexture;  // Track current texture for batching
    bool m_batchingTextures;             // Enable intelligent batching
    
    // Frame-based flushing experiment
    int m_frameCounter;
    float m_lastFlushTime;  // Track last flush time for time-based flushing
    
    // Helper methods for modern OpenGL
    void InitializeShaders();
    void SetupVertexArrays();
    void FlushIfTextureChanged(unsigned int newTextureID, bool useTexture);  // New batching helper
    bool ShouldFlushThisFrame();  // Time-based flushing experiment
    void FlushTriangles(bool useTexture);  // Separate flush functions for each primitive type
    void FlushLines();
    void FlushAllBuffers();  // Flush both buffers

    // PERFORMANCE FIX: New specialized flush methods
    void FlushUITriangles();      // Flush UI triangle buffer
    void FlushUILines();          // Flush UI line buffer  
    void FlushTextBuffer();       // Flush text/font buffer
    void FlushSpriteBuffer();     // Flush sprite/image buffer
    void FlushAllSpecializedBuffers(); // Flush all specialized buffers
    
    // Unit rendering specialized flush methods
    void FlushUnitTrails();       // Flush unit trail/history lines (untextured)
    void FlushUnitMainSprites();  // Flush main unit sprites (texture batched by type) - GROUND/SEA ONLY
    void FlushUnitRotating();     // Flush rotating sprites (aircraft/nukes) - SEPARATE BUFFER
    void FlushUnitHighlights();   // Flush unit selection highlights (blur textures)
    void FlushUnitStateIcons();   // Flush unit state indicators (fighter/bomber/nuke icons)
    void FlushUnitCounters();     // Flush unit counter text/numbers (font texture)
    void FlushUnitNukeIcons();    // Flush unit nuke supply symbols (smallnuke.bmp)
    
    // Effect rendering specialized flush methods
    void FlushEffectsLines();     // Flush effects lines (gunfire trails, radar sweeps)
    void FlushEffectsSprites();   // Flush effects sprites (explosions, particles)
    void FlushEffectsLinesIfFull( int segmentsNeeded ); // Auto-flush if effects line buffer getting full
    
    // Unit buffer overflow management
    void FlushUnitStateIconsIfFull( int verticesNeeded ); // Auto-flush if unit state buffer getting full
    void FlushUnitNukeIconsIfFull( int verticesNeeded );  // Auto-flush if unit nuke buffer getting full
    void FlushUnitRotatingIfFull( int verticesNeeded );   // Auto-flush if rotating buffer getting full
    
    // Complete buffer system flush methods
    void FlushAllUnitBuffers();   // Flush all unit-related buffers efficiently
    void FlushAllEffectBuffers(); // Flush all effect-related buffers efficiently
    
    // Buffer selection for drawing operations
    BufferType m_activeBuffer;
    
    // Smart buffer management
    void SetActiveBuffer(BufferType bufferType);
    void AddVertexToActiveBuffer(const Vertex2D& vertex, bool useTexture = false);
    bool HasSpaceInBuffer(BufferType bufferType, int vertexCount);
    void FlushIfBufferFull(BufferType bufferType, int verticesNeeded);

public:
    // Public shader creation for 3D renderer
    unsigned int CreateShader(const char* vertexSource, const char* fragmentSource);
    
    // Public flush method for font batching optimization
    void FlushVertices(unsigned int primitiveType, bool useTexture = false);
    
    // Safety controls for batching system
    void SetTextureBatching(bool enabled) { m_batchingTextures = enabled; }
    bool IsTextureBatchingEnabled() const { return m_batchingTextures; }
    
    // Frame-based flushing experiment
    void IncrementFrame() { m_frameCounter++; }

    // PERFORMANCE FIX: Public controls for new batching system
    void SetImmediateFlush(bool enabled) { m_allowImmedateFlush = enabled; }
    bool IsImmediateFlushEnabled() const { return m_allowImmedateFlush; }
    
    // Frame management - call at start/end of frame
    void BeginFrame();     // Clear counters, prepare for new frame
    void EndFrame();       // Flush all buffers before swap
    
    // Context switching - flush specific buffers when switching render contexts
    void FlushUIContext();     // Flush UI buffers
    void FlushTextContext();   // Flush text buffer  
    void FlushSpriteContext(); // Flush sprite buffer

    // PERFORMANCE TRACKING: Draw call counters per frame (current frame)
    int m_drawCallsPerFrame;
    int m_legacyTriangleCalls;    // Legacy immediate-mode triangle calls
    int m_legacyLineCalls;        // Legacy immediate-mode line calls
    int m_uiTriangleCalls;        // UI triangle buffer calls
    int m_uiLineCalls;            // UI line buffer calls
    int m_textCalls;              // Text buffer calls
    int m_spriteCalls;            // Sprite buffer calls
    
    // Unit rendering performance tracking
    int m_unitTrailCalls;         // Unit trail buffer calls
    int m_unitMainSpriteCalls;    // Unit main sprite buffer calls  
    int m_unitRotatingCalls;      // Unit rotating buffer calls (aircraft/nukes)
    int m_unitHighlightCalls;     // Unit highlight buffer calls
    int m_unitStateIconCalls;     // Unit state icon buffer calls
    int m_unitCounterCalls;       // Unit counter buffer calls
    int m_unitNukeIconCalls;      // Unit nuke icon buffer calls
    
    // Effect rendering performance tracking
    int m_effectsLineCalls;       // Effects line buffer calls
    int m_effectsSpriteCalls;     // Effects sprite buffer calls
    
    // PERFORMANCE TRACKING: Previous frame data (for display/logging)
    int m_prevDrawCallsPerFrame;
    int m_prevLegacyTriangleCalls;
    int m_prevLegacyLineCalls;
    int m_prevUiTriangleCalls;
    int m_prevUiLineCalls;
    int m_prevTextCalls;
    int m_prevSpriteCalls;
    
    // Unit rendering previous frame data
    int m_prevUnitTrailCalls;
    int m_prevUnitMainSpriteCalls;
    int m_prevUnitRotatingCalls;      // Previous rotating buffer calls
    int m_prevUnitHighlightCalls;
    int m_prevUnitStateIconCalls;
    int m_prevUnitCounterCalls;
    int m_prevUnitNukeIconCalls;
    
    // Effect rendering previous frame data
    int m_prevEffectsLineCalls;
    int m_prevEffectsSpriteCalls;
    
    // Frame statistics (reset each frame)
    void ResetFrameCounters();
    void IncrementDrawCall(const char* bufferType);
    
    // PERFORMANCE MONITORING: Public getters for detailed draw call statistics
    int GetTotalDrawCalls() const { return m_prevDrawCallsPerFrame; }
    int GetLegacyTriangleCalls() const { return m_prevLegacyTriangleCalls; }
    int GetLegacyLineCalls() const { return m_prevLegacyLineCalls; }
    int GetUITriangleCalls() const { return m_prevUiTriangleCalls; }
    int GetUILineCalls() const { return m_prevUiLineCalls; }
    int GetTextCalls() const { return m_prevTextCalls; }
    int GetSpriteCalls() const { return m_prevSpriteCalls; }
    
    // Unit rendering performance getters
    int GetUnitTrailCalls() const { return m_prevUnitTrailCalls; }
    int GetUnitMainSpriteCalls() const { return m_prevUnitMainSpriteCalls; }
    int GetUnitRotatingCalls() const { return m_prevUnitRotatingCalls; }
    int GetUnitHighlightCalls() const { return m_prevUnitHighlightCalls; }
    int GetUnitStateIconCalls() const { return m_prevUnitStateIconCalls; }
    int GetUnitCounterCalls() const { return m_prevUnitCounterCalls; }
    int GetUnitNukeIconCalls() const { return m_prevUnitNukeIconCalls; }
    
    // Effect rendering performance getters
    int GetEffectsLineCalls() const { return m_prevEffectsLineCalls; }
    int GetEffectsSpriteCalls() const { return m_prevEffectsSpriteCalls; }
    
    // Aggregate performance analysis methods
    int GetTotalUnitCalls() const { 
        return m_prevUnitTrailCalls + m_prevUnitMainSpriteCalls + m_prevUnitRotatingCalls + m_prevUnitHighlightCalls + 
               m_prevUnitStateIconCalls + m_prevUnitCounterCalls + m_prevUnitNukeIconCalls; 
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

	int   m_blendSrcFactor;
	int   m_blendDstFactor;
    
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
    
    void    Set2DViewport       ( float l, float r, float b, float t,
                                    int x, int y, int w, int h );
    void    Reset2DViewport     ();

    void    BeginScene          ();
    void    ClearScreen         ( bool _colour, bool _depth );

    void    SaveScreenshot      ();


    //
    // Rendering modes

    void    SetBlendMode        ( int _blendMode );
	void	SetBlendFunc        ( int srcFactor, int dstFactor );
    void    SetDepthBuffer      ( bool _enabled, bool _clearNow );

    //
    // Text output

    void    SetDefaultFont      ( const char *font, const char *_langName = NULL );
    void    SetFontSpacing      ( const char *font, float _spacing );
    float   GetFontSpacing      ( const char *font );
    void    SetFont             ( const char *font = NULL, 
                                  bool horizFlip = false, 
                                  bool negative = false,
                                  bool fixedWidth = false,
                                  const char *_langName = NULL );
    void    SetFont             ( const char *font, 
                                  const char *_langName );
    bool    IsFontLanguageSpecific ();
    BitmapFont *GetBitmapFont   ();

    void    Text                ( float x, float y, Colour const &col, float size, const char *text, ... );
    void    TextCentre          ( float x, float y, Colour const &col, float size, const char *text, ... );
    void    TextRight           ( float x, float y, Colour const &col, float size, const char *text, ... );
    void    TextSimple          ( float x, float y, Colour const &col, float size, const char *text );
    void    TextCentreSimple    ( float x, float y, Colour const &col, float size, const char *text );
    void    TextRightSimple     ( float x, float y, Colour const &col, float size, const char *text );
    
    float   TextWidth           ( const char *text, float size );
    float   TextWidth           ( const char *text, unsigned int textLen, float size, BitmapFont *bitmapFont );

    //
    // Drawing primitives

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

    // Line strip rendering for continuous lines (replaces GL_LINE_STRIP)
    void    BeginLineStrip2D    ( Colour const &col, float lineWidth );
    void    LineStripVertex2D   ( float x, float y );
    void    EndLineStrip2D      ();

    // VBO caching system for high-performance rendering (replaces display lists)
    void    BeginCachedLineStrip( const char* cacheKey, Colour const &col, float lineWidth );
    void    CachedLineStripVertex( float x, float y );
    void    EndCachedLineStrip  ();
    void    RenderCachedLineStrip( const char* cacheKey );
    void    InvalidateCachedVBO ( const char* cacheKey );

    // Mega-VBO batching system for maximum performance (single draw calls)
    void    BeginMegaVBO        ( const char* megaVBOKey, Colour const &col, float lineWidth );
    void    AddLineStripToMegaVBO( float* vertices, int vertexCount );
    void    EndMegaVBO          ();
    void    RenderMegaVBO       ( const char* megaVBOKey );

    void    SetClip             ( int x, int y, int w, int h );      
    void    ResetClip           ();

    void    Blit                ( Image *src, float x, float y, Colour const &col);
    void    Blit                ( Image *src, float x, float y, float w, float h, Colour const &col);
    void    Blit                ( Image *src, float x, float y, float w, float h, Colour const &col, float angle);
    void    BlitChar            ( unsigned int textureID, float x, float y, float w, float h, 
                                  float texX, float texY, float texW, float texH, Colour const &col);
    
    // PHASE 6.1.1: Selective batching functions for safe performance optimization
    void    BlitBatched         ( Image *src, float x, float y, Colour const &col);
    void    BlitBatched         ( Image *src, float x, float y, float w, float h, Colour const &col);
    void    BlitBatched         ( Image *src, float x, float y, float w, float h, Colour const &col, float angle);

    // PERFORMANCE FIX: Optimized drawing functions using specialized buffers
    void    RectOptimized       ( float x, float y, float w, float h, Colour const &col, float lineWidth=1.0f );
    void    RectFillOptimized   ( float x, float y, float w, float h, Colour const &col );
    void    LineOptimized       ( float x1, float y1, float x2, float y2, Colour const &col, float lineWidth=1.0f );
    void    CircleOptimized     ( float x, float y, float radius, int numPoints, Colour const &col, float lineWidth=1.0f );
    void    CircleFillOptimized ( float x, float y, float radius, int numPoints, Colour const &col );

    // Batch management for UI rendering
    void    BeginUIBatch        ();
    void    EndUIBatch          ();  // Flushes UI buffers efficiently

    // Optimized text rendering (uses specialized text buffer)
    void    TextOptimized       ( float x, float y, Colour const &col, float size, const char *text );
    void    BeginTextBatch      ();
    void    EndTextBatch        ();

    // Optimized sprite rendering (uses specialized sprite buffer)
    void    BlitOptimized       ( Image *src, float x, float y, float w, float h, Colour const &col);
    void    BeginSpriteBatch    ();
    void    EndSpriteBatch      ();
    
    // UNIT RENDERING SYSTEM: High-performance specialized methods for complex unit rendering
    // These replace the legacy WorldObject::Render() pipeline to eliminate 9000+ draw calls
    
    // Unit trail/movement history rendering (replaces legacy RenderHistory())
    void    BeginUnitTrailBatch ();
    void    UnitTrailLine       ( float x1, float y1, float x2, float y2, Colour const &col );
    void    EndUnitTrailBatch   ();
    void    FlushUnitTrailsIfFull( int segmentsNeeded );
    
    // Main unit sprite rendering (replaces legacy main sprite Blit())
    void    BeginUnitMainBatch  ();  
    void    UnitMainSprite      ( Image *src, float x, float y, float w, float h, Colour const &col );
    void    EndUnitMainBatch    ();
    void    FlushUnitMainSpritesIfFull( int verticesNeeded );
    
    // Rotating sprite rendering (aircraft/nukes with rotation) - SEPARATE SYSTEM  
    void    BeginUnitRotatingBatch();
    void    UnitRotating        ( Image *src, float x, float y, float w, float h, Colour const &col, float angle );
    void    EndUnitRotatingBatch();
    
    // Unit selection/highlight rendering (replaces legacy highlight Blit())
    void    BeginUnitHighlightBatch();
    void    UnitHighlight       ( Image *blurSrc, float x, float y, float w, float h, Colour const &col );
    void    EndUnitHighlightBatch();
    
    // Unit state indicator rendering (fighters/bombers/nukes on carriers/silos/etc)
    void    BeginUnitStateBatch ();
    void    UnitStateIcon       ( Image *stateSrc, float x, float y, float w, float h, Colour const &col );
    void    EndUnitStateBatch   ();
    
    // Unit counter/text rendering (ammo counts, supply levels, etc)
    void    BeginUnitCounterBatch();
    void    UnitCounterText     ( float x, float y, Colour const &col, float size, const char *text );
    void    EndUnitCounterBatch ();
    
    // Unit nuke supply icon rendering (smallnuke.bmp)
    void    BeginUnitNukeBatch  ();
    void    UnitNukeIcon        ( float x, float y, float w, float h, Colour const &col );
    void    UnitNukeIcon        ( float x, float y, float w, float h, Colour const &col, float angle );
    void    EndUnitNukeBatch    ();
    
    // EFFECTS RENDERING SYSTEM: Optimized for explosions, gunfire, particles
    
    // Effect lines (gunfire trails, radar sweeps, etc)
    void    BeginEffectsLineBatch();
    void    EffectsLine         ( float x1, float y1, float x2, float y2, Colour const &col );
    void    EndEffectsLineBatch ();
    
    // Effect sprites (explosions, particles, etc)  
    void    BeginEffectsSpriteBatch();
    void    EffectsSprite       ( Image *src, float x, float y, float w, float h, Colour const &col );
    void    EndEffectsSpriteBatch();

protected:
};


extern Renderer *g_renderer;

#endif
