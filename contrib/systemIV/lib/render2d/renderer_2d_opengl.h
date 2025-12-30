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
    
    virtual unsigned int CreateMegaVBOVertexBuffer (size_t size, BufferUsageHint usageHint) override;
    virtual unsigned int CreateMegaVBOIndexBuffer  (size_t size, BufferUsageHint usageHint) override;
    virtual unsigned int CreateMegaVBOVertexArray  () override;
 
    virtual void DeleteMegaVBOVertexBuffer (unsigned int buffer) override;
    virtual void DeleteMegaVBOIndexBuffer  (unsigned int buffer) override;
    virtual void DeleteMegaVBOVertexArray  (unsigned int vao)    override;
 
    virtual void SetupMegaVBOVertexAttributes2D(unsigned int vao, unsigned int vbo, unsigned int ibo) override;
 
    virtual void UploadMegaVBOIndexData (unsigned int ibo, const unsigned int* indices, 
                                         int indexCount, BufferUsageHint usageHint) override;
    virtual void UploadMegaVBOVertexData(unsigned int vbo, const Vertex2D* vertices,
                                         int vertexCount, BufferUsageHint usageHint) override;
 
    virtual void DrawMegaVBOIndexed            (PrimitiveType primitiveType, unsigned int indexCount) override;
    virtual void EnableMegaVBOPrimitiveRestart (unsigned int restartIndex) override;
    virtual void DisableMegaVBOPrimitiveRestart() override;
};

#endif // RENDERER_OPENGL
#endif // _included_renderer2d_opengl_h

