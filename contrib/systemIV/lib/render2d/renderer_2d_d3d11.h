#ifndef _included_renderer2d_d3d11_h
#define _included_renderer2d_d3d11_h

#ifdef RENDERER_DIRECTX11

#include "renderer_2d.h"
#include <d3d11.h>
#include <map>
#include <vector>

class Renderer2DD3D11 : public Renderer2D
{
private:
    ID3D11Device* m_device;
    ID3D11DeviceContext* m_deviceContext;
    
    ID3D11VertexShader* m_colorVertexShader;
    ID3D11PixelShader* m_colorPixelShader;
    ID3D11VertexShader* m_textureVertexShader;
    ID3D11PixelShader* m_texturePixelShader;
    ID3D11InputLayout* m_inputLayout;
    ID3D11GeometryShader* m_lineGeometryShaderThin;
    ID3D11GeometryShader* m_lineGeometryShaderThick;
    ID3D11Buffer* m_lineWidthConstantBuffer;

    unsigned int m_lineShaderProgram;
    
    ID3D11VertexShader* m_currentlyBoundVS;
    ID3D11PixelShader* m_currentlyBoundPS;
    ID3D11GeometryShader* m_currentlyBoundGS;
    ID3D11InputLayout* m_currentlyBoundInputLayout;

    struct LineWidthBuffer 
    {
        float lineWidth;
        float viewportWidth;
        float viewportHeight;
        float padding;
    };
    
    struct TransformBuffer 
    {
        float uProjection[16];
        float uModelView[16];
    };

    bool m_matricesNeedUpdate;
    float m_lastProjectionMatrix[16];
    float m_lastModelViewMatrix[16];

    int m_batchedConstantBufferCount;
    bool m_supportsConstantBufferOffsets;

    ID3D11Buffer* m_transformConstantBuffer;
    ID3D11Buffer* m_batchedConstantBuffer;
    ID3D11Buffer* m_lineBuffer;
    ID3D11Buffer* m_staticSpriteBuffer;
    ID3D11Buffer* m_rotatingSpriteBuffer;
    ID3D11Buffer* m_textBuffer;
    ID3D11Buffer* m_circleBuffer;
    ID3D11Buffer* m_circleFillBuffer;
    ID3D11Buffer* m_rectBuffer;
    ID3D11Buffer* m_rectFillBuffer;
    ID3D11Buffer* m_triangleFillBuffer;
    ID3D11Buffer* m_immediateBuffer;

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
    };

    std::map<unsigned int, VAOBinding> m_vaoMap;
    unsigned int m_currentVAO;
    
    bool m_primitiveRestartEnabled;
    unsigned int m_primitiveRestartIndex;
    
    void GetDeviceAndContext();
    void CreateConstantBuffer();
    void UpdateConstantBuffer();
    void InvalidateStateCache();
    void RegisterVBO(unsigned int vboId, ID3D11Buffer* buffer);
    void RegisterIBO(unsigned int iboId, ID3D11Buffer* buffer);

    ID3D11Buffer* GetIBOFromID(unsigned int ibo);
    
public:
    Renderer2DD3D11();
    virtual ~Renderer2DD3D11();

    void UpdateCurrentVAO(unsigned int vao);
    
    virtual void Reset2DViewport() override;
    
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
    
    virtual void FlushTriangles          (bool useTexture) override;
    virtual void FlushTextBuffer         () override;
    virtual void FlushLines              () override;
    virtual void FlushStaticSprites      () override;
    virtual void FlushRotatingSprite     () override;
    virtual void FlushCircles            () override;
    virtual void FlushCircleFills        () override;
    virtual void FlushRects              () override;
    virtual void FlushRectFills          () override;
    virtual void FlushTriangleFills      () override;
    
    virtual void CleanupBuffers          () override;
    
    virtual unsigned int CreateMegaVBOVertexBuffer (size_t size, BufferUsageHint usageHint) override;
    virtual unsigned int CreateMegaVBOIndexBuffer  (size_t size, BufferUsageHint usageHint) override;
    virtual unsigned int CreateMegaVBOVertexArray  () override;
 
    virtual void DeleteMegaVBOVertexBuffer (unsigned int buffer) override;
    virtual void DeleteMegaVBOIndexBuffer  (unsigned int buffer) override;
    virtual void DeleteMegaVBOVertexArray  (unsigned int vao) override;
 
    virtual void SetupMegaVBOVertexAttributes2D(unsigned int vao, unsigned int vbo, unsigned int ibo) override;
 
    virtual void UploadMegaVBOIndexData  (unsigned int ibo, const unsigned int* indices, 
                                          int indexCount, BufferUsageHint usageHint) override;
    virtual void UploadMegaVBOVertexData (unsigned int vbo, const Vertex2D* vertices,
                                          int vertexCount, BufferUsageHint usageHint) override;
 
    virtual void DrawMegaVBOIndexed(PrimitiveType primitiveType, unsigned int indexCount) override;
    
    virtual void EnableMegaVBOPrimitiveRestart (unsigned int restartIndex) override;
    virtual void DisableMegaVBOPrimitiveRestart() override;
    
private:
    void DrawVertices(ID3D11Buffer* vertexBuffer, int vertexCount, bool useTexture);
    void BindTexture (unsigned int textureID);

    ID3D11Buffer* GetVBOFromID(unsigned int vbo);
    void SetupVBOs();
    bool UpdateBufferData(ID3D11Buffer* buffer, const Vertex2D* vertices, int vertexCount);
    
    bool CheckHR(HRESULT hr, const char* operation);
    bool CheckHR(HRESULT hr, const char* operation, ID3DBlob* errorBlob);
};

#endif // RENDERER_DIRECTX11
#endif // _included_renderer2d_d3d11_h