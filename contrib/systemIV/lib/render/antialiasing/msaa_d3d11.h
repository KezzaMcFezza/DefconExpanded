#ifndef _included_anti_aliasing_msaa_d3d11_h
#define _included_anti_aliasing_msaa_d3d11_h

#include "anti_aliasing.h"

#ifdef RENDERER_DIRECTX11

#include <d3d11.h>

class AntiAliasingMSAA_D3D11 : public AntiAliasing
{
private:
	ID3D11Device* m_device;
	ID3D11DeviceContext* m_deviceContext;

	ID3D11Texture2D* m_renderTarget;
	ID3D11RenderTargetView* m_renderTargetView;
	ID3D11Texture2D* m_depthStencil;
	ID3D11DepthStencilView* m_depthStencilView;

	int m_width;
	int m_height;
	int m_samples;
	bool m_enabled;

public:
	AntiAliasingMSAA_D3D11( Renderer* renderer, ID3D11Device* device, ID3D11DeviceContext* deviceContext );
	virtual ~AntiAliasingMSAA_D3D11();

	virtual void Initialize( int width, int height, int samples ) override;
	virtual void BeginRendering() override;
	virtual void EndRendering() override;
	virtual void Resize( int width, int height ) override;
	virtual void Destroy() override;

	virtual bool IsEnabled() const override { return m_enabled; }
	virtual int GetSamples() const override { return m_samples; }
	virtual AntiAliasingRenderTargetHandle GetRenderTargetHandle() const override;
};

#endif // RENDERER_DIRECTX11

#endif // _included_anti_aliasing_msaa_d3d11_h
