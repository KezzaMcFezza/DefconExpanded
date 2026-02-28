#include "lib/universal_include.h"
#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/render2d/renderer_2d.h"
#include "lib/math/vector3.h"
#include "lib/math/random_number.h"
#include "lib/language_table.h"

#include "app/app.h"
#include "app/globals.h"

#include "renderer/world_renderer.h"

#include "interface/interface.h"

#include "world/world.h"
#include "world/aew.h"
#include "world/team.h"


AEW::AEW()
:   MovingObject()
{
    SetType( TypeAEW );

    strcpy( bmpImageFilename, "graphics/aew.bmp" );

    m_stealthType = 100;
    m_radarRange = 15;
    m_speed = Fixed::Hundredths(4);
    m_turnRate = Fixed::Hundredths(2);
    m_selectable = true;
    m_maxHistorySize = 10;
    m_range = 140;

    m_movementType = MovementTypeAir;

    AddState( LANGUAGEPHRASE("state_radar"), 0, 0, 15, 0, false );

    InitialiseTimers();
}


void AEW::Action( int targetObjectId, Fixed longitude, Fixed latitude )
{
    m_targetObjectId = -1;
    m_isLanding = -1;
    m_isEscorting = -1;

    WorldObject *target = g_app->GetWorld()->GetWorldObject( targetObjectId );
    if( target &&
        g_app->GetWorld()->IsFriend( m_teamId, target->m_teamId ) &&
        ( target->IsAircraftLauncher() || target->m_type == TypeTanker ) )
    {
        Land( targetObjectId );
    }
    else if( target &&
             g_app->GetWorld()->IsFriend( m_teamId, target->m_teamId ) &&
             target->IsAircraft() )
    {
        SetWaypoint( target->m_longitude, target->m_latitude );
        m_isEscorting = targetObjectId;
    }

    if( m_teamId == g_app->GetWorld()->m_myTeamId && targetObjectId == -1 )
    {
        g_app->GetWorldRenderer()->CreateAnimation( WorldRenderer::AnimationTypeActionMarker, m_objectId,
                                                    longitude.DoubleValue(), latitude.DoubleValue() );
    }

    MovingObject::Action( targetObjectId, longitude, latitude );
}

bool AEW::Update()
{
    MoveToWaypoint();

    Fixed timePerUpdate = SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor();

    if( m_targetLongitude == 0 && m_targetLatitude == 0 )
    {
        if( m_range <= 0 )
        {
            m_life = 0;
            m_lastHitByTeamId = m_teamId;
            g_app->GetWorld()->AddOutOfFueldMessage( m_objectId );
        }
        else
        {
            m_longitude += m_vel.x * Fixed(timePerUpdate);
            m_latitude += m_vel.y * Fixed(timePerUpdate);
            if( m_longitude <= -180 || m_longitude >= 180 )
            {
                CrossSeam();
            }
            if( m_range <= 0 )
            {
                m_life = 0;
                m_lastHitByTeamId = m_teamId;
                g_app->GetWorld()->AddOutOfFueldMessage( m_objectId );
            }
            if( IsIdle() )
            {
                Land( GetClosestLandingPad() );
            }
        }
    }

    return MovingObject::Update();
}

void AEW::RunAI()
{
    if( IsIdle() )
    {
        Land( GetClosestLandingPad() );
    }
}

void AEW::Land( int targetId )
{
    WorldObject *target = g_app->GetWorld()->GetWorldObject( targetId );
    if( target )
    {
        SetWaypoint( target->m_longitude, target->m_latitude );
        m_isLanding = targetId;
    }
}

int AEW::GetAttackState()
{
    return -1;
}

int AEW::IsValidCombatTarget( int _objectId )
{
    WorldObject *obj = g_app->GetWorld()->GetWorldObject( _objectId );
    if( !obj ) return TargetTypeInvalid;

    if( obj->IsAircraftLauncher() || obj->m_type == TypeTanker )
    {
        if( obj->m_teamId == m_teamId ||
            g_app->GetWorld()->GetTeam(m_teamId)->m_ceaseFire[obj->m_teamId] )
        {
            return TargetTypeLand;
        }
    }

    if( obj->IsAircraft() &&
        ( obj->m_teamId == m_teamId ||
          g_app->GetWorld()->GetTeam(m_teamId)->m_ceaseFire[obj->m_teamId] ) )
    {
        return TargetTypePursue;
    }

    return MovingObject::IsValidCombatTarget( _objectId );
}
