#include "lib/universal_include.h"
#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/language_table.h"
#include "lib/math/random_number.h"
#include "lib/string_utils.h"

#include "app/app.h"
#include "app/globals.h"
#include "app/game.h"

#include "renderer/map_renderer.h"

#include "lib/debug/profiler.h"

#include "interface/interface.h"

#include "world/world.h"
#include "world/subc.h"
#include "world/sub.h"
#include "world/lacm.h"
#include "world/fleet.h"


SubC::SubC()
:   Sub()
{
    SetType( TypeSubC );

    strcpy( bmpImageFilename, "graphics/subc.bmp" );

    // Replace state 3 (nuke) with LACM; state 2 (standby) stays
    m_states[0]->m_timeToPrepare = 30;
    m_states[0]->m_timeToReload = 20;
    m_states[1]->m_timeToPrepare = 30;
    m_states[1]->m_timeToReload = 20;

    if( m_states.Size() > 3 )
    {
        m_states[3]->m_stateName = newStr( LANGUAGEPHRASE("state_sublacm") );
        m_states[3]->m_timeToPrepare = 60;
        m_states[3]->m_timeToReload = 6;
        m_states[3]->m_numTimesPermitted = 20;
        m_states[3]->m_defconPermitted = 3;
        m_states[3]->m_actionRange = Fixed(90);
    }

    InitialiseTimers();
}

void SubC::Action( int targetObjectId, Fixed longitude, Fixed latitude )
{
    if( !CheckCurrentState() )
    {
        return;
    }

    if( m_currentState == 0 || m_currentState == 1 )
    {
        WorldObject *targetObject = g_app->GetWorld()->GetWorldObject(targetObjectId);
        if( targetObject )
        {
            if( targetObject->m_visible[m_teamId] &&
                g_app->GetWorld()->GetAttackOdds( m_type, targetObject->m_type ) > 0 )
            {
                m_targetObjectId = targetObjectId;
            }
        }
    }
    else if( m_currentState == 3 )
    {
        WorldObject *targetObj = g_app->GetWorld()->GetWorldObject( targetObjectId );
        if( !targetObj || targetObj->m_life <= 0 )
        {
            return;
        }
        if( m_stateTimer <= 0 )
        {
            g_app->GetWorld()->LaunchCruiseMissile( m_teamId, m_objectId, longitude, latitude, Fixed(90), targetObjectId );

            Fleet *fleet = g_app->GetWorld()->GetTeam( m_teamId )->GetFleet( m_fleetId );
            if( fleet ) fleet->m_subNukesLaunched++;
        }
        else
        {
            return;
        }
    }

    WorldObject::Action( targetObjectId, longitude, latitude );
}

bool SubC::Update()
{
    bool isEnemy = ( g_app->GetWorld()->m_myTeamId >= 0 &&
                     !g_app->GetWorld()->IsFriend( m_teamId, g_app->GetWorld()->m_myTeamId ) );
    bool surfaced = ( m_currentState == 3 );
    if( isEnemy )
    {
        strcpy( bmpImageFilename, surfaced ? "graphics/sub_surfaced.bmp" : "graphics/sub.bmp" );
    }
    else
    {
        strcpy( bmpImageFilename, surfaced ? "graphics/subc_surfaced.bmp" : "graphics/subc.bmp" );
    }

    if( m_currentState == 0 )
        m_speed = Fixed::Hundredths(2);
    else if( m_currentState == 1 )
        m_speed = Fixed::Hundredths(3);

    Fleet *fleet = g_app->GetWorld()->GetTeam( m_teamId )->GetFleet( m_fleetId );
    if( fleet )
    {
        bool canAttack = false;
        if( m_stateTimer <= 0 && m_currentState == 1 )
        {
            Ping();
            m_stateTimer = m_states[1]->m_timeToReload;
            canAttack = true;
        }
        else if( m_stateTimer <= 0 && m_currentState == 0 )
        {
            PassivePing();
            m_stateTimer = m_states[0]->m_timeToReload;
            canAttack = true;
        }

        bool haveValidTarget = false;

        if( m_currentState != 3 && m_targetObjectId != -1 )
        {
            WorldObject *targetObject = g_app->GetWorld()->GetWorldObject(m_targetObjectId);
            if( targetObject )
            {
                if( targetObject->m_visible[m_teamId] || targetObject->m_seen[m_teamId] )
                {
                    Fixed distanceSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, targetObject->m_longitude, targetObject->m_latitude);
                    if( distanceSqd <= GetActionRangeSqd() )
                    {
                        haveValidTarget = true;
                        if( m_stateTimer <= 0 || canAttack )
                        {
                            if( g_app->GetWorld()->GetDefcon() <= 3 )
                            {
                                FireGun( GetActionRange() );
                                fleet->FleetAction( m_targetObjectId );
                            }
                            if( m_currentState == 0 )
                            {
                                m_stateTimer = m_states[0]->m_timeToReload;
                            }
                        }
                    }
                }
            }
        }

        if( m_currentState != 3 && !haveValidTarget && m_retargetTimer <= 0 )
        {
            START_PROFILE("FindTarget");
            m_targetObjectId = -1;
            if( g_app->GetWorld()->GetDefcon() <= 3 )
            {
                m_retargetTimer = 5 + syncfrand(10);
                WorldObject *obj = FindTarget( nullptr );
                if( obj )
                {
                    m_targetObjectId = obj->m_objectId;
                }
            }
            END_PROFILE("FindTarget");
        }
    }

    if( m_currentState != 3 )
    {
        MoveToWaypoint();
    }
    else if( m_states[m_currentState]->m_numTimesPermitted == 0 )
    {
        SetState(0);
    }

    return MovingObject::Update();
}

void SubC::RunAI()
{
    START_PROFILE("SubCAI");
    AppDebugAssert( m_type == WorldObject::TypeSubC );
    if( m_aiTimer <= 0 )
    {
        m_aiTimer = m_aiSpeed;
        Team *team = g_app->GetWorld()->GetTeam( m_teamId );
        Fleet *fleet = team->GetFleet( m_fleetId );

        for( int i = 0; i < m_actionQueue.Size(); ++i )
        {
            ActionOrder *action = m_actionQueue[i];
            if( action->m_targetObjectId != -1 )
            {
                WorldObject *obj = g_app->GetWorld()->GetWorldObject( action->m_targetObjectId );
                if( !obj )
                {
                    m_actionQueue.RemoveData(i);
                    delete action;
                    --i;
                }
            }
        }

        if( g_app->GetWorld()->GetDefcon() <= 3 && fleet )
        {
            if( fleet->IsInNukeRange() )
            {
                if( team->m_maxTargetsSeen >= 4 ||
                    (team->m_currentState >= Team::StateAssault && m_stateTimer == 0) )
                {
                    if( m_states[3]->m_numTimesPermitted - m_actionQueue.Size() <= 0 )
                    {
                        END_PROFILE("SubCAI");
                        return;
                    }
                    Fixed maxRange = 40;
                    Fixed maxRangeSqd = 40 * 40;
                    Fixed longitude = 0;
                    Fixed latitude = 0;
                    int objectId = -1;
                    int targetTeam = team->m_targetTeam;
                    LACM::FindTarget( m_teamId, targetTeam, m_objectId, maxRange, &longitude, &latitude, &objectId );
                    if( (longitude != 0 || latitude != 0) &&
                        m_states[3]->m_numTimesPermitted - m_actionQueue.Size() > 0 )
                    {
                        if( m_currentState != 3 )
                            SetState(3);
                        ActionOrder *action = new ActionOrder();
                        action->m_longitude = longitude;
                        action->m_latitude = latitude;
                        action->m_targetObjectId = objectId;
                        RequestAction( action );
                    }
                    else if( m_actionQueue.Size() == 0 )
                    {
                        SetState(0);
                    }
                }
            }
            else if( m_actionQueue.Size() == 0 && m_currentState != 0 )
            {
                SetState(0);
            }
        }
    }
    END_PROFILE("SubCAI");
}

int SubC::GetAttackState()
{
    return 0;
}

bool SubC::UsingNukes()
{
    return false;
}

bool SubC::UsingLACM()
{
    return ( m_currentState == 3 );
}

void SubC::SetState( int state )
{
    MovingObject::SetState( state );
    if( m_currentState == 3 )
    {
        Fleet *fleet = g_app->GetWorld()->GetTeam( m_teamId )->GetFleet( m_fleetId );
        if( fleet ) fleet->StopFleet();
    }
}

bool SubC::IsActionQueueable()
{
    return ( m_currentState == 3 );
}

bool SubC::IsGroupable( int unitType )
{
    return ( unitType == TypeSubC );
}

void SubC::RequestAction(ActionOrder *_action)
{
    if( m_currentState == 3 )
    {
        if( _action->m_targetObjectId != -1 )
        {
            WorldObject *target = g_app->GetWorld()->GetWorldObject( _action->m_targetObjectId );
            if( target && (target->IsAircraft() || target->IsCruiseMissileClass() || target->IsBallisticMissileClass()) )
            {
                delete _action;
                return;
            }
        }
        Fixed actionRange = m_states[3]->m_actionRange;
        if( g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, _action->m_longitude, _action->m_latitude ) < actionRange * actionRange )
        {
            WorldObject::RequestAction(_action);
        }
        else
        {
            delete _action;
        }
    }
    else
    {
        WorldObject::RequestAction(_action);
    }
}

int SubC::GetAttackOdds( int _defenderType )
{
    if( m_currentState == 3 && m_states[3]->m_numTimesPermitted > 0 )
    {
        return g_app->GetWorld()->GetAttackOdds( TypeLACM, _defenderType );
    }
    if( m_currentState == 3 )
        return 0;
    return MovingObject::GetAttackOdds( _defenderType );
}

int SubC::IsValidCombatTarget( int _objectId )
{
    WorldObject *obj = g_app->GetWorld()->GetWorldObject( _objectId );
    if( !obj ) return TargetTypeInvalid;

    int basicResult = MovingObject::IsValidCombatTarget( _objectId );
    if( basicResult < TargetTypeInvalid )
        return basicResult;

    if( !g_app->GetWorld()->IsFriend( m_teamId, obj->m_teamId ) )
    {
        if( m_currentState == 3 )
        {
            if( obj->IsAircraft() || obj->IsCruiseMissileClass() || obj->IsBallisticMissileClass() )
                return TargetTypeInvalid;
            Fixed distanceSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, obj->m_longitude, obj->m_latitude );
            Fixed actionRangeSqd = m_states[3]->m_actionRange * m_states[3]->m_actionRange;
            if( distanceSqd >= actionRangeSqd )
                return TargetTypeOutOfRange;
            if( obj->IsTargetableSurfaceNavy() )
                return TargetTypeLaunchLACM;
            if( !obj->IsMovingObject() )
                return TargetTypeLaunchLACM;
        }
        if( m_currentState == 0 || m_currentState == 1 )
        {
            if( obj->IsMovingObject() )
            {
                MovingObject *mobj = (MovingObject *) obj;
                if( mobj->m_movementType == MovementTypeSea )
                    return TargetTypeValid;
            }
        }
    }
    return TargetTypeInvalid;
}

int SubC::IsValidMovementTarget( Fixed longitude, Fixed latitude )
{
    if( m_currentState == 3 )
        return TargetTypeInvalid;
    return MovingObject::IsValidMovementTarget( longitude, latitude );
}

WorldObject *SubC::FindTarget( const LList<int> *excludeIds )
{
    Team *team = g_app->GetWorld()->GetTeam( m_teamId );
    Fleet *fleet = team->GetFleet( m_fleetId );
    if( !fleet ) return nullptr;

    for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
    {
        if( !g_app->GetWorld()->m_objects.ValidIndex(i) ) continue;
        WorldObject *obj = g_app->GetWorld()->m_objects[i];
        if( excludeIds )
        {
            bool excluded = false;
            for( int e = 0; e < excludeIds->Size(); ++e )
                if( excludeIds->GetData( e ) == obj->m_objectId ) { excluded = true; break; }
            if( excluded ) continue;
        }
        Fleet *fleetTarget = g_app->GetWorld()->GetTeam( obj->m_teamId )->GetFleet( obj->m_fleetId );
        if( !fleetTarget ) continue;
        if( !g_app->GetWorld()->IsFriend( obj->m_teamId, m_teamId ) &&
            ( obj->m_visible[m_teamId] || obj->m_seen[m_teamId] ) &&
            !team->m_ceaseFire[ obj->m_teamId ] &&
            g_app->GetWorld()->GetAttackOdds( TypeSubC, obj->m_type ) > 0 )
        {
            bool safeTarget = ( m_states[3]->m_numTimesPermitted == 0 || IsSafeTarget( fleetTarget ) );
            if( safeTarget &&
                g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, obj->m_longitude, obj->m_latitude) <= 5 * 5 )
            {
                if( fleet->m_pursueTarget )
                {
                    if( obj->m_teamId == fleet->m_targetTeam && obj->m_fleetId == fleet->m_targetFleet )
                        return obj;
                }
                else if( m_currentState == 1 )
                {
                    return obj;
                }
            }
        }
    }
    return nullptr;
}

bool SubC::IsSafeTarget( Fleet *_fleet )
{
    for( int i = 0; i < _fleet->m_fleetMembers.Size(); ++i )
    {
        WorldObject *ship = g_app->GetWorld()->GetWorldObject( _fleet->m_fleetMembers[i] );
        if( ship && ship->IsCarrierClass() && ( ship->m_visible[ m_teamId ] || ship->m_seen[ m_teamId ] ) )
            return false;
    }
    return true;
}
