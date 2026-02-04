#include "systemiv.h"
#include "lib/language_table.h"
#include "lib/render/renderer.h"
#include "lib/render/antialiasing/anti_aliasing.h"
#include "lib/render2d/renderer_2d.h"
#include "lib/preferences.h"
#include "lib/gucci/window_manager.h"

#include "app/globals.h"
#include "app/app.h"
#include "interface/interface.h"

#include "lib/eclipse/components/drop_down_menu.h"
#include "screenoptions_window.h"

class MonitorDropDownMenu : public DropDownMenu
{
    void SelectOption( int _option )
    {
        ScreenOptionsWindow *parent = (ScreenOptionsWindow *) m_parent;

        DropDownMenu::SelectOption( _option );

        g_windowManager->ListAllDisplayModes( _option );

        DropDownMenu *screenRes = (DropDownMenu *) m_parent->GetButton( "Resolution" );
        if( !screenRes )
            return;

        screenRes->Empty();

        int currentWidth = g_preferences->GetInt(PREFS_SCREEN_WIDTH);
        int currentHeight = g_preferences->GetInt(PREFS_SCREEN_HEIGHT);
        
        for( int i = 0; i < g_windowManager->m_resolutions.Size(); ++i )
        {
            WindowResolution *resolution = g_windowManager->m_resolutions[i];

            char caption[128];
            strcpy( caption, LANGUAGEPHRASE("dialog_resolution_W_H") );
            LPREPLACEINTEGERFLAG( 'W', resolution->m_width, caption );
            LPREPLACEINTEGERFLAG( 'H', resolution->m_height, caption );

            screenRes->AddOption( caption, i );
        }

        parent->m_resId = g_windowManager->GetResolutionId( currentWidth, currentHeight );
        if( parent->m_resId < 0 )
        {
            parent->m_resId = 0;
        }
        screenRes->SelectOption( parent->m_resId );

        DropDownMenu *refresh = (DropDownMenu *) m_parent->GetButton( "Refresh Rate" );
        if( !refresh )
            return;

        refresh->Empty();

        WindowResolution *resolution = g_windowManager->GetResolution( parent->m_resId );
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

class AntiAliasTypeDropDownMenu : public DropDownMenu
{
    void SelectOption( int _option )
    {
        DropDownMenu::SelectOption( _option );

        if( !m_parent )
            return;

        ScreenOptionsWindow *parent = (ScreenOptionsWindow *) m_parent;

        DropDownMenu *antiAlias = (DropDownMenu *) m_parent->GetButton( "AntiAlias" );
        if( !antiAlias )
            return;

        antiAlias->Empty();

        AntiAliasingType aaType = (AntiAliasingType)_option;
        parent->m_antiAliasType = _option;

        switch( aaType )
        {
            case AA_TYPE_MSAA:
            {
                antiAlias->AddOption( "dialog_antialias_select", AA_TYPE_NONE, true );
                antiAlias->AddOption( "dialog_antialias_msaa_2x", 2, true );
                antiAlias->AddOption( "dialog_antialias_msaa_4x", 4, true );
                antiAlias->AddOption( "dialog_antialias_msaa_8x", 8, true );
                
                if( GetRendererType() == RENDERER_TYPE_OPENGL )
                {
                    antiAlias->AddOption( "dialog_antialias_msaa_16x", 16, true );
                }
                break;
            }
            #if 0
            case AA_TYPE_FXAA:
            {
                antiAlias->AddOption( "dialog_antialias_select", AA_TYPE_NONE, true );
                antiAlias->AddOption( "dialog_enabled", 1, true );
                break;
            }
            case AA_TYPE_SMAA:
            {
                antiAlias->AddOption( "dialog_antialias_select", AA_TYPE_NONE, true );
                antiAlias->AddOption( "dialog_antialias_smaa_1x", 1, true );
                antiAlias->AddOption( "dialog_antialias_smaa_2x", 2, true );
                antiAlias->AddOption( "dialog_antialias_smaa_4x", 4, true );
                break;
            }
            case AA_TYPE_TAA:
            {
                antiAlias->AddOption( "dialog_antialias_select", AA_TYPE_NONE, true );
                antiAlias->AddOption( "dialog_antialias_taa_low", 1, true );
                antiAlias->AddOption( "dialog_antialias_taa_medium", 2, true );
                antiAlias->AddOption( "dialog_antialias_taa_high", 3, true );
                break;
            }
            #endif
            case AA_TYPE_NONE:
            default:
            {
                antiAlias->AddOption( "dialog_none", 0, true );
                break;
            }
        }

        int currentSamples = parent->m_antiAlias;
        if( currentSamples == 0 || (aaType == AA_TYPE_NONE) )
        {
            antiAlias->SelectOption( 0 );
            parent->m_antiAlias = 0;
        }
        else
        {
            antiAlias->SelectOption( currentSamples );
            if( antiAlias->GetSelectionValue() != currentSamples )
            {
                antiAlias->SelectOption( 0 );
                parent->m_antiAlias = 0;
            }
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
        int applyW = resolution ? resolution->m_width : g_preferences->GetInt( PREFS_SCREEN_WIDTH );
        int applyH = resolution ? resolution->m_height : g_preferences->GetInt( PREFS_SCREEN_HEIGHT );

        //
        // convert screen mode to windowed/borderless flags
        // screenMode: 0 = fullscreen, 1 = borderless fullscreen, 2 = windowed
        
        int windowed = ( parent->m_screenMode == 2 ) ? 1 : 0;
        int borderless = ( parent->m_screenMode == 1 ) ? 1 : 0;
        
        int displayIndex = parent->m_displayIndex;
        if ( displayIndex < 0 || displayIndex >= g_windowManager->GetNumDisplays() )
        {
            displayIndex = g_windowManager->GetCurrentDisplayIndex();
        }

        //
        // In windowed mode, if user selects native/desktop resolution we use usable bounds
        // and start maximized so the window fits correctly.
        // If they pick any other resolution (not native, not usable bounds), exit maximized.

        int desktopW = 0, desktopH = 0, usableW = 0, usableH = 0;
        if ( windowed )
        {
            g_windowManager->GetDisplayDesktopSize( displayIndex, &desktopW, &desktopH );
            g_windowManager->GetDisplayUsableSize( displayIndex, &usableW, &usableH );
            if ( resolution && resolution->m_width == desktopW && resolution->m_height == desktopH )
            {
                applyW = usableW;
                applyH = usableH;
                g_preferences->SetInt( PREFS_SCREEN_MAXIMIZED, 1 );
            }
            else if ( applyW != desktopW || applyH != desktopH )
            {
                if ( applyW != usableW || applyH != usableH )
                {
                    g_preferences->SetInt( PREFS_SCREEN_MAXIMIZED, 0 );
                }
            }
        }

        g_preferences->SetInt( PREFS_SCREEN_WIDTH, applyW );
        g_preferences->SetInt( PREFS_SCREEN_HEIGHT, applyH );
        g_preferences->SetInt( PREFS_SCREEN_REFRESH, parent->m_refreshRate );
        g_preferences->SetInt( PREFS_SCREEN_WINDOWED, windowed );
        g_preferences->SetInt( PREFS_SCREEN_BORDERLESS, borderless );
        g_preferences->SetInt( PREFS_SCREEN_COLOUR_DEPTH, parent->m_colourDepth );
        g_preferences->SetInt( PREFS_SCREEN_Z_DEPTH, parent->m_zDepth );
        g_preferences->SetInt( PREFS_SCREEN_UI_SCALE, parent->m_uiScale );
        g_preferences->SetInt( PREFS_SCREEN_ANTIALIAS, parent->m_antiAlias );
        g_preferences->SetInt( PREFS_SCREEN_ANTIALIAS_TYPE, parent->m_antiAliasType );
        g_preferences->SetInt( PREFS_SCREEN_FPS_LIMIT, parent->m_fpsLimit );
        g_preferences->SetInt( PREFS_SCREEN_RENDERER, parent->m_renderer );
        g_preferences->SetInt( PREFS_SCREEN_MIPMAP_LEVEL, parent->m_mipmapLevel );
        g_preferences->SetInt( PREFS_SCREEN_DISPLAY_INDEX, parent->m_displayIndex );
		
        if( g_app && g_app->GetInterface() )
        {
            g_app->GetInterface()->HandleWindowResize( applyW,
                                                       applyH,
                                                       oldWidth,
                                                       oldHeight );
        }

        g_preferences->Save();

        parent->RestartWindowManager();        

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
        parent->m_antiAliasType = g_preferences->GetInt( PREFS_SCREEN_ANTIALIAS_TYPE, AA_TYPE_MSAA);
        parent->m_fpsLimit      = g_preferences->GetInt( PREFS_SCREEN_FPS_LIMIT, 0 );
        parent->m_renderer      = g_preferences->GetInt( PREFS_SCREEN_RENDERER, PREFS_RENDERER_OPENGL );
        parent->m_mipmapLevel   = g_preferences->GetInt( PREFS_SCREEN_MIPMAP_LEVEL, 4 );
        parent->m_displayIndex  = g_preferences->GetInt( PREFS_SCREEN_DISPLAY_INDEX, -1 );

        if( parent->m_displayIndex < 0 || parent->m_displayIndex >= g_windowManager->GetNumDisplays() )
        {
            parent->m_displayIndex = g_windowManager->GetCurrentDisplayIndex();
        }

        parent->Remove();
        parent->Create();
    }
};


void ScreenOptionsWindow::RestartWindowManager()
{
    g_app->RequestWindowReinit();
}


ScreenOptionsWindow::ScreenOptionsWindow()
:   InterfaceWindow( "ScreenOptions", "dialog_screenoptions", true )
{
#ifdef TARGET_MSVC
	int height = 490;
#else
    int height = 450;
#endif

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
    m_antiAliasType = g_preferences->GetInt( PREFS_SCREEN_ANTIALIAS_TYPE, AA_TYPE_MSAA );
    m_fpsLimit      = g_preferences->GetInt( PREFS_SCREEN_FPS_LIMIT, 0 );
    m_renderer      = g_preferences->GetInt( PREFS_SCREEN_RENDERER, PREFS_RENDERER_OPENGL );
    m_mipmapLevel   = g_preferences->GetInt( PREFS_SCREEN_MIPMAP_LEVEL, 4 );
    m_displayIndex  = g_preferences->GetInt( PREFS_SCREEN_DISPLAY_INDEX, -1 );

    if( m_displayIndex < 0 || m_displayIndex >= g_windowManager->GetNumDisplays() )
    {
        m_displayIndex = g_windowManager->GetCurrentDisplayIndex();
    }
	
    SetSize( 390, height );
}


void ScreenOptionsWindow::Create()
{
    InterfaceWindow::Create();

    //
    // Ensure current resolution is in the list (for windowed modes)

    int currentWidth = g_preferences->GetInt(PREFS_SCREEN_WIDTH);
    int currentHeight = g_preferences->GetInt(PREFS_SCREEN_HEIGHT);
    if( g_windowManager->GetResolutionId( currentWidth, currentHeight ) == -1 )
    {
        WindowResolution *res = new WindowResolution( currentWidth, currentHeight );
        g_windowManager->m_resolutions.PutData( res );
    }

    int x = 200;
    int w = m_w - x - 20;
    int y = 30;
    int h = 30;

    InvertedBox *box = new InvertedBox();
    box->SetProperties( "invert", 10, 50, m_w - 20, m_h - 140, " ", " ", false, false );        
    RegisterButton( box );

    ScreenResDropDownMenu *screenRes = new ScreenResDropDownMenu();
    screenRes->SetProperties( "Resolution", x, y+=h, w, 20, "dialog_resolution", " ", true, false );

    m_resId = g_windowManager->GetResolutionId( currentWidth, currentHeight );
    
    for( int i = 0; i < g_windowManager->m_resolutions.Size(); ++i )
    {
        WindowResolution *resolution = g_windowManager->m_resolutions[i];

        char caption[128];
        strcpy( caption, LANGUAGEPHRASE("dialog_resolution_W_H") );
		LPREPLACEINTEGERFLAG( 'W', resolution->m_width, caption );
		LPREPLACEINTEGERFLAG( 'H', resolution->m_height, caption );

        screenRes->AddOption( caption, i );
    }

    MonitorDropDownMenu *monitor = new MonitorDropDownMenu();
    monitor->SetProperties( "Monitor", x, y+=h, w, 20, "dialog_monitor", " ", true, false );
    
    int numDisplays = g_windowManager->GetNumDisplays();
    for( int i = 0; i < numDisplays; ++i )
    {
        const char *displayName = g_windowManager->GetDisplayName( i );
        char caption[256];

        if( displayName && strlen( displayName ) > 0 )
        {
            sprintf( caption, "%d: %s", i, displayName );
        }
        else
        {
            sprintf( caption, "%d", i );
        }
        monitor->AddOption( caption, i, false );
    }
    
    RegisterButton( monitor );
    monitor->RegisterInt( &m_displayIndex );

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

    DropDownMenu *mipmapLevel = new DropDownMenu();
    mipmapLevel->SetProperties( "Mipmap Level", x, y+=h, w, 20, "dialog_mipmaplevel", " ", true, false );
    mipmapLevel->AddOption( "dialog_mipmaplevel_0", 0, true );
    mipmapLevel->AddOption( "dialog_mipmaplevel_2", 2, true );
    mipmapLevel->AddOption( "dialog_mipmaplevel_4", 4, true );
    mipmapLevel->AddOption( "dialog_mipmaplevel_6", 6, true );
    mipmapLevel->AddOption( "dialog_mipmaplevel_8", 8, true );
    mipmapLevel->RegisterInt( &m_mipmapLevel );
    RegisterButton( mipmapLevel );

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
    antiAlias->SetProperties( "AntiAlias", x, y+=h, w, 20, "dialog_antialias", " ", true, false );
    
    AntiAliasingType currentAaType = (AntiAliasingType)m_antiAliasType;
    switch( currentAaType )
    {
        case AA_TYPE_MSAA:
        {
            antiAlias->AddOption( "dialog_antialias_select", AA_TYPE_NONE, true );
            antiAlias->AddOption( "dialog_antialias_msaa_2x", 2, true );
            antiAlias->AddOption( "dialog_antialias_msaa_4x", 4, true );
            antiAlias->AddOption( "dialog_antialias_msaa_8x", 8, true );
            
            if( GetRendererType() == RENDERER_TYPE_OPENGL )
            {
                antiAlias->AddOption( "dialog_antialias_msaa_16x", 16, true );
            }
            break;
        }
        #if 0
        case AA_TYPE_FXAA:
        {
            antiAlias->AddOption( "dialog_antialias_select", AA_TYPE_NONE, true );
            antiAlias->AddOption( "dialog_enabled", 1, true );
            break;
        }
        case AA_TYPE_SMAA:
        {
            antiAlias->AddOption( "dialog_antialias_select", AA_TYPE_NONE, true );
            antiAlias->AddOption( "dialog_antialias_smaa_1x", 1, true );
            antiAlias->AddOption( "dialog_antialias_smaa_2x", 2, true );
            antiAlias->AddOption( "dialog_antialias_smaa_4x", 4, true );
            break;
        }
        case AA_TYPE_TAA:
        {
            antiAlias->AddOption( "dialog_antialias_select", AA_TYPE_NONE, true );
            antiAlias->AddOption( "dialog_antialias_taa_low", 1, true );
            antiAlias->AddOption( "dialog_antialias_taa_medium", 2, true );
            antiAlias->AddOption( "dialog_antialias_taa_high", 3, true );
            break;
        }
        #endif
        case AA_TYPE_NONE:
        default:
        {
            antiAlias->AddOption( "dialog_none", AA_TYPE_NONE, true );
            break;
        }
    }

    antiAlias->RegisterInt( &m_antiAlias );
    RegisterButton( antiAlias );

    AntiAliasTypeDropDownMenu *antiAliasType = new AntiAliasTypeDropDownMenu();
    antiAliasType->SetProperties( "Anti-Aliasing Type", x, y+=h, w, 20, "dialog_antialias_type", " ", true, false );
    antiAliasType->AddOption( "dialog_none", AA_TYPE_NONE, true );
    antiAliasType->AddOption( "dialog_alias_type_msaa", AA_TYPE_MSAA, true );
    #if 0
    antiAliasType->AddOption( "dialog_alias_type_fxaa", AA_TYPE_FXAA, true );
    antiAliasType->AddOption( "dialog_alias_type_smaa", AA_TYPE_SMAA, true );
    antiAliasType->AddOption( "dialog_alias_type_taa", AA_TYPE_TAA, true );
    #endif
    RegisterButton( antiAliasType );
    antiAliasType->RegisterInt( &m_antiAliasType );

    DropDownMenu *fpsLimit = new DropDownMenu();
    fpsLimit->SetProperties( "FPS Limit", x, y+=h, w, 20, "dialog_fpslimit", " ", true, false );
    fpsLimit->AddOption( "dialog_fpslimit_no_limit", 0, true );
    fpsLimit->AddOption( "dialog_fpslimit_refresh_rate", g_windowManager->GetCurrentRefreshRate(), true );
    fpsLimit->AddOption( "dialog_fpslimit_60fps", 60, true );
    fpsLimit->AddOption( "dialog_fpslimit_144fps", 144, true );
    fpsLimit->AddOption( "dialog_fpslimit_240fps", 240, true );
    fpsLimit->AddOption( "dialog_fpslimit_480fps", 480, true );
    fpsLimit->AddOption( "dialog_fpslimit_1000fps", 1000, true );
    fpsLimit->RegisterInt( &m_fpsLimit );
    RegisterButton( fpsLimit );
    
#ifdef TARGET_MSVC
    DropDownMenu *renderer = new DropDownMenu();
    renderer->SetProperties( "Renderer", x, y+=h, w, 20, "dialog_renderer", " ", true, false );
    renderer->AddOption( "dialog_renderer_opengl", PREFS_RENDERER_OPENGL, true );
    renderer->AddOption( "dialog_renderer_directx11", PREFS_RENDERER_DIRECTX11, true );
    renderer->RegisterInt( &m_renderer );
    RegisterButton( renderer );
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

    g_renderer2d->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_resolution") );
    g_renderer2d->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_monitor") );
    g_renderer2d->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_screenmode") );
    g_renderer2d->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_refreshrate") );
    g_renderer2d->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_colourdepth") );
    g_renderer2d->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_zbufferdepth") );
    g_renderer2d->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_mipmaplevel") );
    g_renderer2d->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_windowscaling") );
    g_renderer2d->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_antialias") );
    g_renderer2d->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_antialias_type") );
    g_renderer2d->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_fpslimit") );
#ifdef TARGET_MSVC
    g_renderer2d->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_renderer") );
#endif
}
