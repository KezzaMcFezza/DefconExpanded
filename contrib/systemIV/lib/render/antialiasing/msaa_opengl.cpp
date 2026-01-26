#include "systemiv.h"

#ifdef RENDERER_OPENGL
#include "msaa_opengl.h"
#include "lib/render/renderer.h"
#include "lib/debug/debug_utils.h"

AntiAliasingMSAA_OpenGL::AntiAliasingMSAA_OpenGL( Renderer *renderer )
	: AntiAliasing( renderer ),
	  m_fbo( 0 ),
	  m_colorRBO( 0 ),
	  m_depthRBO( 0 ),
	  m_width( 0 ),
	  m_height( 0 ),
	  m_samples( 0 ),
	  m_enabled( false )
{
}


AntiAliasingMSAA_OpenGL::~AntiAliasingMSAA_OpenGL()
{
	Destroy();
}


void AntiAliasingMSAA_OpenGL::Initialize( int width, int height, int samples )
{
	m_width = width;
	m_height = height;
	m_samples = samples;

	if ( samples <= 0 )
	{
		m_enabled = false;
		m_fbo = 0;
		m_colorRBO = 0;
		m_depthRBO = 0;
		return;
	}

	//
	// Query the maximum supported samples

	GLint maxSamples = 0;
	glGetIntegerv( GL_MAX_SAMPLES, &maxSamples );

	//
	// Generate and bind the MSAA framebuffer

	glGenFramebuffers( 1, &m_fbo );
	glBindFramebuffer( GL_FRAMEBUFFER, m_fbo );

	//
	// Create the multisampled color renderbuffer

	glGenRenderbuffers( 1, &m_colorRBO );
	glBindRenderbuffer( GL_RENDERBUFFER, m_colorRBO );
	glRenderbufferStorageMultisample( GL_RENDERBUFFER, m_samples,
									  GL_RGBA8, width, height );
	glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
							   GL_RENDERBUFFER, m_colorRBO );

	//
	// Create the multisampled depth renderbuffer

	glGenRenderbuffers( 1, &m_depthRBO );
	glBindRenderbuffer( GL_RENDERBUFFER, m_depthRBO );
	glRenderbufferStorageMultisample( GL_RENDERBUFFER, m_samples,
									  GL_DEPTH_COMPONENT24, width, height );
	glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
							   GL_RENDERBUFFER, m_depthRBO );

	//
	// Check the framebuffer for completeness

	GLenum status = glCheckFramebufferStatus( GL_FRAMEBUFFER );
	if ( status != GL_FRAMEBUFFER_COMPLETE )
	{
		AppDebugOut( "MSAA creation failed with status: 0x%x\n", status );

		if ( m_colorRBO )
			glDeleteRenderbuffers( 1, &m_colorRBO );
		if ( m_depthRBO )
			glDeleteRenderbuffers( 1, &m_depthRBO );
		if ( m_fbo )
			glDeleteFramebuffers( 1, &m_fbo );

		m_fbo = 0;
		m_colorRBO = 0;
		m_depthRBO = 0;
		m_enabled = false;
		m_samples = 0;
	}
	else
	{
		m_enabled = true;
		AppDebugOut( "MSAA applied successfully\n" );
	}

	//
	// Unbind the framebuffer

	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	glBindRenderbuffer( GL_RENDERBUFFER, 0 );
}


void AntiAliasingMSAA_OpenGL::Resize( int width, int height )
{
	if ( !m_enabled || m_samples <= 0 )
	{
		return;
	}

	if ( width == m_width && height == m_height )
	{
		return;
	}

	//
	// Destroy and reincarnate the FBO with new dimensions

	Destroy();
	Initialize( width, height, m_samples );
}


void AntiAliasingMSAA_OpenGL::Destroy()
{
	if ( m_colorRBO )
	{
		glDeleteRenderbuffers( 1, &m_colorRBO );
		m_colorRBO = 0;
	}

	if ( m_depthRBO )
	{
		glDeleteRenderbuffers( 1, &m_depthRBO );
		m_depthRBO = 0;
	}

	if ( m_fbo )
	{
		glDeleteFramebuffers( 1, &m_fbo );
		m_fbo = 0;
	}

	m_enabled = false;
	m_samples = 0;
	m_width = 0;
	m_height = 0;
}


void AntiAliasingMSAA_OpenGL::BeginRendering()
{
	if ( m_enabled && m_fbo != 0 )
	{
		glBindFramebuffer( GL_FRAMEBUFFER, m_fbo );
	}
}


void AntiAliasingMSAA_OpenGL::EndRendering()
{
	if ( m_enabled && m_fbo != 0 )
	{
		glBindFramebuffer( GL_READ_FRAMEBUFFER, m_fbo );
		glBindFramebuffer( GL_DRAW_FRAMEBUFFER, 0 );

		glBlitFramebuffer( 0, 0, m_width, m_height,
						   0, 0, m_width, m_height,
						   GL_COLOR_BUFFER_BIT, GL_NEAREST );

		glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	}
}


AntiAliasingRenderTargetHandle AntiAliasingMSAA_OpenGL::GetRenderTargetHandle() const
{
	AntiAliasingRenderTargetHandle handle = {};
	if ( m_enabled && m_fbo != 0 )
	{
		handle.renderTarget = reinterpret_cast<void *>( static_cast<uintptr_t>( m_fbo ) );
		handle.depthStencil = reinterpret_cast<void *>( static_cast<uintptr_t>( m_depthRBO ) );
	}
	return handle;
}

#endif // RENDERER_OPENGL
