#ifndef _included_renderer2d_opengl_h
#define _included_renderer2d_opengl_h

#ifdef RENDERER_OPENGL

#include "renderer_2d.h"

class Renderer2DOpenGL : public Renderer2D
{
public:
    Renderer2DOpenGL();
    virtual ~Renderer2DOpenGL();
    
protected:
    virtual void InitializeShaders       () override;
    virtual void CacheUniformLocations   () override;
    virtual void SetColorShaderUniforms  () override;
    virtual void SetTextureShaderUniforms() override;
    
    virtual void SetupVertexArrays       () override;
    virtual void UploadVertexData        (const Vertex2D* vertices, int vertexCount) override;
    virtual void UploadVertexDataToVBO   (unsigned int vbo, const Vertex2D* vertices, 
                                         int vertexCount, unsigned int usageHint)    override;
    
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

#endif // RENDERER_OPENGL
#endif // _included_renderer2d_opengl_h

