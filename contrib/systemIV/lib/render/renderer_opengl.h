/*
 * ========
 * RENDERER OPENGL
 * ========
 */

#ifndef _included_renderer_opengl_h
#define _included_renderer_opengl_h

#include "renderer.h"

#ifdef RENDERER_OPENGL

class RendererOpenGL : public Renderer
{
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
    float m_currentLineWidth;
    GLuint m_currentShaderProgram;
    GLuint m_currentVAO;
    GLuint m_currentArrayBuffer;
    GLuint m_currentElementBuffer;
    int m_currentTextureMagFilter;
    int m_currentTextureMinFilter;
    GLuint m_currentBoundTexture;
    
    bool m_scissorTestEnabled;
    bool m_cullFaceEnabled;
    GLenum m_cullFaceMode;
    
    bool m_blendEnabled;
    bool m_depthTestEnabled;
    bool m_depthMaskEnabled;
    int m_depthComparisonFunc;
    bool m_colorMaskR;
    bool m_colorMaskG;
    bool m_colorMaskB;
    bool m_colorMaskA;
    int m_currentBlendSrcFactor;
    int m_currentBlendDstFactor;

    GLenum ConvertTextureAddressMode(int mode);
    GLenum ConvertDepthComparisonFunc(int func);
    
    void Setup2DRenderState();
    void Setup3DRenderState();
    void CleanupRenderState();

public:
    RendererOpenGL();
    virtual ~RendererOpenGL();
    
    virtual void SetViewport        (int x, int y, int width, int height) override;
    virtual void SetActiveTexture   (unsigned int textureUnit)            override;
    virtual void SetShaderProgram   (unsigned int program)                override;
    virtual void SetVertexArray     (unsigned int vao)                    override;
    virtual void SetArrayBuffer     (unsigned int buffer)                 override;
    virtual void SetElementBuffer   (unsigned int buffer)                 override;
    virtual void SetLineWidth       (float width)                         override;
    virtual float GetLineWidth      () const                              override;
    virtual void SetBoundTexture    (unsigned int texture)                override;
    virtual void SetScissorTest     (bool enabled)                        override;
    virtual void SetScissor         (int x, int y, int width, int height) override;
    virtual void SetTextureParameter(unsigned int pname, int param)       override;
    
    virtual void SetBlendMode       (int blendMode)                       override;
    virtual void SetBlendFunc       (int srcFactor, int dstFactor)        override;
    virtual void SetDepthBuffer     (bool enabled, bool clearNow)         override;
    virtual void SetDepthMask       (bool enabled)                        override;
    virtual void SetDepthComparison (int comparisonFunc)                  override;
    virtual void SetCullFace        (bool enabled, int mode)              override;
    virtual void SetColorMask       (bool r, bool g, bool b, bool a)      override;
    virtual void SetClearColor      (float r, float g, float b, float a)  override;
    
    virtual unsigned int CreateShader(const char* vertexSource, const char* fragmentSource) override;
    
    virtual void InitializeAntiAliasing(AntiAliasingType type, int width, int height, int samples) override;
    virtual void ResizeAntiAliasing    (int width, int height)                override;
 
    virtual void DestroyAntiAliasing() override;
    virtual void BeginAntiAliasing    () override;
    virtual void EndAntiAliasing      () override;
    
    virtual void BeginFrame () override;
    virtual void EndFrame   () override;
    
    virtual void Begin2DRendering() override;
    virtual void End2DRendering() override;
    virtual void Begin3DRendering() override;
    virtual void End3DRendering() override;
    
    virtual void ClearScreen(bool colour, bool depth) override;
    
    virtual void HandleWindowResize()                 override;
    
    virtual void StartFlushTiming  (const char* name) override;
    virtual void EndFlushTiming    (const char* name) override;
    virtual void UpdateGpuTimings  () override;
    virtual void ResetFlushTimings () override;
    
    virtual void SaveScreenshot() override;
    
    virtual unsigned int CreateTexture(int width, int height, const Colour* pixels, int mipmapLevel) override;
    virtual void DeleteTexture        (unsigned int textureID) override;
    
    virtual unsigned int GetCurrentBoundTexture() const override { return m_currentBoundTexture; }
    virtual int GetCurrentBlendSrcFactor       () const override { return m_currentBlendSrcFactor; }
    virtual int GetCurrentBlendDstFactor       () const override { return m_currentBlendDstFactor; }
};

#endif // RENDERER_OPENGL
#endif // _included_renderer_opengl_h

