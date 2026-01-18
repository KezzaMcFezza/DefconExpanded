#ifndef _included_renderer3d_opengl_h
#define _included_renderer3d_opengl_h

#ifdef RENDERER_OPENGL

#include "renderer_3d.h"

class Renderer3DOpenGL : public Renderer3D
{
public:
    Renderer3DOpenGL();
    virtual ~Renderer3DOpenGL();
    
protected:
    virtual void Initialize3DShaders          () override;
    virtual void Cache3DUniformLocations      () override;
    virtual void Set3DShaderUniforms          () override;
    virtual void SetTextured3DShaderUniforms  () override;

    virtual void SetFogUniforms3D                 (unsigned int shaderProgram) override;
    virtual void Set3DModelShaderUniforms         (const Matrix4f& modelMatrix, const Colour& modelColor) override;
    virtual void Set3DModelShaderUniformsInstanced(const Matrix4f* modelMatrices, const Colour* modelColors, 
                                                   int instanceCount) override;
    
    virtual void Setup3DVertexArrays          () override;
    virtual void Setup3DTexturedVertexArrays  () override;

    virtual void UploadVertexDataTo3DVBO      (unsigned int vbo, const Vertex3D* vertices, 
                                               int vertexCount, unsigned int usageHint) override;
    virtual void UploadVertexDataTo3DVBO      (unsigned int vbo, const Vertex3DTextured* vertices, 
                                               int vertexCount, unsigned int usageHint) override;
    
    virtual void FlushLine3D                  (bool isImmediate) override;
    virtual void FlushStaticSprites3D         (bool isImmediate) override;
    virtual void FlushRotatingSprite3D        (bool isImmediate) override;
    virtual void FlushTextBuffer3D            (bool isImmediate) override;
    virtual void FlushCircles3D               (bool isImmediate) override;
    virtual void FlushCircleFills3D           (bool isImmediate) override;
    virtual void FlushRects3D                 (bool isImmediate) override;
    virtual void FlushRectFills3D             (bool isImmediate) override;
    virtual void FlushTriangleFills3D         (bool isImmediate) override;
    
    virtual void CleanupBuffers3D             () override;
    
    virtual unsigned int CreateMegaVBOVertexBuffer3D (size_t size, BufferUsageHint usageHint) override;
    virtual unsigned int CreateMegaVBOIndexBuffer3D  (size_t size, BufferUsageHint usageHint) override;
    virtual unsigned int CreateMegaVBOVertexArray3D  () override;
    
    virtual void DeleteMegaVBOVertexBuffer3D  (unsigned int buffer) override;
    virtual void DeleteMegaVBOIndexBuffer3D   (unsigned int buffer) override;
    virtual void DeleteMegaVBOVertexArray3D   (unsigned int vao) override;
    
    virtual void SetupMegaVBOVertexAttributes3D        (unsigned int vao, unsigned int vbo, unsigned int ibo) override;
    virtual void SetupMegaVBOVertexAttributes3DTextured(unsigned int vao, unsigned int vbo, unsigned int ibo) override;
    
    virtual void UploadMegaVBOIndexData3D (unsigned int ibo, const unsigned int* indices, 
                                           int indexCount, BufferUsageHint usageHint) override;
    virtual void UploadMegaVBOVertexData3D(unsigned int vbo, const Vertex3D* vertices,
                                           int vertexCount, BufferUsageHint usageHint) override;

    virtual void UploadMegaVBOVertexData3DTextured(unsigned int vbo, const Vertex3DTextured* vertices,
                                                   int vertexCount, BufferUsageHint usageHint) override;
    
    virtual void DrawMegaVBOIndexed3D          (PrimitiveType primitiveType, unsigned int indexCount) override;
    virtual void DrawMegaVBOIndexedInstanced3D (PrimitiveType primitiveType, unsigned int indexCount, 
                                                unsigned int instanceCount) override;
    
    virtual void SetupMegaVBOInstancedVertexAttributes3DTextured(unsigned int vao, unsigned int vbo, 
                                                                 unsigned int ibo) override;
    
    virtual void EnableMegaVBOPrimitiveRestart3D (unsigned int restartIndex) override;
    virtual void DisableMegaVBOPrimitiveRestart3D() override;
};

#endif // RENDERER_OPENGL
#endif // _included_renderer3d_opengl_h

