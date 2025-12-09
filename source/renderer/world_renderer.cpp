#include "lib/universal_include.h"

#include <math.h>

#include "app/app.h"
#include "app/globals.h"

#include "interface/interface.h"

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
	m_showRadar(false),
	m_radarLocked(false),
	m_showPopulation(false),
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
    
}

void WorldRenderer::Reset()
{
    m_animations.EmptyAndDelete();
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
#if RECORDING_PARSING
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
#endif

	return g_app->GetWorld()->GetMyTeam();
}