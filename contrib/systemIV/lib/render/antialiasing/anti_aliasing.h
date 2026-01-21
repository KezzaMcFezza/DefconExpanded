#ifndef _included_anti_aliasing_h
#define _included_anti_aliasing_h

#include "systemiv.h"

class Renderer;

enum AntiAliasingType {
	AA_TYPE_NONE = 0,
	AA_TYPE_MSAA = 1,
	AA_TYPE_FXAA = 2,
	AA_TYPE_SMAA = 3,
	AA_TYPE_TAA = 4
};

//
// Render target access

struct AntiAliasingRenderTargetHandle {
	void* renderTarget;
	void* depthStencil;
};

class AntiAliasing
{
public:
	virtual ~AntiAliasing() {}

	static AntiAliasing* Create( AntiAliasingType type, Renderer* renderer );

	virtual void Initialize( int width, int height, int samples ) = 0;
	virtual void BeginRendering() = 0;
	virtual void EndRendering() = 0;
	virtual void Resize( int width, int height ) = 0;
	virtual void Destroy() = 0;

	virtual bool IsEnabled() const = 0;
	virtual int GetSamples() const = 0;

	//
	// Get render target for clearing/binding

	virtual AntiAliasingRenderTargetHandle GetRenderTargetHandle() const = 0;

protected:
	Renderer* m_renderer;

	AntiAliasing( Renderer* renderer )
		: m_renderer( renderer )
	{
	}
};

#endif // _included_anti_aliasing_h
