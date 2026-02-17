#ifndef _included_renderer3d_d3d11_h
#define _included_renderer3d_d3d11_h

#ifdef RENDERER_DIRECTX11

#include "renderer_3d.h"
#include <d3d11.h>
#include <map>
#include <vector>

class Renderer3DD3D11 : public Renderer3D
{
private:
    ID3D11Device* m_device;
    ID3D11DeviceContext* m_deviceContext;
    
    ID3D11VertexShader* m_vertexShader3D;
    ID3D11PixelShader* m_pixelShader3D;
    ID3D11VertexShader* m_texturedVertexShader3D;
    ID3D11PixelShader* m_texturedPixelShader3D;
    ID3D11VertexShader* m_modelVertexShader3D;
    ID3D11PixelShader* m_modelPixelShader3D;
    ID3D11InputLayout* m_inputLayout3D;
    ID3D11InputLayout* m_inputLayoutTextured3D;
    ID3D11GeometryShader* m_lineGeometryShaderThin3D;
    ID3D11GeometryShader* m_lineGeometryShaderThick3D;
    
    struct TransformBuffer 
    {
        float uProjection[16];
        float uModelView[16];
    };
    
    struct FogBuffer 
    {
        int uFogEnabled;
        int uFogOrientationBased;
        float uFogStart;
        float uFogEnd;
        float uFogColor[4];
        float uCameraPos[3];
        float padding;
    };
    
    struct ModelBuffer 
    {
        float uModelMatrix[16];
        float uModelColor[4];
        int uUseInstancing;
        int uInstanceCount;
        float padding1;
        float padding2;
        
        static constexpr int MAX_INSTANCES = 64;
        float uModelMatrices[MAX_INSTANCES * 16];
        float uModelColors[MAX_INSTANCES * 4];
    };
    
    struct LineWidthBuffer3D 
    {
        float lineWidth;
        float viewportWidth;
        float viewportHeight;
        float padding;
    };
    
    static constexpr int MAX_INSTANCES = 64;
    
    ID3D11Buffer* m_transformConstantBuffer;
    ID3D11Buffer* m_fogConstantBuffer;
    ID3D11Buffer* m_modelConstantBuffer;
    ID3D11Buffer* m_lineWidthConstantBuffer3D;
    
    ID3D11Buffer* m_lineBuffer3D;
    ID3D11Buffer* m_staticSpriteBuffer3D;
    ID3D11Buffer* m_rotatingSpriteBuffer3D;
    ID3D11Buffer* m_textBuffer3D;
    ID3D11Buffer* m_circleBuffer3D;
    ID3D11Buffer* m_circleFillBuffer3D;
    ID3D11Buffer* m_rectBuffer3D;
    ID3D11Buffer* m_rectFillBuffer3D;
    ID3D11Buffer* m_triangleFillBuffer3D;
    
    struct LineStripSegment 
    {
        unsigned int startIndex;
        unsigned int indexCount;
    };
    
    std::map<unsigned int, std::vector<LineStripSegment>> m_lineStripSegments;
    
    ID3D11ShaderResourceView* m_currentTextureSRV;
    unsigned int m_currentTextureID;

    std::map<unsigned int, ID3D11Buffer*> m_vboMap;
    std::map<unsigned int, ID3D11Buffer*> m_iboMap;
    unsigned int m_nextVBOId;
    
    struct VAOBinding 
    {
        unsigned int vboId;
        unsigned int iboId;
        bool vboUploaded;
        bool iboUploaded;
        bool isTextured;
    };
    
    std::map<unsigned int, VAOBinding> m_vaoMap;
    unsigned int m_currentVAO;
    
    bool m_primitiveRestartEnabled;
    unsigned int m_primitiveRestartIndex;
    
    void GetDeviceAndContext();
    void CreateConstantBuffers();
    void UpdateTransformBuffer();
    void UpdateFogBuffer();
    void UpdateModelBuffer(const Matrix4f& modelMatrix, const Colour& modelColor);
    void UpdateModelBufferInstanced(const Matrix4f* modelMatrices, const Colour* modelColors, int instanceCount);
    
    void RegisterVBO(unsigned int vboId, ID3D11Buffer* buffer);
    void RegisterIBO(unsigned int iboId, ID3D11Buffer* buffer);
    
    ID3D11Buffer* GetVBOFromID(unsigned int vbo);
    ID3D11Buffer* GetIBOFromID(unsigned int ibo);
    
    void BindTexture(unsigned int textureID);
    bool UpdateBufferData(ID3D11Buffer* buffer, const Vertex3D* vertices, int vertexCount);
    bool UpdateBufferData(ID3D11Buffer* buffer, const Vertex3DTextured* vertices, int vertexCount);
    
    bool CheckHR(HRESULT hr, const char* operation);
    bool CheckHR(HRESULT hr, const char* operation, ID3DBlob* errorBlob);
    
public:
    Renderer3DD3D11();
    virtual ~Renderer3DD3D11();

    void UpdateCurrentVAO(unsigned int vao);
    void InvalidateStateCache();
    void HandleDeviceReset();
    void ReleaseShaderResources();
    void ReleaseDeviceAndContext();

protected:
    virtual void Initialize3DShaders          () override;
    virtual void Cache3DUniformLocations      () override;
    virtual void Setup3DVertexArrays          () override;
    virtual void Setup3DTexturedVertexArrays  () override;
    virtual void Set3DShaderUniforms          () override;
    virtual void SetTextured3DShaderUniforms  () override;
    virtual void SetLineShaderUniforms3D      (float lineWidth) override;

    virtual void SetFogUniforms3D                 (unsigned int shaderProgram) override;
    virtual void Set3DModelShaderUniforms         (const Matrix4f& modelMatrix, const Colour& modelColor) override;
    virtual void Set3DModelShaderUniformsInstanced(const Matrix4f* modelMatrices, const Colour* modelColors, 
                                                   int instanceCount) override;
    
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

#endif // RENDERER_DIRECTX11
#endif // _included_renderer3d_d3d11_h