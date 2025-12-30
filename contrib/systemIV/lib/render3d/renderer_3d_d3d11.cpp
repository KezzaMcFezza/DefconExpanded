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

#endif // RENDERER_DIRECTX11

