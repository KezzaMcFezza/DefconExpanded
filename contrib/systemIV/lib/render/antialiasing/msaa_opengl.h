#ifndef _included_anti_aliasing_msaa_opengl_h
#define _included_anti_aliasing_msaa_opengl_h

#include "anti_aliasing.h"

#ifdef RENDERER_OPENGL

class AntiAliasingMSAA_OpenGL : public AntiAliasing
{
private:
	GLuint m_fbo;
	GLuint m_colorRBO;
	GLuint m_depthRBO;

	int m_width;
	int m_height;
	int m_samples;
	bool m_enabled;

public:
	AntiAliasingMSAA_OpenGL( Renderer* renderer );
	virtual ~AntiAliasingMSAA_OpenGL();

	virtual void Initialize( int width, int height, int samples ) override;
	virtual void BeginRendering() override;
	virtual void EndRendering() override;
	virtual void Resize( int width, int height ) override;
	virtual void Destroy() override;

	virtual bool IsEnabled() const override { return m_enabled; }
	virtual int GetSamples() const override { return m_samples; }
	virtual AntiAliasingRenderTargetHandle GetRenderTargetHandle() const override;
};

#endif // RENDERER_OPENGL

#endif // _included_anti_aliasing_msaa_opengl_h
