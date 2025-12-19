/*
 * ========
 * RENDERER
 * ========
 * 
 * Main rendering coordinator and state manager
 * Coordinates 2D and 3D renderers with unified state management
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

class Renderer
{
public:
    static const int MAX_FONT_ATLASES = 4;  // bitlow, kremlin, lucon, zerothre

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

private:
  
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
    GLuint m_currentBoundTexture;
    
    bool m_scissorTestEnabled;
    
    int m_textureSwitches;
    int m_prevTextureSwitches;

    bool m_cullFaceEnabled;
    GLenum m_cullFaceMode;
 
public:
    Renderer();
    ~Renderer();

    int m_blendMode;
    int m_blendSrcFactor;
    int m_blendDstFactor;
    
    bool m_blendEnabled;
    bool m_depthTestEnabled;
    bool m_depthMaskEnabled;
  
    int m_currentBlendSrcFactor;
    int m_currentBlendDstFactor;

    enum {
        BlendModeDisabled,
        BlendModeNormal,
        BlendModeAdditive,
        BlendModeSubtractive
    };

    void SetBlendMode   (int _blendMode);
    void SetBlendFunc   (int srcFactor, int dstFactor);
    void SetDepthBuffer (bool _enabled, bool _clearNow);
    void SetDepthMask   (bool enabled);
    void SetCullFace    (bool enabled, GLenum mode = GL_BACK);

    void UpdateGpuTimings();
    void ResetFlushTimings();
    
    void SetViewport(int x, int y, int width, int height);
    void SetActiveTexture(GLenum texture);
    void SetShaderProgram(GLuint program);
    void SetVertexArray(GLuint vao);
    void SetArrayBuffer(GLuint buffer);
    void SetElementBuffer(GLuint buffer);
    void SetLineWidth(GLfloat width);
    void SetBoundTexture(GLuint texture);
    void SetScissorTest(bool enabled);
    void SetScissor(int x, int y, int width, int height);
    void SetTextureParameter(GLenum pname, GLint param);
    
    unsigned int CreateShader(const char* vertexSource, const char* fragmentSource);
    void GetImageUVCoords(Image* image, float& u1, float& v1, float& u2, float& v2);
    unsigned int GetEffectiveTextureID(Image* image);
    
    void BeginFrame();
    void EndFrame();

    void HandleWindowResize();
    
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
    const FlushTiming* GetFlushTimings(int& count) const;
    
    int GetTextureSwitches() const { return m_prevTextureSwitches; }
    GLuint GetCurrentBoundTexture() const { return m_currentBoundTexture; }

    BitmapFont *GetBitmapFont();
    void SaveScreenshot();
    char *ScreenshotsDirectory();

    void SetDefaultFont           (const char *font, const char *_langName = NULL);
    void SetFontSpacing           (const char *font, float _spacing);
    float GetFontSpacing          (const char *font);
    void SetFont                  (const char *font = NULL, bool horizFlip = false,
                                   bool negative = false, bool fixedWidth = false,
                                   const char *_langName = NULL);
  
    void SetFont                  (const char *font, const char *_langName);
    bool IsFontLanguageSpecific   ();
    
    const char* GetCurrentFontName() const { return m_currentFontName; }
    const char* GetCurrentFontFilename() const { return m_currentFontFilename; }
    const char* GetDefaultFontName() const { return m_defaultFontName; }
    const char* GetDefaultFontFilename() const { return m_defaultFontFilename; }
    bool GetHorizFlip() const { return m_horizFlip; }
    bool GetFixedWidth() const { return m_fixedWidth; }
    
    void BeginScene();
    void ClearScreen(bool _colour, bool _depth);
    
    GLuint m_msaaFBO;
    GLuint m_msaaColorRBO;
    GLuint m_msaaDepthRBO;
    
    bool m_msaaEnabled;
    int m_msaaSamples;
    int m_msaaWidth;
    int m_msaaHeight;
    
    void DestroyMSAAFramebuffer();
    void BeginMSAARendering();
    void EndMSAARendering();
    void InitializeMSAAFramebuffer(int width, int height, int samples);
    void ResizeMSAAFramebuffer(int width, int height);
};

extern Renderer* g_renderer;
extern Renderer2D* g_renderer2d;
extern Renderer3D* g_renderer3d;

#endif

