#include "lib/universal_include.h"

#include <stdlib.h>

#include "lib/render/renderer.h"
#include "lib/render2d/renderer_2d.h"
#include "lib/gucci/window_manager.h"
#include "lib/filesys/text_stream_readers.h"
#include "lib/filesys/file_system.h"

#include "interface/version_popup.h"
#include "interface/components/core.h"

//
// Hardcode version popup to prevent a paradox where if someone
// has a main.dat without the necessary language phrases, the
// information wont even render.

static const int s_numLines = 4;
static const char *s_title = "DATA FILES OUT OF DATE";
static const char *s_lines[] = {
    "Your data folder or main.dat is out of date.",
    "",
    "You can ignore this message and play anyway but",
    "expect broken text rendering or graphical bugs."
};


VersionPopupWindow::VersionPopupWindow()
:   InterfaceWindow( "Version Warning", s_title, false )
{
    int windowW = 350;
    int windowH = 80 + s_numLines * 14;
    
    SetSize( windowW, windowH );
    
    int x = g_windowManager->WindowW() / 2 - windowW / 2;
    int y = g_windowManager->WindowH() / 2 - windowH / 2;
    SetPosition( x, y );
}


void VersionPopupWindow::Create()
{
    InvertedBox *box = new InvertedBox();
    box->SetProperties( "invert", 10, 30, m_w - 20, m_h - 60, " ", " ", false, false );
    RegisterButton( box );

    CloseButton *close = new CloseButton();
    close->SetProperties( "Close", (m_w - 80) / 2, m_h - 25, 80, 18, "Close", " ", false, false );
    RegisterButton( close );
}


void VersionPopupWindow::Render( bool _hasFocus )
{
    InterfaceWindow::Render( _hasFocus );
    
    for( int i = 0; i < s_numLines; i++ )
    {
        g_renderer2d->TextSimple( m_x + 15, m_y + 35 + i * 14, White, 12, s_lines[i] );
    }
}


bool CheckMainDatVersion()
{
    TextReader *reader = g_fileSystem->GetTextReader( "data/version.txt" );
    
    if( !reader || !reader->IsOpen() )
    {
        delete reader;
        return false;
    }

    float datVersion = 0.0f;
    bool found = false;

    while( reader->ReadLine() )
    {
        char *line = reader->m_line;
        if( !line ) continue;
        
        if( strnicmp( line, "VERSION", 7 ) == 0 )
        {
            const char *p = line;
            while( *p && !( *p >= '0' && *p <= '9' ) ) p++;
            
            if( *p )
            {
                datVersion = (float)atof( p );
                found = true;
            }
            break;
        }
    }

    delete reader;

    if( !found ) return false;

    float clientVersion = (float)atof( APP_BUILD_NUMBER );

    return datVersion >= clientVersion;
}
