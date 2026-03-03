#include "lib/universal_include.h"

#include "lib/language_table.h"
#include "lib/render/renderer.h"
#include "lib/render2d/renderer_2d.h"
#include "lib/gucci/window_manager.h"

#include "interface/info_window.h"
#include "interface/fading_window.h"

#include "app/app.h"
#include "app/globals.h"
#include "lib/multiline_text.h"

#include "lib/resource/resource.h"

#include "renderer/world_renderer.h"
#include "renderer/map_renderer.h"

#include "world/world.h"
#include "world/worldobject.h"

#include "lib/eclipse/components/core.h"


// -----------------------------------------------------------------------------
// InfoCPUToggleWindow - CPU toggle button (above Info window)
// -----------------------------------------------------------------------------

class ToggleCPUButton : public InterfaceButton
{
    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        World *world = g_app->GetWorld();
        bool cpuActive = world && world->GetAIToggleCPU();

        InterfaceButton::Render( realX, realY, cpuActive, clicked );

        if( cpuActive )
        {
            FadingWindow *parentWin = (FadingWindow *)m_parent;
            float alpha = parentWin ? parentWin->m_alpha : 1.0f;
            g_renderer2d->RectFill( realX - 2, realY - 2, m_w + 4, m_h + 4, Colour( 255, 255, 255, 50.0f * alpha ) );
            g_renderer2d->Rect( realX - 2, realY - 2, m_w + 4, m_h + 4, Colour( 255, 255, 255, 180.0f * alpha ) );
        }
    }

    void MouseUp()
    {
        World *world = g_app->GetWorld();
        if( world )
        {
            world->SetAIToggleCPU( !world->GetAIToggleCPU() );
        }
    }
};


InfoCPUToggleWindow::InfoCPUToggleWindow()
:   FadingWindow( "InfoCPUToggle" )
{
    SetSize( 120, 46 );
    m_minW = 120;
    m_minH = 46;

    SetPosition( 10, 10 );
}


void InfoCPUToggleWindow::Create()
{
    ToggleCPUButton *toggleBtn = new ToggleCPUButton();
    toggleBtn->SetProperties( "ToggleCPU", m_w/2 - 50, m_h/2 - 15, 100, 30, "dialog_toolbar_cpu_toggle", " ", true, true );
    RegisterButton( toggleBtn );
}


void InfoCPUToggleWindow::Render( bool _hasFocus )
{
    FadingWindow::Render( _hasFocus );
}


// -----------------------------------------------------------------------------
// InfoWindow
// -----------------------------------------------------------------------------

InfoWindow::InfoWindow()
:   FadingWindow("Info"),
    m_infoType(-1),
    m_objectId(-1)
{
    SetSize( 300, 150 );
    m_minW = 50;
    m_minH = 50;
    
	if( g_windowManager->WindowH() > 480 )
	{
		SetPosition( g_windowManager->WindowW() - m_w, 
					 g_windowManager->WindowH() - m_h - 70 );
	}
	else
	{
		SetPosition( g_windowManager->WindowW() - m_w - 85, 
					 g_windowManager->WindowH() - m_h - 70 );
	}
    SwitchInfoDisplay( WorldObject::TypeFighter, -1 );
}



void InfoWindow::Create()
{
/*
  Since the RegisterButton calls are commented out, there isn't any point
  in creating these boxes.

    InvertedBox *iconBox = new InvertedBox();
    iconBox->SetProperties( "Icon", 10, 30, 40, 40, " ", " ", false, false );
    //RegisterButton( iconBox );

    InvertedBox *nameBox = new InvertedBox();
    nameBox->SetProperties( "Name", 60, 30, m_w - 70, 40, " ", " ", false, false );
    //RegisterButton( nameBox );

    InvertedBox *mainBox = new InvertedBox();
    mainBox->SetProperties( "Main", 10, 80, m_w - 20, m_h - 95, " ", " ", false, false );
    //RegisterButton( mainBox );
    */
}


void InfoWindow::Render( bool _hasFocus )
{
    //
    // Find out what is selected

    int selectionId = g_app->GetWorldRenderer()->GetCurrentSelectionId();
    int highlightId = g_app->GetWorldRenderer()->GetCurrentHighlightId();
    if( selectionId != -1 || highlightId != -1 )
    {
        WorldObject *selection = g_app->GetWorld()->GetWorldObject( selectionId );
        WorldObject *highlight = g_app->GetWorld()->GetWorldObject( highlightId );
        if( selection )
        {
            SwitchInfoDisplay( selection->m_type, selection->m_objectId );
        }
        else if( highlight )
        {
            if( !highlight->IsCityClass() )
            {
                SwitchInfoDisplay( highlight->m_type, highlight->m_objectId );
            }
        }
    }
    else
    {
        EclWindow *panel = EclGetWindow("Side Panel");
        if( ((panel && !EclMouseInWindow( panel )) || !panel ) &&
            !EclGetWindow("Placement") )
        {
            SwitchInfoDisplay(-1,-1);
        }
    }


    //
    // Render the background Window

    if( m_infoType != -1 )
    {
        m_alpha = 1.0f;
    }

    FadingWindow::Render( _hasFocus );
       

    
    //
    // Render info on what is selected
    
    if( m_infoType != -1 && m_infoType != WorldObject::TypeCity )
    {
        g_renderer2d->SetClip( m_x, m_y, m_w, m_h );

        Image *img = g_resource->GetImage( g_app->GetWorldRenderer()->GetImageFile( m_infoType ));
        if( img )
        {
            g_renderer->SetBlendMode( Renderer::BlendModeAdditive );
            g_renderer2d->StaticSprite ( img, m_x+5, m_y+5, 30.0f, 30.0f, White );                       
            g_renderer2d->StaticSprite( img, m_x+m_w-200, m_y, 180.0f, 180.0f, Colour(155,155,255,50));           
            g_renderer2d->StaticSprite( img, m_x+m_w-230, m_y-50, 240.0f, 240.0f, Colour(155,155,255,5));           
            g_renderer->SetBlendMode( Renderer::BlendModeNormal );
        }
        
        WorldObject *obj = g_app->GetWorld()->GetWorldObject(m_objectId);
        Team *team = obj ? g_app->GetWorld()->GetTeam(obj->m_teamId) : NULL;
        if( !team )
            team = g_app->GetWorld()->GetMyTeam();
        int primaryTerritory = -1;
        if( team && team->m_territories.Size() > 0 )
            primaryTerritory = team->m_territories[0];
        const char *objName = WorldObject::GetTerritoryName( m_infoType, primaryTerritory );
		const char *objTypeName = WorldObject::GetTerritoryUnitInfoSuffix( m_infoType, primaryTerritory );
        const char *teamName = team ? team->GetTeamName() : "";

        char unitName[256];
		sprintf( unitName, "%s", teamName );
        if ( strlen(unitName) + strlen(objName) > 24 )
		{
			strcpy ( &unitName[23-strlen(objName)], "..." );
		}
        sprintf( unitName, "%s  %s", unitName, objName );
        strupr( unitName );

        g_renderer->SetFont( "kremlin" );        
        g_renderer2d->TextCentreSimple( m_x + m_w/2, m_y + 10, White, 16.0f, unitName );
        g_renderer->SetFont();
        
        char infoId[256];
        sprintf(infoId, "unitinfo_%s", objTypeName);

        int lineSize = m_w-15;

        MultiLineText wrapped( LANGUAGEPHRASE(infoId), lineSize, 11 );

        Colour textCol = White;

        for( int i = 0; i < wrapped.Size(); ++i )
        {
            float xPos = m_x + 10;
            float yPos = m_y + 40 + i * 12;

            if( i == 0 ) textCol = White;
            
            char *thisLine = wrapped[i];
            
            if( thisLine[0] == '+' )
            {
                g_renderer2d->CircleFill( xPos+2, yPos+5, 4, 10, Colour(0,255,0,255) );
                textCol.Set(50,255,50,255);                
            }

            if( thisLine[0] == '-')
            {
                g_renderer2d->CircleFill( xPos+2, yPos+5, 4, 10, Colour(255,0,0,255) );
                textCol.Set(255,50,50,255);
            }

            g_renderer2d->TextSimple( xPos, yPos, textCol, 11, thisLine );
        }

        g_renderer2d->ResetClip();
    }
    else
    {
        Colour col(255,255,255,m_alpha*255);
        g_renderer2d->TextCentreSimple( m_x+m_w/2, m_y+m_h/3, col, 14, LANGUAGEPHRASE("unitinfo_title") );
        g_renderer2d->TextCentreSimple( m_x+m_w/2, m_y+m_h/3+20, col, 14, LANGUAGEPHRASE("unitinfo_title2") );
    }
}

void InfoWindow::SwitchInfoDisplay( int _type, int _objectId )
{
    m_infoType = _type;
    m_objectId = _objectId;
}
