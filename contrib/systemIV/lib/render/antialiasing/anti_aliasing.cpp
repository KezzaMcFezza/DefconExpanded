#include "systemiv.h"

#include "anti_aliasing.h"
#include "lib/render/renderer.h"

#ifdef RENDERER_DIRECTX11
#include "msaa_d3d11.h"
#include "lib/gucci/window_manager_d3d11.h"
#endif

#ifdef RENDERER_OPENGL
#include "msaa_opengl.h"
#endif

AntiAliasing *AntiAliasing::Create( AntiAliasingType type, Renderer *renderer )
{
	if ( !renderer )
		return nullptr;

	if ( type == AA_TYPE_NONE )
		return nullptr;

	RendererType rendererType = GetRendererType();

	switch ( type )
	{
		case AA_TYPE_MSAA:
		{
#ifdef RENDERER_DIRECTX11
			if ( rendererType == RENDERER_TYPE_DIRECTX11 )
			{
				WindowManagerD3D11 *windowManager = static_cast<WindowManagerD3D11 *>( g_windowManager );
				if ( windowManager )
				{
					return new AntiAliasingMSAA_D3D11( renderer,
													   windowManager->GetDevice(),
													   windowManager->GetDeviceContext() );
				}
			}
#endif
#ifdef RENDERER_OPENGL
			if ( rendererType == RENDERER_TYPE_OPENGL )
			{
				return new AntiAliasingMSAA_OpenGL( renderer );
			}
#endif
			break;
		}

			//
			// Yes, i do plan on adding these anti aliasing types

		case AA_TYPE_FXAA:
		case AA_TYPE_SMAA:
		case AA_TYPE_TAA:
			return nullptr;
		default:
			return nullptr;
	}

	return nullptr;
}
