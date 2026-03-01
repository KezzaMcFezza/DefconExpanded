#include "lib/universal_include.h"

#include "lib/resource/resource.h"
#include "lib/language_table.h"

#include "app/app.h"
#include "app/globals.h"
#include "app/game.h"
#include "lib/debug/profiler.h"

#include "world/world.h"
#include "world/silo.h"
#include "world/silomed.h"
#include "world/ascm.h"
#include "world/team.h"
#include "world/lacm.h"
#include "lib/render3d/renderer_3d.h"
#include "renderer/world_renderer.h"
#include "renderer/globe_renderer.h"


ASCM::ASCM()
:   SiloMed(),
    m_ammoRegenTimer(0)
{
    SetType( TypeASCM );
    strcpy( bmpImageFilename, "graphics/ascmbattery.bmp" );

    m_life = 3;
    m_stealthType = 50;

    // Replace parent states with single launch-only state. No standby.
    m_states.EmptyAndDelete();
    AddState( LANGUAGEPHRASE("state_ascmlaunch"), 60, 3, 2, 25, true, 50, 3 );

    m_currentState = 0;
    m_nukeSupply = -1;   // LACM, not nukes - do not use m_nukeSupply
}


void ASCM::Action( int targetObjectId, Fixed longitude, Fixed latitude )
{
    if( !CheckCurrentState() )
        return;

    if( m_stateTimer <= 0 )
    {
        if( m_states[0]->m_numTimesPermitted > 0 )
        {
            g_app->GetWorld()->LaunchCruiseMissile( m_teamId, m_objectId, longitude, latitude, GetNukeLaunchRange(), targetObjectId );
            for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
            {
                Team *team = g_app->GetWorld()->m_teams[i];
                m_visible[team->m_teamId] = true;
                m_lastSeenTime[team->m_teamId] = m_ghostFadeTime;
                m_seen[team->m_teamId] = true;
            }
        }
        WorldObject::Action( targetObjectId, longitude, latitude );
    }
}


bool ASCM::Update()
{
    // Regenerate ammo: 1 per 15 minutes
    Fixed timePerUpdate = SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor();
    m_ammoRegenTimer += timePerUpdate;
    const Fixed regenInterval = Fixed( 900 );  // 15 minutes in game seconds
    if( m_ammoRegenTimer >= regenInterval )
    {
        m_ammoRegenTimer -= regenInterval;
        if( m_states[0]->m_numTimesPermitted < 60 )
            m_states[0]->m_numTimesPermitted++;
    }

    // Skip Silo::Update (would access m_states[1] and return-to-standby logic)
    return WorldObject::Update();
}


void ASCM::RunAI()
{
    START_PROFILE("ASCMAI");
    Team *team = g_app->GetWorld()->GetTeam( m_teamId );
    if( !team ) { END_PROFILE("ASCMAI"); return; }

    if( g_app->GetWorld()->GetDefcon() > 3 )
    {
        END_PROFILE("ASCMAI");
        return;
    }

    bool haveAmmo = ( m_states[0]->m_numTimesPermitted - (int)m_actionQueue.Size() > 0 );
    if( !haveAmmo || m_stateTimer > 0 )
    {
        END_PROFILE("ASCMAI");
        return;
    }

    // Auto-find nearest non-ceasefire ship in range
    Fixed maxRange = GetNukeLaunchRange();
    Fixed maxRangeSqd = maxRange * maxRange;
    WorldObject *nearestShip = nullptr;
    Fixed nearestSqd = Fixed::MAX;

    for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
    {
        if( !g_app->GetWorld()->m_objects.ValidIndex(i) ) continue;
        WorldObject *obj = g_app->GetWorld()->m_objects[i];
        if( g_app->GetWorld()->IsFriend( obj->m_teamId, m_teamId ) ) continue;
        if( team->m_ceaseFire[obj->m_teamId] ) continue;
        if( !obj->m_seen[m_teamId] ) continue;
        bool isShip = obj->IsTargetableSurfaceNavy();
        if( !isShip ) continue;
        Fixed dSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, obj->m_longitude, obj->m_latitude );
        if( dSqd >= maxRangeSqd ) continue;
        if( dSqd < nearestSqd )
        {
            nearestSqd = dSqd;
            nearestShip = obj;
        }
    }

    if( nearestShip )
    {
        ActionOrder *action = new ActionOrder();
        action->m_longitude = nearestShip->m_longitude;
        action->m_latitude = nearestShip->m_latitude;
        action->m_targetObjectId = nearestShip->m_objectId;
        RequestAction( action );
    }
    END_PROFILE("ASCMAI");
}


void ASCM::SetState( int state )
{
    (void)state;
    // ASCM has no standby; ignore state changes (always in launch mode)
}


int ASCM::GetAttackOdds( int _defenderType )
{
    if( m_states[0]->m_numTimesPermitted > 0 )
        return g_app->GetWorld()->GetAttackOdds( TypeLACM, _defenderType );
    return 0;
}


int ASCM::IsValidCombatTarget( int _objectId )
{
    WorldObject *obj = g_app->GetWorld()->GetWorldObject( _objectId );
    if( !obj ) return TargetTypeInvalid;

    int basicResult = WorldObject::IsValidCombatTarget( _objectId );
    if( basicResult < TargetTypeInvalid ) return basicResult;

    bool isShip = obj->IsTargetableSurfaceNavy();
    if( !isShip ) return TargetTypeInvalid;

    int attackOdds = g_app->GetWorld()->GetAttackOdds( TypeLACM, obj->m_type );
    if( m_states[0]->m_numTimesPermitted > 0 && attackOdds > 0 )
        return TargetTypeLaunchLACM;
    return TargetTypeInvalid;
}


int ASCM::IsValidMovementTarget( Fixed longitude, Fixed latitude )
{
    (void)longitude;
    (void)latitude;
    // ASCM can only target ships, not arbitrary map positions
    return TargetTypeInvalid;
}


void ASCM::Render2D()
{
    WorldObject::Render2D();
    // Ammunition shown in mouse-over tooltip instead of on-sprite LACM icons
}


void ASCM::Render3D()
{
    WorldObject::Render3D();
    // Ammunition shown in mouse-over tooltip instead of on-sprite LACM icons
}


Image *ASCM::GetBmpImage( int state )
{
    (void)state;
    return g_resource->GetImage( "graphics/ascmbattery.bmp" );
}
