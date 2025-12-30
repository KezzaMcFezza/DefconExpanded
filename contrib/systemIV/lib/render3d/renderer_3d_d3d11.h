#ifndef _included_renderer3d_d3d11_h
#define _included_renderer3d_d3d11_h

#ifdef RENDERER_DIRECTX11

#include "renderer_3d.h"

class Renderer3DD3D11 : public Renderer3D
{
public:
    Renderer3DD3D11();
    virtual ~Renderer3DD3D11();
    
protected:
    virtual void Initialize3DShaders          () override;
    virtual void Cache3DUniformLocations      () override;
    virtual void Setup3DVertexArrays          () override;
    virtual void Setup3DTexturedVertexArrays  () override;
    virtual void Set3DShaderUniforms          () override;
    virtual void SetTextured3DShaderUniforms  () override;

    virtual void SetFogUniforms3D                 (unsigned int shaderProgram) override;
    virtual void Set3DModelShaderUniforms         (const Matrix4f& modelMatrix, const Colour& modelColor) override;
    virtual void Set3DModelShaderUniformsInstanced(const Matrix4f* modelMatrices, const Colour* modelColors, 
                                                   int instanceCount) override;
    
    virtual void UploadVertexDataTo3DVBO      (unsigned int vbo, const Vertex3D* vertices, 
                                               int vertexCount, unsigned int usageHint) override;
    virtual void UploadVertexDataTo3DVBO      (unsigned int vbo, const Vertex3DTextured* vertices, 
                                               int vertexCount, unsigned int usageHint) override;
    
    virtual void Flush3DVertices              (unsigned int primitiveType) override;
    virtual void Flush3DTexturedVertices      () override;
    virtual void FlushLine3D                  () override;
    virtual void FlushStaticSprites3D         () override;
    virtual void FlushRotatingSprite3D        () override;
    virtual void FlushTextBuffer3D            () override;
    virtual void FlushCircles3D               () override;
    virtual void FlushCircleFills3D           () override;
    virtual void FlushRects3D                 () override;
    virtual void FlushRectFills3D             () override;
    virtual void FlushTriangleFills3D         () override;
    virtual void CleanupBuffers3D             () override;
};

#endif // RENDERER_DIRECTX11
#endif // _included_renderer3d_directx11_h

