#include "lib/universal_include.h"

#include "app/app.h"
#include "app/globals.h"

#include "interface/interface.h"

#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/render/colour.h"
#ifdef TOGGLE_SOUND
#include "lib/sound/soundsystem.h"
#endif

#include "world/world.h"
#include "world/worldobject.h"
#include "world/team.h"
#include "world/unit_graphics.h"

#include "renderer/world_renderer.h"
#include "renderer/animated_icon.h"

WorldRenderer::WorldRenderer()
:   m_renderEverything(false),
    m_showWhiteBoard(false),
    m_editWhiteBoard(false),
    m_showPlanning(false),
    m_showAllWhiteBoards(true),
    m_drawingPlanning(false),
    m_erasingPlanning(false),
    m_drawingPlanningTime(0.0f),
    m_territories{},
    m_bmpSailableWater(NULL),
    m_bmpTravelNodes(NULL),
    m_currentSelectionId(-1),
    m_currentHighlightId(-1),
    m_previousSelectionId(-1),
	m_showRadar(false),
	m_radarLocked(false),
	m_showPopulation(false),
	m_showRadiation(false),
	m_showOrders(false),
	m_showNukeUnits(false)
{
}

WorldRenderer::~WorldRenderer()
{
    for( int i = 0; i < m_animations.Size(); ++i )
    {
        delete m_animations[i];
    }
    m_animations.Empty();
}

void WorldRenderer::Init()
{
    m_territories[World::TerritoryUSA]          = g_resource->GetImage( "earth/usa.bmp" );
    m_territories[World::TerritoryNATO]         = g_resource->GetImage( "earth/nato.bmp" );
    m_territories[World::TerritoryRussia]       = g_resource->GetImage( "earth/russia.bmp" );
    m_territories[World::TerritoryChina]        = g_resource->GetImage( "earth/china.bmp" );
    m_territories[World::TerritoryIndia]        = g_resource->GetImage( "earth/india.bmp" );
    m_territories[World::TerritoryPakistan]     = g_resource->GetImage( "earth/pakistan.bmp" );
    m_territories[World::TerritoryJapan]        = g_resource->GetImage( "earth/japan.bmp" );
    m_territories[World::TerritoryKorea]        = g_resource->GetImage( "earth/korea.bmp" );
    m_territories[World::TerritoryAustralia]    = g_resource->GetImage( "earth/australia.bmp" );
    m_territories[World::TerritoryTaiwan]       = g_resource->GetImage( "earth/taiwan.bmp" );
    m_territories[World::TerritoryUkraine]      = g_resource->GetImage( "earth/ukraine.bmp" );
    m_territories[World::TerritoryPhilippines]  = g_resource->GetImage( "earth/philippines.bmp" );
    m_territories[World::TerritoryThailand]     = g_resource->GetImage( "earth/thailand.bmp" );
    m_territories[World::TerritoryIndonesia]    = g_resource->GetImage( "earth/indonesia.bmp" );
    m_territories[World::TerritoryNorthKorea]   = g_resource->GetImage( "earth/northkorea.bmp" );
    m_territories[World::TerritoryIran]         = g_resource->GetImage( "earth/iran.bmp" );
    m_territories[World::TerritoryIsrael]       = g_resource->GetImage( "earth/israel.bmp" );
    m_territories[World::TerritorySaudi]        = g_resource->GetImage( "earth/saudi.bmp" );
    m_territories[World::TerritoryEgypt]        = g_resource->GetImage( "earth/egypt.bmp" );
    m_territories[World::TerritoryVietnam]      = g_resource->GetImage( "earth/vietnam.bmp" );
    m_territories[World::TerritoryBrazil]       = g_resource->GetImage( "earth/brazil.bmp" );
    m_territories[World::TerritorySouthAfrica]  = g_resource->GetImage( "earth/southafrica.bmp" );
    m_territories[World::TerritoryNeutralAmerica] = g_resource->GetImage( "earth/neutralamerica.bmp" );
    m_territories[World::TerritoryNeutralAfrica]  = g_resource->GetImage( "earth/neutralafrica.bmp" );
    m_territories[World::TerritoryNeutralEurope]  = g_resource->GetImage( "earth/neutraleurope.bmp" );
    m_territories[World::TerritoryNeutralAsia]    = g_resource->GetImage( "earth/neutralasia.bmp" );    

    m_bmpSailableWater = g_resource->GetImage( "earth/sailable.bmp" );
    m_bmpTravelNodes = g_resource->GetImage( "earth/travel_nodes.bmp" );

    sprintf(m_imageFiles[WorldObject::TypeSilo], "graphics/silo.bmp");
    sprintf(m_imageFiles[WorldObject::TypeSAM], "graphics/sam.bmp");
    sprintf(m_imageFiles[WorldObject::TypeRadarStation], "graphics/radarstation.bmp");
    sprintf(m_imageFiles[WorldObject::TypeSub], "graphics/subn.bmp");
    sprintf(m_imageFiles[WorldObject::TypeAirBase], "graphics/airbase.bmp");
    sprintf(m_imageFiles[WorldObject::TypeCarrier], "graphics/wasp.bmp");
    sprintf(m_imageFiles[WorldObject::TypeBattleShip], "graphics/battleship.bmp");
    sprintf(m_imageFiles[WorldObject::TypeFighter], "graphics/fighter.bmp");
    sprintf(m_imageFiles[WorldObject::TypeFighterLight], "graphics/f16.bmp");
    sprintf(m_imageFiles[WorldObject::TypeFighterStealth], "graphics/f22.bmp");
    sprintf(m_imageFiles[WorldObject::TypeFighterNavyStealth], "graphics/f35c.bmp");
    sprintf(m_imageFiles[WorldObject::TypeBomber], "graphics/bomber.bmp");
    sprintf(m_imageFiles[WorldObject::TypeBomberFast], "graphics/b1b.bmp");
    sprintf(m_imageFiles[WorldObject::TypeBomberStealth], "graphics/stealthbomber.bmp");
    sprintf(m_imageFiles[WorldObject::TypeAEW], "graphics/aew.bmp");
    sprintf(m_imageFiles[WorldObject::TypeTanker], "graphics/kc10.bmp");
    sprintf(m_imageFiles[WorldObject::TypeNuke], "graphics/nuke.bmp");
    sprintf(m_imageFiles[WorldObject::TypeLACM], "graphics/lacm.bmp");
    sprintf(m_imageFiles[WorldObject::TypeCBM], "graphics/nuke.bmp");
    sprintf(m_imageFiles[WorldObject::TypeLANM], "graphics/lacm.bmp");
    sprintf(m_imageFiles[WorldObject::TypeABM], "graphics/abm.bmp");
    sprintf(m_imageFiles[WorldObject::TypeSiloMed], "graphics/silo.bmp");
    sprintf(m_imageFiles[WorldObject::TypeSiloMobile], "graphics/silomob.bmp");
    sprintf(m_imageFiles[WorldObject::TypeSiloMobileCon], "graphics/silomob.bmp");
    sprintf(m_imageFiles[WorldObject::TypeASCM], "graphics/ascmbattery.bmp");
    sprintf(m_imageFiles[WorldObject::TypeRadarEW], "graphics/pavepaws.bmp");
    sprintf(m_imageFiles[WorldObject::TypeSubG], "graphics/subg.bmp");
    sprintf(m_imageFiles[WorldObject::TypeSubC], "graphics/subc.bmp");
    sprintf(m_imageFiles[WorldObject::TypeSubK], "graphics/subk.bmp");
    sprintf(m_imageFiles[WorldObject::TypeBattleShip2], "graphics/battleship.bmp");
    sprintf(m_imageFiles[WorldObject::TypeBattleShip3], "graphics/lcs.bmp");
    sprintf(m_imageFiles[WorldObject::TypeAirBase2], "graphics/airbase.bmp");
    sprintf(m_imageFiles[WorldObject::TypeAirBase3], "graphics/airbase.bmp");
    sprintf(m_imageFiles[WorldObject::TypeCarrierLight], "graphics/wasp.bmp");
    sprintf(m_imageFiles[WorldObject::TypeCarrierLHD], "graphics/type075.bmp");
    sprintf(m_imageFiles[WorldObject::TypeCarrierSuper], "graphics/carrier.bmp");
}

void WorldRenderer::Reset()
{
    m_animations.EmptyAndDelete();
    m_selectedIds.Empty();
}

void WorldRenderer::SetRenderEverything( bool _renderEverything )
{
    m_renderEverything = _renderEverything;
}

void WorldRenderer::LockRadarRenderer()
{
    m_radarLocked = true;
    m_showRadar = false;
}

void WorldRenderer::UnlockRadarRenderer()
{
    m_radarLocked = false;
}

int WorldRenderer::CreateAnimation( int animationType, int _fromObjectId, float longitude, float latitude )
{
    AnimatedIcon *anim = NULL;
    switch( animationType )
    {
        case AnimationTypeActionMarker  : anim = new ActionMarker();    break;
        case AnimationTypeSonarPing     : anim = new SonarPing();       break; 
        case AnimationTypeNukePointer   : anim = new NukePointer();     break;

        default: return -1;
    }

    anim->m_longitude = longitude;
    anim->m_latitude = latitude;
    anim->m_fromObjectId = _fromObjectId;
    anim->m_animationType = animationType;
    
    int index = m_animations.PutData( anim );
    return index;
}

bool WorldRenderer::GetShowWhiteBoard() const
{
	return m_showWhiteBoard;
}

void WorldRenderer::SetShowWhiteBoard( bool showWhiteBoard )
{
	m_showWhiteBoard = showWhiteBoard;
}

bool WorldRenderer::GetEditWhiteBoard() const
{
	return m_editWhiteBoard;
}

void WorldRenderer::SetEditWhiteBoard( bool editWhiteBoard )
{
	if ( GetEditWhiteBoard() != editWhiteBoard )
	{
		m_editWhiteBoard = editWhiteBoard;
		if ( GetEditWhiteBoard() )
		{
			if ( m_showPlanning )
			{
				g_app->GetInterface()->SetMouseCursor( "gui/pen.bmp" );
			}
		}
		else
		{
			g_app->GetInterface()->SetMouseCursor();
		}
		SetDrawingPlanning( false );
		SetErasingPlanning( false );
	}
}

bool WorldRenderer::GetShowPlanning() const
{
	return m_showPlanning;
}

void WorldRenderer::SetShowPlanning( bool showPlanning )
{
	if ( GetShowPlanning() != showPlanning )
	{
		m_showPlanning = showPlanning;
		if ( m_showPlanning )
		{
			g_app->GetInterface()->SetMouseCursor( "gui/pen.bmp" );
		}
		else
		{
			g_app->GetInterface()->SetMouseCursor();
		}
		SetDrawingPlanning( false );
		SetErasingPlanning( false );
	}
}


void WorldRenderer::SetDrawingPlanning( bool drawingPlanning )
{
	m_drawingPlanning = drawingPlanning;
}

void WorldRenderer::SetErasingPlanning( bool erasingPlanning )
{
	m_erasingPlanning = erasingPlanning;
}

void WorldRenderer::SetDrawingPlanningTime( double drawingPlanningTime )
{
	m_drawingPlanningTime = drawingPlanningTime;
}

bool WorldRenderer::GetShowAllWhiteBoards() const
{
	return m_showAllWhiteBoards;
}

void WorldRenderer::SetShowAllWhiteBoards( bool showAllWhiteBoards )
{
	m_showAllWhiteBoards = showAllWhiteBoards;
}

Team* WorldRenderer::GetEffectiveWhiteBoardTeam()
{
	if( g_app->GetServer() && g_app->GetServer()->IsRecordingPlaybackMode() )
	{
		extern int g_desiredPerspectiveTeamId;
		
		if( g_desiredPerspectiveTeamId != -1 )
		{
			Team *perspectiveTeam = g_app->GetWorld()->GetTeam(g_desiredPerspectiveTeamId);
			if( perspectiveTeam )
			{
				return perspectiveTeam;
			}
		}
	}

	return g_app->GetWorld()->GetMyTeam();
}

bool WorldRenderer::IsValidTerritory( int teamId, Fixed longitude, Fixed latitude, bool seaUnit )
{    
    if( latitude > 100 || latitude < -100 )
    {
        return false;
    }

    if( longitude < -180 )
    {
        longitude = -180;
    }
    else if( longitude > 180 )
    {
        longitude = 179;
    }    
    if( teamId == -1 )
    {
        int pixelX = ( m_bmpSailableWater->Width() * (longitude+180)/360 ).IntValue();
        int pixelY = ( m_bmpSailableWater->Height() * (latitude+100)/200 ).IntValue();
        Colour theCol = m_bmpSailableWater->GetColour( pixelX, pixelY );

        return ( theCol.m_r > 20 && 
                 theCol.m_g > 20 &&
                 theCol.m_b > 20 );
    }
    else
    {
        bool isValid = false;
        Team *team = g_app->GetWorld()->GetTeam(teamId);
        for( int i = 0; i < team->m_territories.Size(); ++i )
        {
            Image *img = m_territories[team->m_territories[i]];
            if( !img ) continue;

            int pixelX = ( img->Width() * (longitude+180)/360 ).IntValue();
            int pixelY = ( img->Height() * (latitude+100)/200 ).IntValue();

            Colour theCol = img->GetColour( pixelX, pixelY );
            Colour sailableCol = m_bmpSailableWater->GetColour( pixelX, pixelY );
            int landthreshold = 130;
            int waterthreshold = 60;
            if( !seaUnit )
            {
                if ( theCol.m_r > landthreshold && 
                     theCol.m_g > landthreshold &&
                     theCol.m_b > landthreshold &&
                     sailableCol.m_r <= waterthreshold && 
                     sailableCol.m_g <= waterthreshold &&
                     sailableCol.m_b <= waterthreshold )
                {
                    isValid = true;
                }
            }
            else
            {
                if ( theCol.m_r > waterthreshold && 
                     theCol.m_g > waterthreshold &&
                     theCol.m_b > waterthreshold &&
                     sailableCol.m_r > waterthreshold && 
                     sailableCol.m_g > waterthreshold &&
                     sailableCol.m_b > waterthreshold )
                {
                    isValid = true;
                }

            }
        }
        return isValid;
    }
    return false;
}

bool WorldRenderer::IsCoastline( Fixed longitude, Fixed latitude )
{
    Image *img = g_resource->GetImage( "earth/coastlines.bmp" );
    int pixelX = ( img->Width() * (longitude+180)/360 ).IntValue();
    int pixelY = ( img->Height() * (latitude+100)/200 ).IntValue();

    Colour theCol = img->GetColour( pixelX, pixelY );

    return ( theCol.m_r > 20 &&
             theCol.m_g > 20 &&
             theCol.m_b > 20 );
}

int WorldRenderer::GetTerritory( Fixed longitude, Fixed latitude, bool seaArea )
{
    for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
    {
        Team *team = g_app->GetWorld()->m_teams[i];
        if( IsValidTerritory( team->m_teamId, longitude, latitude, seaArea ) )
        {
            return team->m_teamId;
        }
    }
    return -1;
}

int WorldRenderer::GetTerritoryId( Fixed longitude, Fixed latitude )
{
    for( int i = 0; i < World::NumTerritories; ++i )
    {
        Image *img = m_territories[i];
        if( !img ) continue;

        int pixelX = ( img->Width() * (longitude+180)/360 ).IntValue();
        int pixelY = ( img->Height() * (latitude+100)/200 ).IntValue();

        Colour theCol = img->GetColour( pixelX, pixelY );
        if ( theCol.m_r > 20 && 
             theCol.m_g > 20 &&
             theCol.m_b > 20 )
        {
            return i;
        }
    }
    return -1;
}

int WorldRenderer::GetTerritoryIdUnique( Fixed longitude, Fixed latitude )
{
    int prevTerritory = -1;
    for( int i = 0; i < World::NumTerritories; ++i )
    {
        Image *img = m_territories[i];
        if( !img ) continue;

        int pixelX = ( img->Width() * (longitude+180)/360 ).IntValue();
        int pixelY = ( img->Height() * (latitude+100)/200 ).IntValue();

        Colour theCol = img->GetColour( pixelX, pixelY );
        if ( theCol.m_r > 20 && 
             theCol.m_g > 20 &&
             theCol.m_b > 20 )
        {
            if( prevTerritory != -1 )
            {
                return -1;
            }
            prevTerritory = i;
        }
    }
    return prevTerritory;
}

Image *WorldRenderer::GetTerritoryImage( int territoryId )
{
    if( territoryId < 0 || territoryId >= World::NumTerritories ) return NULL;
    return m_territories[territoryId];
}

int WorldRenderer::GetCurrentSelectionId()
{
    return m_currentSelectionId; 
}

void WorldRenderer::ClearSelection()
{
    m_selectedIds.Empty();
    m_currentSelectionId = -1;
}

void WorldRenderer::AddToSelection( int id )
{
    if( id == -1 ) return;
    for( int i = 0; i < m_selectedIds.Size(); ++i )
    {
        if( m_selectedIds[i] == id ) return;
    }
    m_selectedIds.PutData( id );
    m_currentSelectionId = id;
}

void WorldRenderer::RemoveFromSelection( int id )
{
    for( int i = 0; i < m_selectedIds.Size(); ++i )
    {
        if( m_selectedIds[i] == id )
        {
            m_selectedIds.RemoveData( i );
            if( m_currentSelectionId == id )
            {
                m_currentSelectionId = ( m_selectedIds.Size() > 0 ? m_selectedIds[m_selectedIds.Size()-1] : -1 );
            }
            return;
        }
    }
}

bool WorldRenderer::IsSelected( int id ) const
{
    for( int i = 0; i < m_selectedIds.Size(); ++i )
    {
        if( m_selectedIds.GetData(i) == id ) return true;
    }
    return false;
}

int WorldRenderer::GetSelectionCount() const
{
    return m_selectedIds.Size();
}

int WorldRenderer::GetSelectedId( int index ) const
{
    return m_selectedIds.ValidIndex( index ) ? m_selectedIds.GetData( index ) : -1;
}

void WorldRenderer::SetPrimarySelectionId( int id )
{
    m_currentSelectionId = id;
}

void WorldRenderer::SetCurrentSelectionId( int id )
{
    if( m_currentSelectionId != -1 && id == -1 )
    {
#ifdef TOGGLE_SOUND
        g_soundSystem->TriggerEvent( "Interface", "DeselectObject" );
#endif
    }
    else if( id != -1 )
    {
#ifdef TOGGLE_SOUND
        g_soundSystem->TriggerEvent( "Interface", "SelectObject" );
#endif
    }

    if( m_currentSelectionId != -1 )
    {
        m_previousSelectionId = m_currentSelectionId;
    }
    m_currentSelectionId = id;

    m_selectedIds.Empty();
    if( id != -1 )
    {
        m_selectedIds.PutData( id );
    }
}

int WorldRenderer::GetCurrentHighlightId()
{
    return m_currentHighlightId;
}

void WorldRenderer::SetCurrentHighlightId( int id )
{
    m_currentHighlightId = id;
}

Image *WorldRenderer::GetTravelNodesImage()
{
    return m_bmpTravelNodes;
}

const char *WorldRenderer::GetImageFile( int objectType, int teamId )
{
    const char *defaultPath = m_imageFiles[objectType];
    if ( teamId < 0 ) return defaultPath;
    Team *team = g_app->GetWorld()->GetTeam( teamId );
    if ( !team || team->m_territories.Size() == 0 ) return defaultPath;
    int primaryTerritory = team->m_territories[0];
    return GetUnitGraphicForTerritory( primaryTerritory, objectType, defaultPath );
}