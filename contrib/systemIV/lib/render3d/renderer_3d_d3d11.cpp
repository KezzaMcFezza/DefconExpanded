#ifdef RENDERER_DIRECTX11

#include "systemiv.h"
#include "renderer_3d_d3d11.h"  
#include "lib/render/renderer.h"
#include "lib/gucci/window_manager.h"

Renderer3DD3D11::Renderer3DD3D11()
{

}

Renderer3DD3D11::~Renderer3DD3D11()
{

}

void Renderer3DD3D11::Initialize3DShaders()
{

}

void Renderer3DD3D11::Cache3DUniformLocations()
{

}

void Renderer3DD3D11::Setup3DVertexArrays()
{

}

void Renderer3DD3D11::Setup3DTexturedVertexArrays()
{

}

void Renderer3DD3D11::Set3DShaderUniforms()
{

}

void Renderer3DD3D11::SetTextured3DShaderUniforms()
{

}

void Renderer3DD3D11::SetFogUniforms3D(unsigned int shaderProgram)
{

}

void Renderer3DD3D11::Set3DModelShaderUniforms(const Matrix4f& modelMatrix, const Colour& modelColor)
{

}

void Renderer3DD3D11::Set3DModelShaderUniformsInstanced(const Matrix4f* modelMatrices, const Colour* modelColors, 
                                                            int instanceCount)
{

}

void Renderer3DD3D11::UploadVertexDataTo3DVBO(unsigned int vbo, const Vertex3D* vertices, 
                                                  int vertexCount, unsigned int usageHint)
{

}

void Renderer3DD3D11::UploadVertexDataTo3DVBO(unsigned int vbo, const Vertex3DTextured* vertices, 
                                                  int vertexCount, unsigned int usageHint)
{

}

void Renderer3DD3D11::Flush3DVertices(unsigned int primitiveType)
{
    m_vertex3DCount = 0;
}

void Renderer3DD3D11::Flush3DTexturedVertices()
{
    m_vertex3DTexturedCount = 0;
}

void Renderer3DD3D11::FlushLine3D()
{
    m_lineVertexCount3D = 0;
}

void Renderer3DD3D11::FlushStaticSprites3D()
{
    m_staticSpriteVertexCount3D = 0;
}

void Renderer3DD3D11::FlushRotatingSprite3D()
{
    m_rotatingSpriteVertexCount3D = 0;
}

void Renderer3DD3D11::FlushTextBuffer3D()
{
    m_textVertexCount3D = 0;
}

void Renderer3DD3D11::FlushCircles3D()
{
    m_circleVertexCount3D = 0;
}

void Renderer3DD3D11::FlushCircleFills3D()
{
    m_circleFillVertexCount3D = 0;
}

void Renderer3DD3D11::FlushRects3D()
{
    m_rectVertexCount3D = 0;
}

void Renderer3DD3D11::FlushRectFills3D()
{
    m_rectFillVertexCount3D = 0;
}

void Renderer3DD3D11::FlushTriangleFills3D()
{
    m_triangleFillVertexCount3D = 0;
}

void Renderer3DD3D11::CleanupBuffers3D()
{

}

unsigned int Renderer3DD3D11::CreateMegaVBOVertexBuffer3D(size_t size, BufferUsageHint usageHint)
{
    (void)size;
    (void)usageHint;
    return 0;
}

unsigned int Renderer3DD3D11::CreateMegaVBOIndexBuffer3D(size_t size, BufferUsageHint usageHint)
{
    (void)size;
    (void)usageHint;
    return 0;
}

unsigned int Renderer3DD3D11::CreateMegaVBOVertexArray3D()
{
    return 0;
}

void Renderer3DD3D11::DeleteMegaVBOVertexBuffer3D(unsigned int buffer)
{
    (void)buffer;
}

void Renderer3DD3D11::DeleteMegaVBOIndexBuffer3D(unsigned int buffer)
{
    (void)buffer;
}

void Renderer3DD3D11::DeleteMegaVBOVertexArray3D(unsigned int vao)
{
    (void)vao;
}

void Renderer3DD3D11::SetupMegaVBOVertexAttributes3D(unsigned int vao, unsigned int vbo, unsigned int ibo)
{
    (void)vao;
    (void)vbo;
    (void)ibo;
}

void Renderer3DD3D11::SetupMegaVBOVertexAttributes3DTextured(unsigned int vao, unsigned int vbo, unsigned int ibo)
{
    (void)vao;
    (void)vbo;
    (void)ibo;
}

void Renderer3DD3D11::UploadMegaVBOIndexData3D(unsigned int ibo, const unsigned int* indices, 
                                               int indexCount, BufferUsageHint usageHint)
{
    (void)ibo;
    (void)indices;
    (void)indexCount;
    (void)usageHint;
}

void Renderer3DD3D11::UploadMegaVBOVertexData3D(unsigned int vbo, const Vertex3D* vertices,
                                                 int vertexCount, BufferUsageHint usageHint)
{
    (void)vbo;
    (void)vertices;
    (void)vertexCount;
    (void)usageHint;
}

void Renderer3DD3D11::UploadMegaVBOVertexData3DTextured(unsigned int vbo, const Vertex3DTextured* vertices,
                                                         int vertexCount, BufferUsageHint usageHint)
{
    (void)vbo;
    (void)vertices;
    (void)vertexCount;
    (void)usageHint;
}

void Renderer3DD3D11::DrawMegaVBOIndexed3D(PrimitiveType primitiveType, unsigned int indexCount)
{
    (void)primitiveType;
    (void)indexCount;
}

void Renderer3DD3D11::DrawMegaVBOIndexedInstanced3D(PrimitiveType primitiveType, unsigned int indexCount, unsigned int instanceCount)
{
    (void)primitiveType;
    (void)indexCount;
    (void)instanceCount;
}

void Renderer3DD3D11::SetupMegaVBOInstancedVertexAttributes3DTextured(unsigned int vao, unsigned int vbo, unsigned int ibo)
{
    (void)vao;
    (void)vbo;
    (void)ibo;
}

void Renderer3DD3D11::UpdateCurrentVAO(unsigned int vao)
{
    (void)vao;
}

void Renderer3DD3D11::EnableMegaVBOPrimitiveRestart3D(unsigned int restartIndex)
{
    (void)restartIndex;
}

void Renderer3DD3D11::DisableMegaVBOPrimitiveRestart3D()
{
}

#endif // RENDERER_DIRECTX11

