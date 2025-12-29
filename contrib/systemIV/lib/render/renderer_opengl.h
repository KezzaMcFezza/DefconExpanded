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
    int m_currentBlendSrcFactor;
    int m_currentBlendDstFactor;

    GLuint m_msaaFBO;
    GLuint m_msaaColorRBO;
    GLuint m_msaaDepthRBO;

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
    virtual void SetBoundTexture    (unsigned int texture)                override;
    virtual void SetScissorTest     (bool enabled)                        override;
    virtual void SetScissor         (int x, int y, int width, int height) override;
    virtual void SetTextureParameter(unsigned int pname, int param)       override;
    
    virtual void SetBlendMode       (int blendMode)                       override;
    virtual void SetBlendFunc       (int srcFactor, int dstFactor)        override;
    virtual void SetDepthBuffer     (bool enabled, bool clearNow)         override;
    virtual void SetDepthMask       (bool enabled)                        override;
    virtual void SetCullFace        (bool enabled, int mode)              override;
    
    virtual unsigned int CreateShader(const char* vertexSource, const char* fragmentSource) override;
    
    virtual void InitializeMSAAFramebuffer(int width, int height, int samples) override;
    virtual void ResizeMSAAFramebuffer    (int width, int height)              override;

    virtual void DestroyMSAAFramebuffer() override;
    virtual void BeginMSAARendering    () override;
    virtual void EndMSAARendering      () override;
    
    virtual void BeginFrame () override;
    virtual void EndFrame   () override;
    virtual void BeginScene () override;
    virtual void ClearScreen(bool colour, bool depth) override;
    
    virtual void HandleWindowResize()                 override;
    
    virtual void StartFlushTiming  (const char* name) override;
    virtual void EndFlushTiming    (const char* name) override;
    virtual void UpdateGpuTimings  () override;
    virtual void ResetFlushTimings () override;
    
    virtual void SaveScreenshot() override;
    
    virtual unsigned int GetCurrentBoundTexture() const override { return m_currentBoundTexture; }
    virtual int GetCurrentBlendSrcFactor       () const override { return m_currentBlendSrcFactor; }
    virtual int GetCurrentBlendDstFactor       () const override { return m_currentBlendDstFactor; }
};

#endif // RENDERER_OPENGL
#endif // _included_renderer_opengl_h

