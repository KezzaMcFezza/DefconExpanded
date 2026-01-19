/*
 * ========
 * RENDERER
 * ========
 * 
 * Main rendering coordinator and state manager
 * Coordinates 2D/3D renderers and provides a 
 * unified state management interface for both.
 * Implements OpenGL and DirectX11 renderers.
 */

#ifndef _included_renderer_h
#define _included_renderer_h

#include "systemiv.h"
#include "lib/tosser/btree.h"
#include "lib/tosser/llist.h"

class Renderer2D;
class Renderer2DVBO;
class Renderer3D;
class Image;
class Bitmap;
class BitmapFont;
class AtlasImage;
struct AtlasCoord;
class RendererOpenGL;
class RendererD3D11;
class Renderer;
struct RendererFlushTiming;
class Colour;

enum TextureParameter {
    TEXTURE_MAG_FILTER = 0,
    TEXTURE_MIN_FILTER = 1,
    TEXTURE_WRAP_S = 2,
    TEXTURE_WRAP_T = 3
};

enum TextureFilter {
    TEXTURE_FILTER_NEAREST = 0,
    TEXTURE_FILTER_LINEAR = 1,
    TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST = 2,
    TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST = 3,
    TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR = 4,
    TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR = 5
};

enum CullFaceMode {
    CULL_FACE_BACK = 0,
    CULL_FACE_FRONT = 1,
    CULL_FACE_FRONT_AND_BACK = 2
};

enum BlendFactor {
    BLEND_ZERO = 0,
    BLEND_ONE = 1,
    BLEND_SRC_ALPHA = 2,
    BLEND_ONE_MINUS_SRC_ALPHA = 3,
    BLEND_ONE_MINUS_SRC_COLOR = 4
};

enum BufferUsageHint {
    BUFFER_USAGE_STATIC_DRAW = 0,   // Data set once, used many times
    BUFFER_USAGE_DYNAMIC_DRAW = 1,  // Data modified repeatedly, used many times
    BUFFER_USAGE_STREAM_DRAW = 2    // Data modified repeatedly, used a few times
};

enum PrimitiveType {
    PRIMITIVE_TRIANGLES = 0,
    PRIMITIVE_LINE_STRIP = 1,
    PRIMITIVE_LINES = 2
};

enum DrawCallType {
    DRAW_CALL_IMMEDIATE_TRIANGLES = 0,
    DRAW_CALL_IMMEDIATE_LINES,
    DRAW_CALL_IMMEDIATE_VERTICES_3D,
    DRAW_CALL_IMMEDIATE_TEXT,
    DRAW_CALL_IMMEDIATE_STATIC_SPRITES,
    DRAW_CALL_IMMEDIATE_ROTATING_SPRITES,
    DRAW_CALL_IMMEDIATE_CIRCLES,
    DRAW_CALL_IMMEDIATE_CIRCLE_FILLS,
    DRAW_CALL_IMMEDIATE_RECTS,
    DRAW_CALL_IMMEDIATE_RECT_FILLS,
    DRAW_CALL_IMMEDIATE_TRIANGLE_FILLS,
    DRAW_CALL_TEXT,
    DRAW_CALL_LINES,
    DRAW_CALL_STATIC_SPRITES,
    DRAW_CALL_ROTATING_SPRITES,
    DRAW_CALL_CIRCLES,
    DRAW_CALL_CIRCLE_FILLS,
    DRAW_CALL_RECTS,
    DRAW_CALL_RECT_FILLS,
    DRAW_CALL_TRIANGLE_FILLS,
    DRAW_CALL_LINE_VBO,
    DRAW_CALL_QUAD_VBO,
    DRAW_CALL_TRIANGLE_VBO
};

enum TextureAddressMode {
    TEXTURE_ADDRESS_CLAMP = 0,
    TEXTURE_ADDRESS_REPEAT = 1,
    TEXTURE_ADDRESS_MIRROR = 2,
    TEXTURE_ADDRESS_MIRROR_ONCE = 3,
    TEXTURE_ADDRESS_BORDER = 4
};

enum DepthComparisonFunc {
    DEPTH_COMPARISON_NEVER = 0,
    DEPTH_COMPARISON_LESS = 1,
    DEPTH_COMPARISON_EQUAL = 2,
    DEPTH_COMPARISON_LESS_EQUAL = 3,
    DEPTH_COMPARISON_GREATER = 4,
    DEPTH_COMPARISON_NOT_EQUAL = 5,
    DEPTH_COMPARISON_GREATER_EQUAL = 6,
    DEPTH_COMPARISON_ALWAYS = 7
};

enum BlendOperation {
    BLEND_OP_ADD = 0,
    BLEND_OP_SUBTRACT = 1,
    BLEND_OP_REV_SUBTRACT = 2,
    BLEND_OP_MIN = 3,
    BLEND_OP_MAX = 4
};

struct RendererFlushTiming {
    const char* name;
    double totalTime;
    double totalGpuTime;
    int callCount;
    unsigned int queryObject;
    bool queryPending;
    
    static const int MAX_QUERIES_PER_TYPE = 32;
    unsigned int queryPool[MAX_QUERIES_PER_TYPE];
    int queryPoolSize;
    int nextQueryIndex;
};

enum RenderPassType {
    RENDER_PASS_NONE = 0,
    RENDER_PASS_2D = 1,
    RENDER_PASS_3D = 2
};

class Renderer
{
public:
    static const int MAX_FONT_ATLASES = 4;  // bitlow, kremlin, lucon, zerothre
    static Renderer* Create();
    typedef RendererFlushTiming FlushTiming;

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
    
    int m_textureSwitches;
    int m_prevTextureSwitches;
    
    static const int MAX_FLUSH_TYPES = 50;
    
    FlushTiming m_flushTimings[MAX_FLUSH_TYPES];
    int m_flushTimingCount;
    double m_currentFlushStartTime;
    
    bool m_msaaEnabled;
    int m_msaaSamples;
    int m_msaaWidth;
    int m_msaaHeight;
    
    RenderPassType m_currentPass;
    
    float m_clearColorR;
    float m_clearColorG;
    float m_clearColorB;
    float m_clearColorA;

public:
    Renderer();
    virtual ~Renderer();

    enum {
        BlendModeDisabled,
        BlendModeNormal,
        BlendModeAdditive,
        BlendModeSubtractive
    };

    int m_blendMode;
    int m_blendSrcFactor;
    int m_blendDstFactor;
    
    virtual void SetViewport        (int x, int y, int width, int height) = 0;
    virtual void SetActiveTexture   (unsigned int textureUnit) = 0;
    virtual void SetShaderProgram   (unsigned int program) = 0;
    virtual void SetVertexArray     (unsigned int vao) = 0;
    virtual void SetArrayBuffer     (unsigned int buffer) = 0;
    virtual void SetElementBuffer   (unsigned int buffer) = 0;
    virtual void SetLineWidth       (float width) = 0;
    virtual float GetLineWidth      () const = 0;
    virtual void SetBoundTexture    (unsigned int texture) = 0;
    virtual void SetScissorTest     (bool enabled) = 0;
    virtual void SetScissor         (int x, int y, int width, int height) = 0;
    virtual void SetTextureParameter(unsigned int pname, int param) = 0;
    
    virtual unsigned int GetCurrentBoundTexture  () const = 0;
    virtual int          GetCurrentBlendSrcFactor() const = 0;
    virtual int          GetCurrentBlendDstFactor() const = 0;
    
    virtual void SetBlendMode       (int blendMode) = 0;
    virtual void SetBlendFunc       (int srcFactor, int dstFactor) = 0;
    virtual void SetDepthBuffer     (bool enabled, bool clearNow) = 0;
    virtual void SetDepthMask       (bool enabled) = 0;
    virtual void SetDepthComparison (int comparisonFunc) = 0;
    virtual void SetCullFace        (bool enabled, int mode) = 0;
    virtual void SetColorMask       (bool r, bool g, bool b, bool a) = 0;
    virtual void SetClearColor      (float r, float g, float b, float a = 1.0f) = 0;

    virtual unsigned int CreateShader(const char* vertexSource, const char* fragmentSource) = 0;
    
    virtual void InitializeMSAAFramebuffer(int width, int height, int samples) = 0;
    virtual void ResizeMSAAFramebuffer    (int width, int height) = 0;

    virtual void DestroyMSAAFramebuffer() = 0;
    virtual void BeginMSAARendering() = 0;
    virtual void EndMSAARendering() = 0;
    
    virtual void BeginFrame() = 0;
    virtual void EndFrame() = 0;
    
    virtual void Begin2DRendering() = 0;
    virtual void End2DRendering() = 0;
    virtual void Begin3DRendering() = 0;
    virtual void End3DRendering() = 0;

    virtual void ClearScreen       (bool colour, bool depth) = 0;
    virtual void HandleWindowResize() = 0;
    
    virtual void StartFlushTiming (const char* name) = 0;
    virtual void EndFlushTiming   (const char* name) = 0;
    virtual void UpdateGpuTimings () = 0;
    virtual void ResetFlushTimings() = 0;
    
    virtual void SaveScreenshot() = 0;
    
    virtual unsigned int CreateTexture(int width, int height, const Colour* pixels, int mipmapLevel) = 0;
    virtual void DeleteTexture        (unsigned int textureID) = 0;
    
    void GetImageUVCoords             (Image* image, float& u1, float& v1, float& u2, float& v2);
    unsigned int GetEffectiveTextureID(Image* image);
    
    BitmapFont *GetBitmapFont();

    void SetDefaultFont   (const char *font, const char *_langName = NULL);
    void SetFontSpacing   (const char *font, float _spacing);
    float GetFontSpacing  (const char *font);

    void SetFont(const char *font = NULL, bool horizFlip = false,
                 bool negative = false, bool fixedWidth = false,
                 const char *_langName = NULL);

    void SetFont(const char *font, const char *_langName);
    bool IsFontLanguageSpecific();
    
    const char* GetCurrentFontName    () const { return m_currentFontName; }
    const char* GetCurrentFontFilename() const { return m_currentFontFilename; }
    const char* GetDefaultFontName    () const { return m_defaultFontName; }
    const char* GetDefaultFontFilename() const { return m_defaultFontFilename; }
    
    bool GetHorizFlip   () const { return m_horizFlip; }
    bool GetFixedWidth  () const { return m_fixedWidth; }
    
    char *ScreenshotsDirectory();
    
    const FlushTiming* GetFlushTimings(int& count) const;
    int GetTextureSwitches() const { return m_prevTextureSwitches; }
    
    int GetBlendMode  () const { return m_blendMode; }
    
    bool IsMSAAEnabled() const { return m_msaaEnabled; }
    int GetMSAASamples() const { return m_msaaSamples; }
};

extern Renderer* g_renderer;
extern Renderer2D* g_renderer2d;
extern Renderer3D* g_renderer3d;

#endif

