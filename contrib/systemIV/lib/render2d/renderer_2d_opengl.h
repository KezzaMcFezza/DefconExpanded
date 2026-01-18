#ifndef _included_renderer2d_opengl_h
#define _included_renderer2d_opengl_h

#ifdef RENDERER_OPENGL

#include "renderer_2d.h"

class Renderer2DOpenGL : public Renderer2D
{
public:
    Renderer2DOpenGL();
    virtual ~Renderer2DOpenGL();
    
    void InvalidateStateCache();
    
protected:
    virtual void InitializeShaders       () override;
    virtual void CacheUniformLocations   () override;
    virtual void SetColorShaderUniforms  () override;
    virtual void SetLineShaderUniforms   (float lineWidth) override;
    virtual void SetTextureShaderUniforms() override;
    
    virtual void SetupVertexArrays       () override;
    virtual void UploadVertexData        (const Vertex2D* vertices, int vertexCount) override;
    virtual void UploadVertexDataToVBO   (unsigned int vbo, const Vertex2D* vertices, 
                                         int vertexCount, unsigned int usageHint)    override;
    
    virtual void FlushTextBuffer         (bool isImmediate) override;
    virtual void FlushLines              (bool isImmediate) override;
    virtual void FlushStaticSprites      (bool isImmediate) override;
    virtual void FlushRotatingSprite     (bool isImmediate) override;
    virtual void FlushCircles            (bool isImmediate) override;
    virtual void FlushCircleFills        (bool isImmediate) override;
    virtual void FlushRects              (bool isImmediate) override;
    virtual void FlushRectFills          (bool isImmediate) override;
    virtual void FlushTriangleFills      (bool isImmediate) override;
    
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

