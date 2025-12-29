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
class BitmapFont;
class AtlasImage;
struct AtlasCoord;
class RendererOpenGL;
class RendererD3D11;
class Renderer;
struct RendererFlushTiming;

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

struct RendererFlushTiming {
    const char* name;
    double totalTime;
    double totalGpuTime;
    int callCount;
    unsigned int queryObject;
    bool queryPending;
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
    virtual void SetBoundTexture    (unsigned int texture) = 0;
    virtual void SetScissorTest     (bool enabled) = 0;
    virtual void SetScissor         (int x, int y, int width, int height) = 0;
    virtual void SetTextureParameter(unsigned int pname, int param) = 0;
    
    virtual unsigned int GetCurrentBoundTexture  () const = 0;
    virtual int          GetCurrentBlendSrcFactor() const = 0;
    virtual int          GetCurrentBlendDstFactor() const = 0;
    
    virtual void SetBlendMode   (int blendMode) = 0;
    virtual void SetBlendFunc   (int srcFactor, int dstFactor) = 0;
    virtual void SetDepthBuffer (bool enabled, bool clearNow) = 0;
    virtual void SetDepthMask   (bool enabled) = 0;
    virtual void SetCullFace    (bool enabled, int mode) = 0;

    virtual unsigned int CreateShader(const char* vertexSource, const char* fragmentSource) = 0;
    
    virtual void InitializeMSAAFramebuffer(int width, int height, int samples) = 0;
    virtual void ResizeMSAAFramebuffer    (int width, int height) = 0;

    virtual void DestroyMSAAFramebuffer() = 0;
    virtual void BeginMSAARendering() = 0;
    virtual void EndMSAARendering() = 0;
    
    virtual void BeginFrame() = 0;
    virtual void EndFrame() = 0;
    virtual void BeginScene() = 0;

    virtual void ClearScreen       (bool colour, bool depth) = 0;
    virtual void HandleWindowResize() = 0;
    
    virtual void StartFlushTiming (const char* name) = 0;
    virtual void EndFlushTiming   (const char* name) = 0;
    virtual void UpdateGpuTimings () = 0;
    virtual void ResetFlushTimings() = 0;
    
    virtual void SaveScreenshot() = 0;
    
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

