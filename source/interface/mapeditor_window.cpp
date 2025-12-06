#include "lib/universal_include.h"
#include "lib/render/renderer.h"
#include "lib/render2d/renderer_2d.h"
#include "lib/filesys/filesys_utils.h"
#include "lib/filesys/binary_stream_readers.h"
#include "lib/resource/bitmap.h"
#include "lib/resource/image.h"
#include "lib/gucci/window_manager.h"
#include "lib/preferences.h"
#include "lib/language_table.h"
#include "interface/mapeditor_window.h"

void MapEditorWindow::Create()
{
    SetSize( 200, 300 );
    SetTitle( "MapEditor TEST NOT LOCALISED");
    InterfaceWindow::Create();
};

void MapEditorWindow::Render(bool _hasFocus)
{
    InterfaceWindow::Render( _hasFocus );
    g_renderer2d->RectFill( 0, 0, m_w, m_h, Colour(100,100,200,100) );
};


