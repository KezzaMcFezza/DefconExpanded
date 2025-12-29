#ifndef _included_renderer2d_directx11_h
#define _included_renderer2d_directx11_h

#ifdef RENDERER_DIRECTX11

#include "renderer_2d.h"

class Renderer2DDirectX11 : public Renderer2D
{
public:
    Renderer2DDirectX11();
    virtual ~Renderer2DDirectX11();
    
protected:
    virtual void InitializeShaders       () override;
    virtual void CacheUniformLocations   () override;
    virtual void SetColorShaderUniforms  () override;
    virtual void SetTextureShaderUniforms() override;
    
    virtual void SetupVertexArrays       () override;
    virtual void UploadVertexData        (const Vertex2D* vertices, int vertexCount) override;
    virtual void UploadVertexDataToVBO   (unsigned int vbo, const Vertex2D* vertices, 
                                          int vertexCount, unsigned int usageHint)   override;
    
    virtual void FlushTriangles          (bool useTexture) override;
    virtual void FlushTextBuffer         () override;
    virtual void FlushLines              () override;
    virtual void FlushStaticSprites      () override;
    virtual void FlushRotatingSprite     () override;
    virtual void FlushCircles            () override;
    virtual void FlushCircleFills        () override;
    virtual void FlushRects              () override;
    virtual void FlushRectFills          () override;
    virtual void FlushTriangleFills      () override;
    
    virtual void CleanupBuffers          () override;
};

#endif // RENDERER_DIRECTX11
#endif // _included_renderer2d_directx11_h

