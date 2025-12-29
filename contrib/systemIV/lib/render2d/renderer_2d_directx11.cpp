#ifdef RENDERER_DIRECTX11

#include "systemiv.h"
#include "renderer_2d_directx11.h"
#include "lib/render/renderer.h"
#include "lib/gucci/window_manager.h"
#include "lib/preferences.h"

Renderer2DDirectX11::Renderer2DDirectX11()
{

}

Renderer2DDirectX11::~Renderer2DDirectX11()
{

}

void Renderer2DDirectX11::InitializeShaders()
{

}

void Renderer2DDirectX11::CacheUniformLocations()
{

}

void Renderer2DDirectX11::SetColorShaderUniforms()
{

}

void Renderer2DDirectX11::SetTextureShaderUniforms()
{

}

void Renderer2DDirectX11::SetupVertexArrays()
{

}

void Renderer2DDirectX11::UploadVertexData(const Vertex2D* vertices, int vertexCount)
{

}

void Renderer2DDirectX11::UploadVertexDataToVBO(unsigned int vbo, const Vertex2D* vertices,
                                                 int vertexCount, unsigned int usageHint)
{

}


void Renderer2DDirectX11::FlushTriangles(bool useTexture)
{
    m_triangleVertexCount = 0;
}

void Renderer2DDirectX11::FlushTextBuffer()
{

    m_textVertexCount = 0;
}

void Renderer2DDirectX11::FlushLines()
{
    m_lineVertexCount = 0;
}

void Renderer2DDirectX11::FlushStaticSprites()
{
    m_staticSpriteVertexCount = 0;
}

void Renderer2DDirectX11::FlushRotatingSprite()
{
    m_rotatingSpriteVertexCount = 0;
}

void Renderer2DDirectX11::FlushCircles()
{
    m_circleVertexCount = 0;
}

void Renderer2DDirectX11::FlushCircleFills()
{
    m_circleFillVertexCount = 0;
}

void Renderer2DDirectX11::FlushRects()
{
    m_rectVertexCount = 0;
}

void Renderer2DDirectX11::FlushRectFills()
{
    m_rectFillVertexCount = 0;
}

void Renderer2DDirectX11::FlushTriangleFills()
{
    m_triangleFillVertexCount = 0;
}

void Renderer2DDirectX11::CleanupBuffers()
{

}

#endif // RENDERER_DIRECTX11

