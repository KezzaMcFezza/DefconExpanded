#ifdef RENDERER_DIRECTX11

#include "renderer_2d_d3d11.h"
#include "lib/render/renderer.h"
#include "lib/render/renderer_d3d11.h"
#include "lib/gucci/window_manager.h"
#include "lib/gucci/window_manager_d3d11.h"
#include "lib/preferences.h"
#include "lib/debug_utils.h"
#include "shaders/vertex.hlsl.h"
#include "shaders/color_fragment.hlsl.h"
#include "shaders/texture_fragment.hlsl.h"
#include "shaders/line_geometry.hlsl.h"

#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

// ============================================================================
// CONSTRUCTOR & DESTRUCTOR
// ============================================================================

Renderer2DD3D11::Renderer2DD3D11()
    : m_device(nullptr),
      m_deviceContext(nullptr),
      m_colorVertexShader(nullptr),
      m_colorPixelShader(nullptr),
      m_textureVertexShader(nullptr),
      m_texturePixelShader(nullptr),
      m_inputLayout(nullptr),
      m_lineGeometryShaderThin(nullptr),
      m_lineGeometryShaderThick(nullptr),
      m_lineWidthConstantBuffer(nullptr),
      m_lineShaderProgram(0),
      m_currentlyBoundVS(nullptr),
      m_currentlyBoundPS(nullptr),
      m_currentlyBoundGS(nullptr),
      m_currentlyBoundInputLayout(nullptr),
      m_matricesNeedUpdate(true),
      m_batchedConstantBufferCount(0),
      m_supportsConstantBufferOffsets(false),
      m_transformConstantBuffer(nullptr),
      m_batchedConstantBuffer(nullptr),
      m_lineBuffer(nullptr),
      m_staticSpriteBuffer(nullptr),
      m_rotatingSpriteBuffer(nullptr),
      m_textBuffer(nullptr),
      m_circleBuffer(nullptr),
      m_circleFillBuffer(nullptr),
      m_rectBuffer(nullptr),
      m_rectFillBuffer(nullptr),
      m_triangleFillBuffer(nullptr),
      m_immediateBuffer(nullptr),
      m_currentTextureSRV(nullptr),
      m_currentTextureID(0),
      m_nextVBOId(1),
      m_currentVAO(0),
      m_primitiveRestartEnabled(false),
      m_primitiveRestartIndex(0)
{
    //
    // Initialize last matrices to invalid values to force first update
    
    for (int i = 0; i < 16; i++) 
    {
        m_lastProjectionMatrix[i] = -999999.0f;
        m_lastModelViewMatrix[i] = -999999.0f;
    }
    
    GetDeviceAndContext();
    InitializeShaders();
    CacheUniformLocations();
    SetupVertexArrays();
    CreateConstantBuffer();
    
    g_renderer->ResetFlushTimings();
    
    int msaaSamples = g_preferences ? g_preferences->GetInt(PREFS_SCREEN_ANTIALIAS, 0) : 0;
    
    int screenW = g_windowManager->DrawableWidth();
    int screenH = g_windowManager->DrawableHeight();

    g_renderer->InitializeMSAAFramebuffer(screenW, screenH, msaaSamples);
}

Renderer2DD3D11::~Renderer2DD3D11()
{
    CleanupBuffers();
}

// ============================================================================
// DEVICE & CONTEXT
// ============================================================================

void Renderer2DD3D11::GetDeviceAndContext()
{
    WindowManagerD3D11* windowManager = dynamic_cast<WindowManagerD3D11*>(g_windowManager);
    if (windowManager) 
    {
        m_device = windowManager->GetDevice();
        m_deviceContext = windowManager->GetDeviceContext();
        
        if (m_device) m_device->AddRef();
        if (m_deviceContext) m_deviceContext->AddRef();
    }
}

// ============================================================================
// HELPER METHODS
// ============================================================================

bool Renderer2DD3D11::CheckHR(HRESULT hr, const char* operation)
{
    return RendererD3D11::CheckHRResult(hr, operation);
}

bool Renderer2DD3D11::CheckHR(HRESULT hr, const char* operation, ID3DBlob* errorBlob)
{
    return RendererD3D11::CheckHRResult(hr, operation, errorBlob);
}

// ============================================================================
// SHADER MANAGEMENT
// ============================================================================

void Renderer2DD3D11::InitializeShaders()
{
    if (!m_device) 
    {
        return;
    }
    
    HRESULT hr;
    ID3DBlob* vertexBlob = nullptr;
    ID3DBlob* pixelBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;
    
    //
    // Compile vertex shader, same for both color and texture

    hr = D3DCompile(VERTEX_2D_SHADER_SOURCE_HLSL, strlen(VERTEX_2D_SHADER_SOURCE_HLSL), 
                    nullptr, nullptr, nullptr, "main", "vs_5_0", 0, 0, &vertexBlob, &errorBlob);
    
    if (!CheckHR(hr, "vertex shader compilation", errorBlob)) 
    {
        if (vertexBlob) vertexBlob->Release();
        return;
    }
    
    //
    // Create vertex shader

    hr = m_device->CreateVertexShader(vertexBlob->GetBufferPointer(), vertexBlob->GetBufferSize(),
                                      nullptr, &m_colorVertexShader);

    if (!CheckHR(hr, "create color vertex shader")) 
    {
        vertexBlob->Release();
        return;
    }
    
    m_textureVertexShader = m_colorVertexShader;
    m_textureVertexShader->AddRef();
    
    //
    // Create input layout

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    
    hr = m_device->CreateInputLayout(layout, ARRAYSIZE(layout),
                                    vertexBlob->GetBufferPointer(), vertexBlob->GetBufferSize(),
                                    &m_inputLayout);

    if (!CheckHR(hr, "create input layout")) 
    {
        vertexBlob->Release();
        return;
    }
    
    vertexBlob->Release();
    
    //
    // Compile color pixel shader

    hr = D3DCompile(COLOR_FRAGMENT_2D_SHADER_SOURCE_HLSL, strlen(COLOR_FRAGMENT_2D_SHADER_SOURCE_HLSL),
                    nullptr, nullptr, nullptr, "main", "ps_5_0", 0, 0, &pixelBlob, &errorBlob);
    
    if (!CheckHR(hr, "color pixel shader compilation", errorBlob)) 
    {
        if (pixelBlob) pixelBlob->Release();
        return;
    }
    
    hr = m_device->CreatePixelShader(pixelBlob->GetBufferPointer(), pixelBlob->GetBufferSize(),
                                     nullptr, &m_colorPixelShader);

    if (!CheckHR(hr, "create color pixel shader")) 
    {
        pixelBlob->Release();
        return;
    }
    
    pixelBlob->Release();
    
    //
    // Compile texture pixel shader

    hr = D3DCompile(TEXTURE_FRAGMENT_2D_SHADER_SOURCE_HLSL, strlen(TEXTURE_FRAGMENT_2D_SHADER_SOURCE_HLSL),
                    nullptr, nullptr, nullptr, "main", "ps_5_0", 0, 0, &pixelBlob, &errorBlob);
    
    if (!CheckHR(hr, "texture pixel shader compilation", errorBlob)) 
    {
        if (pixelBlob) pixelBlob->Release();
        return;
    }
    
    hr = m_device->CreatePixelShader(pixelBlob->GetBufferPointer(), pixelBlob->GetBufferSize(),
                                     nullptr, &m_texturePixelShader);

    if (!CheckHR(hr, "create texture pixel shader")) 
    {
        pixelBlob->Release();
        return;
    }
    
    pixelBlob->Release();

    //
    // Compile thin line geometry shader 

    ID3DBlob* geometryBlob = nullptr;

    hr = D3DCompile(LINE_GEOMETRY_2D_THIN_SHADER_SOURCE_HLSL, 
                    strlen(LINE_GEOMETRY_2D_THIN_SHADER_SOURCE_HLSL),
                    nullptr, nullptr, nullptr, "main", "gs_5_0", 0, 0, 
                    &geometryBlob, &errorBlob);

    if (!CheckHR(hr, "thin line geometry shader compilation", errorBlob)) 
    {
        if (geometryBlob) geometryBlob->Release();
        return;
    }

    hr = m_device->CreateGeometryShader(geometryBlob->GetBufferPointer(), 
                                        geometryBlob->GetBufferSize(),
                                        nullptr, &m_lineGeometryShaderThin);

    if (!CheckHR(hr, "create thin line geometry shader")) 
    {
        geometryBlob->Release();
        return;
    }

    geometryBlob->Release();

    //
    // Compile thick line geometry shader 

    hr = D3DCompile(LINE_GEOMETRY_2D_THICK_SHADER_SOURCE_HLSL, 
                    strlen(LINE_GEOMETRY_2D_THICK_SHADER_SOURCE_HLSL),
                    nullptr, nullptr, nullptr, "main", "gs_5_0", 0, 0, 
                    &geometryBlob, &errorBlob);

    if (!CheckHR(hr, "thick line geometry shader compilation", errorBlob)) 
    {
        if (geometryBlob) geometryBlob->Release();
        return;
    }

    hr = m_device->CreateGeometryShader(geometryBlob->GetBufferPointer(), 
                                        geometryBlob->GetBufferSize(),
                                        nullptr, &m_lineGeometryShaderThick);

    if (!CheckHR(hr, "create thick line geometry shader")) 
    {
        geometryBlob->Release();
        return;
    }

    geometryBlob->Release();

    m_colorShaderProgram = 1;
    m_textureShaderProgram = 2;
    m_lineShaderProgram = 3;
    m_shaderProgram = m_colorShaderProgram;
}

void Renderer2DD3D11::CacheUniformLocations()
{
    // Uniforms are in constant buffers, not individual locations
    // The constant buffer is bound at slot 0 (register b0)
}

void Renderer2DD3D11::SetColorShaderUniforms()
{
    UpdateConstantBuffer();
    
    if (m_deviceContext) 
    {
        //
        // Only set shaders/layout if they have changed
        
        if (m_currentlyBoundVS != m_colorVertexShader ||
            m_currentlyBoundPS != m_colorPixelShader ||
            m_currentlyBoundInputLayout != m_inputLayout) 
        {
            m_deviceContext->VSSetShader(m_colorVertexShader, nullptr, 0);
            m_deviceContext->PSSetShader(m_colorPixelShader, nullptr, 0);
            m_deviceContext->IASetInputLayout(m_inputLayout);
            
            m_currentlyBoundVS = m_colorVertexShader;
            m_currentlyBoundPS = m_colorPixelShader;
            m_currentlyBoundInputLayout = m_inputLayout;
        }
        
        m_deviceContext->VSSetConstantBuffers(0, 1, &m_transformConstantBuffer);
        
        //
        // Clear geometry shader and its constant buffers
        
        if (m_currentlyBoundGS != nullptr) 
        {
            m_deviceContext->GSSetShader(nullptr, nullptr, 0);
            m_currentlyBoundGS = nullptr;
        }

        ID3D11Buffer* nullBuffer = nullptr;
        m_deviceContext->GSSetConstantBuffers(0, 1, &nullBuffer);
        m_deviceContext->GSSetConstantBuffers(1, 1, &nullBuffer);
        
        //
        // Clear texture binding for color shader

        ID3D11ShaderResourceView* nullSRV = nullptr;
        m_deviceContext->PSSetShaderResources(0, 1, &nullSRV);
    }
}

void Renderer2DD3D11::SetTextureShaderUniforms()
{
    UpdateConstantBuffer();
    
    if (m_deviceContext) 
    {
        if (m_currentlyBoundVS != m_textureVertexShader ||
            m_currentlyBoundPS != m_texturePixelShader ||
            m_currentlyBoundInputLayout != m_inputLayout) 
        {
            m_deviceContext->VSSetShader(m_textureVertexShader, nullptr, 0);
            m_deviceContext->PSSetShader(m_texturePixelShader, nullptr, 0);
            m_deviceContext->IASetInputLayout(m_inputLayout);
            
            m_currentlyBoundVS = m_textureVertexShader;
            m_currentlyBoundPS = m_texturePixelShader;
            m_currentlyBoundInputLayout = m_inputLayout;
        }
        
        m_deviceContext->VSSetConstantBuffers(0, 1, &m_transformConstantBuffer);
        
        if (m_currentlyBoundGS != nullptr) 
        {
            m_deviceContext->GSSetShader(nullptr, nullptr, 0);
            m_currentlyBoundGS = nullptr;
        }

        ID3D11Buffer* nullBuffer = nullptr;
        m_deviceContext->GSSetConstantBuffers(0, 1, &nullBuffer);
        m_deviceContext->GSSetConstantBuffers(1, 1, &nullBuffer);
        
        //
        // Bind current texture if available

        if (m_currentTextureSRV) 
        {
            m_deviceContext->PSSetShaderResources(0, 1, &m_currentTextureSRV);
        } 
        else 
        {
            ID3D11ShaderResourceView* nullSRV = nullptr;
            m_deviceContext->PSSetShaderResources(0, 1, &nullSRV);
        }
    }
}

void Renderer2DD3D11::SetLineShaderUniforms(float lineWidth)
{
    UpdateConstantBuffer();
    
    if (m_deviceContext) 
    {
        if (m_currentlyBoundVS != m_colorVertexShader ||
            m_currentlyBoundPS != m_colorPixelShader ||
            m_currentlyBoundInputLayout != m_inputLayout) 
        {
            m_deviceContext->VSSetShader(m_colorVertexShader, nullptr, 0);
            m_deviceContext->PSSetShader(m_colorPixelShader, nullptr, 0);
            m_deviceContext->IASetInputLayout(m_inputLayout);
            
            m_currentlyBoundVS = m_colorVertexShader;
            m_currentlyBoundPS = m_colorPixelShader;
            m_currentlyBoundInputLayout = m_inputLayout;
        }
        
        //
        // For line width 1, use normal line rendering without geometry shader

        if (lineWidth == 1.0f)
        {
            //
            // Clear geometry shader for normal lines

            if (m_currentlyBoundGS != nullptr) 
            {
                m_deviceContext->GSSetShader(nullptr, nullptr, 0);
                m_currentlyBoundGS = nullptr;
            }

            ID3D11Buffer* nullBuffer = nullptr;
            m_deviceContext->GSSetConstantBuffers(0, 1, &nullBuffer);
            m_deviceContext->GSSetConstantBuffers(1, 1, &nullBuffer);
        }
        else
        {
            //
            // Update and bind line width constant buffer

            D3D11_MAPPED_SUBRESOURCE mappedResource;
            HRESULT hr = m_deviceContext->Map(m_lineWidthConstantBuffer, 0, 
                                              D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
            
            if (SUCCEEDED(hr)) 
            {
                LineWidthBuffer* lineWidthData = (LineWidthBuffer*)mappedResource.pData;
                lineWidthData->lineWidth = lineWidth;
                lineWidthData->viewportWidth = (float)g_windowManager->DrawableWidth();
                lineWidthData->viewportHeight = (float)g_windowManager->DrawableHeight();
                lineWidthData->padding = 0.0f;
                
                m_deviceContext->Unmap(m_lineWidthConstantBuffer, 0);
            }
            
            //
            // Select shader based on line width

            ID3D11GeometryShader* geometryShader = (lineWidth < 1.8f) ? m_lineGeometryShaderThin : m_lineGeometryShaderThick;
            
            if (m_currentlyBoundGS != geometryShader) 
            {
                m_deviceContext->GSSetShader(geometryShader, nullptr, 0);
                m_currentlyBoundGS = geometryShader;
            }
            
            m_deviceContext->GSSetConstantBuffers(1, 1, &m_lineWidthConstantBuffer);
        }
        
        m_deviceContext->VSSetConstantBuffers(0, 1, &m_transformConstantBuffer);
        
        ID3D11ShaderResourceView* nullSRV = nullptr;
        m_deviceContext->PSSetShaderResources(0, 1, &nullSRV);
    }
}

// ============================================================================
// CONSTANT BUFFER
// ============================================================================

void Renderer2DD3D11::CreateConstantBuffer()
{
    if (!m_device) return;
    
    D3D11_BUFFER_DESC desc = {};
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.ByteWidth = sizeof(TransformBuffer);
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    
    HRESULT hr = m_device->CreateBuffer(&desc, nullptr, &m_transformConstantBuffer);
    CheckHR(hr, "create constant buffer");

    //
    // Create line width constant buffer

    D3D11_BUFFER_DESC lineWidthBufferDesc = {};
    lineWidthBufferDesc.ByteWidth = sizeof(LineWidthBuffer);
    lineWidthBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    lineWidthBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    lineWidthBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = m_device->CreateBuffer(&lineWidthBufferDesc, nullptr, &m_lineWidthConstantBuffer);
    CheckHR(hr, "create line width constant buffer");
}

void Renderer2DD3D11::UpdateConstantBuffer()
{
    if (!m_deviceContext || !m_transformConstantBuffer) return;
    
    //
    // Check if matrices have changed to avoid redundant updates
    
    bool matricesChanged = false;
    if (m_matricesNeedUpdate) 
    {
        matricesChanged = true;
    }
    else 
    {
        //
        // Compare current matrices with last uploaded matrices

        for (int i = 0; i < 16; i++) 
        {
            if (m_projectionMatrix.m[i] != m_lastProjectionMatrix[i] ||
                m_modelViewMatrix.m[i] != m_lastModelViewMatrix[i]) 
            {
                matricesChanged = true;
                break;
            }
        }
    }
    
    if (!matricesChanged) return;
    
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = m_deviceContext->Map(m_transformConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (SUCCEEDED(hr)) 
    {
        TransformBuffer* buffer = (TransformBuffer*)mapped.pData;

        //
        // Copy projection matrix 

        memcpy(buffer->uProjection, m_projectionMatrix.m, sizeof(float) * 16);
        memcpy(buffer->uModelView, m_modelViewMatrix.m, sizeof(float) * 16);
        
        m_deviceContext->Unmap(m_transformConstantBuffer, 0);
        
        //
        // Update last uploaded matrices
        
        memcpy(m_lastProjectionMatrix, m_projectionMatrix.m, sizeof(float) * 16);
        memcpy(m_lastModelViewMatrix, m_modelViewMatrix.m, sizeof(float) * 16);
        m_matricesNeedUpdate = false;
    }
}

// ============================================================================
// BUFFER MANAGEMENT
// ============================================================================

void Renderer2DD3D11::SetupVertexArrays()
{
    SetupVBOs();
}

void Renderer2DD3D11::SetupVBOs()
{
    if (!m_device) return;
    
    //
    // Create persistent buffers
    
    D3D11_BUFFER_DESC desc = {};
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    
    //
    // Line buffer
    
    desc.ByteWidth = sizeof(Vertex2D) * MAX_LINE_VERTICES;
    HRESULT hr = m_device->CreateBuffer(&desc, nullptr, &m_lineBuffer);
    CheckHR(hr, "create line buffer");
    
    //
    // Static sprite buffer
    
    desc.ByteWidth = sizeof(Vertex2D) * MAX_STATIC_SPRITE_VERTICES;
    hr = m_device->CreateBuffer(&desc, nullptr, &m_staticSpriteBuffer);
    CheckHR(hr, "create static sprite buffer");
    
    //
    // Rotating sprite buffer
    
    desc.ByteWidth = sizeof(Vertex2D) * MAX_ROTATING_SPRITE_VERTICES;
    hr = m_device->CreateBuffer(&desc, nullptr, &m_rotatingSpriteBuffer);
    CheckHR(hr, "create rotating sprite buffer");
    
    //
    // Text buffer
    
    desc.ByteWidth = sizeof(Vertex2D) * MAX_TEXT_VERTICES;
    hr = m_device->CreateBuffer(&desc, nullptr, &m_textBuffer);
    CheckHR(hr, "create text buffer");
    
    //
    // Circle buffer
    
    desc.ByteWidth = sizeof(Vertex2D) * MAX_CIRCLE_VERTICES;
    hr = m_device->CreateBuffer(&desc, nullptr, &m_circleBuffer);
    CheckHR(hr, "create circle buffer");
    
    //
    // Circle fill buffer
    
    desc.ByteWidth = sizeof(Vertex2D) * MAX_CIRCLE_FILL_VERTICES;
    hr = m_device->CreateBuffer(&desc, nullptr, &m_circleFillBuffer);
    CheckHR(hr, "create circle fill buffer");
    
    //
    // Rect buffer
    
    desc.ByteWidth = sizeof(Vertex2D) * MAX_RECT_VERTICES;
    hr = m_device->CreateBuffer(&desc, nullptr, &m_rectBuffer);
    CheckHR(hr, "create rect buffer");
    
    //
    // Rect fill buffer
    
    desc.ByteWidth = sizeof(Vertex2D) * MAX_RECT_FILL_VERTICES;
    hr = m_device->CreateBuffer(&desc, nullptr, &m_rectFillBuffer);
    CheckHR(hr, "create rect fill buffer");
    
    //
    // Triangle fill buffer
    
    desc.ByteWidth = sizeof(Vertex2D) * MAX_TRIANGLE_FILL_VERTICES;
    hr = m_device->CreateBuffer(&desc, nullptr, &m_triangleFillBuffer);
    CheckHR(hr, "create triangle fill buffer");
    
    //
    // Immediate mode buffer
    
    desc.ByteWidth = sizeof(Vertex2D) * MAX_VERTICES;
    hr = m_device->CreateBuffer(&desc, nullptr, &m_immediateBuffer);
    CheckHR(hr, "create immediate buffer");
}

bool Renderer2DD3D11::UpdateBufferData(ID3D11Buffer* buffer, const Vertex2D* vertices, int vertexCount)
{
    if (!m_deviceContext || !buffer || vertexCount <= 0 || !vertices) 
    {
        return false;
    }
    
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = m_deviceContext->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    
    if (SUCCEEDED(hr)) 
    {
        memcpy(mapped.pData, vertices, vertexCount * sizeof(Vertex2D));
        m_deviceContext->Unmap(buffer, 0);
        return true;
    }
    
    return false;
}

void Renderer2DD3D11::RegisterVBO(unsigned int vboId, ID3D11Buffer* buffer)
{
    if (buffer) 
    {
        m_vboMap[vboId] = buffer;
    }
}

void Renderer2DD3D11::UploadVertexData(const Vertex2D* vertices, int vertexCount)
{

}

void Renderer2DD3D11::UploadVertexDataToVBO(unsigned int vbo, const Vertex2D* vertices,
                                                 int vertexCount, unsigned int usageHint)
{

}

ID3D11Buffer* Renderer2DD3D11::GetVBOFromID(unsigned int vbo)
{
    auto it = m_vboMap.find(vbo);

    if (it != m_vboMap.end()) 
    {
        return it->second;
    }
    return nullptr;
}

void Renderer2DD3D11::RegisterIBO(unsigned int iboId, ID3D11Buffer* buffer)
{
    if (buffer) 
    {
        m_iboMap[iboId] = buffer;
    }
}

ID3D11Buffer* Renderer2DD3D11::GetIBOFromID(unsigned int ibo)
{
    auto it = m_iboMap.find(ibo);

    if (it != m_iboMap.end()) 
    {
        return it->second;
    }
    return nullptr;
}

// ============================================================================
// TEXTURE BINDING
// ============================================================================

void Renderer2DD3D11::BindTexture(unsigned int textureID)
{
    if (m_currentTextureID == textureID && m_currentTextureSRV) 
    {
        return;
    }
    
    m_currentTextureID = textureID;
    m_currentTextureSRV = (ID3D11ShaderResourceView*)(uintptr_t)textureID;
}

// ============================================================================
// DRAWING
// ============================================================================

void Renderer2DD3D11::DrawVertices(ID3D11Buffer* vertexBuffer, int vertexCount, bool useTexture)
{
    if (!m_deviceContext || !vertexBuffer || vertexCount == 0) return;
    
    if (useTexture) 
    {
        SetTextureShaderUniforms();
    } 
    else 
    {
        SetColorShaderUniforms();
    }
    
    //
    // Set vertex buffer

    UINT stride = sizeof(Vertex2D);
    UINT offset = 0;
    m_deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
    
    m_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_deviceContext->Draw(vertexCount, 0);
}

// ============================================================================
// CORE FLUSH FUNCTIONS
// ============================================================================

void Renderer2DD3D11::FlushTriangles(bool useTexture)
{
    if (m_triangleVertexCount == 0) return;
    
    g_renderer->StartFlushTiming("Immediate_Triangles");
    IncrementDrawCall("immediate_triangles");
    
    if (m_immediateBuffer && UpdateBufferData(m_immediateBuffer, m_triangleVertices, m_triangleVertexCount)) 
    {
        DrawVertices(m_immediateBuffer, m_triangleVertexCount, useTexture);
    }
    
    m_triangleVertexCount = 0;
    
    g_renderer->EndFlushTiming("Immediate_Triangles");
}

void Renderer2DD3D11::FlushTextBuffer()
{
    if (m_textVertexCount == 0) return;
    
    g_renderer->StartFlushTiming("Text");
    IncrementDrawCall("text");
    
    int currentBlendSrc = g_renderer->GetCurrentBlendSrcFactor();
    int currentBlendDst = g_renderer->GetCurrentBlendDstFactor();
    
    if (g_renderer->GetBlendMode() != Renderer::BlendModeSubtractive) 
    {
        g_renderer->SetBlendMode(Renderer::BlendModeAdditive);
    }
    
    if (m_currentTextTexture != 0) 
    {
        BindTexture(m_currentTextTexture);
        g_renderer->SetActiveTexture(0);
        g_renderer->SetBoundTexture(m_currentTextTexture);
    } 
    else 
    {
        m_currentTextureSRV = nullptr;
        m_currentTextureID = 0;
    }
    
    if (m_textBuffer && UpdateBufferData(m_textBuffer, m_textVertices, m_textVertexCount)) 
    {
        DrawVertices(m_textBuffer, m_textVertexCount, true);
    }
    
    g_renderer->SetBlendFunc(currentBlendSrc, currentBlendDst);
    
    m_textVertexCount = 0;
    m_currentTextTexture = 0;
    
    g_renderer->EndFlushTiming("Text");
}

void Renderer2DD3D11::FlushLines()
{
    if (m_lineVertexCount == 0) return;
    
    g_renderer->StartFlushTiming("Lines");
    IncrementDrawCall("lines");
    
    if (m_lineBuffer && UpdateBufferData(m_lineBuffer, m_lineVertices, m_lineVertexCount)) 
    {
        if (m_currentLineWidth == 1.0f)
        {
            SetColorShaderUniforms();
            g_renderer->SetShaderProgram(m_colorShaderProgram);
        }
        else
        {
            SetColorShaderUniforms();
            g_renderer->SetShaderProgram(m_lineShaderProgram);
            SetLineShaderUniforms(m_currentLineWidth);
        }

        UINT stride = sizeof(Vertex2D);
        UINT offset = 0;
        m_deviceContext->IASetVertexBuffers(0, 1, &m_lineBuffer, &stride, &offset);
        m_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        m_deviceContext->Draw(m_lineVertexCount, 0);
    }
    
    m_lineVertexCount = 0;
    
    g_renderer->EndFlushTiming("Lines");
}

void Renderer2DD3D11::FlushStaticSprites()
{
    if (m_staticSpriteVertexCount == 0) return;
    
    g_renderer->StartFlushTiming("Static_Sprite");
    IncrementDrawCall("static_sprites");
    
    if (m_currentStaticSpriteTexture != 0) 
    {
        BindTexture(m_currentStaticSpriteTexture);
        g_renderer->SetActiveTexture(0);
        g_renderer->SetBoundTexture(m_currentStaticSpriteTexture);
    } 
    else 
    {
        m_currentTextureSRV = nullptr;
        m_currentTextureID = 0;
    }
    
    if (m_staticSpriteBuffer && UpdateBufferData(m_staticSpriteBuffer, m_staticSpriteVertices, m_staticSpriteVertexCount)) 
    {
        DrawVertices(m_staticSpriteBuffer, m_staticSpriteVertexCount, true);
    }
    
    m_staticSpriteVertexCount = 0;
    
    g_renderer->EndFlushTiming("Static_Sprite");
}

void Renderer2DD3D11::FlushRotatingSprite()
{
    if (m_rotatingSpriteVertexCount == 0) return;
    
    g_renderer->StartFlushTiming("Rotating_Sprite");
    IncrementDrawCall("rotating_sprites");
    
    if (m_currentRotatingSpriteTexture != 0) 
    {
        BindTexture(m_currentRotatingSpriteTexture);
        g_renderer->SetActiveTexture(0);
        g_renderer->SetBoundTexture(m_currentRotatingSpriteTexture);
    } 
    else 
    {
        m_currentTextureSRV = nullptr;
        m_currentTextureID = 0;
    }
    
    if (m_rotatingSpriteBuffer && UpdateBufferData(m_rotatingSpriteBuffer, m_rotatingSpriteVertices, m_rotatingSpriteVertexCount)) 
    {
        DrawVertices(m_rotatingSpriteBuffer, m_rotatingSpriteVertexCount, true);
    }
    
    m_rotatingSpriteVertexCount = 0;
    
    g_renderer->EndFlushTiming("Rotating_Sprite");
}

void Renderer2DD3D11::FlushCircles()
{
    if (m_circleVertexCount == 0) return;
    
    g_renderer->StartFlushTiming("Circles");
    IncrementDrawCall("circles");
    
    if (m_circleBuffer && UpdateBufferData(m_circleBuffer, m_circleVertices, m_circleVertexCount)) 
    {
        if (m_currentCircleWidth == 1.0f)
        {
            SetColorShaderUniforms();
            g_renderer->SetShaderProgram(m_colorShaderProgram);
        }
        else
        {
            SetColorShaderUniforms();
            g_renderer->SetShaderProgram(m_lineShaderProgram);
            SetLineShaderUniforms(m_currentCircleWidth);
        }

        UINT stride = sizeof(Vertex2D);
        UINT offset = 0;
        m_deviceContext->IASetVertexBuffers(0, 1, &m_circleBuffer, &stride, &offset);
        m_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        m_deviceContext->Draw(m_circleVertexCount, 0);
    }
    
    m_circleVertexCount = 0;
    
    g_renderer->EndFlushTiming("Circles");
}

void Renderer2DD3D11::FlushCircleFills()
{
    if (m_circleFillVertexCount == 0) return;
    
    g_renderer->StartFlushTiming("Circle_Fills");
    IncrementDrawCall("circle_fills");
    
    if (m_circleFillBuffer && UpdateBufferData(m_circleFillBuffer, m_circleFillVertices, m_circleFillVertexCount)) 
    {
        DrawVertices(m_circleFillBuffer, m_circleFillVertexCount, false);
    }
    
    m_circleFillVertexCount = 0;
    
    g_renderer->EndFlushTiming("Circle_Fills");
}

void Renderer2DD3D11::FlushRects()
{
    if (m_rectVertexCount == 0) return;
    
    g_renderer->StartFlushTiming("Rects");
    IncrementDrawCall("rects");
    
    if (m_rectBuffer && UpdateBufferData(m_rectBuffer, m_rectVertices, m_rectVertexCount)) 
    {
        if (m_currentRectWidth == 1.0f)
        {
            SetColorShaderUniforms();
            g_renderer->SetShaderProgram(m_colorShaderProgram);
        }
        else
        {
            SetColorShaderUniforms();
            g_renderer->SetShaderProgram(m_lineShaderProgram);
            SetLineShaderUniforms(m_currentRectWidth);
        }
        
        UINT stride = sizeof(Vertex2D);
        UINT offset = 0;
        m_deviceContext->IASetVertexBuffers(0, 1, &m_rectBuffer, &stride, &offset);
        m_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        m_deviceContext->Draw(m_rectVertexCount, 0);
    }
    
    m_rectVertexCount = 0;
    
    g_renderer->EndFlushTiming("Rects");
}

void Renderer2DD3D11::FlushRectFills()
{
    if (m_rectFillVertexCount == 0) return;
    
    g_renderer->StartFlushTiming("Rect_Fills");
    IncrementDrawCall("rect_fills");
    
    if (m_rectFillBuffer && UpdateBufferData(m_rectFillBuffer, m_rectFillVertices, m_rectFillVertexCount)) 
    {
        DrawVertices(m_rectFillBuffer, m_rectFillVertexCount, false);
    }
    
    m_rectFillVertexCount = 0;
    
    g_renderer->EndFlushTiming("Rect_Fills");
}

void Renderer2DD3D11::FlushTriangleFills()
{
    if (m_triangleFillVertexCount == 0) return;
    
    g_renderer->StartFlushTiming("Triangle_Fills");
    IncrementDrawCall("triangle_fills");
    
    if (m_triangleFillBuffer && UpdateBufferData(m_triangleFillBuffer, m_triangleFillVertices, m_triangleFillVertexCount)) 
    {
        DrawVertices(m_triangleFillBuffer, m_triangleFillVertexCount, false);
    }
    
    m_triangleFillVertexCount = 0;
    
    g_renderer->EndFlushTiming("Triangle_Fills");
}

// ============================================================================
// CLEANUP
// ============================================================================

void Renderer2DD3D11::CleanupBuffers()
{
    if (m_colorVertexShader) { m_colorVertexShader->Release(); m_colorVertexShader = nullptr; }
    if (m_colorPixelShader) { m_colorPixelShader->Release(); m_colorPixelShader = nullptr; }

    if (m_textureVertexShader && m_textureVertexShader != m_colorVertexShader) 
    { 
        m_textureVertexShader->Release(); 
        m_textureVertexShader = nullptr; 
    }

    if (m_texturePixelShader) { m_texturePixelShader->Release(); m_texturePixelShader = nullptr; }
    if (m_inputLayout) { m_inputLayout->Release(); m_inputLayout = nullptr; }
    
    if (m_transformConstantBuffer) { m_transformConstantBuffer->Release(); m_transformConstantBuffer = nullptr; }
    if (m_batchedConstantBuffer) { m_batchedConstantBuffer->Release(); m_batchedConstantBuffer = nullptr; }
    
    //
    // Release persistent buffers
    
    if (m_lineBuffer) { m_lineBuffer->Release(); m_lineBuffer = nullptr; }
    if (m_staticSpriteBuffer) { m_staticSpriteBuffer->Release(); m_staticSpriteBuffer = nullptr; }
    if (m_rotatingSpriteBuffer) { m_rotatingSpriteBuffer->Release(); m_rotatingSpriteBuffer = nullptr; }
    if (m_textBuffer) { m_textBuffer->Release(); m_textBuffer = nullptr; }
    if (m_circleBuffer) { m_circleBuffer->Release(); m_circleBuffer = nullptr; }
    if (m_circleFillBuffer) { m_circleFillBuffer->Release(); m_circleFillBuffer = nullptr; }
    if (m_rectBuffer) { m_rectBuffer->Release(); m_rectBuffer = nullptr; }
    if (m_rectFillBuffer) { m_rectFillBuffer->Release(); m_rectFillBuffer = nullptr; }
    if (m_triangleFillBuffer) { m_triangleFillBuffer->Release(); m_triangleFillBuffer = nullptr; }
    if (m_immediateBuffer) { m_immediateBuffer->Release(); m_immediateBuffer = nullptr; }

    if (m_lineGeometryShaderThin) 
    {
        m_lineGeometryShaderThin->Release();
        m_lineGeometryShaderThin = nullptr;
    }

    if (m_lineGeometryShaderThick) 
    {
        m_lineGeometryShaderThick->Release();
        m_lineGeometryShaderThick = nullptr;
    }

    if (m_lineWidthConstantBuffer) 
    {
        m_lineWidthConstantBuffer->Release();
        m_lineWidthConstantBuffer = nullptr;
    }
    
    //
    // Release all VBOs from map

    for (auto& pair : m_vboMap) 
    {
        if (pair.second) 
        {
            pair.second->Release();
        }
    }
    m_vboMap.clear();
    
    //
    // Release all IBOs from map

    for (auto& pair : m_iboMap) 
    {
        if (pair.second) 
        {
            pair.second->Release();
        }
    }
    m_iboMap.clear();
    
    m_vaoMap.clear();
    
    if (m_device) { m_device->Release(); m_device = nullptr; }
    if (m_deviceContext) { m_deviceContext->Release(); m_deviceContext = nullptr; }
}

// ============================================================================
// MEGAVBO BUFFER MANAGEMENT
// ============================================================================

unsigned int Renderer2DD3D11::CreateMegaVBOVertexBuffer(size_t size, BufferUsageHint usageHint)
{
    if (!m_device) return 0;
    
    D3D11_USAGE d3dUsage;
    UINT cpuAccessFlags = 0;
    
    switch (usageHint) 
    {
        case BUFFER_USAGE_STATIC_DRAW:
            d3dUsage = D3D11_USAGE_DEFAULT;
            cpuAccessFlags = 0;
            break;
        case BUFFER_USAGE_DYNAMIC_DRAW:
        case BUFFER_USAGE_STREAM_DRAW:
            d3dUsage = D3D11_USAGE_DYNAMIC;
            cpuAccessFlags = D3D11_CPU_ACCESS_WRITE;
            break;
        default:
            d3dUsage = D3D11_USAGE_DEFAULT;
            cpuAccessFlags = 0;
            break;
    }
    
    D3D11_BUFFER_DESC desc = {};
    desc.Usage = d3dUsage;
    desc.ByteWidth = (UINT)size;
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags = cpuAccessFlags;
    
    //
    // Create buffer without initial data, will be uploaded separately

    ID3D11Buffer* buffer = nullptr;
    HRESULT hr = m_device->CreateBuffer(&desc, nullptr, &buffer);
    if (!CheckHR(hr, "create MegaVBO vertex buffer")) 
    {
        return 0;
    }
    
    unsigned int vboId = m_nextVBOId++;
    RegisterVBO(vboId, buffer);
    return vboId;
}

unsigned int Renderer2DD3D11::CreateMegaVBOIndexBuffer(size_t size, BufferUsageHint usageHint)
{
    if (!m_device) return 0;

    D3D11_USAGE d3dUsage;
    UINT cpuAccessFlags = 0;
    
    switch (usageHint) 
    {
        case BUFFER_USAGE_STATIC_DRAW:
            d3dUsage = D3D11_USAGE_DEFAULT;
            cpuAccessFlags = 0;
            break;
        case BUFFER_USAGE_DYNAMIC_DRAW:
        case BUFFER_USAGE_STREAM_DRAW:
            d3dUsage = D3D11_USAGE_DYNAMIC;
            cpuAccessFlags = D3D11_CPU_ACCESS_WRITE;
            break;
        default:
            d3dUsage = D3D11_USAGE_DEFAULT;
            cpuAccessFlags = 0;
            break;
    }
    
    D3D11_BUFFER_DESC desc = {};
    desc.Usage = d3dUsage;
    desc.ByteWidth = (UINT)size;
    desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    desc.CPUAccessFlags = cpuAccessFlags;
    
    ID3D11Buffer* buffer = nullptr;
    HRESULT hr = m_device->CreateBuffer(&desc, nullptr, &buffer);
    if (!CheckHR(hr, "create MegaVBO index buffer")) 
    {
        return 0;
    }
    
    unsigned int iboId = m_nextVBOId++;
    RegisterIBO(iboId, buffer);
    return iboId;
}

unsigned int Renderer2DD3D11::CreateMegaVBOVertexArray()
{
    return m_nextVBOId++;
}

void Renderer2DD3D11::DeleteMegaVBOVertexBuffer(unsigned int buffer)
{
    if (buffer == 0) return;
    
    //
    // Clear uploaded flag in VAO mappings

    for (auto& pair : m_vaoMap) 
    {
        if (pair.second.vboId == buffer) 
        {
            pair.second.vboUploaded = false;
        }
    }
    
    auto it = m_vboMap.find(buffer);
    if (it != m_vboMap.end()) 
    {
        if (it->second) 
        {
            it->second->Release();
        }
        m_vboMap.erase(it);
    }
}

void Renderer2DD3D11::DeleteMegaVBOIndexBuffer(unsigned int buffer)
{
    ID3D11Buffer* d3dBuffer = GetIBOFromID(buffer);
    if (d3dBuffer) 
    {
        d3dBuffer->Release();
        m_iboMap.erase(buffer);
        
        m_lineStripSegments.erase(buffer);
    }
}

void Renderer2DD3D11::DeleteMegaVBOVertexArray(unsigned int vao)
{
    if (vao == 0) return;
    
    auto it = m_vaoMap.find(vao);
    if (it != m_vaoMap.end()) 
    {
        m_vaoMap.erase(it);
    }
}

void Renderer2DD3D11::SetupMegaVBOVertexAttributes2D(unsigned int vao, unsigned int vbo, unsigned int ibo)
{
    //
    // DirectX11 uses input layout set at shader creation
    // Store the VAO -> (VBO, IBO) mapping for later use in DrawMegaVBOIndexed

    VAOBinding binding;
    binding.vboId = vbo;
    binding.iboId = ibo;
    binding.vboUploaded = false;
    binding.iboUploaded = false;
    m_vaoMap[vao] = binding;
    
    m_currentVAO = vao;
}

void Renderer2DD3D11::UpdateCurrentVAO(unsigned int vao)
{
    if (m_vaoMap.find(vao) != m_vaoMap.end()) 
    {
        m_currentVAO = vao;
    }
}

void Renderer2DD3D11::UploadMegaVBOIndexData(unsigned int ibo, const unsigned int* indices,
                                                  int indexCount, BufferUsageHint usageHint)
{
    if (!m_deviceContext || indexCount <= 0 || !indices) return;
    
    ID3D11Buffer* buffer = GetIBOFromID(ibo);
    if (!buffer) return;
    
    //
    // For static buffers, check if already uploaded

    if (usageHint == BUFFER_USAGE_STATIC_DRAW)
     {
        for (auto& pair : m_vaoMap) 
        {
            if (pair.second.iboId == ibo && pair.second.iboUploaded) 
            {
                return; 
            }
        }
    }
    
    if (m_primitiveRestartEnabled) 
    {
        std::vector<LineStripSegment> segments;
        unsigned int startIndex = 0;
        
        for (int i = 0; i < indexCount; i++) 
        {
            if (indices[i] == m_primitiveRestartIndex) 
            {
                //
                // Found a restart marker, save this segment

                if (i > startIndex) 
                {
                    LineStripSegment segment;
                    segment.startIndex = startIndex;
                    segment.indexCount = i - startIndex;
                    segments.push_back(segment);
                }
                startIndex = i + 1;
            }
        }
        
        //
        // Add the last segment

        if (startIndex < (unsigned int)indexCount) 
        {
            LineStripSegment segment;
            segment.startIndex = startIndex;
            segment.indexCount = indexCount - startIndex;
            segments.push_back(segment);
        }
        
        m_lineStripSegments[ibo] = segments;
    }
    
    size_t dataSize = indexCount * sizeof(unsigned int);
    
    if (usageHint == BUFFER_USAGE_STATIC_DRAW) 
    {
        m_deviceContext->UpdateSubresource(buffer, 0, nullptr, indices, 0, 0);
        
        for (auto& pair : m_vaoMap) 
        {
            if (pair.second.iboId == ibo) 
            {
                pair.second.iboUploaded = true;
                break;
            }
        }
    }
    else 
    {
        D3D11_MAPPED_SUBRESOURCE mapped;
        HRESULT hr = m_deviceContext->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (SUCCEEDED(hr)) 
        {
            memcpy(mapped.pData, indices, dataSize);
            m_deviceContext->Unmap(buffer, 0);
        }
    }
}

void Renderer2DD3D11::UploadMegaVBOVertexData(unsigned int vbo, const Vertex2D* vertices,
                                                  int vertexCount, BufferUsageHint usageHint)
{
    if (!m_deviceContext || vertexCount <= 0 || !vertices) return;
    
    ID3D11Buffer* buffer = GetVBOFromID(vbo);
    if (!buffer) return;
    
    if (usageHint == BUFFER_USAGE_STATIC_DRAW) 
    {
        //
        // Find the VAO that uses this VBO to check if its already uploaded

        for (auto& pair : m_vaoMap) 
        {
            if (pair.second.vboId == vbo) 
            {
                if (pair.second.vboUploaded) 
                {
                    return;
                }
                break;
            }
        }
    }
    
    size_t dataSize = vertexCount * sizeof(Vertex2D);
    
    //
    // For static buffers, use UpdateSubresource with initial data
    // For dynamic buffers, use Map/Unmap

    if (usageHint == BUFFER_USAGE_STATIC_DRAW) 
    {
        m_deviceContext->UpdateSubresource(buffer, 0, nullptr, vertices, 0, 0);
        
        for (auto& pair : m_vaoMap) 
        {
            if (pair.second.vboId == vbo) 
            {
                pair.second.vboUploaded = true;
                break;
            }
        }
    } 
    else 
    {
        D3D11_MAPPED_SUBRESOURCE mapped;
        HRESULT hr = m_deviceContext->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (SUCCEEDED(hr)) 
        {
            memcpy(mapped.pData, vertices, dataSize);
            m_deviceContext->Unmap(buffer, 0);
        }
    }
}

void Renderer2DD3D11::DrawMegaVBOIndexed(PrimitiveType primitiveType, unsigned int indexCount)
{
    if (!m_deviceContext || indexCount == 0) return;
    
    auto vaoIt = m_vaoMap.find(m_currentVAO);
    if (vaoIt == m_vaoMap.end()) 
    {
        if (m_vaoMap.empty()) 
        {
            return;
        }
        vaoIt = m_vaoMap.begin();
        m_currentVAO = vaoIt->first;
    }
    
    ID3D11Buffer* vbo = GetVBOFromID(vaoIt->second.vboId);
    ID3D11Buffer* ibo = GetIBOFromID(vaoIt->second.iboId);
    
    if (!vbo || !ibo) 
    {
        return;
    }
    
    //
    // Map primitive type to D3D11 topology

    D3D11_PRIMITIVE_TOPOLOGY topology;
    switch (primitiveType) 
    {
        case PRIMITIVE_TRIANGLES:
            topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            break;
        case PRIMITIVE_LINE_STRIP:
            topology = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
            break;
        case PRIMITIVE_LINES:
            topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
            break;
        default:
            topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            break;
    }
    
    //
    // Set vertex and index buffers

    UINT stride = sizeof(Vertex2D);
    UINT offset = 0;
    m_deviceContext->IASetVertexBuffers(0, 1, &vbo, &stride, &offset);
    m_deviceContext->IASetIndexBuffer(ibo, DXGI_FORMAT_R32_UINT, 0);
    m_deviceContext->IASetPrimitiveTopology(topology);
    
    if (m_primitiveRestartEnabled && primitiveType == PRIMITIVE_LINE_STRIP) 
    {
        auto segmentsIt = m_lineStripSegments.find(vaoIt->second.iboId);
        
        if (segmentsIt != m_lineStripSegments.end()) 
        {
            for (const auto& segment : segmentsIt->second) 
            {
                m_deviceContext->DrawIndexed(segment.indexCount, segment.startIndex, 0);
            }
        } 
        else 
        {
            m_deviceContext->DrawIndexed(indexCount, 0, 0);
        }
    } 
    else 
    {
        m_deviceContext->DrawIndexed(indexCount, 0, 0);
    }
}

void Renderer2DD3D11::EnableMegaVBOPrimitiveRestart(unsigned int restartIndex)
{
    m_primitiveRestartEnabled = true;
    m_primitiveRestartIndex = restartIndex;
}

void Renderer2DD3D11::DisableMegaVBOPrimitiveRestart()
{
    m_primitiveRestartEnabled = false;
    m_primitiveRestartIndex = 0;
}

#endif // RENDERER_DIRECTX11