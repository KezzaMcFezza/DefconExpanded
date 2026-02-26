#include "lib/universal_include.h"

#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/render/renderer.h"
#include "lib/render2d/renderer_2d.h"
#include "lib/render3d/renderer_3d.h"
#include "lib/math/vector3.h"
#include "lib/math/math_utils.h"
#include "lib/language_table.h"
 
#include "app/app.h"
#include "app/globals.h"
#include "app/game.h"

#include "renderer/world_renderer.h"
#include "renderer/map_renderer.h"
#include "renderer/globe_renderer.h"

#include "interface/interface.h"

#include "world/world.h"
#include "world/bomber.h"
#include "world/team.h"


Bomber::Bomber()
:   MovingObject(),
    m_bombingRun(false),
    m_lacmLoadout(false)
{
    SetType( TypeBomber );

    m_stealthType = 130;  // early1 - visible at longer range
    strcpy( bmpImageFilename, "graphics/bomber.bmp" );

    m_radarRange = 5;
    m_speed = Fixed::Hundredths(5);
    m_turnRate = Fixed::Hundredths(1);
    m_selectable = true;
    m_maxHistorySize = 10;
    m_range = 140;

    m_nukeSupply = 1;

    m_movementType = MovementTypeAir;

    AddState( LANGUAGEPHRASE("state_standby"), 0, 0, 5, 25, true, 2, 3 );
    AddState( LANGUAGEPHRASE("state_bombernuke"), 240, 120, 5, 25, true, 2, 1 );
    AddState( LANGUAGEPHRASE("state_bomberlacm"), 240, 120, 5, 25, true, 6, 3 );

    m_states[0]->m_numTimesPermitted = m_states[1]->m_numTimesPermitted;
    m_states[2]->m_numTimesPermitted = 0;

    InitialiseTimers();
}


void Bomber::Action( int targetObjectId, Fixed longitude, Fixed latitude )
{
    if( !CheckCurrentState() )
    {
        return;
    }
    if( m_stateTimer > 0 )
    {
        if( m_currentState == 1 && m_states[1]->m_numTimesPermitted > 0 &&
            ( targetObjectId != -1 || ( longitude != 0 || latitude != 0 ) ) )
        {
            ActionOrder *retry = new ActionOrder();
            retry->m_targetObjectId = targetObjectId;
            retry->m_longitude = longitude;
            retry->m_latitude = latitude;
            m_actionQueue.PutDataAtStart( retry );
        }
        if( m_currentState == 2 && m_states[2]->m_numTimesPermitted > 0 &&
            targetObjectId != -1 )
        {
            ActionOrder *retry = new ActionOrder();
            retry->m_targetObjectId = targetObjectId;
            retry->m_longitude = longitude;
            retry->m_latitude = latitude;
            m_actionQueue.PutDataAtStart( retry );
        }
        return;
    }

    if( m_currentState == 2 )
    {
        if( m_states[2]->m_numTimesPermitted <= 0 )
            return;
        if( targetObjectId == -1 )
            return;
        WorldObject *targetObj = g_app->GetWorld()->GetWorldObject( targetObjectId );
        if( !targetObj || targetObj->m_life <= 0 )
            return;

        Fixed nukeRange = GetActionRange();
        Fixed distSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, longitude, latitude );
        bool inRange = ( distSqd <= nukeRange * nukeRange );

        if( inRange )
        {
            MovingObject::Action( targetObjectId, longitude, latitude );
            g_app->GetWorld()->LaunchCruiseMissile( m_teamId, m_objectId, longitude, latitude, nukeRange, targetObjectId );
            m_bombingRun = true;
            m_states[0]->m_numTimesPermitted = m_states[2]->m_numTimesPermitted;

            int remainingAfterQueue = m_states[2]->m_numTimesPermitted - m_actionQueue.Size();
            if( remainingAfterQueue > 0 && m_actionQueue.Size() == 0 )
            {
                ActionOrder *repeat = new ActionOrder();
                repeat->m_targetObjectId = targetObjectId;
                repeat->m_longitude = longitude;
                repeat->m_latitude = latitude;
                RequestAction( repeat );
            }
            else if( m_actionQueue.Size() > 0 )
            {
                ActionOrder *next = m_actionQueue[0];
                SetWaypoint( next->m_longitude, next->m_latitude );
            }
            return;
        }
        else
        {
            ActionOrder *retry = new ActionOrder();
            retry->m_targetObjectId = targetObjectId;
            retry->m_longitude = longitude;
            retry->m_latitude = latitude;
            m_actionQueue.PutDataAtStart( retry );
            SetWaypoint( longitude, latitude );
            m_bombingRun = true;
            return;
        }
    }

    if( m_currentState == 1 )
    {
        if( m_states[1]->m_numTimesPermitted <= 0 )
            return;

        int deadTargetCheck = targetObjectId;
        if( deadTargetCheck != -1 )
        {
            WorldObject *targetObj = g_app->GetWorld()->GetWorldObject( deadTargetCheck );
            if( !targetObj || targetObj->m_life <= 0 )
                return;
        }

        Fixed nukeRange = GetActionRange();
        Fixed distSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, longitude, latitude );
        bool inRange = ( distSqd <= nukeRange * nukeRange );

        if( inRange )
        {
            if( targetObjectId == -1 && m_actionQueue.Size() > 0 &&
                m_actionQueue[0]->m_targetObjectId == -1 &&
                m_actionQueue[0]->m_longitude == longitude &&
                m_actionQueue[0]->m_latitude == latitude )
            {
                delete m_actionQueue[0];
                m_actionQueue.RemoveData( 0 );
            }
            MovingObject::Action( targetObjectId, longitude, latitude );
            g_app->GetWorld()->LaunchLANM( m_teamId, m_objectId, longitude, latitude, nukeRange, targetObjectId );
            m_bombingRun = true;
            m_states[0]->m_numTimesPermitted = m_states[1]->m_numTimesPermitted;

            int remainingAfterQueue = m_states[1]->m_numTimesPermitted - m_actionQueue.Size();
            if( remainingAfterQueue > 0 && m_actionQueue.Size() == 0 )
            {
                ActionOrder *repeat = new ActionOrder();
                repeat->m_targetObjectId = targetObjectId;
                repeat->m_longitude = longitude;
                repeat->m_latitude = latitude;
                RequestAction( repeat );
            }
            else if( m_actionQueue.Size() > 0 )
            {
                ActionOrder *next = m_actionQueue[0];
                SetWaypoint( next->m_longitude, next->m_latitude );
            }
            return;
        }
        else
        {
            if( targetObjectId == -1 )
            {
                ActionOrder *retry = new ActionOrder();
                retry->m_targetObjectId = -1;
                retry->m_longitude = longitude;
                retry->m_latitude = latitude;
                m_actionQueue.PutDataAtStart( retry );
                SetWaypoint( longitude, latitude );
                m_bombingRun = true;
                return;
            }
            ActionOrder *retry = new ActionOrder();
            retry->m_targetObjectId = targetObjectId;
            retry->m_longitude = longitude;
            retry->m_latitude = latitude;
            m_actionQueue.PutDataAtStart( retry );
            SetWaypoint( longitude, latitude );
            m_bombingRun = true;
            return;
        }
    }
    else
    {
        MovingObject::Action( targetObjectId, longitude, latitude );
    }
}

bool Bomber::Update()
{
    Fixed gameScale = World::GetUnitScaleFactor();

    m_speed = Fixed::Hundredths(5) / gameScale;         
    m_turnRate = Fixed::Hundredths(1) / gameScale;

    // Switch to attack mode when defcon allows (original Defcon behaviour: bomber launched at target, attacks when defcon drops)
    // Only when we have m_targetObjectId (launched from airbase with target) and no standby queue - do not switch if user queued orders
    if( m_currentState == 0 && m_actionQueue.Size() == 0 && m_targetObjectId != -1 )
    {
        int attackState = m_lacmLoadout ? 2 : 1;
        if( m_states[attackState]->m_numTimesPermitted > 0 &&
            g_app->GetWorld()->GetDefcon() <= m_states[attackState]->m_defconPermitted &&
            CanSetState( attackState ) )
        {
            WorldObject *targetObj = g_app->GetWorld()->GetWorldObject( m_targetObjectId );
            if( targetObj && targetObj->m_life > 0 )
            {
                ActionOrder *order = new ActionOrder();
                order->m_targetObjectId = m_targetObjectId;
                order->m_longitude = targetObj->m_longitude;
                order->m_latitude = targetObj->m_latitude;
                RequestAction( order );
            }
            SetState( attackState );
        }
    }

    bool arrived = MoveToWaypoint();

    if( arrived && m_actionQueue.Size() == 0 )
    {
        Land( GetClosestLandingPad() );
    }
    else if( arrived && ( ( m_currentState == 1 && m_states[1]->m_numTimesPermitted > 0 ) ||
                          ( m_currentState == 2 && m_states[2]->m_numTimesPermitted > 0 ) ) )
    {
        // If we arrived at waypoint but have a queued attack or pending nuke, keep waypoint set so we orbit until reload allows firing
        Fixed orbitLon = 0, orbitLat = 0;
        if( m_actionQueue.Size() > 0 )
        {
            ActionOrder *next = m_actionQueue[0];
            orbitLon = next->m_longitude;
            orbitLat = next->m_latitude;
            if( next->m_targetObjectId != -1 )
            {
                WorldObject *obj = g_app->GetWorld()->GetWorldObject( next->m_targetObjectId );
                if( obj )
                {
                    orbitLon = obj->m_longitude;
                    orbitLat = obj->m_latitude;
                }
            }
        }
        else if( GetNukeTargetLongitude() != 0 || GetNukeTargetLatitude() != 0 )
        {
            orbitLon = GetNukeTargetLongitude();
            orbitLat = GetNukeTargetLatitude();
        }
        if( orbitLon != 0 || orbitLat != 0 )
        {
            SetWaypoint( orbitLon, orbitLat );
        }
    }

    Vector3<Fixed> oldVel = m_vel;

    if( arrived )
    {
        m_vel = oldVel;
    }
    
    if( m_targetLongitude == 0 && m_targetLatitude == 0 )
    {
        Fixed timePerUpdate = SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor();
        m_longitude += m_vel.x * Fixed(timePerUpdate);
        m_latitude  += m_vel.y * Fixed(timePerUpdate);
        if( m_range <= 0 )
        {
            m_life = 0;
			m_lastHitByTeamId = m_teamId;
            g_app->GetWorld()->AddOutOfFueldMessage( m_objectId );
        }
    }
    
    if( m_currentState == 1 )
    {
        Fixed nukeLon = GetNukeTargetLongitude();
        Fixed nukeLat = GetNukeTargetLatitude();
        if( nukeLon != 0 || nukeLat != 0 )
        {
            if( m_stateTimer == 0 )
            {
                Fixed distanceSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, nukeLon, nukeLat );
                if( distanceSqd <= GetActionRangeSqd() )
                {
                    int targetId = -1;
                    if( m_actionQueue.Size() > 0 && m_actionQueue[0]->m_targetObjectId != -1 )
                        targetId = m_actionQueue[0]->m_targetObjectId;
                    Action( targetId, nukeLon, nukeLat );
                    m_bombingRun = true;
                }
            }
        }
    }
    else if( m_currentState == 2 && m_actionQueue.Size() > 0 && m_stateTimer == 0 )
    {
        ActionOrder *head = m_actionQueue[0];
        if( head->m_targetObjectId != -1 )
        {
            WorldObject *obj = g_app->GetWorld()->GetWorldObject( head->m_targetObjectId );
            if( obj )
            {
                Fixed distSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, obj->m_longitude, obj->m_latitude );
                if( distSqd <= GetActionRangeSqd() )
                {
                    Action( head->m_targetObjectId, obj->m_longitude, obj->m_latitude );
                    m_bombingRun = true;
                }
            }
        }
    }

    bool hasQueuedAttack = ( m_actionQueue.Size() > 0 );
    int ammoState = m_lacmLoadout ? 2 : 1;
    bool hasAmmoAndWaypoint = ( m_states[ammoState]->m_numTimesPermitted > 0 ) &&
        ( m_targetLongitude != 0 || m_targetLatitude != 0 );
    if( IsIdle() &&
        ( ( m_bombingRun && m_states[ammoState]->m_numTimesPermitted == 0 ) ||
          ( !hasQueuedAttack && !hasAmmoAndWaypoint ) ) )
    {
        Land( GetClosestLandingPad() );
    }

    if( ( m_currentState == 1 || m_currentState == 2 ) &&
        m_states[m_currentState]->m_numTimesPermitted == 0 )
    {
        m_states[0]->m_numTimesPermitted = 0;
        SetState(0);
        Land( GetClosestLandingPad() );
    }

    //
    // Maybe we're in a holding pattern

    /*if( holdingPattern  )
    {
        float timePerUpdate = SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor();

        m_vel.RotateAroundZ( 0.02f * timePerUpdate );
        m_longitude += m_vel.x * timePerUpdate;
        m_latitude += m_vel.y * timePerUpdate;

        if( m_longitude <= -180.0f ||
            m_longitude >= 180.0f )
        {
            CrossSeam();
        }

        if( m_range <= 0 )
        {
            m_life = 0;
            g_app->GetWorld()->AddOutOfFueldMessage( m_objectId );
        }

    }*/

    return MovingObject::Update();   
}

Fixed Bomber::GetActionRange()
{
    //if( m_currentState == 0 )
    //{
    //    return m_range;
    //}
    //else
    {
        return MovingObject::GetActionRange();
    }
}

void Bomber::RunAI()
{
    if( IsIdle() )
    {
        int attackState = m_lacmLoadout ? 2 : 1;
        if( m_states[attackState]->m_numTimesPermitted > 0 &&
            g_app->GetWorld()->GetDefcon() == 1 )
        {
            for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
            {
                if( g_app->GetWorld()->m_objects.ValidIndex(i) )
                {
                    WorldObject *obj = g_app->GetWorld()->m_objects[i];
                    if( obj && !obj->IsMovingObject() &&
                        !g_app->GetWorld()->IsFriend( m_teamId, obj->m_teamId ) &&                        
                        obj->m_visible[m_teamId] &&
                        g_app->GetWorld()->GetDistance( m_longitude, m_latitude, obj->m_longitude, obj->m_latitude ) < m_range - 15 )
                    {
                        if( m_lacmLoadout && WorldObject::GetArchetypeForType(obj->m_type) != WorldObject::ArchetypeBuilding )
                            continue;
                        if( m_currentState != attackState )
                            SetState( attackState );
                        ActionOrder *action = new ActionOrder();
                        action->m_targetObjectId = obj->m_objectId;
                        action->m_longitude = obj->m_longitude;
                        action->m_latitude = obj->m_latitude;
                        RequestAction( action );
                        return;
                    }
                }
            }
        }
        Land( GetClosestLandingPad() );
    }
}



void Bomber::Land( int targetId )
{
    WorldObject *target = g_app->GetWorld()->GetWorldObject(targetId);
    if( target )
    {
        SetWaypoint( target->m_longitude, target->m_latitude );
        m_isLanding = targetId;
    }
}

bool Bomber::UsingNukes()
{
    if( m_currentState == 1 && !m_lacmLoadout )
    {
        return true;
    }
    else
    {
        return false;
    }
}

Fixed Bomber::GetNukeTargetLongitude()
{
    if( m_actionQueue.Size() > 0 )
    {
        ActionOrder *head = m_actionQueue[0];
        if( head->m_targetObjectId == -1 )
            return head->m_longitude;
        WorldObject *obj = g_app->GetWorld()->GetWorldObject( head->m_targetObjectId );
        if( obj )
            return obj->m_longitude;
    }
    return 0;
}

Fixed Bomber::GetNukeTargetLatitude()
{
    if( m_actionQueue.Size() > 0 )
    {
        ActionOrder *head = m_actionQueue[0];
        if( head->m_targetObjectId == -1 )
            return head->m_latitude;
        WorldObject *obj = g_app->GetWorld()->GetWorldObject( head->m_targetObjectId );
        if( obj )
            return obj->m_latitude;
    }
    return 0;
}

void Bomber::Retaliate( int attackerId )
{
    if( g_app->GetWorld()->GetTeam( m_teamId )->m_type == Team::TypeAI )
    {
        if( m_currentState == 1 && !m_lacmLoadout &&
            m_states[1]->m_numTimesPermitted > 0 &&
            m_stateTimer == 0)
        {
            WorldObject *obj = g_app->GetWorld()->GetWorldObject( attackerId );
            if( obj )
            {
                if( obj->IsSiloClass() &&
                    obj->m_seen[ m_teamId ] )
                {
                    if( g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, obj->m_longitude, obj->m_latitude ) <= GetActionRangeSqd() )
                    {
                        Action( -1, obj->m_longitude, obj->m_latitude );
                        m_bombingRun = true;
                    }
                }
            }
        }
    }
}

bool Bomber::CanSetState( int state )
{
    if( state == 0 && ( m_currentState == 1 || m_currentState == 2 ) &&
        m_states[m_currentState]->m_numTimesPermitted == 0 )
        return true;
    if( m_lacmLoadout && state == 1 )
        return false;
    if( !m_lacmLoadout && state == 2 )
        return false;
    return MovingObject::CanSetState( state );
}

void Bomber::SetState( int state )
{
    if( !CanSetState( state ) )
        return;

    bool preserveQueue = ( m_currentState == 0 && ( state == 1 || ( state == 2 && m_lacmLoadout ) ) );

    if( preserveQueue )
    {
        Fixed actionRangeSqd = m_states[state]->m_actionRange * m_states[state]->m_actionRange;
        LList<ActionOrder *> outOfRange;
        for( int i = m_actionQueue.Size() - 1; i >= 0; --i )
        {
            ActionOrder *action = m_actionQueue[i];
            if( state == 2 && action->m_targetObjectId == -1 )
            {
                delete action;
                m_actionQueue.RemoveData( i );
                continue;
            }
            Fixed lon = action->m_longitude;
            Fixed lat = action->m_latitude;
            if( action->m_targetObjectId != -1 )
            {
                WorldObject *obj = g_app->GetWorld()->GetWorldObject( action->m_targetObjectId );
                if( obj )
                {
                    lon = obj->m_longitude;
                    lat = obj->m_latitude;
                }
            }
            Fixed distSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, lon, lat );
            if( distSqd > actionRangeSqd )
            {
                outOfRange.PutDataAtStart( action );
                m_actionQueue.RemoveData( i );
            }
        }
        for( int i = 0; i < outOfRange.Size(); ++i )
            m_actionQueue.PutData( outOfRange[i] );

        WorldObjectState *theState = m_states[state];
        m_currentState = state;
        m_stateTimer = theState->m_timeToPrepare;
        m_targetObjectId = -1;

        if( m_actionQueue.Size() > 0 )
        {
            ActionOrder *first = m_actionQueue[0];
            SetWaypoint( first->m_longitude, first->m_latitude );
        }
    }
    else
    {
        MovingObject::SetState( state );
    }

    if( m_currentState != 1 && m_currentState != 2 )
    {
        m_bombingRun = false;
    }
}


int Bomber::GetAttackOdds( int _defenderType )
{
    if( m_lacmLoadout && m_states[2]->m_numTimesPermitted > 0 )
        return g_app->GetWorld()->GetAttackOdds( TypeLACM, _defenderType );
    if( !m_lacmLoadout && m_states[1]->m_numTimesPermitted > 0 )
        return g_app->GetWorld()->GetAttackOdds( TypeNuke, _defenderType );
    return MovingObject::GetAttackOdds( _defenderType );
}

int Bomber::IsValidMovementTarget( Fixed longitude, Fixed latitude )
{
    if( m_lacmLoadout )
        return TargetTypeInvalid;
    return MovingObject::IsValidMovementTarget( longitude, latitude );
}

int Bomber::IsValidCombatTarget( int _objectId )
{
    WorldObject *obj = g_app->GetWorld()->GetWorldObject( _objectId );
    if( !obj ) return TargetTypeInvalid;

    int basicResult = MovingObject::IsValidCombatTarget( _objectId );
    if( basicResult < TargetTypeInvalid )
    {
        return basicResult;
    }

    if( obj->IsAircraftLauncher() )
    {
        if( obj->m_teamId == m_teamId )
        {
            return TargetTypeLand;
        }
    }

    // LACM targets: state 2 (active) or state 0 (standby - queue for when we engage)
    if( m_lacmLoadout && m_states[2]->m_numTimesPermitted > 0 )
    {
        if( obj->IsAircraft() || obj->IsCruiseMissileClass() || obj->IsBallisticMissileClass() )
            return TargetTypeInvalid;
        if( obj->IsTargetableSurfaceNavy() )
            return TargetTypeLaunchLACM;
        if( !obj->IsMovingObject() && WorldObject::GetArchetypeForType(obj->m_type) == WorldObject::ArchetypeBuilding )
            return TargetTypeLaunchLACM;
    }
    else if( !m_lacmLoadout && m_states[1]->m_numTimesPermitted > 0 )
    {
        // Buildings: ballistic nuke. Ships: LANM (nuclear cruise missile)
        if( !obj->IsMovingObject() )
            return TargetTypeLaunchNuke;
        if( obj->IsTargetableSurfaceNavy() )
            return TargetTypeLaunchNuke;
    }

    return TargetTypeInvalid;
}


bool Bomber::IsActionQueueable()
{
    return ( m_currentState == 0 || m_currentState == 1 || m_currentState == 2 );
}

bool Bomber::ShouldProcessActionQueue()
{
    return ( m_currentState != 0 );
}

bool Bomber::ShouldProcessNextQueuedAction()
{
    if( m_currentState == 2 )
    {
        if( m_actionQueue.Size() == 0 )
            return true;
        ActionOrder *head = m_actionQueue[0];
        if( head->m_targetObjectId == -1 )
            return true;
        WorldObject *obj = g_app->GetWorld()->GetWorldObject( head->m_targetObjectId );
        if( !obj )
            return true;
        Fixed distSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, obj->m_longitude, obj->m_latitude );
        return ( distSqd <= GetActionRangeSqd() );
    }
    if( m_currentState != 1 )
        return true;
    Fixed nukeLon = GetNukeTargetLongitude();
    Fixed nukeLat = GetNukeTargetLatitude();
    if( nukeLon == 0 && nukeLat == 0 )
        return true;
    Fixed distSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, nukeLon, nukeLat );
    if( distSqd <= GetActionRangeSqd() )
        return true;
    return false;
}

void Bomber::RequestAction( ActionOrder *_action )
{
    if( m_currentState == 0 || m_currentState == 1 || m_currentState == 2 )
    {
        if( m_lacmLoadout && _action->m_targetObjectId == -1 )
        {
            delete _action;
            return;
        }
        WorldObject *target = g_app->GetWorld()->GetWorldObject( _action->m_targetObjectId );
        if( target && ( target->IsAircraft() || target->IsCruiseMissileClass() || target->IsBallisticMissileClass() ) )
        {
            delete _action;
            return;
        }
        bool wasQueueEmpty = ( m_actionQueue.Size() == 0 );
        WorldObject::RequestAction( _action );
        if( wasQueueEmpty && m_currentState == 1 )
        {
            if( _action->m_targetObjectId != -1 && target )
                SetWaypoint( target->m_longitude, target->m_latitude );
            else if( _action->m_longitude != 0 || _action->m_latitude != 0 )
                SetWaypoint( _action->m_longitude, _action->m_latitude );
        }
        if( wasQueueEmpty && m_currentState == 2 )
        {
            if( _action->m_targetObjectId != -1 && target )
            {
                SetWaypoint( target->m_longitude, target->m_latitude );
            }
        }
    }
    else
    {
        WorldObject::RequestAction( _action );
    }
}


void Bomber::CeaseFire( int teamId )
{
    if( ( m_currentState == 0 || m_currentState == 1 ) && m_actionQueue.Size() > 0 )
    {
        ClearActionQueue();
        Land( GetClosestLandingPad() );
    }
    if( GetNukeTargetLongitude() != 0 || GetNukeTargetLatitude() != 0 )
    {
        if( g_app->GetWorldRenderer()->IsValidTerritory( teamId, GetNukeTargetLongitude(), GetNukeTargetLatitude(), false ) )
        {
            ClearActionQueue();
            m_bombingRun = false;
            Land( GetClosestLandingPad() );
        }
    }
    WorldObject::CeaseFire( teamId );
}


void Bomber::Render2D()
{
    MovingObject::Render2D();
    // Ammunition shown in mouse-over tooltip instead of smallnuke sprite
}

void Bomber::Render3D()
{
    MovingObject::Render3D();
    // Ammunition shown in mouse-over tooltip instead of smallnuke sprite
}

bool Bomber::SetWaypointOnAction()
{
    if( m_currentState == 0 )
        return ( m_actionQueue.Size() == 0 );
    if( m_currentState == 2 )
    {
        if( m_actionQueue.Size() > 0 )
            return false;
        return true;
    }
    if( GetNukeTargetLongitude() != 0 || GetNukeTargetLatitude() != 0 )
        return false;
    return ( m_actionQueue.Size() == 0 );
}
