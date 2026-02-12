#include "lib/universal_include.h"

#include "lib/render/renderer.h"
#include "lib/render2d/renderer_2d.h"

#include "interface/worldobject_window.h"



WorldObjectWindow::WorldObjectWindow( char *name )
:   EclWindow( name )
{
    SetMovable(false);
}


void WorldObjectWindow::Render( bool hasFocus )
{
    g_renderer2d->Rect( m_x, m_y, m_w, m_h, White );
}

