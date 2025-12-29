#ifdef RENDERER_DIRECTX11

#include "systemiv.h"

#include "lib/render2d/renderer_2d.h"
#include "lib/render3d/renderer_3d.h"
#include "lib/render2d/megavbo/megavbo_2d.h"
#include "lib/render3d/megavbo/megavbo_3d.h"

#include "renderer.h"
#include "renderer_d3d11.h"

RendererD3D11::RendererD3D11()
{
}

RendererD3D11::~RendererD3D11()
{
}

void RendererD3D11::SetViewport(int x, int y, int width, int height)
{
}

void RendererD3D11::SetActiveTexture(unsigned int textureUnit)
{
}

void RendererD3D11::SetShaderProgram(unsigned int program)
{
}

void RendererD3D11::SetVertexArray(unsigned int vao)
{
}

void RendererD3D11::SetArrayBuffer(unsigned int buffer)
{
}

void RendererD3D11::SetElementBuffer(unsigned int buffer)
{
}

void RendererD3D11::SetLineWidth(float width)
{
}

void RendererD3D11::SetBoundTexture(unsigned int texture)
{
}

void RendererD3D11::SetScissorTest(bool enabled)
{
}

void RendererD3D11::SetScissor(int x, int y, int width, int height)
{
}

void RendererD3D11::SetTextureParameter(unsigned int pname, int param)
{
}

void RendererD3D11::SetBlendMode(int blendMode)
{
}

void RendererD3D11::SetBlendFunc(int srcFactor, int dstFactor)
{
}

void RendererD3D11::SetDepthBuffer(bool enabled, bool clearNow)
{
}

void RendererD3D11::SetDepthMask(bool enabled)
{
}

void RendererD3D11::SetCullFace(bool enabled, int mode)
{
}

unsigned int RendererD3D11::CreateShader(const char* vertexSource, const char* fragmentSource)
{
    return 0;
}

void RendererD3D11::InitializeMSAAFramebuffer(int width, int height, int samples)
{
}

void RendererD3D11::ResizeMSAAFramebuffer(int width, int height)
{
}

void RendererD3D11::DestroyMSAAFramebuffer()
{
}

void RendererD3D11::BeginMSAARendering()
{
}

void RendererD3D11::EndMSAARendering()
{
}

void RendererD3D11::BeginFrame()
{
}

void RendererD3D11::EndFrame()
{
}

void RendererD3D11::BeginScene()
{
}

void RendererD3D11::ClearScreen(bool colour, bool depth)
{
}

void RendererD3D11::HandleWindowResize()
{
}

void RendererD3D11::StartFlushTiming(const char* name)
{
}

void RendererD3D11::EndFlushTiming(const char* name)
{
}

void RendererD3D11::UpdateGpuTimings()
{
}

void RendererD3D11::ResetFlushTimings()
{
}

void RendererD3D11::SaveScreenshot()
{
}

int RendererD3D11::GetCurrentBlendSrcFactor() const
{
    return 0;
}

int RendererD3D11::GetCurrentBlendDstFactor() const
{
    return 0;
}

void RendererD3D11::GetDeviceAndContext()
{
}

void RendererD3D11::CreateStateObjects()
{
}

void RendererD3D11::ReleaseStateObjects()
{
}

#endif // RENDERER_DIRECTX11
