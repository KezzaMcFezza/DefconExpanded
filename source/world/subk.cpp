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
#include "world/subk.h"
#include "world/sub.h"
#include "world/fleet.h"


SubK::SubK()
:   Sub()
{
    SetType( TypeSubK );

    strcpy( bmpImageFilename, "graphics/subk.bmp" );

    m_range = Fixed(50);
    m_nukeSupply = 0;

    Fixed gameScale = World::GetUnitScaleFactor();
    m_states[0]->m_actionRange = Fixed(3) / gameScale;
    m_states[1]->m_actionRange = Fixed(2) / gameScale;

    // Replace state 2: Recharge (was standby in Sub)
    m_states[0]->m_timeToPrepare = 45;
    m_states[1]->m_timeToPrepare = 60;
    m_states[0]->m_timeToReload = 15;
    m_states[1]->m_timeToReload = 25;

    if( m_states.Size() > 2 )
    {
        m_states[2]->m_stateName = newStr( LANGUAGEPHRASE("state_subrecharge") );
        m_states[2]->m_timeToPrepare = 90;
        m_states[2]->m_timeToReload = Fixed(3);
        m_states[2]->m_numTimesPermitted = -1;
        m_states[2]->m_actionRange = Fixed(0);
        m_states[2]->m_isActionable = false;
        m_states[2]->m_defconPermitted = 5;
    }
    if( m_states.Size() > 3 )
    {
        delete m_states[3];
        m_states.RemoveData(3);
    }
}

void SubK::Action( int targetObjectId, Fixed longitude, Fixed latitude )
{
    if( !CheckCurrentState() )
        return;

    if( m_currentState == 0 || m_currentState == 1 )
    {
        WorldObject *targetObject = g_app->GetWorld()->GetWorldObject(targetObjectId);
        if( targetObject &&
            targetObject->m_visible[m_teamId] &&
            g_app->GetWorld()->GetAttackOdds( m_type, targetObject->m_type ) > 0 )
        {
            m_targetObjectId = targetObjectId;
        }
    }

    WorldObject::Action( targetObjectId, longitude, latitude );
}

bool SubK::Update()
{
    if( m_range <= Fixed(1) && m_fleetId != -1 )
    {
        Fleet *fleet = g_app->GetWorld()->GetTeam( m_teamId )->GetFleet( m_fleetId );
        if( fleet )
            fleet->StopFleet();
    }

    bool isEnemy = ( g_app->GetWorld()->m_myTeamId >= 0 &&
                     !g_app->GetWorld()->IsFriend( m_teamId, g_app->GetWorld()->m_myTeamId ) );
    bool surfaced = ( m_currentState == 2 );
    if( isEnemy )
    {
        strcpy( bmpImageFilename, surfaced ? "graphics/sub_surfaced.bmp" : "graphics/sub.bmp" );
    }
    else
    {
        strcpy( bmpImageFilename, surfaced ? "graphics/subk_surfaced.bmp" : "graphics/subk.bmp" );
    }

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
        else if( m_stateTimer <= 0 && m_currentState == 2 )
        {
            if( m_range < Fixed(50) )
                m_range = m_range + Fixed(1);
            m_stateTimer = m_states[2]->m_timeToReload;
        }

        bool haveValidTarget = false;
        if( m_targetObjectId != -1 )
        {
            WorldObject *targetObject = g_app->GetWorld()->GetWorldObject(m_targetObjectId);
            if( targetObject && ( targetObject->m_visible[m_teamId] || targetObject->m_seen[m_teamId] ) )
            {
                Fixed distanceSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, targetObject->m_longitude, targetObject->m_latitude );
                if( distanceSqd <= GetActionRangeSqd() )
                {
                    haveValidTarget = true;
                    if( ( m_stateTimer <= 0 || canAttack ) && g_app->GetWorld()->GetDefcon() <= 3 )
                    {
                        FireGun( GetActionRange() );
                        fleet->FleetAction( m_targetObjectId );
                    }
                    if( m_currentState == 0 )
                        m_stateTimer = m_states[0]->m_timeToReload;
                }
            }
        }

        if( !haveValidTarget && m_retargetTimer <= 0 && m_currentState == 1 )
        {
            m_targetObjectId = -1;
            if( g_app->GetWorld()->GetDefcon() <= 3 )
            {
                m_retargetTimer = 5 + syncfrand(10);
                WorldObject *obj = FindTarget( nullptr );
                if( obj )
                    m_targetObjectId = obj->m_objectId;
            }
        }
    }

    Fixed gs = World::GetUnitScaleFactor();
    if( m_currentState == 0 )
        m_speed = Fixed::Hundredths(2) / gs;
    else if( m_currentState == 1 )
        m_speed = Fixed::Hundredths(3) / gs;

    if( m_currentState != 2 )
        MoveToWaypoint();

    return MovingObject::Update();
}

void SubK::RunAI()
{
    AppDebugAssert( m_type == WorldObject::TypeSubK );
    if( m_aiTimer <= 0 )
    {
        m_aiTimer = m_aiSpeed;
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
    }
}

WorldObject *SubK::FindTarget( const LList<int> *excludeIds )
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
            g_app->GetWorld()->GetAttackOdds( TypeSubK, obj->m_type ) > 0 &&
            m_currentState == 1 )
        {
            if( IsSafeTarget( fleetTarget ) &&
                g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, obj->m_longitude, obj->m_latitude) <= GetActionRangeSqd() )
            {
                if( fleet->m_pursueTarget )
                {
                    if( obj->m_teamId == fleet->m_targetTeam && obj->m_fleetId == fleet->m_targetFleet )
                        return obj;
                }
                else
                    return obj;
            }
        }
    }
    return nullptr;
}

int SubK::GetAttackState()
{
    return 0;
}

bool SubK::UsingNukes()
{
    return false;
}

void SubK::SetState( int state )
{
    MovingObject::SetState( state );
    if( m_currentState == 2 )
    {
        Fleet *fleet = g_app->GetWorld()->GetTeam( m_teamId )->GetFleet( m_fleetId );
        if( fleet )
            fleet->StopFleet();
    }
}

bool SubK::IsActionQueueable()
{
    return false;
}

bool SubK::IsGroupable( int unitType )
{
    return ( unitType == TypeSubK );
}

void SubK::RequestAction( ActionOrder *_action )
{
    WorldObject::RequestAction(_action);
}

int SubK::IsValidMovementTarget( Fixed longitude, Fixed latitude )
{
    if( m_currentState == 2 )
        return TargetTypeInvalid;
    return MovingObject::IsValidMovementTarget( longitude, latitude );
}

int SubK::GetAttackOdds( int _defenderType )
{
    if( m_currentState == 2 )
        return 0;
    return MovingObject::GetAttackOdds( _defenderType );
}

int SubK::IsValidCombatTarget( int _objectId )
{
    WorldObject *obj = g_app->GetWorld()->GetWorldObject( _objectId );
    if( !obj ) return TargetTypeInvalid;

    int basicResult = MovingObject::IsValidCombatTarget( _objectId );
    if( basicResult < TargetTypeInvalid )
        return basicResult;

    if( !g_app->GetWorld()->IsFriend( m_teamId, obj->m_teamId ) &&
        ( m_currentState == 0 || m_currentState == 1 ) )
    {
        if( obj->IsMovingObject() )
        {
            MovingObject *mobj = (MovingObject *) obj;
            if( mobj->m_movementType == MovementTypeSea )
                return TargetTypeValid;
        }
    }
    return TargetTypeInvalid;
}

bool SubK::IsSafeTarget( Fleet *_fleet )
{
    for( int i = 0; i < _fleet->m_fleetMembers.Size(); ++i )
    {
        WorldObject *ship = g_app->GetWorld()->GetWorldObject( _fleet->m_fleetMembers[i] );
        if( ship && ship->IsCarrierClass() && ( ship->m_visible[ m_teamId ] || ship->m_seen[ m_teamId ] ) )
            return false;
    }
    return true;
}
