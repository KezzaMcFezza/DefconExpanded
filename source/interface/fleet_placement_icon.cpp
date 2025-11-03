#include "lib/universal_include.h"
#include "lib/gucci/input.h"
#include "lib/gucci/window_manager.h"
#include "lib/render/colour.h"
#include "lib/resource/resource.h"
#include "lib/language_table.h"
#include "lib/sound/soundsystem.h"

#include "app/app.h"
#include "app/globals.h"

#include "world/world.h"
#include "world/team.h"

#include "network/ClientToServer.h"

#include "renderer/map_renderer.h"

#include "interface/interface.h"
#include "interface/fleet_placement_icon.h"
#include "interface/side_panel.h"

#include "world/silo.h"
#include "world/sub.h"
#include "world/radarstation.h"
#include "world/battleship.h"
#include "world/airbase.h"
#include "world/carrier.h"
#include "world/fleet.h"


FleetPlacementIconWindow::FleetPlacementIconWindow( char *name, int fleetId )
:   InterfaceWindow( name )
{
	m_fleetId = fleetId;
    SetSize( 128, 128 );
}


void FleetPlacementIconWindow::Render( bool hasFocus )
{
	m_x = g_inputManager->m_mouseX - 80;
    m_y = g_inputManager->m_mouseY - 80;
           
    EclWindow::Render( hasFocus );       
}

void FleetPlacementIconWindow::Create()
{
	int x = 64;
    int y = 64;
    int w = 32;
    int h = 32;

    FleetPlacementIconButton *icon = new FleetPlacementIconButton(m_fleetId);
    icon->SetProperties( "Icon", x, y, w, h, "Icon", "", false, false );
    RegisterButton( icon );
}


// UnitPlacementButton class implementation
// Used to draw image buttons for unit placement
FleetPlacementIconButton::FleetPlacementIconButton( int fleetId )
{
	m_fleetId = fleetId;
}

void FleetPlacementIconButton::Render( int realX, int realY, bool highlighted, bool clicked )
{ 
    Team *team          = g_app->GetWorld()->GetTeam(g_app->GetWorld()->m_myTeamId);
    if( !team ) return;

    //
    // check if we have any units available for this fleet and remove units that exceed available counts

    bool hasUnitsAvailable = false;
    Fleet *fleet = team->m_fleets[m_fleetId];
    
    //
    // count how many of each type are in the template
    
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
    // remove units from template that exceed available count or credits

    for( int i = fleet->m_memberType.Size() - 1; i >= 0; --i )
    {
        int unitType = fleet->m_memberType[i];
        int unitsOfThisType = typeCounts[unitType];
        
        if( unitsOfThisType > team->m_unitsAvailable[unitType] )
        {
            fleet->m_memberType.RemoveData(i);
            typeCounts[unitType]--;
        }
        else if( team->m_unitCredits < g_app->GetWorld()->GetUnitValue(unitType) )
        {
            fleet->m_memberType.RemoveData(i);
            typeCounts[unitType]--;
        }
    }
    
    //
    // check if we have any units available after removing unavailable ones

    for( int i = 0; i < fleet->m_memberType.Size(); ++i )
    {
        int unitType = fleet->m_memberType[i];
        if( team->m_unitsAvailable[unitType] > 0 &&
            team->m_unitCredits >= g_app->GetWorld()->GetUnitValue(unitType) )
        {
            hasUnitsAvailable = true;
            break;
        }
    }
    
    //
    // if no units available exit placement mode and clear fleet 

    if( !hasUnitsAvailable )
    {
        g_app->GetMapRenderer()->m_showTeam[ g_app->GetWorld()->m_myTeamId ] = false;
        EclRemoveWindow( "Placement" ); 
        
        //
        // clear the fleet composition from the side panel

        SidePanel *sidepanel = (SidePanel *)EclGetWindow("Side Panel");
        if( sidepanel && sidepanel->m_currentFleetId != -1 )
        {
            Fleet *fleet = team->m_fleets[sidepanel->m_currentFleetId];
            if( fleet && !fleet->m_active )
            {
                fleet->m_memberType.Empty();
            }
        }
        return;
    }

	Colour colour       = team->GetTeamColour();

    float longitude = 0.0f;
    float latitude = 0.0f;
    
    g_app->GetMapRenderer()->ConvertPixelsToAngle( g_inputManager->m_mouseX, g_inputManager->m_mouseY, &longitude, &latitude );
    if( !team->m_fleets[m_fleetId]->ValidFleetPlacement( Fixed::FromDouble(longitude), Fixed::FromDouble(latitude) ) )
    {
        colour = Colour(50,50,50,200);

        g_app->GetMapRenderer()->m_tooltip->PutData( LANGUAGEPHRASE("tooltip_invalidplacement") );
    }
    else
    {
        g_app->GetMapRenderer()->m_tooltip->PutData( LANGUAGEPHRASE("tooltip_placefleet") );
    }

    g_app->GetMapRenderer()->m_tooltip->PutData( LANGUAGEPHRASE("tooltip_spacetocancel") );


    for( int i = 0; i < team->m_fleets[m_fleetId]->m_memberType.Size(); ++i )
    {
        int unitType = team->m_fleets[m_fleetId]->m_memberType[i];
        bmpImage = g_resource->GetImage( g_app->GetMapRenderer()->m_imageFiles[unitType] );
        float size = 32.0f;

        Fixed exactX = 0;
        Fixed exactY = 0;
        float x, y;

        /*switch(i)
        {
            case 0  :   x = realX; y = realY - 32;           break;
            case 1  :   x = realX; y = realY + 32;      break;
            case 2  :   x = realX + 32; y = realY - 16; break;
            case 3  :   x = realX - 32; y = realY - 16; break;
            case 4  :   x = realX + 32; y = realY + 16; break;
            case 5  :   x = realX - 32; y = realY + 16; break;
        }*/

        team->m_fleets[m_fleetId]->GetFormationPosition( team->m_fleets[m_fleetId]->m_memberType.Size(), i, &exactX, &exactY );
		x = exactX.DoubleValue();
		y = exactY.DoubleValue();
        x *= 10.667;
        y *= -10.667;

        x+= realX;
        y+= realY;
	    
	    if( bmpImage )
	    {
            g_renderer->SetBlendMode( Renderer::BlendModeAdditive );
		    g_renderer->EclipseSprite( bmpImage, x, y, size, size, colour );
            g_renderer->SetBlendMode( Renderer::BlendModeNormal );
	    }
    }
    
    if( g_keys[KEY_SPACE] &&
        g_app->GetWorld()->m_myTeamId != -1 )
    {
        g_app->GetMapRenderer()->m_showTeam[ g_app->GetWorld()->m_myTeamId ] = false;
        EclRemoveWindow( "Placement" );
    }
}
    
void FleetPlacementIconButton::MouseUp()
{
	float longitude, latitude;
	Fixed exactLongitude, exactLatitude;

    g_app->GetMapRenderer()->ConvertPixelsToAngle( g_inputManager->m_mouseX, 
                                                   g_inputManager->m_mouseY, 
                                                   &longitude, &latitude );
	exactLongitude = Fixed::FromDouble(longitude);
	exactLatitude = Fixed::FromDouble(latitude);

    Team *team = g_app->GetWorld()->GetTeam(g_app->GetWorld()->m_myTeamId);
    team->m_fleets[m_fleetId]->m_teamId = team->m_teamId;
	
    if( team->m_fleets[m_fleetId]->ValidFleetPlacement( exactLongitude, exactLatitude ) )
    {
        //
        // create a new fleet for the actual placement

        int newFleetId = team->m_fleets.Size();
        g_app->GetClientToServer()->RequestFleet( team->m_teamId );
        
        for( int i = 0; i < team->m_fleets[m_fleetId]->m_memberType.Size(); ++i )
        {
            g_app->GetMapRenderer()->ConvertPixelsToAngle( g_inputManager->m_mouseX, 
                                                           g_inputManager->m_mouseY, 
                                                           &longitude, &latitude );
            Fixed thisLong = exactLongitude;
            Fixed thisLat = exactLatitude;

            team->m_fleets[m_fleetId]->GetFormationPosition( team->m_fleets[m_fleetId]->m_memberType.Size(), i, &thisLong, &thisLat );
            if( thisLong > 180 )
            {
                thisLong -= 360;
            }
            else if( thisLong < -180 )
            {
                thisLong += 360;
            }

            //
            // place units in the new fleet, not the template fleet

            g_app->GetClientToServer()->RequestPlacement( team->m_teamId, team->m_fleets[m_fleetId]->m_memberType[i],
                                                          thisLong, thisLat, newFleetId );
        }
        
        //
        // mark the new fleet as active, not the template fleet
        
        if( team->m_fleets.ValidIndex(newFleetId) )
        {
            team->m_fleets[newFleetId]->m_longitude = exactLongitude;
            team->m_fleets[newFleetId]->m_latitude = exactLatitude;
            team->m_fleets[newFleetId]->m_active = true;
        }
        
        //
        // remove units from template that exceed available counts after placement

        Fleet *templateFleet = team->m_fleets[m_fleetId];

        int typeCountsAfter[WorldObject::NumObjectTypes];
        for( int i = 0; i < WorldObject::NumObjectTypes; ++i )
        {
            typeCountsAfter[i] = 0;
        }
        
        for( int i = 0; i < templateFleet->m_memberType.Size(); ++i )
        {
            typeCountsAfter[ templateFleet->m_memberType[i] ]++;
        }
        
        for( int i = templateFleet->m_memberType.Size() - 1; i >= 0; --i )
        {
            int unitType = templateFleet->m_memberType[i];
            int unitsOfThisType = typeCountsAfter[unitType];
            
            if( unitsOfThisType > team->m_unitsAvailable[unitType] )
            {
                templateFleet->m_memberType.RemoveData(i);
                typeCountsAfter[unitType]--;
            }
            else if( team->m_unitCredits < g_app->GetWorld()->GetUnitValue(unitType) )
            {
                templateFleet->m_memberType.RemoveData(i);
                typeCountsAfter[unitType]--;
            }
        }
        
        //
        // check if we have any ships left to place

        int shipsRemaining = 0;
        shipsRemaining += team->m_unitsAvailable[WorldObject::TypeBattleShip];
        shipsRemaining += team->m_unitsAvailable[WorldObject::TypeCarrier];
        shipsRemaining += team->m_unitsAvailable[WorldObject::TypeSub];
        
        if( shipsRemaining <= 0 )
        {
            //
            // no more ships available, exit fleet placement mode and clear fleet
            
            g_app->GetMapRenderer()->m_showTeam[ g_app->GetWorld()->m_myTeamId ] = false;        
            EclRemoveWindow( "Placement" );
            
            SidePanel *sidepanel = (SidePanel *)EclGetWindow("Side Panel");
            if( sidepanel )
            {
                //
                // clear the fleet before changing modes
                
                if( sidepanel->m_currentFleetId != -1 )
                {
                    Fleet *fleet = team->m_fleets[sidepanel->m_currentFleetId];
                    if( fleet && !fleet->m_active )
                    {
                        fleet->m_memberType.Empty();
                    }
                }
                
                sidepanel->ChangeMode(SidePanel::ModeUnitPlacement);
                sidepanel->m_currentFleetId = -1;
            }
        }
    }
    else
    {
#ifdef TOGGLE_SOUND
        g_soundSystem->TriggerEvent( "Interface", "Error" );
#endif
    }
}
