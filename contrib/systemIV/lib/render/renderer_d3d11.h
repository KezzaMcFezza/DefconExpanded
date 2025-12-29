/*
 * ========
 * RENDERER DIRECTX11
 * ========
 */

#ifndef _included_renderer_d3d11_h
#define _included_renderer_d3d11_h

#include "renderer.h"

#ifdef RENDERER_DIRECTX11

#include <d3d11.h>

class RendererD3D11 : public Renderer
{
private:

    unsigned int m_currentBoundTexture;

public:
    RendererD3D11();
    virtual ~RendererD3D11();
    
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
    
    virtual void SetBlendMode   (int blendMode)                override;
    virtual void SetBlendFunc   (int srcFactor, int dstFactor) override;
    virtual void SetDepthBuffer (bool enabled, bool clearNow)  override;
    virtual void SetDepthMask   (bool enabled)                 override;
    virtual void SetCullFace    (bool enabled, int mode)       override;
    
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
    
    virtual void HandleWindowResize() override;
    
    virtual void StartFlushTiming (const char* name) override;
    virtual void EndFlushTiming   (const char* name) override;
    virtual void UpdateGpuTimings () override;
    virtual void ResetFlushTimings() override;
    
    virtual void SaveScreenshot() override;
    
    virtual unsigned int GetCurrentBoundTexture() const override { return m_currentBoundTexture; }
    virtual int GetCurrentBlendSrcFactor       () const override;
    virtual int GetCurrentBlendDstFactor       () const override;
    
private:
    void CreateStateObjects();
    void ReleaseStateObjects();
    void GetDeviceAndContext();
};

#endif // RENDERER_DIRECTX11
#endif // _included_renderer_d3d11_h
