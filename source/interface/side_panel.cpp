#include "lib/universal_include.h"

#include "lib/render/colour.h"
#include "lib/render/renderer.h"
#include "lib/render2d/renderer_2d.h"
#include "lib/resource/image.h"
#include "lib/language_table.h"
#include "lib/gucci/input.h"
#include "lib/gucci/window_manager.h"

#include "app/app.h"
#include "app/globals.h"
#include "app/game.h"

#include "interface/side_panel.h"
#include "interface/placement_icon.h"
#include "interface/fleet_placement_icon.h"
#include "interface/info_window.h"

#include "renderer/world_renderer.h"
#include "renderer/map_renderer.h"

#include "world/world.h"
#include "world/team.h"
#include "world/fleet.h"
#include "world/territory_roster.h"


SidePanel::SidePanel( const char *name )
:   FadingWindow(name),
    m_expanded(false),
    m_moving(false),
    m_mode(ModeUnitPlacement),
    m_currentFleetId(-1),
    m_fleetTeamId(-1),
    m_previousUnitCount(0),
	m_fontsize(13.0f)
{
    //SetMovable(false);
    SetSize( 250, 550 );  // 2x6 buildings, 6 fleet slots (single column)
	if( g_windowManager->WindowH() > 480 )
	{
		SetPosition( 0, 100 );
	}
	else
	{
		SetPosition( 0, 0 );
	}

    LoadProperties( "WindowUnitCreation" );
}


SidePanel::~SidePanel()
{
	if( m_currentFleetId != -1 && m_fleetTeamId != -1 )
	{
		Team *fleetTeam = g_app->GetWorld()->GetTeam( m_fleetTeamId );
		
		if( fleetTeam &&
		    fleetTeam->m_fleets.ValidIndex( m_currentFleetId ) &&
		    !fleetTeam->m_fleets[ m_currentFleetId ]->m_active )
		{
			Fleet *fleet = fleetTeam->GetFleet( m_currentFleetId );
			fleetTeam->m_fleets.RemoveData( m_currentFleetId );
			delete fleet;
		}
	}
	
    SaveProperties( "WindowUnitCreation" );
}


void SidePanel::Create()
{
    int x = 26;
    int y = 50;
    int colGap = 75;
    int rowGap = 75;

	m_fontsize = 48 / 1.2f / 4.0f;

    // Territory roster: use first territory (or USA fallback if none)
    Team *myTeam = g_app->GetWorld()->GetMyTeam();
    int primaryTerritory = World::TerritoryUSA;
    if( myTeam && myTeam->m_territories.Size() > 0 )
        primaryTerritory = myTeam->m_territories[0];

    TerritoryRosterSlot buildingSlots[6][2];
    int rowsUsed = GetTerritoryBuildingRoster( primaryTerritory, buildingSlots );

    for( int r = 0; r < rowsUsed && r < 6; ++r )
    {
        for( int c = 0; c < 2; ++c )
        {
            int unitType = buildingSlots[r][c].unitType;
            if( unitType < 0 ) continue;

            char name[64];
            snprintf( name, sizeof(name), "Build_%d_%d", r, c );
            UnitPlacementButton *btn = new UnitPlacementButton( unitType );
            btn->SetProperties( name, x + c * colGap, y + r * rowGap, 48, 48, "", "tooltip_place_unit", false, true );
            RegisterButton( btn );
        }
    }

    PanelModeButton *fmb = new PanelModeButton( ModeFleetPlacement, true );
    fmb->SetProperties( "FleetMode", x, m_h - 58, 48, 48, "dialog_fleets", "tooltip_fleet_button", true, true );
    strcpy( fmb->bmpImageFilename, "graphics/fleet.bmp" );
    RegisterButton( fmb );
}


void SidePanel::Render( bool hasFocus )
{
    int currentSelectionId = g_app->GetWorldRenderer()->GetCurrentSelectionId();
    if( currentSelectionId == -1 )
    {
        if( m_mode != ModeUnitPlacement &&
            m_mode != ModeFleetPlacement )
        {
            ChangeMode( ModeUnitPlacement );
            m_currentFleetId = -1;
            m_fleetTeamId = -1;
        }
    }

    if( m_currentFleetId != -1 && m_fleetTeamId != g_app->GetWorld()->m_myTeamId )
    {
        EclRemoveWindow( "Placement" );
        ChangeMode( ModeUnitPlacement );
    }

    Team *myTeam = g_app->GetWorld()->GetMyTeam();

    if( m_mode == ModeUnitPlacement )
    {
        PanelModeButton *button = (PanelModeButton *)GetButton("FleetMode");
        //if( !button->m_disabled )
        {
            int shipsRemaining = 0;
            if( myTeam )
            {
                shipsRemaining += myTeam->m_unitsAvailable[WorldObject::TypeBattleShip];
                shipsRemaining += myTeam->m_unitsAvailable[WorldObject::TypeBattleShip2];
                shipsRemaining += myTeam->m_unitsAvailable[WorldObject::TypeBattleShip3];
                shipsRemaining += myTeam->m_unitsAvailable[WorldObject::TypeCarrier];
                shipsRemaining += myTeam->m_unitsAvailable[WorldObject::TypeCarrierLight];
                shipsRemaining += myTeam->m_unitsAvailable[WorldObject::TypeSub];
                shipsRemaining += myTeam->m_unitsAvailable[WorldObject::TypeSubG];
                shipsRemaining += myTeam->m_unitsAvailable[WorldObject::TypeSubC];
                shipsRemaining += myTeam->m_unitsAvailable[WorldObject::TypeSubK];

                bool unplacedFleets = false;
                bool outOfCredits = myTeam->m_unitCredits <= 0;
                if( !g_app->GetGame()->GetOptionValue("VariableUnitCounts") ) outOfCredits = false;

                if( myTeam->m_fleets.Size() > 0 &&
                    myTeam->GetFleet( myTeam->m_fleets.Size() - 1 )->m_active == false )
                {
                    unplacedFleets = true;
                }
                if( (shipsRemaining <= 0 ||
                    myTeam->m_unitCredits <= 0 )&&
                    !unplacedFleets )
                {
                    button->m_disabled = true;
                }
                else
                {
                    button->m_disabled = false;
                }
            }
        }
    }

    FadingWindow::Render( hasFocus, true );    

    char text[128];
    switch(m_mode)
    {
        case ModeUnitPlacement:     strcpy(text, LANGUAGEPHRASE("dialog_placeunit"));   break;
        case ModeFleetPlacement:    strcpy(text, LANGUAGEPHRASE("dialog_createfleet"));  break;
    }
    g_renderer2d->TextCentreSimple(m_x+m_w/2, m_y+10, White, m_fontsize, text );

    if( m_mode == ModeFleetPlacement )
    {
        if( m_currentFleetId != -1 )
        {
            Team *myTeam = g_app->GetWorld()->GetTeam( g_app->GetWorld()->m_myTeamId );

            //
            // clean up template
            
            if( myTeam && myTeam->m_fleets.ValidIndex(m_currentFleetId) )
            {          
                Fleet *fleet = myTeam->m_fleets[ m_currentFleetId ];
                
                //
                // Count how many of each type are in the template

                int typeCounts[WorldObject::NumObjectTypes];
                for( int i = 0; i < WorldObject::NumObjectTypes; ++i )
                {
                    typeCounts[i] = 0;
                }
                
                for( int i = 0; i < fleet->m_memberType.Size(); ++i )
                {
                    typeCounts[ fleet->m_memberType[i] ]++;
                }
                
                //
                // Remove units from template that exceed available count or credits

                for( int i = fleet->m_memberType.Size() - 1; i >= 0; --i )
                {
                    int unitType = fleet->m_memberType[i];
                    int unitsOfThisType = typeCounts[unitType];
                    
                    //
                    // Remove if we don't have enough of this type available

                    if( unitsOfThisType > myTeam->m_unitsAvailable[unitType] )
                    {
                        fleet->m_memberType.RemoveData(i);
                        typeCounts[unitType]--;
                    }

                    //
                    // Or if we cant afford this specific unit

                    else if( myTeam->m_unitCredits < g_app->GetWorld()->GetUnitValue(unitType) )
                    {
                        fleet->m_memberType.RemoveData(i);
                        typeCounts[unitType]--;
                    }
                }

                g_renderer->SetBlendMode( Renderer::BlendModeNormal );
                
                if( m_mode == ModeFleetPlacement )
                {
                    // 6 fleet slots in single column, like original Defcon
                    int fleetSlotX = 160, slotW = 40, slotH = 40, slotY = 55, slotGap = 35;
                    for( int i = 0; i < 6; ++i )
                    {
                        int sx = m_x + fleetSlotX;
                        int sy = m_y + slotY + i * (slotH + slotGap);
                        g_renderer2d->RectFill( sx, sy, slotW, slotH, Colour(90,90,170,200) );
                        g_renderer2d->Rect(sx, sy, slotW, slotH, White );
                    }
                }

                //
                // end rect fill batch and begin sprite batch to ensure sprites render on top

                g_renderer2d->EndRectFillBatch();
                g_renderer2d->BeginStaticSpriteBatch();
                
                int fleetSlotX = 160, slotW = 40, slotH = 40, slotY = 55, slotGap = 35;
                for( int i = 0; i < myTeam->m_fleets[ m_currentFleetId ]->m_memberType.Size(); ++i )
                {
                    int type = myTeam->m_fleets[ m_currentFleetId ]->m_memberType[i];
                    int sx = m_x + fleetSlotX + 2;
                    int sy = m_y + slotY + i * (slotH + slotGap) + 2;
                    Image *bmpImage	= g_resource->GetImage( g_app->GetWorldRenderer()->GetImageFile(type, myTeam->m_teamId) );
                    g_renderer->SetBlendMode( Renderer::BlendModeAdditive );
                    g_renderer2d->StaticSprite( bmpImage, sx, sy, 36, 36, myTeam->GetTeamColour() );
                    g_renderer->SetBlendMode( Renderer::BlendModeNormal );
                }
            }
        }
    }
    int variableTeamUnits = g_app->GetGame()->GetOptionValue("VariableUnitCounts");
    if( myTeam )
    {
        if( variableTeamUnits == 1 )
        {
            Colour col = Colour(255,255,0,255);
            char msg[128];
            strcpy( msg, LANGUAGEPHRASE("dialog_credits_1") );
            LPREPLACEINTEGERFLAG( 'C', myTeam->m_unitCredits, msg );
            g_renderer2d->TextCentreSimple( m_x+m_w/2, m_y+m_h-28, col, m_fontsize, msg );

            strcpy( msg, LANGUAGEPHRASE("dialog_credits_2"));
            g_renderer2d->TextCentreSimple( m_x+m_w/2, m_y+m_h-15, col, m_fontsize, msg );
        }

        int totalUnits = 0;
        for( int i = 0; i < WorldObject::NumObjectTypes; ++i )
        {
            totalUnits += myTeam->m_unitsAvailable[i];
        }

        if( totalUnits == 0 &&
            m_previousUnitCount > 0 &&
            m_currentFleetId == -1 &&
            !g_app->GetTutorial() )
        {
            EclRemoveWindow("Side Panel" );
            return;
        }
        m_previousUnitCount = totalUnits;
    }
}


void SidePanel::ChangeMode( int mode )
{
    m_mode = mode;

    int x = 26;
    int y = 50;
    m_w = 250;

    int currentTeamId = g_app->GetWorld()->m_myTeamId;

    if( m_currentFleetId != -1 && m_fleetTeamId != currentTeamId )
    {
        Team *oldTeam = g_app->GetWorld()->GetTeam( m_fleetTeamId );
        if( oldTeam &&
            oldTeam->m_fleets.ValidIndex( m_currentFleetId ) &&
            !oldTeam->m_fleets[ m_currentFleetId ]->m_active )
        {
            Fleet *fleet = oldTeam->GetFleet( m_currentFleetId );
            oldTeam->m_fleets.RemoveData( m_currentFleetId );
            delete fleet;
        }
        m_currentFleetId = -1;
        m_fleetTeamId = -1;
    }

    InterfaceWindow::Remove();
//    CreateExpandButton();

        if( m_mode == ModeUnitPlacement )
        {
        int colGap = 75, rowGap = 75;

        Team *myTeam = g_app->GetWorld()->GetTeam( currentTeamId );
        int primaryTerritory = World::TerritoryUSA;
        if( myTeam && myTeam->m_territories.Size() > 0 )
            primaryTerritory = myTeam->m_territories[0];

        TerritoryRosterSlot buildingSlots[6][2];
        int rowsUsed = GetTerritoryBuildingRoster( primaryTerritory, buildingSlots );

        for( int r = 0; r < rowsUsed && r < 6; ++r )
        {
            for( int c = 0; c < 2; ++c )
            {
                int unitType = buildingSlots[r][c].unitType;
                if( unitType < 0 ) continue;

                char name[64];
                snprintf( name, sizeof(name), "Build_%d_%d", r, c );
                UnitPlacementButton *btn = new UnitPlacementButton( unitType );
                btn->SetProperties( name, x + c * colGap, y + r * rowGap, 48, 48, "", "tooltip_place_unit", false, true );
                RegisterButton( btn );
            }
        }

        int shipsRemaining = 0;
        if( myTeam )
        {
            shipsRemaining += myTeam->m_unitsAvailable[WorldObject::TypeBattleShip];
            shipsRemaining += myTeam->m_unitsAvailable[WorldObject::TypeBattleShip2];
            shipsRemaining += myTeam->m_unitsAvailable[WorldObject::TypeBattleShip3];
            shipsRemaining += myTeam->m_unitsAvailable[WorldObject::TypeCarrier];
            shipsRemaining += myTeam->m_unitsAvailable[WorldObject::TypeCarrierLight];
            shipsRemaining += myTeam->m_unitsAvailable[WorldObject::TypeSub];
            shipsRemaining += myTeam->m_unitsAvailable[WorldObject::TypeSubG];
            shipsRemaining += myTeam->m_unitsAvailable[WorldObject::TypeSubC];
            shipsRemaining += myTeam->m_unitsAvailable[WorldObject::TypeSubK];
        }

        PanelModeButton *fmb = new PanelModeButton( ModeFleetPlacement, true );
        fmb->SetProperties( "FleetMode", x, m_h - 58, 48, 48, "dialog_fleets", "tooltip_fleet_button", true, true );
        strcpy( fmb->bmpImageFilename, "graphics/fleet.bmp" );
        if( shipsRemaining == 0 )
        {
            fmb->m_disabled = true;
        }

        RegisterButton( fmb );
    }
    else if( m_mode == ModeFleetPlacement )
    {
        m_w = 250;
        int colGap = 75, rowGap = 75;

        Team *myTeam = g_app->GetWorld()->GetTeam( currentTeamId );
        int primaryTerritory = World::TerritoryUSA;
        if( myTeam && myTeam->m_territories.Size() > 0 )
            primaryTerritory = myTeam->m_territories[0];

        TerritoryRosterSlot navySlots[6][2];
        int navyRows = GetTerritoryNavyRoster( primaryTerritory, navySlots );

        for( int r = 0; r < navyRows && r < 6; ++r )
        {
            for( int c = 0; c < 2; ++c )
            {
                int unitType = navySlots[r][c].unitType;
                if( unitType < 0 ) continue;

                char name[64];
                snprintf( name, sizeof(name), "Navy_%d_%d", r, c );
                AddToFleetButton *btn = new AddToFleetButton( unitType );
                btn->SetProperties( name, x + c * colGap, y + r * rowGap, 48, 48, "", "tooltip_place_ship", false, true );
                RegisterButton( btn );
            }
        }
        PanelModeButton *umb = new PanelModeButton( ModeUnitPlacement, true );
        umb->SetProperties( "UnitMode", x, m_h - 58, 48, 48, "dialog_units", "", true, false );
        strcpy( umb->bmpImageFilename, "graphics/units.bmp" );
        RegisterButton( umb );

        int fleetSlotX = 160;  // Right column, to the right of 2x6 ship grid (26+75+48=149)
        ClearFleetButton *cfb = new ClearFleetButton();
        cfb->SetProperties( "ClearFleet", fleetSlotX, 20, 88, 28, "dialog_clear_fleet", "tooltip_clear_fleet", false, true );
        RegisterButton( cfb );

        // 6 fleet slots in single column, like original Defcon
        int slotW = 40, slotH = 40, slotY = 55, slotGap = 35;
        for( int i = 0; i < 6; ++i )
        {
            char name[128];
            sprintf( name, "Remove Unit %d", i );
            int sx = fleetSlotX;
            int sy = slotY + i * (slotH + slotGap);
            RemoveUnitButton *rb = new RemoveUnitButton();
            rb->SetProperties( name, sx, sy, slotW, slotH, "", "tooltip_fleet_remove", false, true );
            rb->m_memberId = i;
            RegisterButton( rb );
        }

        FleetPlacementButton *fpb = new FleetPlacementButton();
        fpb->SetProperties( "PlaceFleet", fleetSlotX, slotY + 6*(slotH+slotGap), 40, 40, "dialog_place_fleet", "tooltip_fleet_place", true, true );
        RegisterButton( fpb );

        if( myTeam )
        {
            bool needNewFleet = ( m_currentFleetId == -1 ||
                                  myTeam->m_fleets.Size() == 0 ||
                                  myTeam->m_fleets[myTeam->m_fleets.Size()-1]->m_active == true );
            if( needNewFleet )
            {
                int shipsRemaining = 0;
                shipsRemaining += myTeam->m_unitsAvailable[WorldObject::TypeBattleShip];
                shipsRemaining += myTeam->m_unitsAvailable[WorldObject::TypeBattleShip2];
                shipsRemaining += myTeam->m_unitsAvailable[WorldObject::TypeBattleShip3];
                shipsRemaining += myTeam->m_unitsAvailable[WorldObject::TypeCarrier];
                shipsRemaining += myTeam->m_unitsAvailable[WorldObject::TypeCarrierLight];
                shipsRemaining += myTeam->m_unitsAvailable[WorldObject::TypeSub];
                shipsRemaining += myTeam->m_unitsAvailable[WorldObject::TypeSubG];
                shipsRemaining += myTeam->m_unitsAvailable[WorldObject::TypeSubC];
                shipsRemaining += myTeam->m_unitsAvailable[WorldObject::TypeSubK];
                if( shipsRemaining > 0 )
                {
                    m_currentFleetId = myTeam->m_fleets.Size();
                    m_fleetTeamId = myTeam->m_teamId;
                    if( myTeam->m_type == Team::TypeAI )
                    {
                        myTeam->CreateFleet();
                    }
                    else
                    {
                        g_app->GetClientToServer()->RequestFleet( myTeam->m_teamId );
                    }
                }
            }
        }
    }
}

/*  ####################
    Unit Placement Button
    Places individual units
    ####################
*/

UnitPlacementButton::UnitPlacementButton( int unitType )
{
	m_unitType = unitType;
	m_disabled = false;
}

void UnitPlacementButton::Render( int realX, int realY, bool highlighted, bool clicked )
{ 
#ifndef NON_PLAYABLE
    SidePanel *parent = (SidePanel *)m_parent;
   
    m_disabled = false;
	Team *team          = g_app->GetWorld()->GetMyTeam();
	if( !team ) 
    {
        m_disabled = true;
        return;
    }
    else
    {
        m_disabled = false;
    }

	Image *bmpImage			= g_resource->GetImage( g_app->GetWorldRenderer()->GetImageFile(m_unitType, team->m_teamId) );
    g_renderer->SetBlendMode( Renderer::BlendModeSubtractive );
    Colour col(30,30,30,0);
    for( int x = -1; x <= 1; ++x )
    {
        for( int y = -1; y <= 1; ++y )
        {
            g_renderer2d->StaticSprite( bmpImage, realX+x, realY+y, m_w, m_h, col );
        }
    }
    g_renderer->SetBlendMode( Renderer::BlendModeAdditive );
	Colour colour       = team->GetTeamColour();
	
	if( highlighted )
	{
		colour.m_b += 40;
		colour.m_g += 40;
		colour.m_r += 40;
	}
	if( clicked ) 
	{
		colour.m_b += 80;
		colour.m_g += 80;
		colour.m_r += 80;
	}
	if( team->m_unitsAvailable[m_unitType] <= 0 ||
        team->m_unitCredits < g_app->GetWorld()->GetUnitValue(m_unitType) )
	{
		m_disabled = true;
		colour = Colour(50, 50, 50, 255);
	}
    else
    {
        m_disabled = false;
    }

    if( m_disabled )
    {
        colour = Colour(50, 50, 50, 255);
    }
	
    colour.m_a = 255;
    
	//float size = 32.0f;
	if( bmpImage )
	{
		g_renderer2d->StaticSprite( bmpImage, realX, realY, m_w, m_h, colour );
        g_renderer->SetBlendMode( Renderer::BlendModeNormal );
	}

    const char *unitName = WorldObject::GetName( m_unitType );

	char caption[256];
	sprintf(caption, "%s(%u)", unitName, team->m_unitsAvailable[m_unitType]);
    
    Colour textCol = White;
    if( m_disabled )
    {
        textCol = Colour(50,50,50,255);
    }
	g_renderer2d->TextCentreSimple( realX + m_w/2, realY+m_h, textCol, parent->m_fontsize, caption );

    int variableTeamUnits = g_app->GetGame()->GetOptionValue("VariableUnitCounts");
    if( variableTeamUnits == 1 )
    {
        char caption[8];
        sprintf( caption, "%d", g_app->GetWorld()->GetUnitValue( m_unitType ));
        Colour col = Colour(255,255,0,255);

	    g_renderer2d->TextCentreSimple( realX + m_w/2, realY+m_h+14, col, parent->m_fontsize, caption );
    }

    if( highlighted || clicked )
    {
        InfoWindow *info = (InfoWindow *)EclGetWindow("Info");
        if( info )
        {
            info->SwitchInfoDisplay( m_unitType, -1 );
        }
    }
#endif
}
    


void UnitPlacementButton::MouseUp()
{
#ifndef NON_PLAYABLE
    if( g_app->GetWorld()->GetTimeScaleFactor() == 0 )
    {
        return;
    }
	if( !m_disabled )
	{
		if( !EclGetWindow( "Placement" ) )
		{
            SidePanel *parent = (SidePanel *)m_parent;
            parent->m_mode = SidePanel::ModeUnitPlacement;
			PlacementIconWindow *icon = new PlacementIconWindow( "Placement", m_unitType );
			EclRegisterWindow( icon, m_parent );

			g_app->GetMapRenderer()->m_showTeam[ g_app->GetWorld()->m_myTeamId ] = true;
            if( m_unitType == WorldObject::TypeRadarStation || m_unitType == WorldObject::TypeRadarEW )
            {
                g_app->GetWorldRenderer()->m_showRadar = true;
            }
            InfoWindow *info = (InfoWindow *)EclGetWindow("Info");
            if( info )
            {
                info->SwitchInfoDisplay( m_unitType, -1 );
            }
		}
		else
		{
	        EclRemoveWindow( "Placement" );
		}
	}
#endif
}

/*  ####################
    Fleet Mode Button
    Switches side panel to fleet creation mode
    ####################
*/

PanelModeButton::PanelModeButton( int mode, bool useImage )
:   m_disabled(false)
{
    m_mode = mode;
    m_imageButton = useImage;
}

void PanelModeButton::Render(int realX, int realY, bool highlighted, bool clicked)
{
#ifndef NON_PLAYABLE
    SidePanel *parent = (SidePanel *)m_parent;

    if( m_imageButton )
    {
	    Team *team          = g_app->GetWorld()->GetTeam(g_app->GetWorld()->m_myTeamId);
	    if( !team ) return;

	    Image *bmpImage			= g_resource->GetImage( bmpImageFilename );

        g_renderer->SetBlendMode( Renderer::BlendModeSubtractive );
        Colour col(30,30,30,0);
        for( int x = -1; x <= 1; ++x )
        {
            for( int y = -1; y <= 1; ++y )
            {
                g_renderer2d->StaticSprite( bmpImage, realX+x, realY+y, m_w, m_h, col );
            }
        }
        g_renderer->SetBlendMode( Renderer::BlendModeAdditive );
	    Colour colour       = team->GetTeamColour();
	    
	    if( highlighted )
	    {
		    colour.m_b += 40;
		    colour.m_g += 40;
		    colour.m_r += 40;
	    }
	    if( clicked ) 
	    {
		    colour.m_b += 80;
		    colour.m_g += 80;
		    colour.m_r += 80;
	    }
        colour.m_a = 255;

        if( m_disabled )
        {
            colour = Colour(50, 50, 50, 255);
        }
	    
	    float size = 32.0f;
	    if( bmpImage )
	    {
		    g_renderer2d->StaticSprite( bmpImage, realX, realY, m_w, m_h, colour );
            g_renderer->SetBlendMode( Renderer::BlendModeNormal );
	    }
        Colour textCol = White;
        if( m_disabled )
        {
            textCol = Colour(50,50,50,255);
        }

		if( m_captionIsLanguagePhrase )
		{
			g_renderer2d->TextCentreSimple( realX + m_w/2, realY+m_h, textCol, parent->m_fontsize, LANGUAGEPHRASE(m_caption) );
		}
		else
		{
			g_renderer2d->TextCentreSimple( realX + m_w/2, realY+m_h, textCol, parent->m_fontsize, m_caption );
		}
    }
    else
    {
        InterfaceButton::Render( realX, realY, highlighted, clicked );
    }
#endif
}

void PanelModeButton::MouseUp()
{
#ifndef NON_PLAYABLE
    if( g_app->GetWorld()->GetTimeScaleFactor() == 0 )
    {
        return;
    }
    if( !m_disabled )
    {
        SidePanel *parent = (SidePanel *)m_parent;
        parent->ChangeMode(m_mode);
    }
#endif
}

/*  ####################
    AddToFleet Button
    Adds a ship to the current fleet
    ####################
*/

AddToFleetButton::AddToFleetButton( int unitType )
{
	m_unitType = unitType;
	m_disabled = false;
}

void AddToFleetButton::Render( int realX, int realY, bool highlighted, bool clicked )
{ 
#ifndef NON_PLAYABLE
    SidePanel *parent = (SidePanel *)m_parent;

    Team *team = g_app->GetWorld()->GetTeam(g_app->GetWorld()->m_myTeamId);
    if( parent->m_mode == SidePanel::ModeFleetPlacement )
    {
	    m_disabled = false;
	    Image *bmpImage		= g_resource->GetImage( g_app->GetWorldRenderer()->GetImageFile(m_unitType, team->m_teamId) );
        g_renderer->SetBlendMode( Renderer::BlendModeSubtractive );
        Colour col(30,30,30,0);
        for( int x = -1; x <= 1; ++x )
        {
            for( int y = -1; y <= 1; ++y )
            {
                g_renderer2d->StaticSprite( bmpImage, realX+x, realY+y, m_w, m_h, col );
            }
        }
        g_renderer->SetBlendMode( Renderer::BlendModeAdditive );
	    Colour colour       = team->GetTeamColour();
	    
	    if( highlighted )
	    {
		    colour.m_b += 40;
		    colour.m_g += 40;
		    colour.m_r += 40;
	    }
	    if( clicked ) 
	    {
		    colour.m_b += 80;
		    colour.m_g += 80;
		    colour.m_r += 80;
	    }
	    if( team->m_unitsAvailable[m_unitType] <= 0 ||
            team->m_unitCredits < g_app->GetWorld()->GetUnitValue(m_unitType) ||
            ( parent->m_currentFleetId != -1 && team->m_fleets.ValidIndex( parent->m_currentFleetId ) &&
              team->m_fleets[ parent->m_currentFleetId ]->m_memberType.Size() >= 6 ) )
	    {
		    m_disabled = true;
	    }
        else
        {
            m_disabled = false;
        }

        if( m_disabled )
        {
            colour = Colour(50, 50, 50, 255);
        }
    	    
	    //float size = 32.0f;
	    if( bmpImage )
	    {
		    g_renderer2d->StaticSprite( bmpImage, realX, realY, m_w, m_h, colour );
            g_renderer->SetBlendMode( Renderer::BlendModeNormal );
	    }

        const char *unitName = WorldObject::GetName( m_unitType );

        char caption[256];
	    sprintf(caption, "%s(%u)", unitName, team->m_unitsAvailable[m_unitType]);

        Colour textCol = White;
        if( m_disabled )
        {
            textCol = Colour(50,50,50,255);
        }
	    g_renderer2d->TextCentreSimple( realX + m_w/2, realY+m_h, textCol, parent->m_fontsize, caption );

        int variableTeamUnits = g_app->GetGame()->GetOptionValue("VariableUnitCounts");
        if( variableTeamUnits == 1 )
        {
            char caption[8];
            sprintf( caption, "%d", g_app->GetWorld()->GetUnitValue( m_unitType ));
            Colour col = Colour(255,255,0,255);

	        g_renderer2d->TextCentreSimple( realX + m_w/2, realY+m_h+14, col, parent->m_fontsize, caption );
        }
        if( highlighted || clicked )
        {
            InfoWindow *info = (InfoWindow *)EclGetWindow("Info");
            if( info )
            {
                info->SwitchInfoDisplay( m_unitType, -1 );
            }
        }
    }
    else
    {
        m_disabled = true;
    }
#endif
}

void AddToFleetButton::MouseUp()
{
#ifndef NON_PLAYABLE
    if( g_app->GetWorld()->GetTimeScaleFactor() == 0 )
    {
        return;
    }
    if( !m_disabled )
    {
        SidePanel *parent = (SidePanel *)m_parent;
        Team *team = g_app->GetWorld()->GetTeam(g_app->GetWorld()->m_myTeamId);

        if( parent->m_currentFleetId != -1 &&
            team->m_fleets.ValidIndex( parent->m_currentFleetId ) )
        {
            if( team->m_fleets[ parent->m_currentFleetId ]->m_memberType.Size() < 6 )
            {
                Fleet *fleet = team->m_fleets[ parent->m_currentFleetId ];
                if( g_keys[KEY_SHIFT] )
                {
                    for( int i = team->m_fleets[ parent->m_currentFleetId ]->m_memberType.Size(); i < 6; ++i )
                    {
                        if( team->m_unitsAvailable[m_unitType] > 0 &&
                            team->m_unitCredits >= g_app->GetWorld()->GetUnitValue( m_unitType ) )
                        {
                            
                            //
                            // dont consume units when building fleet

                            fleet->m_memberType.PutData( m_unitType );
                        }
                    }

                }
                else
                {

                    //
                    // dont consume units when building fleet
                    
                    fleet->m_memberType.PutData( m_unitType );
                }

                if( fleet->m_memberType.Size() == 6 )
                {
                    FleetPlacementButton *button = (FleetPlacementButton *)parent->GetButton("PlaceFleet");
                    button->m_disabled = false;                    
                    button->MouseUp();
                }
            }
        }
    }
#endif
}

/*  ####################
    RemoveUnit Button
    Removes the selected unit from current fleet
    ####################
*/

ClearFleetButton::ClearFleetButton()
:   InterfaceButton(),
    m_disabled(false)
{
}

void ClearFleetButton::Render( int realX, int realY, bool highlighted, bool clicked )
{
    SidePanel *parent = (SidePanel *)m_parent;
    if( parent->m_mode != SidePanel::ModeFleetPlacement )
        return;

    m_disabled = false;
    Team *team = g_app->GetWorld()->GetTeam( g_app->GetWorld()->m_myTeamId );
    if( !team || parent->m_currentFleetId == -1 ||
        !team->m_fleets.ValidIndex( parent->m_currentFleetId ) )
    {
        m_disabled = true;
    }
    else if( team->m_fleets[ parent->m_currentFleetId ]->m_memberType.Size() == 0 )
    {
        m_disabled = true;
    }

    int transparency = g_renderer2d->m_alpha * 255;
    if( highlighted || clicked ) transparency = 255;

    Colour col = Colour(80,80,120, transparency);
    if( m_disabled ) col = Colour(60,60,80, transparency);
    else if( highlighted ) col = Colour(100,100,140, transparency);
    else if( clicked ) col = Colour(120,120,160, transparency);

    g_renderer2d->RectFill( realX, realY, m_w, m_h, col );
    g_renderer2d->Rect( realX, realY, m_w, m_h, Colour(150,150,180, transparency) );

    Colour textCol = m_disabled ? Colour(100,100,100,255) : White;
    g_renderer2d->TextCentreSimple( realX + m_w/2, realY + m_h/2 - 6, textCol, parent->m_fontsize, LANGUAGEPHRASE("dialog_clear_fleet") );
}

void ClearFleetButton::MouseUp()
{
    if( g_app->GetWorld()->GetTimeScaleFactor() == 0 )
        return;
    if( m_disabled )
        return;

    SidePanel *parent = (SidePanel *)m_parent;
    Team *team = g_app->GetWorld()->GetTeam( g_app->GetWorld()->m_myTeamId );
    if( parent->m_currentFleetId != -1 &&
        team->m_fleets.ValidIndex( parent->m_currentFleetId ) &&
        !team->m_fleets[ parent->m_currentFleetId ]->m_active )
    {
        team->m_fleets[ parent->m_currentFleetId ]->m_memberType.Empty();
    }
}

RemoveUnitButton::RemoveUnitButton()
:   InterfaceButton(),
    m_disabled(true)
{
}

void RemoveUnitButton::Render( int realX, int realY, bool highlighted, bool clicked )
{
    SidePanel *parent = (SidePanel *)m_parent;
    if( parent->m_mode == SidePanel::ModeFleetPlacement )
    {
        m_disabled = false;
    }
    else
    {
        m_disabled = true;
    }

    //g_renderer2d->RectFill( m_x, m_y, m_w, m_h, Colour(255,0,0,255) );
}

void RemoveUnitButton::MouseUp()
{
    if( g_app->GetWorld()->GetTimeScaleFactor() == 0 )
    {
        return;
    }
    if( !m_disabled )
    {
        SidePanel *parent = (SidePanel *)m_parent;
        Team *myTeam = g_app->GetWorld()->GetTeam( g_app->GetWorld()->m_myTeamId );
        if( parent->m_currentFleetId != -1 &&
            myTeam->m_fleets.ValidIndex( parent->m_currentFleetId ) )
        {
            if( myTeam->m_fleets[ parent->m_currentFleetId ]->m_memberType.ValidIndex(m_memberId) &&
                !myTeam->m_fleets[ parent->m_currentFleetId ]->m_active )
            {
                //
                // dont add back to unitsAvailable

                myTeam->m_fleets[ parent->m_currentFleetId ]->m_memberType.RemoveData(m_memberId);
            }
        }
    }
}

/*  ####################
    NewFleetButton
    Creates a new fleet
    ####################
*/

NewFleetButton::NewFleetButton()
:   InterfaceButton(),
    m_disabled(false)
{
}

void NewFleetButton::Render( int realX, int realY, bool highlighted, bool clicked )
{
    SidePanel *parent = (SidePanel *)m_parent;

    int transparency = g_renderer2d->m_alpha * 255;
    if( highlighted || clicked ) transparency = 255;
    
    Colour col = Colour(50,50,170, transparency);
    if( g_app->GetWorld()->GetTeam( g_app->GetWorld()->m_myTeamId )->m_fleets.Size() >= 9 )
    {
        m_disabled = true;
    }

    if( highlighted )   col = Colour(70,70,170, transparency);    
    if( clicked )       col = Colour(100,100,170, transparency);
    if( m_disabled )    col = Colour(100,100,100, transparency);

    
    g_renderer2d->RectFill ( realX, realY, m_w, m_h, col );
    g_renderer2d->Rect     ( realX, realY, m_w, m_h, Colour(150,150,200,transparency) );   

	if( m_captionIsLanguagePhrase )
	{
	    g_renderer2d->TextCentre ( realX + m_w/2, realY + m_h/5, Colour(255,255,255,transparency), parent->m_fontsize, LANGUAGEPHRASE(m_caption) );
	}
	else
	{
	    g_renderer2d->TextCentre ( realX + m_w/2, realY + m_h/5, Colour(255,255,255,transparency), parent->m_fontsize, m_caption );
	}
}

void NewFleetButton::MouseUp()
{
    Team *myTeam = g_app->GetWorld()->GetTeam( g_app->GetWorld()->m_myTeamId );

    if( myTeam->m_fleets.Size() < 9 )
    {
        if( myTeam->m_type == Team::TypeAI )
        {
            myTeam->CreateFleet();
        }
        else
        {
            g_app->GetClientToServer()->RequestFleet( myTeam->m_teamId );
        }
    }
}

/*  ####################
    Fleet Placement Button
    Creates fleet placement icon
    ####################
*/

FleetPlacementButton::FleetPlacementButton()
{
	m_disabled = false;
}

void FleetPlacementButton::Render( int realX, int realY, bool highlighted, bool clicked )
{
    SidePanel *parent = (SidePanel *)m_parent;

    g_renderer2d->SetClip( realX, realY, m_w, m_h );
    Team *myTeam = g_app->GetWorld()->GetTeam( g_app->GetWorld()->m_myTeamId );
    int currentFleetId = parent->m_currentFleetId;
    m_disabled = false;
    if( currentFleetId == -1 ||
        !myTeam->m_fleets.ValidIndex(currentFleetId) )
    {
        m_disabled = true;
    }
    else
    {
        if( myTeam->m_fleets[ currentFleetId ]->m_memberType.Size() == 0 ||
            myTeam->m_fleets[ currentFleetId ]->m_active )
        {
            m_disabled = true;
        }
    }

    if( !m_disabled )
    {
        Colour col = myTeam->GetTeamColour();
        if( EclMouseInButton( m_parent, this ) )
        {
            col += Colour(60,60,60,0);
        }
        if( clicked )
        {
            col += Colour(60,60,60,0);
        }

        Image *img = g_resource->GetImage( "graphics/cursor_target.bmp" );
        g_renderer->SetBlendMode( Renderer::BlendModeAdditive );
        g_renderer2d->StaticSprite( img, realX, realY, m_w, m_h, col );
        g_renderer->SetBlendMode( Renderer::BlendModeNormal );
    }
    g_renderer2d->ResetClip();
}
    


void FleetPlacementButton::MouseUp()
{
    if( g_app->GetWorld()->GetTimeScaleFactor() == 0 )
    {
        return;
    }
	if( !m_disabled )
	{
        if( !EclGetWindow( "Placement" ) )
		{
            g_app->GetMapRenderer()->m_showTeam[ g_app->GetWorld()->m_myTeamId ] = true;
            SidePanel *parent = (SidePanel *)m_parent;
			FleetPlacementIconWindow *icon = new FleetPlacementIconWindow( "Placement", parent->m_currentFleetId );
			EclRegisterWindow( icon, m_parent );
		}
		else
		{
	        EclRemoveWindow( "Placement" );
		}
		
	}
}


