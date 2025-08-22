#include "lib/universal_include.h"
#include "lib/language_table.h"
#include "lib/render2d/renderer.h"
#include "lib/preferences.h"
#include "lib/gucci/window_manager.h"

#include "interface/components/drop_down_menu.h"
#include "screenoptions_window.h"


#define HAVE_REFRESH_RATES


class ScreenResDropDownMenu : public DropDownMenu
{
#ifdef HAVE_REFRESH_RATES
    void SelectOption( int _option )
    {
        ScreenOptionsWindow *parent = (ScreenOptionsWindow *) m_parent;

        DropDownMenu::SelectOption( _option );

        // Update available refresh rates

        DropDownMenu *refresh = (DropDownMenu *) m_parent->GetButton( "Refresh Rate" );
        refresh->Empty();

        WindowResolution *resolution = g_windowManager->GetResolution( _option );
        if( resolution )
        {
            for( int i = 0; i < resolution->m_refreshRates.Size(); ++i )
            {
                int thisRate = resolution->m_refreshRates[i];

                char caption[128];
                strcpy( caption, LANGUAGEPHRASE("dialog_refreshrate_X") );
                LPREPLACEINTEGERFLAG( 'R', thisRate, caption );

                refresh->AddOption( caption, thisRate );
            }
            refresh->SelectOption( parent->m_refreshRate );
        }
    }
#endif
};


class FullscreenRequiredMenu : public DropDownMenu
{
    void Render( int realX, int realY, bool highlighted, bool clicked )
    {    
        DropDownMenu *windowed = (DropDownMenu *) m_parent->GetButton( "Windowed" );
        bool available = (windowed->GetSelectionValue() == 0);
        if( available )
        {
            DropDownMenu::Render( realX, realY, highlighted, clicked );
        }
        else
        {
            // Removed glColor4f - color is handled by the modern renderer system
            g_renderer->TextSimple( realX+10, realY+9, White, 13, LANGUAGEPHRASE("dialog_windowedmode") );
        }
    }

    void MouseUp()
    {
        DropDownMenu *windowed = (DropDownMenu *) m_parent->GetButton( "Windowed" );
        bool available = (windowed->GetSelectionValue() == 0);
        if( available )
        {
            DropDownMenu::MouseUp();
        }
    }    
};

#ifndef TARGET_OS_LINUX
static void AdjustWindowPositions(int _newWidth, int _newHeight, int _oldWidth, int _oldHeight)
{
	if (_newWidth != _oldWidth || _newHeight != _oldHeight)
    {
		// Resolution has changed, adjust the window positions accordingly
		EclInitialise( _newWidth, _newHeight );

		LList<EclWindow *> *windows = EclGetWindows();
		for (int i = 0; i < windows->Size(); i++) 
        {
			EclWindow *w = windows->GetData(i);
			
            int oldCentreX = _oldWidth/2.0f - w->m_w/2;
            int oldCentreY = _oldHeight/2.0f - w->m_h/2;

            if( fabs(float(w->m_x - oldCentreX)) < 30 &&
                fabs(float(w->m_y - oldCentreY)) < 30 )
            {
                // This window was centralised
                w->m_x = _newWidth/2.0f - w->m_w/2.0f;
                w->m_y = _newHeight/2.0f - w->m_h/2.0f;
            }
            else
            {
                //
                // Find the two nearest edges, and keep those distances the same
                // How far are we from each edge?

                float distances[4];
                distances[0] = w->m_x;                              // left
                distances[1] = _oldWidth - (w->m_x + w->m_w);       // right
                distances[2] = w->m_y;                              // top
                distances[3] = _oldHeight - (w->m_y + w->m_h);      // bottom

                //
                // Sort those distances, closest first

                LList<int> sortedDistances;
                for( int j = 0; j < 4; ++j )
                {
                    float thisDistance = distances[j];
                    bool added = false;
                    for( int k = 0; k < sortedDistances.Size(); ++k )
                    {
                        float sortedDistance = distances[sortedDistances[k]];
                        if( thisDistance < sortedDistance )
                        {
                            sortedDistances.PutDataAtIndex( j, k );
                            added = true;
                            break;
                        }
                    }
                    if( !added )
                    {
                        sortedDistances.PutDataAtEnd( j );
                    }
                }
            
                //
                // Preserve the two nearest edge distances;

                for( int j = 0; j < 2; ++j )
                {
                    int thisNearest = sortedDistances[j];
                    
                    if( thisNearest == 0 )      w->m_x = distances[thisNearest];
                    if( thisNearest == 1 )      w->m_x = _newWidth - w->m_w - distances[thisNearest];
                    if( thisNearest == 2 )      w->m_y = distances[thisNearest];
                    if( thisNearest == 3 )      w->m_y = _newHeight - w->m_h - distances[thisNearest];
                }
            }
		}
	}
}
#endif


// SetWindowed - switch to windowed or fullscreen mode.
// Used by lib/input_sdl.cpp 
//         debugmenu.cpp
//
// sets _isSwitchingToWindowed true if we actually leave fullscreen mode and
// switch to windowed mode.
//
// This is useful for Mac OS X automatic switch back, when leaving fullscreen mode
// Command-Tab

void SetWindowed(bool _isWindowed, bool _isPermanent, bool &_isSwitchingToWindowed)
{
	bool oldIsWindowedPref = g_preferences->GetInt( PREFS_SCREEN_WINDOWED );
	bool oldIsWindowed = g_windowManager->Windowed();
	g_preferences->SetInt( PREFS_SCREEN_WINDOWED, _isWindowed );

	if (oldIsWindowed != _isWindowed) {
	
		_isSwitchingToWindowed = _isWindowed; 
		
		//RestartWindowManagerAndRenderer();
	}
	else
		_isSwitchingToWindowed = false;

	if (!_isPermanent) 
		g_preferences->SetInt( PREFS_SCREEN_WINDOWED, oldIsWindowedPref );
	else
		g_preferences->Save(); 
}


class SetScreenButton : public InterfaceButton
{
    void MouseUp()
    {
        ScreenOptionsWindow *parent = (ScreenOptionsWindow *) m_parent;
        
        int oldWidth = g_preferences->GetInt( PREFS_SCREEN_WIDTH );
        int oldHeight = g_preferences->GetInt( PREFS_SCREEN_HEIGHT );

        WindowResolution *resolution = g_windowManager->GetResolution( parent->m_resId );
        if( resolution )
        {
            g_preferences->SetInt( PREFS_SCREEN_WIDTH, resolution->m_width );
            g_preferences->SetInt( PREFS_SCREEN_HEIGHT, resolution->m_height );
        }

#ifdef HAVE_REFRESH_RATES
        g_preferences->SetInt( PREFS_SCREEN_REFRESH, parent->m_refreshRate );
#endif
        g_preferences->SetInt( PREFS_SCREEN_WINDOWED, parent->m_windowed );
        g_preferences->SetInt( PREFS_SCREEN_COLOUR_DEPTH, parent->m_colourDepth );
        g_preferences->SetInt( PREFS_SCREEN_Z_DEPTH, parent->m_zDepth );
#if !defined(TARGET_OS_LINUX) || !defined(TARGET_EMSCRIPTEN)
        g_preferences->SetInt( PREFS_SCREEN_UI_SCALE, parent->m_uiScale );
#endif

#ifdef TARGET_EMSCRIPTEN
        g_preferences->SetInt( PREFS_SCREEN_ANTIALIAS, parent->m_antiAlias );
#endif

#if !defined(TARGET_OS_LINUX) || !defined(TARGET_EMSCRIPTEN)
        g_preferences->SetInt( PREFS_SCREEN_FPS_LIMIT, parent->m_fpsLimit );
#endif
		
#ifndef TARGET_OS_LINUX
        if( resolution )
        {
            AdjustWindowPositions( resolution->m_width, resolution->m_height,
                                   oldWidth, oldHeight );
        }
#endif

        parent->RestartWindowManager();
        
        g_preferences->Save();        

        parent->m_resId = g_windowManager->GetResolutionId( g_preferences->GetInt(PREFS_SCREEN_WIDTH),
                                                            g_preferences->GetInt(PREFS_SCREEN_HEIGHT) );
        
        parent->m_windowed      = g_preferences->GetInt( PREFS_SCREEN_WINDOWED, 0 );
        parent->m_colourDepth   = g_preferences->GetInt( PREFS_SCREEN_COLOUR_DEPTH, 16 );

#ifdef HAVE_REFRESH_RATES
        parent->m_refreshRate   = g_preferences->GetInt( PREFS_SCREEN_REFRESH, 60 );
#endif		
        parent->m_zDepth        = g_preferences->GetInt( PREFS_SCREEN_Z_DEPTH, 24 );
#if !defined(TARGET_OS_LINUX) || !defined(TARGET_EMSCRIPTEN)
        parent->m_uiScale       = g_preferences->GetInt( PREFS_SCREEN_UI_SCALE, 100 );
#endif

#ifdef TARGET_EMSCRIPTEN
        parent->m_antiAlias     = g_preferences->GetInt( PREFS_SCREEN_ANTIALIAS, 0 );
#endif

#if !defined(TARGET_OS_LINUX) || !defined(TARGET_EMSCRIPTEN)
        parent->m_fpsLimit      = g_preferences->GetInt( PREFS_SCREEN_FPS_LIMIT, 0 );
#endif

        parent->Remove();
        parent->Create();
    }
};


void ScreenOptionsWindow::RestartWindowManager()
{
};


ScreenOptionsWindow::ScreenOptionsWindow()
:   InterfaceWindow( "ScreenOptions", "dialog_screenoptions", true )
{
#if !defined(TARGET_OS_LINUX) || !defined(TARGET_EMSCRIPTEN)
	int height = 360;
#else 
	int height = 300;
#endif
    m_resId = g_windowManager->GetResolutionId( g_preferences->GetInt(PREFS_SCREEN_WIDTH),
                                                g_preferences->GetInt(PREFS_SCREEN_HEIGHT) );

    m_windowed      = g_preferences->GetInt( PREFS_SCREEN_WINDOWED, 0 );
    m_colourDepth   = g_preferences->GetInt( PREFS_SCREEN_COLOUR_DEPTH, 16 );
#ifdef HAVE_REFRESH_RATES
    m_refreshRate   = g_preferences->GetInt( PREFS_SCREEN_REFRESH, 60 );
	height += 30;
#endif
    m_zDepth        = g_preferences->GetInt( PREFS_SCREEN_Z_DEPTH, 24 );
#if !defined(TARGET_OS_LINUX) || !defined(TARGET_EMSCRIPTEN)
    m_uiScale       = g_preferences->GetInt( PREFS_SCREEN_UI_SCALE, 100 );
#endif

#ifdef TARGET_EMSCRIPTEN
	m_antiAlias		= g_preferences->GetInt( PREFS_SCREEN_ANTIALIAS, 0 );
#endif

#if !defined(TARGET_OS_LINUX) || !defined(TARGET_EMSCRIPTEN)
    m_fpsLimit      = g_preferences->GetInt( PREFS_SCREEN_FPS_LIMIT, 0 );
#endif
	
    SetSize( 390, height );
}


void ScreenOptionsWindow::Create()
{
    InterfaceWindow::Create();

    int x = 200;
    int w = m_w - x - 20;
    int y = 30;
    int h = 30;

    InvertedBox *box = new InvertedBox();
    box->SetProperties( "invert", 10, 50, m_w - 20, m_h - 140, " ", " ", false, false );        
    RegisterButton( box );

    ScreenResDropDownMenu *screenRes = new ScreenResDropDownMenu();
    screenRes->SetProperties( "Resolution", x, y+=h, w, 20, "dialog_resolution", " ", true, false );
    for( int i = 0; i < g_windowManager->m_resolutions.Size(); ++i )
    {
        WindowResolution *resolution = g_windowManager->m_resolutions[i];

        char caption[128];
        strcpy( caption, LANGUAGEPHRASE("dialog_resolution_W_H") );
		LPREPLACEINTEGERFLAG( 'W', resolution->m_width, caption );
		LPREPLACEINTEGERFLAG( 'H', resolution->m_height, caption );

        screenRes->AddOption( caption, i );
    }

    DropDownMenu *windowed = new DropDownMenu();
    windowed->SetProperties( "Windowed", x, y+=h, w, 20, "dialog_windowed", " ", true, false );
    windowed->AddOption( "dialog_yes", 1, true );
    windowed->AddOption( "dialog_no", 0, true );
    windowed->RegisterInt( &m_windowed );

#ifdef HAVE_REFRESH_RATES
    FullscreenRequiredMenu *refresh = new FullscreenRequiredMenu();
    refresh->SetProperties( "Refresh Rate", x, y+=h, w, 20, "dialog_refreshrate", " ", true, false );
    refresh->RegisterInt( &m_refreshRate );
    RegisterButton( refresh );
#endif

    RegisterButton( windowed );
    RegisterButton( screenRes );
    screenRes->RegisterInt( &m_resId );   


    FullscreenRequiredMenu *colourDepth = new FullscreenRequiredMenu();
    colourDepth->SetProperties( "Colour Depth", x, y+=h, w, 20, "dialog_colourdepth", " ", true, false );
    colourDepth->AddOption( "dialog_colourdepth_16", 16, true );
    colourDepth->AddOption( "dialog_colourdepth_24", 24, true );
    colourDepth->AddOption( "dialog_colourdepth_32", 32, true );
    colourDepth->RegisterInt( &m_colourDepth );
    RegisterButton( colourDepth );
    
    DropDownMenu *zDepth = new DropDownMenu();
    zDepth->SetProperties( "Z Buffer Depth", x, y+=h, w, 20, "dialog_zbufferdepth", " ", true, false );
    zDepth->AddOption( "dialog_colourdepth_16", 16, true );
    zDepth->AddOption( "dialog_colourdepth_24", 24, true );
    zDepth->RegisterInt( &m_zDepth );
    RegisterButton( zDepth );

#if !defined(TARGET_OS_LINUX) || !defined(TARGET_EMSCRIPTEN)
    DropDownMenu *uiScale = new DropDownMenu();
    uiScale->SetProperties( "UI Scale", x, y+=h, w, 20, "dialog_windowscaling", " ", true, false );
    uiScale->AddOption( "50%", 50, false );
    uiScale->AddOption( "75%", 75, false );
    uiScale->AddOption( "No Scaling", 100, false );
    uiScale->AddOption( "110%", 110, false );
    uiScale->AddOption( "125%", 125, false );
    uiScale->AddOption( "133%", 133, false );
    uiScale->AddOption( "150%", 150, false );
    uiScale->RegisterInt( &m_uiScale );
    RegisterButton( uiScale );
#endif

#ifdef TARGET_EMSCRIPTEN
    DropDownMenu *antiAlias = new DropDownMenu();
    antiAlias->SetProperties( LANGUAGEPHRASE("dialog_antialias"), x, y+=h, w, 20 );
    antiAlias->AddOption( "dialog_no", 0, true );
    antiAlias->AddOption( "dialog_antialias_2x", 2, false );
    antiAlias->AddOption( "dialog_antialias_4x", 4, false );
    antiAlias->AddOption( "dialog_antialias_8x", 8, false );
    antiAlias->AddOption( "dialog_antialias_16x", 16, false );
    antiAlias->RegisterInt( &m_antiAlias );
    RegisterButton( antiAlias );
#endif

#if !defined(TARGET_OS_LINUX) || !defined(TARGET_EMSCRIPTEN)
    DropDownMenu *fpsLimit = new DropDownMenu();
    fpsLimit->SetProperties( "FPS Limit", x, y+=h, w, 20, "dialog_fpslimit", " ", true, false );
    fpsLimit->AddOption( "dialog_fpslimit_no_limit", 0, true );
    fpsLimit->AddOption( "dialog_fpslimit_refresh_rate", 69, true );
    fpsLimit->AddOption( "dialog_fpslimit_60fps", 60, true );
    fpsLimit->AddOption( "dialog_fpslimit_144fps", 144, true );
    fpsLimit->AddOption( "dialog_fpslimit_240fps", 240, true );
    fpsLimit->AddOption( "dialog_fpslimit_480fps", 480, true );
    fpsLimit->AddOption( "dialog_fpslimit_1000fps", 1000, true );
    fpsLimit->RegisterInt( &m_fpsLimit );
    RegisterButton( fpsLimit );
#endif

	CloseButton *cancel = new CloseButton();
    cancel->SetProperties( "Close", 10, m_h - 30, m_w / 2 - 15, 20, "dialog_close", " ", true, false );
    RegisterButton( cancel );

    SetScreenButton *apply = new SetScreenButton();
    apply->SetProperties( "Apply", cancel->m_x+cancel->m_w+10, m_h - 30, m_w / 2 - 15, 20, "dialog_apply", " ", true, false );
    RegisterButton( apply );     
}


void ScreenOptionsWindow::Render( bool _hasFocus )
{
    InterfaceWindow::Render( _hasFocus );

    int x = m_x + 20;
    int y = m_y + 35;
    int h = 30;
    int size = 13;

    g_renderer->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_resolution") );
    g_renderer->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_windowed") );
#ifdef HAVE_REFRESH_RATES	
    g_renderer->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_refreshrate") );
#endif
    g_renderer->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_colourdepth") );
    g_renderer->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_zbufferdepth") );
#if !defined(TARGET_OS_LINUX) || !defined(TARGET_EMSCRIPTEN)
    g_renderer->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_windowscaling") );
#endif

#ifdef TARGET_EMSCRIPTEN
    g_renderer->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_antialias") );
#endif

#if !defined(TARGET_OS_LINUX) || !defined(TARGET_EMSCRIPTEN)
    g_renderer->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_fpslimit") );
#endif
}
