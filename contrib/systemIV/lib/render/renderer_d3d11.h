/*
 * ========
 * RENDERER DIRECTX11
 * ========
 */

 #ifndef _included_renderer_d3d11_h
 #define _included_renderer_d3d11_h
 
 #include "renderer.h"
 
 #ifdef RENDERER_DIRECTX11
 
 #include <d3d11.h>
 #include <d3dcompiler.h>
 #include <map>
 
 class RendererD3D11 : public Renderer
 {
 private:
     int m_currentViewportX;
     int m_currentViewportY;
     int m_currentViewportWidth;
     int m_currentViewportHeight;
     int m_currentScissorX;
     int m_currentScissorY;
     int m_currentScissorWidth;
     int m_currentScissorHeight;
     bool m_scissorTestEnabled;
     
     unsigned int m_currentActiveTexture;
     unsigned int m_currentBoundTexture;
     
     unsigned int m_currentShaderProgram;
     unsigned int m_currentVAO;
     unsigned int m_currentArrayBuffer;
     unsigned int m_currentElementBuffer;
     
     bool m_blendEnabled;
     int m_currentBlendSrcFactor;
     int m_currentBlendDstFactor;
     bool m_depthTestEnabled;
     bool m_depthMaskEnabled;
     int m_depthComparisonFunc;
     bool m_cullFaceEnabled;
     int m_cullFaceMode;
     bool m_colorMaskR;
     bool m_colorMaskG;
     bool m_colorMaskB;
     bool m_colorMaskA;
     
     // Line width doesnt work in DirectX11 :(
     float m_currentLineWidth;
     
     ID3D11Device* m_device;
     ID3D11DeviceContext* m_deviceContext;
     
     ID3D11BlendState* m_blendStateDisabled;
     ID3D11BlendState* m_blendStateNormal;
     ID3D11BlendState* m_blendStateAdditive;
     ID3D11BlendState* m_blendStateSubtractive;
     ID3D11BlendState* m_currentBlendState;
     ID3D11BlendState* m_currentlyBoundBlendState;
     
     ID3D11DepthStencilState* m_depthStateEnabled;
     ID3D11DepthStencilState* m_depthStateDisabled;
     ID3D11DepthStencilState* m_currentDepthState;
     ID3D11DepthStencilState* m_currentlyBoundDepthState;
     
     ID3D11RasterizerState* m_rasterizerStateNoCull;
     ID3D11RasterizerState* m_rasterizerStateCullBack;
     ID3D11RasterizerState* m_rasterizerStateCullFront;
     ID3D11RasterizerState* m_rasterizerStateCullBoth;
     ID3D11RasterizerState* m_rasterizerStateNoCullScissor;
     ID3D11RasterizerState* m_rasterizerStateCullBackScissor;
     ID3D11RasterizerState* m_rasterizerStateCullFrontScissor;
     ID3D11RasterizerState* m_rasterizerStateCullBothScissor;
     ID3D11RasterizerState* m_currentRasterizerState;
     ID3D11RasterizerState* m_currentlyBoundRasterizerState;
     
    ID3D11SamplerState* m_samplerStateLinear;           // For non mipmapped textures
    ID3D11SamplerState* m_samplerStateLinearMipLinear;  // For mipmapped textures
    ID3D11SamplerState* m_samplerStateNearest;          // For nearest filtering
    ID3D11SamplerState* m_samplerStateNearestMipNearest; // For nearest with mipmaps
    ID3D11SamplerState* m_currentSamplerState;
     
     struct ShaderProgram 
     {
         ID3D11VertexShader* vertexShader;
         ID3D11PixelShader* pixelShader;
         ID3D11InputLayout* inputLayout;
     };
 
     std::map<unsigned int, ShaderProgram> m_shaderPrograms;
     unsigned int m_nextShaderProgramId;
    
     std::map<unsigned int, ID3D11ShaderResourceView*> m_textureMap;
     unsigned int m_nextTextureId;
    
     struct TimingQuery
     {
         ID3D11Query* disjointQuery;
         ID3D11Query* beginQuery;
         ID3D11Query* endQuery;
         bool queryPending;
     };

     static const int MAX_QUERIES_PER_TYPE_D3D11 = 32;

     struct TimingQueryPool
     {
         TimingQuery queries[MAX_QUERIES_PER_TYPE_D3D11];
         int poolSize;
         int nextQueryIndex;
     };

     TimingQueryPool m_timingQueryPools[MAX_FLUSH_TYPES];
     int m_timingQueryCount;
 
 public:
     RendererD3D11();
     virtual ~RendererD3D11();
     
     virtual void SetViewport        (int x, int y, int width, int height) override;
     virtual void SetActiveTexture   (unsigned int textureUnit)            override;
     virtual void SetShaderProgram   (unsigned int program)                override;
     virtual void SetVertexArray     (unsigned int vao)                    override;
     virtual void SetArrayBuffer     (unsigned int buffer)                 override;
     virtual void SetElementBuffer   (unsigned int buffer)                 override;
     virtual void SetLineWidth       (float width)                         override;
     virtual float GetLineWidth      () const                              override;
     virtual void SetBoundTexture    (unsigned int texture)                override;
     virtual void SetScissorTest     (bool enabled)                        override;
     virtual void SetScissor         (int x, int y, int width, int height) override;
     virtual void SetTextureParameter(unsigned int pname, int param)       override;
     
     virtual void SetBlendMode       (int blendMode)                       override;
     virtual void SetBlendFunc       (int srcFactor, int dstFactor)        override;
     virtual void SetDepthBuffer     (bool enabled, bool clearNow)         override;
     virtual void SetDepthMask       (bool enabled)                        override;
     virtual void SetDepthComparison (int comparisonFunc)                  override;
     virtual void SetCullFace        (bool enabled, int mode)              override;
     virtual void SetColorMask       (bool r, bool g, bool b, bool a)      override;
     virtual void SetClearColor      (float r, float g, float b, float a)  override;
     
     virtual unsigned int CreateShader(const char* vertexSource, const char* fragmentSource) override;
     
     virtual void InitializeAntiAliasing(AntiAliasingType type, int width, int height, int samples) override;
     virtual void ResizeAntiAliasing    (int width, int height)                override;
 
     virtual void DestroyAntiAliasing() override;
     virtual void BeginAntiAliasing    () override;
     virtual void EndAntiAliasing      () override;
     
     virtual void BeginFrame () override;
     virtual void EndFrame   () override;
    
     virtual void Begin2DRendering() override;
     virtual void End2DRendering() override;
     virtual void Begin3DRendering() override;
     virtual void End3DRendering() override;
    
     virtual void ClearScreen(bool colour, bool depth) override;
     
     virtual void HandleWindowResize() override;
     
     virtual void StartFlushTiming (const char* name) override;
     virtual void EndFlushTiming   (const char* name) override;
     virtual void UpdateGpuTimings () override;
     virtual void ResetFlushTimings() override;
     
     virtual void SaveScreenshot() override;
     
     virtual unsigned int CreateTexture(int width, int height, const Colour* pixels, 
                                        int mipmapLevel) override;
     virtual void DeleteTexture        (unsigned int textureID) override;
     
     virtual unsigned int GetCurrentBoundTexture() const override { return m_currentBoundTexture; }
     virtual int GetCurrentBlendSrcFactor       () const override;
     virtual int GetCurrentBlendDstFactor       () const override;
    
     ID3D11ShaderResourceView* GetTextureSRV(unsigned int textureID) const;
    
    static bool CheckHRResult(HRESULT hr, const char* operation);
    static bool CheckHRResult(HRESULT hr, const char* operation, ID3DBlob* errorBlob);
    
    void UpdateBlendState();
    void UpdateDepthState();
    void UpdateRasterizerState();
    
    void SetupDeviceRenderState();
    void Setup2DRenderState();
    void Setup3DRenderState();
    void CleanupRenderState();
    
protected:
    bool CheckHR(HRESULT hr, const char* operation);
    bool CheckHR(HRESULT hr, const char* operation, ID3DBlob* errorBlob);

private:
    void CreateStateObjects();
    void ReleaseStateObjects();
    void GetDeviceAndContext();
    
    D3D11_BLEND                ConvertBlendFactor(int factor);
    D3D11_TEXTURE_ADDRESS_MODE ConvertTextureAddressMode(int mode);
    D3D11_COMPARISON_FUNC      ConvertDepthComparisonFunc(int func);
    D3D11_BLEND_OP             ConvertBlendOperation(int op);
    
    ID3D11RasterizerState* SelectRasterizerState(bool scissorEnabled, bool cullEnabled, int cullMode);
    
    UINT CalculateColorWriteMask(bool r, bool g, bool b, bool a) const;
    void SetupBlendDescForMode(D3D11_BLEND_DESC& blendDesc, int blendMode, UINT writeMask);
    void CreateAndSetBlendState(const D3D11_BLEND_DESC& blendDesc);
 };
 
 #endif // RENDERER_DIRECTX11
 #endif // _included_renderer_d3d11_h