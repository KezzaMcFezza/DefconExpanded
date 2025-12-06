#include "lib/universal_include.h"
#include "lib/language_table.h"
#include "lib/render/renderer.h"
#include "lib/render2d/renderer_2d.h"
#include "lib/preferences.h"
#include "lib/gucci/window_manager.h"

#include "app/globals.h"
#include "app/app.h"
#include "interface/interface.h"

#include "interface/components/drop_down_menu.h"
#include "screenoptions_window.h"

class ScreenResDropDownMenu : public DropDownMenu
{
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
};


class FullscreenRequiredMenu : public DropDownMenu
{
    void Render( int realX, int realY, bool highlighted, bool clicked )
    {    
        DropDownMenu *screenMode = (DropDownMenu *) m_parent->GetButton( "Screen Mode" );
        bool available = screenMode && (screenMode->GetSelectionValue() == 0);
        if( available )
        {
            DropDownMenu::Render( realX, realY, highlighted, clicked );
        }
        else
        {
            // Removed glColor4f - color is handled by the modern renderer system
            g_renderer2d->TextSimple( realX+10, realY+9, White, 13, LANGUAGEPHRASE("dialog_windowedmode") );
        }
    }

    void MouseUp()
    {
        DropDownMenu *screenMode = (DropDownMenu *) m_parent->GetButton( "Screen Mode" );
        bool available = screenMode && (screenMode->GetSelectionValue() == 0);
        if( available )
        {
            DropDownMenu::MouseUp();
        }
    }    
};

class RefreshRateRequiredMenu : public DropDownMenu
{
    void Render( int realX, int realY, bool highlighted, bool clicked )
    {    
        DropDownMenu *screenMode = (DropDownMenu *) m_parent->GetButton( "Screen Mode" );

        //
        // only available when screen mode is 0 (fullscreen)

        bool available = screenMode && (screenMode->GetSelectionValue() == 0);
        if( available )
        {
            DropDownMenu::Render( realX, realY, highlighted, clicked );
        }
        else
        {
            g_renderer2d->TextSimple( realX+10, realY+9, White, 13, LANGUAGEPHRASE("dialog_windowedmode") );
        }
    }

    void MouseUp()
    {
        DropDownMenu *screenMode = (DropDownMenu *) m_parent->GetButton( "Screen Mode" );
        bool available = screenMode && (screenMode->GetSelectionValue() == 0);
        if( available )
        {
            DropDownMenu::MouseUp();
        }
    }    
};


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

        g_preferences->SetInt( PREFS_SCREEN_REFRESH, parent->m_refreshRate );
        
        //
        // convert screen mode to windowed/borderless flags
        // screenMode: 0 = fullscreen, 1 = borderless fullscreen, 2 = windowed
        
        int windowed = (parent->m_screenMode == 2) ? 1 : 0;
        int borderless = (parent->m_screenMode == 1) ? 1 : 0;
        
        g_preferences->SetInt( PREFS_SCREEN_WINDOWED, windowed );
        g_preferences->SetInt( PREFS_SCREEN_BORDERLESS, borderless );
        g_preferences->SetInt( PREFS_SCREEN_COLOUR_DEPTH, parent->m_colourDepth );
        g_preferences->SetInt( PREFS_SCREEN_Z_DEPTH, parent->m_zDepth );
        g_preferences->SetInt( PREFS_SCREEN_UI_SCALE, parent->m_uiScale );
        g_preferences->SetInt( PREFS_SCREEN_ANTIALIAS, parent->m_antiAlias );
        g_preferences->SetInt( PREFS_SCREEN_FPS_LIMIT, parent->m_fpsLimit );
		
        if( resolution && g_app && g_app->GetInterface() )
        {
            g_app->GetInterface()->HandleWindowResize( resolution->m_width,
                                                       resolution->m_height,
                                                       oldWidth,
                                                       oldHeight );
        }

        parent->RestartWindowManager();
        
        g_preferences->Save();        

        parent->m_resId = g_windowManager->GetResolutionId( g_preferences->GetInt(PREFS_SCREEN_WIDTH),
                                                            g_preferences->GetInt(PREFS_SCREEN_HEIGHT) );
        
        //
        // convert windowed/borderless flags back to screen mode for UI

        int tempWindowed = g_preferences->GetInt( PREFS_SCREEN_WINDOWED, 0 );
        int tempBorderless = g_preferences->GetInt( PREFS_SCREEN_BORDERLESS, 0 );

        if( tempWindowed )
            parent->m_screenMode = 2;
        else if( tempBorderless )
            parent->m_screenMode = 1;
        else
            parent->m_screenMode = 0;
        
        parent->m_colourDepth   = g_preferences->GetInt( PREFS_SCREEN_COLOUR_DEPTH, 16 );
        parent->m_refreshRate   = g_preferences->GetInt( PREFS_SCREEN_REFRESH, 60 );	
        parent->m_zDepth        = g_preferences->GetInt( PREFS_SCREEN_Z_DEPTH, 24 );
        parent->m_uiScale       = g_preferences->GetInt( PREFS_SCREEN_UI_SCALE, 100 );
        parent->m_antiAlias     = g_preferences->GetInt( PREFS_SCREEN_ANTIALIAS, 0 );
        parent->m_fpsLimit      = g_preferences->GetInt( PREFS_SCREEN_FPS_LIMIT, 0 );

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
	int height = 360;

    m_resId = g_windowManager->GetResolutionId( g_preferences->GetInt(PREFS_SCREEN_WIDTH),
                                                g_preferences->GetInt(PREFS_SCREEN_HEIGHT) );

    int windowed = g_preferences->GetInt( PREFS_SCREEN_WINDOWED, 0 );
    int borderless = g_preferences->GetInt( PREFS_SCREEN_BORDERLESS, 0 );
    if( windowed )
        m_screenMode = 2;
    else if( borderless )
        m_screenMode = 1;
    else
        m_screenMode = 0;
    
    m_colourDepth   = g_preferences->GetInt( PREFS_SCREEN_COLOUR_DEPTH, 16 );
    m_refreshRate   = g_preferences->GetInt( PREFS_SCREEN_REFRESH, 60 );
	height += 30;
    m_zDepth        = g_preferences->GetInt( PREFS_SCREEN_Z_DEPTH, 24 );
    m_uiScale       = g_preferences->GetInt( PREFS_SCREEN_UI_SCALE, 100 );
	m_antiAlias		= g_preferences->GetInt( PREFS_SCREEN_ANTIALIAS, 0 );
    m_fpsLimit      = g_preferences->GetInt( PREFS_SCREEN_FPS_LIMIT, 0 );
	
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

    DropDownMenu *screenMode = new DropDownMenu();
    screenMode->SetProperties( "Screen Mode", x, y+=h, w, 20, "dialog_screenmode", " ", true, false );
    screenMode->AddOption( "dialog_screenmode_fullscreen", 0, true );
    screenMode->AddOption( "dialog_screenmode_borderless", 1, true );
    screenMode->AddOption( "dialog_screenmode_windowed", 2, true );
    screenMode->RegisterInt( &m_screenMode );

    RefreshRateRequiredMenu *refresh = new RefreshRateRequiredMenu();
    refresh->SetProperties( "Refresh Rate", x, y+=h, w, 20, "dialog_refreshrate", " ", true, false );
    refresh->RegisterInt( &m_refreshRate );

    RegisterButton( refresh );
    RegisterButton( screenMode );
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

    DropDownMenu *uiScale = new DropDownMenu();
    uiScale->SetProperties( "UI Scale", x, y+=h, w, 20, "dialog_windowscaling", " ", true, false );
    uiScale->AddOption( "25%", 25, false );
    uiScale->AddOption( "50%", 50, false );
    uiScale->AddOption( "75%", 75, false );
    uiScale->AddOption( "No Scaling", 100, false );
    uiScale->AddOption( "110%", 110, false );
    uiScale->AddOption( "125%", 125, false );
    uiScale->AddOption( "133%", 133, false );
    uiScale->AddOption( "150%", 150, false );
    uiScale->AddOption( "200%", 200, false );
    uiScale->RegisterInt( &m_uiScale );
    RegisterButton( uiScale );

    DropDownMenu *antiAlias = new DropDownMenu();
    antiAlias->SetProperties( LANGUAGEPHRASE("dialog_antialias"), x, y+=h, w, 20 );
    antiAlias->AddOption( "dialog_no", 0, true );
    antiAlias->AddOption( "dialog_antialias_2x", 2, true );
    antiAlias->AddOption( "dialog_antialias_4x", 4, true );
    antiAlias->AddOption( "dialog_antialias_8x", 8, true );
    antiAlias->AddOption( "dialog_antialias_16x", 16, true );
    antiAlias->RegisterInt( &m_antiAlias );
    RegisterButton( antiAlias );

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

    g_renderer2d->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_resolution") );
    g_renderer2d->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_screenmode") );
    g_renderer2d->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_refreshrate") );
    g_renderer2d->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_colourdepth") );
    g_renderer2d->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_zbufferdepth") );
    g_renderer2d->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_windowscaling") );
    g_renderer2d->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_antialias") );
    g_renderer2d->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_fpslimit") );
}
