#ifdef RENDERER_DIRECTX11

#include "systemiv.h"

#include <string.h>
#include <time.h>

#include "lib/debug_utils.h"
#include "lib/hi_res_time.h"
#include "lib/resource/image.h"
#include "lib/resource/sprite_atlas.h"
#include "lib/resource/bitmap.h"
#include "lib/resource/bitmapfont.h"
#include "lib/gucci/window_manager.h"
#include "lib/gucci/window_manager_d3d11.h"
#include "lib/filesys/filesys_utils.h"
#include "lib/render/colour.h"
#include "lib/resource/bitmap.h"
#include "lib/render2d/renderer_2d.h"
#include "lib/render3d/renderer_3d.h"
#include "lib/render2d/megavbo/megavbo_2d.h"
#include "lib/render3d/megavbo/megavbo_3d.h"
#include "lib/render2d/renderer_2d_d3d11.h"
#include "lib/render3d/renderer_3d_d3d11.h"

#include "renderer.h"
#include "renderer_d3d11.h"

// ============================================================================
// CONSTRUCTOR & DESTRUCTOR
// ============================================================================

RendererD3D11::RendererD3D11()
    : m_currentViewportX(-1),
      m_currentViewportY(-1),
      m_currentViewportWidth(-1),
      m_currentViewportHeight(-1),
      m_currentScissorX(-1),
      m_currentScissorY(-1),
      m_currentScissorWidth(-1),
      m_currentScissorHeight(-1),
      m_scissorTestEnabled(false),
      m_currentActiveTexture(0),
      m_currentBoundTexture(0),
      m_currentShaderProgram(0),
      m_currentVAO(0),
      m_currentArrayBuffer(0),
      m_currentElementBuffer(0),
      m_blendEnabled(false),
      m_currentBlendSrcFactor(-1),
      m_currentBlendDstFactor(-1),
      m_depthTestEnabled(false),
      m_depthMaskEnabled(false),
      m_cullFaceEnabled(false),
      m_cullFaceMode(0),
      m_currentLineWidth(-1.0f),
      m_device(nullptr),
      m_deviceContext(nullptr),
      m_blendStateDisabled(nullptr),
      m_blendStateNormal(nullptr),
      m_blendStateAdditive(nullptr),
      m_blendStateSubtractive(nullptr),
      m_currentBlendState(nullptr),
      m_depthStateEnabled(nullptr),
      m_depthStateDisabled(nullptr),
      m_currentDepthState(nullptr),
      m_rasterizerStateNoCull(nullptr),
      m_rasterizerStateCullBack(nullptr),
      m_rasterizerStateCullFront(nullptr),
      m_rasterizerStateCullBoth(nullptr),
      m_rasterizerStateNoCullScissor(nullptr),
      m_rasterizerStateCullBackScissor(nullptr),
      m_rasterizerStateCullFrontScissor(nullptr),
      m_rasterizerStateCullBothScissor(nullptr),
      m_currentRasterizerState(nullptr),
      m_samplerStateLinear(nullptr),
      m_samplerStateLinearMipLinear(nullptr),
      m_currentSamplerState(nullptr),
      m_msaaRenderTarget(nullptr),
      m_msaaRenderTargetView(nullptr),
      m_msaaDepthStencil(nullptr),
      m_msaaDepthStencilView(nullptr),
      m_nextShaderProgramId(1),
      m_timingQueryCount(0)
{
    //
    // Initialize flush timings
    
    for (int i = 0; i < MAX_FLUSH_TYPES; i++) {
        m_flushTimings[i].name = NULL;
        m_flushTimings[i].totalTime = 0.0;
        m_flushTimings[i].totalGpuTime = 0.0;
        m_flushTimings[i].callCount = 0;
        m_flushTimings[i].queryObject = 0;
        m_flushTimings[i].queryPending = false;
        
        m_timingQueries[i].disjointQuery = nullptr;
        m_timingQueries[i].beginQuery = nullptr;
        m_timingQueries[i].endQuery = nullptr;
        m_timingQueries[i].queryPending = false;
    }
    
    m_blendMode = BlendModeNormal;
    m_blendSrcFactor = BLEND_ONE;
    m_blendDstFactor = BLEND_ZERO;
    m_flushTimingCount = 0;
    m_currentFlushStartTime = 0.0;
    m_msaaEnabled = false;
    m_msaaSamples = 0;
    m_msaaWidth = 0;
    m_msaaHeight = 0;
    
    g_renderer2d = new Renderer2DD3D11();
    g_megavbo2d = new MegaVBO2D();
    g_renderer3d = new Renderer3DD3D11();
    g_megavbo3d = new MegaVBO3D();
}

RendererD3D11::~RendererD3D11()
{
    DestroyMSAAFramebuffer();
    
    for (int i = 0; i < m_timingQueryCount; i++) 
    {
        if (m_timingQueries[i].disjointQuery) 
        {
            m_timingQueries[i].disjointQuery->Release();
        }
        if (m_timingQueries[i].beginQuery) 
        {
            m_timingQueries[i].beginQuery->Release();
        }
        if (m_timingQueries[i].endQuery) 
        {
            m_timingQueries[i].endQuery->Release();
        }
    }
    
    //
    // Release shader programs

    for (auto& pair : m_shaderPrograms) 
    {
        if (pair.second.vertexShader) pair.second.vertexShader->Release();
        if (pair.second.pixelShader) pair.second.pixelShader->Release();
        if (pair.second.inputLayout) pair.second.inputLayout->Release();
    }
    m_shaderPrograms.clear();
    
    ReleaseStateObjects();
}

// ============================================================================
// DEVICE & CONTEXT
// ============================================================================

void RendererD3D11::GetDeviceAndContext()
{
    WindowManagerD3D11* windowManager = dynamic_cast<WindowManagerD3D11*>(g_windowManager);
    
    if (windowManager) 
    {
        m_device = windowManager->GetDevice();
        m_deviceContext = windowManager->GetDeviceContext();
        
        if (m_device) m_device->AddRef();
        if (m_deviceContext) m_deviceContext->AddRef();
    }
    else
    {
        #ifdef _DEBUG
        AppDebugOut("  ERROR: windowManager cast failed!\n");
        #endif
    }
}

// ============================================================================
// STATE OBJECT CREATION
// ============================================================================

void RendererD3D11::CreateStateObjects()
{
    if (!m_device || !m_deviceContext) 
    {
        GetDeviceAndContext();
    }
    
    if (!m_device) 
    {
        return;
    }
    
    if (m_blendStateNormal) 
    {
        return;
    }
    
    HRESULT hr;
    
    //
    // Create blend states
    
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    
    blendDesc.RenderTarget[0].BlendEnable = FALSE;
    hr = m_device->CreateBlendState(&blendDesc, &m_blendStateDisabled);
    CheckHR(hr, "create disabled blend state");

    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    hr = m_device->CreateBlendState(&blendDesc, &m_blendStateNormal);
    CheckHR(hr, "create normal blend state");
    
    //
    // Additive (SrcAlpha, One)

    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    hr = m_device->CreateBlendState(&blendDesc, &m_blendStateAdditive);
    CheckHR(hr, "create additive blend state");
    
    //
    // Subtractive (SrcAlpha, OneMinusSrcColor)

    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_COLOR;
    hr = m_device->CreateBlendState(&blendDesc, &m_blendStateSubtractive);
    CheckHR(hr, "create subtractive blend state");
    
    //
    // Create depth stencil states
    
    D3D11_DEPTH_STENCIL_DESC depthDesc = {};
    
    depthDesc.DepthEnable = TRUE;
    depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    depthDesc.DepthFunc = D3D11_COMPARISON_LESS;
    depthDesc.StencilEnable = FALSE;
    hr = m_device->CreateDepthStencilState(&depthDesc, &m_depthStateEnabled);
    CheckHR(hr, "create enabled depth state");
    
    depthDesc.DepthEnable = FALSE;
    depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    hr = m_device->CreateDepthStencilState(&depthDesc, &m_depthStateDisabled);
    CheckHR(hr, "create disabled depth state");
    
    //
    // Create rasterizers
    
    D3D11_RASTERIZER_DESC rasterDesc = {};
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.CullMode = D3D11_CULL_NONE;
    rasterDesc.FrontCounterClockwise = FALSE;
    rasterDesc.DepthBias = 0;
    rasterDesc.DepthBiasClamp = 0.0f;
    rasterDesc.SlopeScaledDepthBias = 0.0f;
    rasterDesc.DepthClipEnable = TRUE;
    rasterDesc.ScissorEnable = FALSE;
    rasterDesc.MultisampleEnable = TRUE;
    rasterDesc.AntialiasedLineEnable = TRUE;
    
    hr = m_device->CreateRasterizerState(&rasterDesc, &m_rasterizerStateNoCull);
    CheckHR(hr, "create no-cull rasterizer state");
    rasterDesc.CullMode = D3D11_CULL_BACK;
    hr = m_device->CreateRasterizerState(&rasterDesc, &m_rasterizerStateCullBack);
    CheckHR(hr, "create cull-back rasterizer state");
    
    rasterDesc.CullMode = D3D11_CULL_FRONT;
    hr = m_device->CreateRasterizerState(&rasterDesc, &m_rasterizerStateCullFront);
    CheckHR(hr, "create cull-front rasterizer state");
    
    rasterDesc.CullMode = D3D11_CULL_NONE; // Front and back culling not directly supported
    hr = m_device->CreateRasterizerState(&rasterDesc, &m_rasterizerStateCullBoth);
    CheckHR(hr, "create cull-both rasterizer state");
    
    //
    // Create rasterizer states with scissor enabled
    
    rasterDesc.ScissorEnable = TRUE;
    rasterDesc.CullMode = D3D11_CULL_NONE;
    hr = m_device->CreateRasterizerState(&rasterDesc, &m_rasterizerStateNoCullScissor);
    CheckHR(hr, "create no-cull scissor rasterizer state");
    
    rasterDesc.CullMode = D3D11_CULL_BACK;
    hr = m_device->CreateRasterizerState(&rasterDesc, &m_rasterizerStateCullBackScissor);
    CheckHR(hr, "create cull-back scissor rasterizer state");
    
    rasterDesc.CullMode = D3D11_CULL_FRONT;
    hr = m_device->CreateRasterizerState(&rasterDesc, &m_rasterizerStateCullFrontScissor);
    CheckHR(hr, "create cull-front scissor rasterizer state");
    
    rasterDesc.CullMode = D3D11_CULL_NONE;
    hr = m_device->CreateRasterizerState(&rasterDesc, &m_rasterizerStateCullBothScissor);
    CheckHR(hr, "create cull-both scissor rasterizer state");
    
    //
    // Create sampler states
    
    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MinLOD = 0;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    
    //
    // Linear for non mipmapped textures

    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    hr = m_device->CreateSamplerState(&samplerDesc, &m_samplerStateLinear);
    CheckHR(hr, "create linear sampler state");
    
    //
    // Linear Mip Linear for mipmapped textures

    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    hr = m_device->CreateSamplerState(&samplerDesc, &m_samplerStateLinearMipLinear);
    CheckHR(hr, "create linear mip linear sampler state");
    
    m_currentBlendState = m_blendStateNormal;
    m_currentDepthState = m_depthStateDisabled;
    m_currentRasterizerState = m_rasterizerStateNoCull;
    m_currentSamplerState = m_samplerStateLinear;
    
    if (m_deviceContext && m_currentSamplerState) {
        m_deviceContext->PSSetSamplers(0, 1, &m_currentSamplerState);
    }
}

void RendererD3D11::ReleaseStateObjects()
{
    if (m_blendStateDisabled) { m_blendStateDisabled->Release(); m_blendStateDisabled = nullptr; }
    if (m_blendStateNormal) { m_blendStateNormal->Release(); m_blendStateNormal = nullptr; }
    if (m_blendStateAdditive) { m_blendStateAdditive->Release(); m_blendStateAdditive = nullptr; }
    if (m_blendStateSubtractive) { m_blendStateSubtractive->Release(); m_blendStateSubtractive = nullptr; }
    
    if (m_depthStateEnabled) { m_depthStateEnabled->Release(); m_depthStateEnabled = nullptr; }
    if (m_depthStateDisabled) { m_depthStateDisabled->Release(); m_depthStateDisabled = nullptr; }
    
    if (m_rasterizerStateNoCull) { m_rasterizerStateNoCull->Release(); m_rasterizerStateNoCull = nullptr; }
    if (m_rasterizerStateCullBack) { m_rasterizerStateCullBack->Release(); m_rasterizerStateCullBack = nullptr; }
    if (m_rasterizerStateCullFront) { m_rasterizerStateCullFront->Release(); m_rasterizerStateCullFront = nullptr; }
    if (m_rasterizerStateCullBoth) { m_rasterizerStateCullBoth->Release(); m_rasterizerStateCullBoth = nullptr; }
    if (m_rasterizerStateNoCullScissor) { m_rasterizerStateNoCullScissor->Release(); m_rasterizerStateNoCullScissor = nullptr; }
    if (m_rasterizerStateCullBackScissor) { m_rasterizerStateCullBackScissor->Release(); m_rasterizerStateCullBackScissor = nullptr; }
    if (m_rasterizerStateCullFrontScissor) { m_rasterizerStateCullFrontScissor->Release(); m_rasterizerStateCullFrontScissor = nullptr; }
    if (m_rasterizerStateCullBothScissor) { m_rasterizerStateCullBothScissor->Release(); m_rasterizerStateCullBothScissor = nullptr; }
    
    if (m_samplerStateLinear) { m_samplerStateLinear->Release(); m_samplerStateLinear = nullptr; }
    if (m_samplerStateLinearMipLinear) { m_samplerStateLinearMipLinear->Release(); m_samplerStateLinearMipLinear = nullptr; }
    
    if (m_device) { m_device->Release(); m_device = nullptr; }
    if (m_deviceContext) { m_deviceContext->Release(); m_deviceContext = nullptr; }
}

// ============================================================================
// HELPERS
// ============================================================================

bool RendererD3D11::CheckHRResult(HRESULT hr, const char* operation)
{
    if (FAILED(hr)) 
    {
        #ifdef _DEBUG
        AppDebugOut("RendererD3D11: %s failed, error: 0x%08X\n", operation, hr);
        #endif
        return false;
    }
    return true;
}

bool RendererD3D11::CheckHRResult(HRESULT hr, const char* operation, ID3DBlob* errorBlob)
{
    if (FAILED(hr)) 
    {
        if (errorBlob) 
        {
            #ifdef _DEBUG
            AppDebugOut("RendererD3D11: %s failed: %s\n", operation, (char*)errorBlob->GetBufferPointer());
            #endif
            errorBlob->Release();
        }
        else
        {
            #ifdef _DEBUG
            AppDebugOut("RendererD3D11: %s failed, error: 0x%08X\n", operation, hr);
            #endif
        }
        return false;
    }
    return true;
}

bool RendererD3D11::CheckHR(HRESULT hr, const char* operation)
{
    if (FAILED(hr)) 
    {
        #ifdef _DEBUG
        AppDebugOut("RendererD3D11: %s failed, error: 0x%08X\n", operation, hr);
        #endif
        return false;
    }
    return true;
}

bool RendererD3D11::CheckHR(HRESULT hr, const char* operation, ID3DBlob* errorBlob)
{
    if (FAILED(hr)) 
    {
        if (errorBlob) 
        {
            #ifdef _DEBUG
            AppDebugOut("RendererD3D11: %s failed: %s\n", operation, (char*)errorBlob->GetBufferPointer());
            #endif
            errorBlob->Release();
        }
        else
        {
            #ifdef _DEBUG
            AppDebugOut("RendererD3D11: %s failed, error: 0x%08X\n", operation, hr);
            #endif
        }
        return false;
    }
    return true;
}

D3D11_BLEND RendererD3D11::ConvertBlendFactor(int factor)
{
    switch (factor) {
        case BLEND_ZERO: return D3D11_BLEND_ZERO;
        case BLEND_ONE: return D3D11_BLEND_ONE;
        case BLEND_SRC_ALPHA: return D3D11_BLEND_SRC_ALPHA;
        case BLEND_ONE_MINUS_SRC_ALPHA: return D3D11_BLEND_INV_SRC_ALPHA;
        case BLEND_ONE_MINUS_SRC_COLOR: return D3D11_BLEND_INV_SRC_COLOR;
        default: return D3D11_BLEND_ONE;
    }
}

void RendererD3D11::UpdateBlendState()
{
    if (!m_blendStateNormal) 
    {
        CreateStateObjects();
    }
    
    if (!m_deviceContext || !m_currentBlendState) return;
    
    float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    m_deviceContext->OMSetBlendState(m_currentBlendState, blendFactor, 0xffffffff);
}

void RendererD3D11::UpdateDepthState()
{
    if (!m_depthStateDisabled) 
    {
        CreateStateObjects();
    }
    
    if (!m_deviceContext || !m_currentDepthState) return;
    
    m_deviceContext->OMSetDepthStencilState(m_currentDepthState, 0);
}

void RendererD3D11::UpdateRasterizerState()
{
    if (!m_rasterizerStateNoCull) 
    {
        CreateStateObjects();
    }
    
    if (!m_deviceContext || !m_currentRasterizerState) return;
    
    m_deviceContext->RSSetState(m_currentRasterizerState);
}

ID3D11RasterizerState* RendererD3D11::SelectRasterizerState(bool scissorEnabled, bool cullEnabled, int cullMode)
{
    if (!scissorEnabled) 
    {
        if (!cullEnabled) 
        {
            return m_rasterizerStateNoCull;
        } 
        else 
        {
            switch (cullMode) 
            {
                case CULL_FACE_BACK:
                    return m_rasterizerStateCullBack;
                case CULL_FACE_FRONT:
                    return m_rasterizerStateCullFront;
                case CULL_FACE_FRONT_AND_BACK:
                    return m_rasterizerStateCullBoth;
                default:
                    return m_rasterizerStateNoCull;
            }
        }
    } 
    else 
    {
        if (!cullEnabled) 
        {
            return m_rasterizerStateNoCullScissor;
        } 
        else 
        {
            switch (cullMode) 
            {
                case CULL_FACE_BACK:
                    return m_rasterizerStateCullBackScissor;
                case CULL_FACE_FRONT:
                    return m_rasterizerStateCullFrontScissor;
                case CULL_FACE_FRONT_AND_BACK:
                    return m_rasterizerStateCullBothScissor;
                default:
                    return m_rasterizerStateNoCullScissor;
            }
        }
    }
}

// ============================================================================
// STATE MANAGEMENT
// ============================================================================

void RendererD3D11::SetViewport(int x, int y, int width, int height) 
{
    if (m_currentViewportX != x || m_currentViewportY != y || 
        m_currentViewportWidth != width || m_currentViewportHeight != height) 
    {
        
        CreateStateObjects();
        if (!m_deviceContext) return;
        
        //
        // D3D is top left origin

        int screenHeight = g_windowManager->DrawableHeight();
        int d3dY = screenHeight - y - height;
        
        D3D11_VIEWPORT viewport = {};
        viewport.TopLeftX = (float)x;
        viewport.TopLeftY = (float)d3dY;
        viewport.Width = (float)width;
        viewport.Height = (float)height;
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        
        m_deviceContext->RSSetViewports(1, &viewport);
        
        m_currentViewportX = x;
        m_currentViewportY = y;
        m_currentViewportWidth = width;
        m_currentViewportHeight = height;
    }
}

void RendererD3D11::SetActiveTexture(unsigned int textureUnit)
{
    m_currentActiveTexture = textureUnit;
}

void RendererD3D11::SetShaderProgram(unsigned int program)
{
    if (m_currentShaderProgram != program) 
    {
        auto it = m_shaderPrograms.find(program);
        if (it != m_shaderPrograms.end() && m_deviceContext) 
        {
            m_deviceContext->VSSetShader(it->second.vertexShader, nullptr, 0);
            m_deviceContext->PSSetShader(it->second.pixelShader, nullptr, 0);
            m_deviceContext->IASetInputLayout(it->second.inputLayout);
        }
        m_currentShaderProgram = program;
    }
}

void RendererD3D11::SetVertexArray(unsigned int vao)
{
    m_currentVAO = vao;
    
    if (g_renderer2d) 
    {
        Renderer2DD3D11* renderer2dD3D11 = dynamic_cast<Renderer2DD3D11*>(g_renderer2d);
        if (renderer2dD3D11) {
            renderer2dD3D11->UpdateCurrentVAO(vao);
        }
    }
}

void RendererD3D11::SetArrayBuffer(unsigned int buffer)
{
    m_currentArrayBuffer = buffer;
}

void RendererD3D11::SetElementBuffer(unsigned int buffer)
{
    m_currentElementBuffer = buffer;
}

void RendererD3D11::SetLineWidth(float width)
{
    m_currentLineWidth = width;
}

void RendererD3D11::SetBoundTexture(unsigned int texture)
{
    if (m_currentBoundTexture != texture) 
    {
        m_currentBoundTexture = texture;
        m_textureSwitches++;
    }
}

void RendererD3D11::SetScissorTest(bool enabled)
{
    if (m_scissorTestEnabled != enabled) 
    {
        m_scissorTestEnabled = enabled;
        
        if (!m_rasterizerStateNoCull) 
        {
            CreateStateObjects();
        }
        
        ID3D11RasterizerState* newState = SelectRasterizerState(enabled, m_cullFaceEnabled, m_cullFaceMode);
        
        if (newState != m_currentRasterizerState) 
        {
            m_currentRasterizerState = newState;
            UpdateRasterizerState();
        }
    }
}

void RendererD3D11::SetScissor(int x, int y, int width, int height) 
{
    if (m_currentScissorX != x || m_currentScissorY != y || 
        m_currentScissorWidth != width || m_currentScissorHeight != height) 
    {
        
        CreateStateObjects();
        if (!m_deviceContext) return;

        int screenHeight = g_windowManager->DrawableHeight();
        int d3dY = screenHeight - y - height;
        
        D3D11_RECT rect;
        rect.left = x;
        rect.top = d3dY;
        rect.right = x + width;
        rect.bottom = d3dY + height;
        
        m_deviceContext->RSSetScissorRects(1, &rect);
        
        m_currentScissorX = x;
        m_currentScissorY = y;
        m_currentScissorWidth = width;
        m_currentScissorHeight = height;
    }
}

void RendererD3D11::SetTextureParameter(unsigned int pname, int param)
{
}

// ============================================================================
// BLEND MODES & DEPTH
// ============================================================================

void RendererD3D11::SetBlendMode(int _blendMode)
{
    //
    // Flush any existing sprites if the blend mode changes
    
    if (m_blendMode != _blendMode && g_renderer2d->GetCurrentStaticSpriteVertexCount() > 0) 
    {
        g_renderer2d->EndStaticSpriteBatch();
    }

    m_blendMode = _blendMode;
    
    switch (_blendMode) 
    {
        case BlendModeDisabled:
            m_currentBlendState = m_blendStateDisabled;
            m_blendEnabled = false;
            break;
        case BlendModeNormal:
            SetBlendFunc(BLEND_SRC_ALPHA, BLEND_ONE_MINUS_SRC_ALPHA);
            m_currentBlendState = m_blendStateNormal;
            m_blendEnabled = true;
            break;
        case BlendModeAdditive:
            SetBlendFunc(BLEND_SRC_ALPHA, BLEND_ONE);
            m_currentBlendState = m_blendStateAdditive;
            m_blendEnabled = true;
            break;
        case BlendModeSubtractive:
            SetBlendFunc(BLEND_SRC_ALPHA, BLEND_ONE_MINUS_SRC_COLOR);
            m_currentBlendState = m_blendStateSubtractive;
            m_blendEnabled = true;
            break;
    }
    
    UpdateBlendState();
}

void RendererD3D11::SetBlendFunc(int srcFactor, int dstFactor)
{
    m_blendSrcFactor = srcFactor;
    m_blendDstFactor = dstFactor;
    m_currentBlendSrcFactor = srcFactor;
    m_currentBlendDstFactor = dstFactor;
}

void RendererD3D11::SetDepthBuffer(bool _enabled, bool _clearNow)
{
    if (_enabled) 
    {
        m_currentDepthState = m_depthStateEnabled;
        m_depthTestEnabled = true;
        m_depthMaskEnabled = true;
    } 
    else 
    {
        m_currentDepthState = m_depthStateDisabled;
        m_depthTestEnabled = false;
        m_depthMaskEnabled = false;
    }
    
    UpdateDepthState();
 
    if (_clearNow && m_deviceContext) 
    {
        WindowManagerD3D11* windowManager = dynamic_cast<WindowManagerD3D11*>(g_windowManager);
        if (windowManager && windowManager->GetDepthStencilView()) 
        {
            m_deviceContext->ClearDepthStencilView(windowManager->GetDepthStencilView(), 
                                                   D3D11_CLEAR_DEPTH, 1.0f, 0);
        }
    }
}

void RendererD3D11::SetDepthMask(bool enabled)
{
    if (m_depthMaskEnabled != enabled) {
        m_depthMaskEnabled = enabled;
        
        //
        // Recreate depth state with new mask

        if (m_device) 
        {
            D3D11_DEPTH_STENCIL_DESC desc = {};
            desc.DepthEnable = m_depthTestEnabled;
            desc.DepthWriteMask = enabled ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
            desc.DepthFunc = D3D11_COMPARISON_LESS;
            desc.StencilEnable = FALSE;
            
            ID3D11DepthStencilState* newState = nullptr;
            HRESULT hr = m_device->CreateDepthStencilState(&desc, &newState);
            if (SUCCEEDED(hr) && newState) 
            {
                if (m_currentDepthState && m_currentDepthState != m_depthStateEnabled && 
                    m_currentDepthState != m_depthStateDisabled) 
                {
                    m_currentDepthState->Release();
                }

                m_currentDepthState = newState;
                UpdateDepthState();
            }
        }
    }
}

void RendererD3D11::SetCullFace(bool enabled, int mode)
{
    m_cullFaceEnabled = enabled;
    m_cullFaceMode = mode;
    
    if (!m_rasterizerStateNoCull) 
    {
        CreateStateObjects();
    }
    
    ID3D11RasterizerState* newState = SelectRasterizerState(m_scissorTestEnabled, enabled, mode);
    
    if (newState != m_currentRasterizerState) 
    {
        m_currentRasterizerState = newState;
        UpdateRasterizerState();
    }
}

// ============================================================================
// SHADER SETUP
// ============================================================================

unsigned int RendererD3D11::CreateShader(const char* vertexSource, const char* fragmentSource)
{
    if (!m_device || !m_deviceContext) 
    {
        GetDeviceAndContext();
    }
    
    if (!m_device || !m_deviceContext) 
    {
        #ifdef _DEBUG
        AppDebugOut("RendererD3D11: Device not available for shader creation\n");
        #endif
        return 0;
    }
    
    HRESULT hr;
    ID3DBlob* vertexBlob = nullptr;
    ID3DBlob* pixelBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;
    
    //
    // Compile vertex shader
    
    hr = D3DCompile(vertexSource, strlen(vertexSource), nullptr, nullptr, nullptr, "main",
                    "vs_5_0", 0, 0, &vertexBlob, &errorBlob);
    
    if (!CheckHR(hr, "vertex shader compilation", errorBlob)) 
    {
        if (vertexBlob) vertexBlob->Release();
        return 0;
    }
    
    //
    // Compile pixel shader
    
    hr = D3DCompile(fragmentSource, strlen(fragmentSource), nullptr, nullptr, nullptr, "main",
                    "ps_5_0", 0, 0, &pixelBlob, &errorBlob);
    
    if (!CheckHR(hr, "pixel shader compilation", errorBlob)) 
    {
        if (vertexBlob) vertexBlob->Release();
        if (pixelBlob) pixelBlob->Release();
        return 0;
    }
    
    //
    // Create shader objects
    
    ID3D11VertexShader* vertexShader = nullptr;
    ID3D11PixelShader* pixelShader = nullptr;
    
    hr = m_device->CreateVertexShader(vertexBlob->GetBufferPointer(), vertexBlob->GetBufferSize(),
                                      nullptr, &vertexShader);
    if (!CheckHR(hr, "create vertex shader")) 
    {
        vertexBlob->Release();
        pixelBlob->Release();
        return 0;
    }
    
    hr = m_device->CreatePixelShader(pixelBlob->GetBufferPointer(), pixelBlob->GetBufferSize(),
                                     nullptr, &pixelShader);
    if (!CheckHR(hr, "create pixel shader")) 
    {   
        vertexShader->Release();
        vertexBlob->Release();
        pixelBlob->Release();
        return 0;
    }
    
    //
    // Create input layout, 
    // vertex format: position, color, texcoord
    
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    
    ID3D11InputLayout* inputLayout = nullptr;
    hr = m_device->CreateInputLayout(layout, ARRAYSIZE(layout),
                                     vertexBlob->GetBufferPointer(), vertexBlob->GetBufferSize(),
                                     &inputLayout);
    if (!CheckHR(hr, "create input layout")) 
    {
        vertexShader->Release();
        pixelShader->Release();
        vertexBlob->Release();
        pixelBlob->Release();
        return 0;
    }
    
    vertexBlob->Release();
    pixelBlob->Release();
    
    //
    // Store shader program
    
    unsigned int programId = m_nextShaderProgramId++;
    ShaderProgram program;
    program.vertexShader = vertexShader;
    program.pixelShader = pixelShader;
    program.inputLayout = inputLayout;
    m_shaderPrograms[programId] = program;
    
    return programId;
}

// ============================================================================
// FRAME COORDINATION
// ============================================================================

void RendererD3D11::BeginFrame()
{
    g_renderer2d->BeginFrame2D();
    g_renderer3d->BeginFrame3D();
    
    ResetFlushTimings();
    m_textureSwitches = 0;
}

void RendererD3D11::EndFrame()
{
    g_renderer2d->EndFrame2D();
    g_renderer3d->EndFrame3D();
    
    UpdateGpuTimings();
    
    m_prevTextureSwitches = m_textureSwitches;
}

// ============================================================================
// WINDOW MANAGEMENT
// ============================================================================

void RendererD3D11::HandleWindowResize()
{
    int screenW = g_windowManager->DrawableWidth();
    int screenH = g_windowManager->DrawableHeight();

    ResizeMSAAFramebuffer(screenW, screenH);
    
    if (g_renderer2d) 
    {
        g_renderer2d->Reset2DViewport();
    }
}

// ============================================================================
// GPU TIMING
// ============================================================================

void RendererD3D11::StartFlushTiming(const char* name)
{
    m_currentFlushStartTime = GetHighResTime();
    
    if (!m_device || !m_deviceContext) return;
    
    //
    // Find timing entry
    
    for (int i = 0; i < m_timingQueryCount; i++) 
    {
        if (m_flushTimings[i].name && strcmp(m_flushTimings[i].name, name) == 0) 
        {
            TimingQuery& query = m_timingQueries[i];
            
            if (!query.disjointQuery) 
            {
                D3D11_QUERY_DESC desc = {};
                desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
                m_device->CreateQuery(&desc, &query.disjointQuery);
                
                desc.Query = D3D11_QUERY_TIMESTAMP;
                m_device->CreateQuery(&desc, &query.beginQuery);
                m_device->CreateQuery(&desc, &query.endQuery);
            }
            
            if (!query.queryPending) 
            {
                m_deviceContext->Begin(query.disjointQuery);
                m_deviceContext->End(query.beginQuery);
                query.queryPending = true;
            }
            return;
        }
    }
    
    //
    // Create new timing entry
    
    if (m_flushTimingCount < MAX_FLUSH_TYPES) 
    {
        FlushTiming* timing = &m_flushTimings[m_flushTimingCount];
        TimingQuery& query = m_timingQueries[m_flushTimingCount];
        
        timing->name = name;
        timing->totalTime = 0.0;
        timing->totalGpuTime = 0.0;
        timing->callCount = 0;
        timing->queryObject = 0;
        timing->queryPending = false;
        
        D3D11_QUERY_DESC desc = {};
        desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
        m_device->CreateQuery(&desc, &query.disjointQuery);
        
        desc.Query = D3D11_QUERY_TIMESTAMP;
        m_device->CreateQuery(&desc, &query.beginQuery);
        m_device->CreateQuery(&desc, &query.endQuery);
        
        m_deviceContext->Begin(query.disjointQuery);
        m_deviceContext->End(query.beginQuery);
        query.queryPending = true;
        
        m_flushTimingCount++;
        m_timingQueryCount++;
    }
}

void RendererD3D11::EndFlushTiming(const char* name)
{
    double cpuTime = GetHighResTime() - m_currentFlushStartTime;
    
    if (!m_deviceContext) return;
    
    for (int i = 0; i < m_timingQueryCount; i++) 
    {
        if (m_flushTimings[i].name && strcmp(m_flushTimings[i].name, name) == 0) 
        {
            TimingQuery& query = m_timingQueries[i];
            
            if (query.queryPending) 
            {
                m_deviceContext->End(query.endQuery);
                m_deviceContext->End(query.disjointQuery);
                query.queryPending = false;
            }
            
            m_flushTimings[i].totalTime += cpuTime;
            m_flushTimings[i].callCount++;
            return;
        }
    }
}

void RendererD3D11::UpdateGpuTimings()
{
    if (!m_deviceContext) return;
    
    for (int i = 0; i < m_timingQueryCount; i++) 
    {
        TimingQuery& query = m_timingQueries[i];
        
        if (!query.queryPending && query.disjointQuery && query.beginQuery && query.endQuery) 
        {
            D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjointData;
            if (m_deviceContext->GetData(query.disjointQuery, &disjointData, sizeof(disjointData), 0) == S_OK) 
            {
                if (!disjointData.Disjoint) 
                {
                    UINT64 beginTime, endTime;
                    if (m_deviceContext->GetData(query.beginQuery, &beginTime, sizeof(beginTime), 0) == S_OK &&
                        m_deviceContext->GetData(query.endQuery, &endTime, sizeof(endTime), 0) == S_OK) 
                    {
                        double gpuTime = (endTime - beginTime) / (double)disjointData.Frequency * 1000.0; // Convert to milliseconds
                        m_flushTimings[i].totalGpuTime += gpuTime;
                    }
                }
            }
        }
    }
}

void RendererD3D11::ResetFlushTimings()
{
    for (int i = 0; i < m_flushTimingCount; i++) 
    {
        m_flushTimings[i].totalTime = 0.0;
        m_flushTimings[i].totalGpuTime = 0.0;
        m_flushTimings[i].callCount = 0;
    }
}

// ============================================================================
// SCENE MANAGEMENT
// ============================================================================

void RendererD3D11::BeginScene()
{
    if (!m_device || !m_deviceContext) 
    {
        GetDeviceAndContext();
    }
    
    if (!m_blendStateNormal) 
    {
        CreateStateObjects();
    }

    //
    // Only bind render targets if MSAA is not enabled
    
    if (m_deviceContext && !m_msaaEnabled) 
    {
        WindowManagerD3D11* windowManager = dynamic_cast<WindowManagerD3D11*>(g_windowManager);
        if (windowManager) 
        {
            ID3D11RenderTargetView* renderTarget = windowManager->GetRenderTargetView();
            ID3D11DepthStencilView* depthStencil = windowManager->GetDepthStencilView();
            if (renderTarget) 
            {
                m_deviceContext->OMSetRenderTargets(1, &renderTarget, depthStencil);
            }
        }
    }
    
    UpdateBlendState();
    UpdateDepthState();
    UpdateRasterizerState();
    
    if (m_currentSamplerState == nullptr && m_samplerStateLinear) 
    {
        m_currentSamplerState = m_samplerStateLinear;
    }
    
    if (m_deviceContext && m_currentSamplerState) 
    {
        m_deviceContext->PSSetSamplers(0, 1, &m_currentSamplerState);
    }
    
    SetBoundTexture(0);
    SetBlendMode(BlendModeNormal);
}

void RendererD3D11::ClearScreen(bool colour, bool depth) 
{
    CreateStateObjects();
    if (!m_deviceContext) return;
    
    WindowManagerD3D11* windowManager = dynamic_cast<WindowManagerD3D11*>(g_windowManager);
    if (!windowManager) return;
    
    ID3D11RenderTargetView* renderTarget = m_msaaEnabled && m_msaaRenderTargetView 
        ? m_msaaRenderTargetView 
        : windowManager->GetRenderTargetView();
    
    ID3D11DepthStencilView* depthStencil = m_msaaEnabled && m_msaaDepthStencilView
        ? m_msaaDepthStencilView
        : windowManager->GetDepthStencilView();
    
    if (colour && renderTarget) 
    {
        float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        m_deviceContext->ClearRenderTargetView(renderTarget, clearColor);
    }
    
    if (depth && depthStencil) 
    {
        m_deviceContext->ClearDepthStencilView(depthStencil, 
            D3D11_CLEAR_DEPTH, 1.0f, 0);
    }
}

// ============================================================================
// MSAA FRAMEBUFFER IMPLEMENTATION
// ============================================================================

void RendererD3D11::InitializeMSAAFramebuffer(int width, int height, int samples)
{
    m_msaaWidth = width;
    m_msaaHeight = height;
    m_msaaSamples = samples;
    
    if (!m_device || !m_deviceContext) 
    {
        GetDeviceAndContext();
    }
    
    if (!m_device || samples <= 0) 
    {
        m_msaaEnabled = false;
        m_msaaRenderTarget = nullptr;
        m_msaaRenderTargetView = nullptr;
        m_msaaDepthStencil = nullptr;
        m_msaaDepthStencilView = nullptr;
        return;
    }
    
    DestroyMSAAFramebuffer();
    
    m_msaaSamples = samples;
    
    HRESULT hr;
    
    //
    // Check MSAA support
    // Note: On a GTX 1080, 16x MSAA is not supported
    
    UINT numQualityLevels = 0;
    hr = m_device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, samples, &numQualityLevels);
    if (FAILED(hr) || numQualityLevels == 0) 
    {
        AppDebugOut("MSAA %dx not supported, falling back to no MSAA\n", samples);
        m_msaaEnabled = false;
        return;
    }
    
    //
    // Create MSAA render target
    
    D3D11_TEXTURE2D_DESC renderTargetDesc = {};
    renderTargetDesc.Width = width;
    renderTargetDesc.Height = height;
    renderTargetDesc.MipLevels = 1;
    renderTargetDesc.ArraySize = 1;
    renderTargetDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    renderTargetDesc.SampleDesc.Count = samples;
    renderTargetDesc.SampleDesc.Quality = numQualityLevels - 1;
    renderTargetDesc.Usage = D3D11_USAGE_DEFAULT;
    renderTargetDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
    renderTargetDesc.CPUAccessFlags = 0;
    renderTargetDesc.MiscFlags = 0;
    
    hr = m_device->CreateTexture2D(&renderTargetDesc, nullptr, &m_msaaRenderTarget);
    if (!CheckHR(hr, "create MSAA render target")) 
    {
        m_msaaEnabled = false;
        return;
    }
    
    hr = m_device->CreateRenderTargetView(m_msaaRenderTarget, nullptr, &m_msaaRenderTargetView);
    if (!CheckHR(hr, "create MSAA render target view")) 
    {
        m_msaaRenderTarget->Release();
        m_msaaRenderTarget = nullptr;
        m_msaaEnabled = false;
        return;
    }
    
    //
    // Create MSAA depth stencil
    
    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = width;
    depthDesc.Height = height;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = samples;
    depthDesc.SampleDesc.Quality = numQualityLevels - 1;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    depthDesc.CPUAccessFlags = 0;
    depthDesc.MiscFlags = 0;
    
    hr = m_device->CreateTexture2D(&depthDesc, nullptr, &m_msaaDepthStencil);
    if (!CheckHR(hr, "create MSAA depth stencil")) 
    {
        m_msaaRenderTargetView->Release();
        m_msaaRenderTarget->Release();
        m_msaaRenderTargetView = nullptr;
        m_msaaRenderTarget = nullptr;
        m_msaaEnabled = false;
        return;
    }
    
    hr = m_device->CreateDepthStencilView(m_msaaDepthStencil, nullptr, &m_msaaDepthStencilView);
    if (!CheckHR(hr, "create MSAA depth stencil view")) 
    {
        m_msaaDepthStencil->Release();
        m_msaaRenderTargetView->Release();
        m_msaaRenderTarget->Release();
        m_msaaDepthStencil = nullptr;
        m_msaaRenderTargetView = nullptr;
        m_msaaRenderTarget = nullptr;
        m_msaaEnabled = false;
        return;
    }
    
    m_msaaEnabled = true;

    #ifdef _DEBUG
    AppDebugOut("MSAA %dx applied successfully\n", samples);
    #endif
}

void RendererD3D11::ResizeMSAAFramebuffer(int width, int height)
{
    if (!m_msaaEnabled || m_msaaSamples <= 0) 
    {
        return;
    }
    
    if (width == m_msaaWidth && height == m_msaaHeight) 
    {
        return;
    }

    int preservedSamples = m_msaaSamples;
    
    DestroyMSAAFramebuffer();
    InitializeMSAAFramebuffer(width, height, preservedSamples);
}

void RendererD3D11::DestroyMSAAFramebuffer()
{
    if (m_msaaRenderTargetView) 
    {
        m_msaaRenderTargetView->Release();
        m_msaaRenderTargetView = nullptr;
    }
    
    if (m_msaaRenderTarget) 
    {
        m_msaaRenderTarget->Release();
        m_msaaRenderTarget = nullptr;
    }
    
    if (m_msaaDepthStencilView) 
    {
        m_msaaDepthStencilView->Release();
        m_msaaDepthStencilView = nullptr;
    }
    
    if (m_msaaDepthStencil) 
    {
        m_msaaDepthStencil->Release();
        m_msaaDepthStencil = nullptr;
    }
    
    m_msaaEnabled = false;
    m_msaaSamples = 0;
    m_msaaWidth = 0;
    m_msaaHeight = 0;
}

void RendererD3D11::BeginMSAARendering()
{
    
    if (!m_deviceContext) return;
    
    if (m_msaaEnabled && m_msaaRenderTargetView) 
    {
        m_deviceContext->OMSetRenderTargets(1, &m_msaaRenderTargetView, m_msaaDepthStencilView);
    }
}

void RendererD3D11::EndMSAARendering()
{
    if (m_msaaEnabled && m_msaaRenderTarget && m_deviceContext) 
    {
        WindowManagerD3D11* windowManager = dynamic_cast<WindowManagerD3D11*>(g_windowManager);
        if (windowManager && windowManager->GetRenderTargetView()) 
        {
            //
            // Resolve MSAA render target to back buffer

            ID3D11Resource* backBuffer = nullptr;
            windowManager->GetRenderTargetView()->GetResource(&backBuffer);
            
            if (backBuffer) 
            {
                m_deviceContext->ResolveSubresource(backBuffer, 0, m_msaaRenderTarget, 0, 
                                                    DXGI_FORMAT_R8G8B8A8_UNORM);
                backBuffer->Release();
            }
            
            //
            // Restore normal render target

            ID3D11RenderTargetView* renderTarget = windowManager->GetRenderTargetView();
            ID3D11DepthStencilView* depthStencil = windowManager->GetDepthStencilView();
            m_deviceContext->OMSetRenderTargets(1, &renderTarget, depthStencil);
        }
    }
}

// ============================================================================
// SCREENSHOTS
// ============================================================================

void RendererD3D11::SaveScreenshot()
{
    if (!m_device || !m_deviceContext) return;
    
    #ifdef _DEBUG
    float timeNow = GetHighResTime();
    #endif
    
    char *screenshotsDir = ScreenshotsDirectory();

    int screenW = g_windowManager->DrawableWidth();
    int screenH = g_windowManager->DrawableHeight();
    
    WindowManagerD3D11* windowManager = dynamic_cast<WindowManagerD3D11*>(g_windowManager);
    if (!windowManager) 
    {
        delete[] screenshotsDir;
        return;
    }
    
    ID3D11RenderTargetView* renderTarget = windowManager->GetRenderTargetView();
    if (!renderTarget) 
    {
        delete[] screenshotsDir;
        return;
    }
    
    //
    // Get back buffer texture
    
    ID3D11Resource* backBufferResource = nullptr;
    renderTarget->GetResource(&backBufferResource);
    
    if (!backBufferResource) 
    {
        delete[] screenshotsDir;
        return;
    }
    
    //
    // Create staging texture to read from
    
    D3D11_TEXTURE2D_DESC desc = {};
    ID3D11Texture2D* backBufferTexture = nullptr;
    backBufferResource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&backBufferTexture);
    backBufferResource->Release();
    
    if (!backBufferTexture) 
    {
        delete[] screenshotsDir;
        return;
    }
    
    backBufferTexture->GetDesc(&desc);
    
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    
    ID3D11Texture2D* stagingTexture = nullptr;
    HRESULT hr = m_device->CreateTexture2D(&desc, nullptr, &stagingTexture);
    
    if (!CheckHR(hr, "create staging texture") || !stagingTexture) 
    {
        backBufferTexture->Release();
        delete[] screenshotsDir;
        return;
    }
    
    m_deviceContext->CopyResource(stagingTexture, backBufferTexture);
    backBufferTexture->Release();
    
    //
    // Map and read pixel data
    
    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = m_deviceContext->Map(stagingTexture, 0, D3D11_MAP_READ, 0, &mapped);
    
    if (!CheckHR(hr, "map staging texture")) 
    {
        stagingTexture->Release();
        delete[] screenshotsDir;
        return;
    }
    
    Bitmap bitmap(screenW, screenH);
    bitmap.Clear(Black);
    
    unsigned char* src = (unsigned char*)mapped.pData;
    for (int y = 0; y < screenH; ++y) 
    {
        unsigned char* line = src + y * mapped.RowPitch;
        for (int x = 0; x < screenW; ++x) 
        {
            unsigned char* p = line + x * 4; // RGBA
            bitmap.PutPixel(x, screenH - 1 - y, Colour(p[0], p[1], p[2]));
        }
    }
    
    m_deviceContext->Unmap(stagingTexture, 0);
    stagingTexture->Release();
    
    //
    // Save bitmap
    
    int screenshotIndex = 1;
    while (true) 
    {
        time_t theTimeT = time(NULL);
        tm *theTime = localtime(&theTimeT);
        char filename[2048];
        
        snprintf(filename, sizeof(filename), "%s/screenshot %02d-%02d-%02d %02d.bmp", 
                screenshotsDir, 1900+theTime->tm_year, theTime->tm_mon+1, theTime->tm_mday, screenshotIndex);
        
        if (!DoesFileExist(filename)) 
        {
            bitmap.SaveBmp(filename);
            break;
        }
        ++screenshotIndex;
    }

    delete[] screenshotsDir;

    #ifdef _DEBUG
    float timeTaken = GetHighResTime() - timeNow;
    AppDebugOut("Screenshot %dms\n", int(timeTaken*1000));
    #endif
}

int RendererD3D11::GetCurrentBlendSrcFactor() const
{
    return m_currentBlendSrcFactor;
}

int RendererD3D11::GetCurrentBlendDstFactor() const
{
    return m_currentBlendDstFactor;
}

// ============================================================================
// TEXTURE MANAGEMENT
// ============================================================================

unsigned int RendererD3D11::CreateTexture(int width, int height, const Colour* pixels, bool mipmapping)
{
    if (!m_device || !m_deviceContext) {
        GetDeviceAndContext();
    }
    
    if (!m_device || !m_deviceContext) 
    {
        return 0;
    }
    
    HRESULT hr;
    
    //
    // Create texture description

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = mipmapping ? 0 : 1; // 0 = generate all mip levels
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | (mipmapping ? D3D11_BIND_RENDER_TARGET : 0);
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = mipmapping ? D3D11_RESOURCE_MISC_GENERATE_MIPS : 0;
    
    //
    // Create initial data structure

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = pixels;
    initData.SysMemPitch = width * 4; // 4 bytes per pixel (RGBA)
    initData.SysMemSlicePitch = 0;
    
    //
    // Create texture

    ID3D11Texture2D* texture = nullptr;

    //
    // When using D3D11_RESOURCE_MISC_GENERATE_MIPS, we cant provide initial data
    // Create without data, then upload base level separately

    hr = m_device->CreateTexture2D(&desc, mipmapping ? nullptr : &initData, &texture);
    if (!CheckHR(hr, "create texture")) 
    {
        return 0;
    }
    
    //
    // If mipmapping, upload base level data

    if (mipmapping) 
    {
        m_deviceContext->UpdateSubresource(texture, 0, nullptr, pixels, width * 4, 0);
    }
    
    //
    // Create shader resource view

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = mipmapping ? -1 : 1; 
    srvDesc.Texture2D.MostDetailedMip = 0;
    
    ID3D11ShaderResourceView* srv = nullptr;
    hr = m_device->CreateShaderResourceView(texture, &srvDesc, &srv);
    if (!CheckHR(hr, "create shader resource view")) 
    {
        texture->Release();
        return 0;
    }
    
    if (mipmapping) 
    {
        m_deviceContext->GenerateMips(srv);
    }
    
    return (unsigned int)(uintptr_t)srv;
}

void RendererD3D11::DeleteTexture(unsigned int textureID)
{
    if (textureID == 0) return;
    
    ID3D11ShaderResourceView* srv = (ID3D11ShaderResourceView*)(uintptr_t)textureID;
    
    if (srv) 
    {
        ID3D11Resource* resource = nullptr;
        srv->GetResource(&resource);
        
        srv->Release();
        
        if (resource) 
        {
            resource->Release();
        }
    }
}

#endif // RENDERER_DIRECTX11