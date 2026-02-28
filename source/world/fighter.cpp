#include "lib/universal_include.h"

#include "lib/math/vector3.h"
#include "lib/math/random_number.h"
#include "lib/language_table.h"

#include "app/app.h"
#include "app/globals.h"

#include "renderer/world_renderer.h"
#include "renderer/map_renderer.h"

#include "world/world.h"
#include "world/fighter.h"
#include "world/team.h"


Fighter::Fighter()
:   MovingObject(),
    m_lacmLoadout(false)
{
    SetType( TypeFighter );

    strcpy( bmpImageFilename, "graphics/fighter.bmp" );

    m_radarRange = 5;
    m_speed = Fixed::Hundredths(10);
    m_turnRate = Fixed::Hundredths(4);
    m_selectable = true;
    m_maxHistorySize = 10;
    m_range = 45;

    m_movementType = MovementTypeAir;
    m_opportunityFireOnly = false;

    AddState( LANGUAGEPHRASE("state_attack"), 60, 20, 5, 10, true, 6 );   // Gun: state 0 ammo (6 CAP / 2 Strike)
    AddState( LANGUAGEPHRASE("state_fighterlacm"), 60, 30, 5, 10, true, 0 );  // LACM: state 1 ammo (0 CAP / 2 Strike)

    m_states[1]->m_numTimesPermitted = 0;

    InitialiseTimers();
}


int Fighter::GetAttackOdds( int _defenderType )
{
    WorldObject::Archetype defenderArchetype = WorldObject::GetArchetypeForType( _defenderType );
    if( defenderArchetype == WorldObject::ArchetypeAircraft )
    {
        if( m_states[0]->m_numTimesPermitted > 0 )
            return g_app->GetWorld()->GetAttackOdds( TypeFighter, _defenderType );
    }
    else if( m_currentState == 1 && m_states[1]->m_numTimesPermitted > 0 )
    {
        if( defenderArchetype == WorldObject::ArchetypeBuilding )
            return g_app->GetWorld()->GetAttackOdds( TypeLACM, _defenderType );
        WorldObject::ClassType defenderClass = WorldObject::GetClassTypeForType( _defenderType );
        if( defenderClass == WorldObject::ClassTypeCarrier || defenderClass == WorldObject::ClassTypeBattleShip ||
            defenderClass == WorldObject::ClassTypeSub )
            return g_app->GetWorld()->GetAttackOdds( TypeLACM, _defenderType );
    }
    return MovingObject::GetAttackOdds( _defenderType );
}

void Fighter::AcquireTargetFromAction( ActionOrder *action )
{
    if( action->m_targetObjectId == -1 ) return;
    WorldObject *target = g_app->GetWorld()->GetWorldObject( action->m_targetObjectId );
    if( !target || !target->m_visible[m_teamId] ) return;
    if( g_app->GetWorld()->IsFriend( m_teamId, target->m_teamId ) )
        return;
    if( m_currentState == 1 && GetAttackOdds( target->m_type ) > 0 )
    {
        m_targetObjectId = action->m_targetObjectId;
        return;
    }
    if( m_currentState != 0 ) return;
    if( GetAttackOdds( target->m_type ) > 0 )
        m_targetObjectId = action->m_targetObjectId;
}

void Fighter::Action( int targetObjectId, Fixed longitude, Fixed latitude )
{
    if( m_currentState == 1 )
    {
        if( targetObjectId != -1 )
        {
            WorldObject *landCheck = g_app->GetWorld()->GetWorldObject( targetObjectId );
            if( landCheck && g_app->GetWorld()->IsFriend( m_teamId, landCheck->m_teamId ) &&
                ( landCheck->IsAircraftLauncher() || landCheck->m_type == TypeTanker ) )
            {
                m_targetObjectId = -1;
                m_isLanding = -1;
                m_isEscorting = -1;
                m_opportunityFireOnly = false;
                ClearActionQueue();
                Land( targetObjectId );
                return;
            }
            if( landCheck && g_app->GetWorld()->IsFriend( m_teamId, landCheck->m_teamId ) &&
                landCheck->IsAircraft() )
            {
                m_targetObjectId = -1;
                m_isLanding = -1;
                m_isEscorting = targetObjectId;
                m_opportunityFireOnly = false;
                ClearActionQueue();
                SetWaypoint( landCheck->m_longitude, landCheck->m_latitude );
                return;
            }
        }

        if( m_states[1]->m_numTimesPermitted <= 0 )
            return;
        if( targetObjectId == -1 )
        {
            m_targetObjectId = -1;
            m_isLanding = -1;
            m_isEscorting = -1;
            m_opportunityFireOnly = false;
            return;
        }
        WorldObject *targetObj = g_app->GetWorld()->GetWorldObject( targetObjectId );
        if( !targetObj || targetObj->m_life <= 0 )
            return;

        Fixed nukeRange = GetActionRange();
        Fixed distSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, longitude, latitude );
        bool inRange = ( distSqd <= nukeRange * nukeRange );

        if( m_stateTimer > 0 )
        {
            ActionOrder *retry = new ActionOrder();
            retry->m_targetObjectId = targetObjectId;
            retry->m_longitude = longitude;
            retry->m_latitude = latitude;
            m_actionQueue.PutDataAtStart( retry );
            return;
        }

        if( inRange )
        {
            MovingObject::Action( targetObjectId, longitude, latitude );
            g_app->GetWorld()->LaunchCruiseMissile( m_teamId, m_objectId, longitude, latitude, nukeRange, targetObjectId );

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
            ActionOrder *retry = new ActionOrder();
            retry->m_targetObjectId = targetObjectId;
            retry->m_longitude = longitude;
            retry->m_latitude = latitude;
            m_actionQueue.PutDataAtStart( retry );
            SetWaypoint( longitude, latitude );
            return;
        }
    }

    m_targetObjectId = -1;
    m_isLanding = -1;
    m_isEscorting = -1;
    m_opportunityFireOnly = false;
    ClearActionQueue();

    WorldObject *target = g_app->GetWorld()->GetWorldObject( targetObjectId );
    if( target && target->m_visible[m_teamId] )
    {
        if( g_app->GetWorld()->IsFriend( m_teamId, target->m_teamId ) &&
            ( target->IsAircraftLauncher() || target->m_type == TypeTanker ) )
        {
            Land( targetObjectId );
        }
        else if( g_app->GetWorld()->IsFriend( m_teamId, target->m_teamId ) &&
                 target->IsAircraft() )
        {
            m_isEscorting = targetObjectId;
            SetWaypoint( target->m_longitude, target->m_latitude );
        }
        else if( GetAttackOdds( target->m_type ) > 0 )
            m_targetObjectId = targetObjectId;
    }

    if( m_teamId == g_app->GetWorld()->m_myTeamId &&
        targetObjectId == -1 )
    {
        g_app->GetWorldRenderer()->CreateAnimation( WorldRenderer::AnimationTypeActionMarker, m_objectId,
												  longitude.DoubleValue(), latitude.DoubleValue() );
    }

    // State 0: do not call base Action - ammo consumed only when actually firing in Update()
}

bool Fighter::Update()
{   
    bool arrived = MoveToWaypoint();
    bool holdingPattern = false;

    // State 1 LACM: when in range of queued target, pop and call Action() (mirrors bomber state 2 block)
    if( m_currentState == 1 && m_actionQueue.Size() > 0 && m_stateTimer <= 0 )
    {
        ActionOrder *head = m_actionQueue[0];
        if( head->m_targetObjectId != -1 )
        {
            WorldObject *obj = g_app->GetWorld()->GetWorldObject( head->m_targetObjectId );
            if( obj && obj->m_life > 0 )
            {
                Fixed distSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, obj->m_longitude, obj->m_latitude );
                if( distSqd <= GetActionRangeSqd() )
                {
                    int targetId = head->m_targetObjectId;
                    Fixed lon = obj->m_longitude, lat = obj->m_latitude;
                    m_actionQueue.RemoveData(0);
                    delete head;
                    Action( targetId, lon, lat );
                }
            }
        }
    }

    if( m_targetObjectId != -1 )
    {
        WorldObject *targetObject = g_app->GetWorld()->GetWorldObject(m_targetObjectId);  
        if( targetObject )
        {
            if( g_app->GetWorld()->IsFriend( m_teamId, targetObject->m_teamId ) &&
                ( targetObject->IsAircraftLauncher() || targetObject->m_type == TypeTanker ) )
            {
                Land( m_targetObjectId );
                m_targetObjectId = -1;
                m_opportunityFireOnly = false;
            }
            else
            {
                if( !m_isRetaliating && !m_opportunityFireOnly )
                {
                    SetWaypoint( targetObject->m_lastKnownPosition[m_teamId].x, 
                                 targetObject->m_lastKnownPosition[m_teamId].y );
                }
           
                if( arrived )
                {
                    if( !targetObject->m_visible[m_teamId] )
                    {
                        m_targetObjectId = -1;
                        m_opportunityFireOnly = false;
                    }
                    holdingPattern = true;
                }

                if( m_stateTimer <= 0 )
                {
                    if( targetObject->m_visible[m_teamId] )
                    {
                        Fixed distanceSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, targetObject->m_longitude, targetObject->m_latitude);

                        if( distanceSqd <= GetActionRangeSqd() )
                        {
                            WorldObject::Archetype targetArchetype = WorldObject::GetArchetypeForType( targetObject->m_type );
                            bool isAircraftTarget = ( targetArchetype == WorldObject::ArchetypeAircraft );

                            if( isAircraftTarget && m_states[0]->m_numTimesPermitted > 0 )
                            {
                                FireGun( GetActionRange() );
                                m_states[0]->m_numTimesPermitted--;
                                m_stateTimer = m_states[ m_currentState ]->m_timeToReload;
                                if( BurstFireOnFired( m_targetObjectId ) )
                                {
                                    LList<int> excluded;
                                    BurstFireGetExcludedIds( excluded );
                                    int newId = GetTarget( ( m_range < 40 ) ? m_range : 40, &excluded );
                                    if( newId != -1 )
                                    {
                                        m_targetObjectId = newId;
                                        BurstFireResetShotCount();
                                    }
                                    else
                                    {
                                        BurstFireAccelerateCountdowns( Fixed(10) );
                                        BurstFireRemoveTarget( m_targetObjectId );
                                        BurstFireResetShotCount();
                                    }
                                }
                            }
                            else if( ( targetArchetype == WorldObject::ArchetypeBuilding || targetArchetype == WorldObject::ArchetypeNavy ) &&
                                     m_currentState == 1 && m_states[1]->m_numTimesPermitted > 0 )
                            {
                                // State 1 LACM: base pops queue and calls Action() when in range - no inline fire
                                holdingPattern = true;
                            }
                            else if( ( isAircraftTarget && m_states[0]->m_numTimesPermitted <= 0 ) ||
                                     ( !isAircraftTarget && ( m_currentState != 1 || m_states[1]->m_numTimesPermitted <= 0 ) ) )
                            {
                                m_targetObjectId = -1;
                                if( !isAircraftTarget && m_currentState == 1 )
                                    ClearWaypoints();
                                BurstFireResetShotCount();
                            }
                        }
                    }
                    else
                    {
                        m_targetObjectId = -1;
                        m_opportunityFireOnly = false;
                        BurstFireResetShotCount();
                    }
                }
            }
        }
        else
        {
            m_targetObjectId = -1;
            m_opportunityFireOnly = false;
            BurstFireResetShotCount();
        }
    }
    else if( m_targetObjectId == -1 &&
             m_isLanding == -1 &&
             ( m_targetLongitude != 0 || m_targetLatitude != 0 ) )
    {
        bool hasQueuedAttack = false;
        for( int i = 0; i < m_actionQueue.Size(); ++i )
            if( m_actionQueue[i]->m_targetObjectId != -1 ) { hasQueuedAttack = true; break; }
        if( !hasQueuedAttack )
        {
        // On patrol: opportunity fire at hostiles in range (including LACM at ships when Strike). Skip if we have active target or queued attacks.
        if( m_stateTimer <= 0 )
        {
            LList<int> excluded;
            BurstFireGetExcludedIds( excluded );
            int targetId = GetTarget( ( m_range < 40 ) ? m_range : 40, excluded.Size() > 0 ? &excluded : nullptr );
            if( targetId != -1 )
            {
                WorldObject *targetObject = g_app->GetWorld()->GetWorldObject( targetId );
                if( targetObject &&
                    g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, targetObject->m_longitude, targetObject->m_latitude ) <= GetActionRangeSqd() )
                {
                    m_targetObjectId = targetId;
                    m_opportunityFireOnly = true;
                    WorldObject::Archetype targetArchetype = WorldObject::GetArchetypeForType( targetObject->m_type );
                    bool isAircraftTarget = ( targetArchetype == WorldObject::ArchetypeAircraft );
                    if( isAircraftTarget && m_states[0]->m_numTimesPermitted > 0 )
                    {
                        FireGun( GetActionRange() );
                        m_states[0]->m_numTimesPermitted--;
                        m_stateTimer = m_states[ m_currentState ]->m_timeToReload;
                        BurstFireOnFired( targetId );
                    }
                    else if( ( targetArchetype == WorldObject::ArchetypeBuilding || targetArchetype == WorldObject::ArchetypeNavy ) &&
                             m_currentState == 1 && m_states[1]->m_numTimesPermitted > 0 )
                    {
                        // Opportunity fire LACM: add to queue, base will pop and call Action() (same flow as bomber)
                        ActionOrder *order = new ActionOrder();
                        order->m_targetObjectId = targetId;
                        order->m_longitude = targetObject->m_longitude;
                        order->m_latitude = targetObject->m_latitude;
                        RequestAction( order );
                        m_targetObjectId = targetId;
                        BurstFireResetShotCount();
                    }
                }
            }
        }
        }
    }

    if( m_targetLongitude == 0 && m_targetLatitude == 0 )
    {
        if( m_targetObjectId == -1 )
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
        else
        {
            holdingPattern = true;
        }
    }

    if( m_currentState == 1 && m_states[1]->m_numTimesPermitted <= 0 )
    {
        SetState( 0 );
        Land( GetClosestLandingPad() );
    }
    else if( IsIdle() )
    {
        LList<int> excluded;
        BurstFireGetExcludedIds( excluded );
        int targetId = GetTarget( ( m_range < 40 ) ? m_range : 40, excluded.Size() > 0 ? &excluded : nullptr );
        WorldObject *obj = ( targetId != -1 ) ? g_app->GetWorld()->GetWorldObject( targetId ) : nullptr;
        if( obj )
        {
            SetWaypoint( obj->m_longitude, obj->m_latitude );
            m_targetObjectId = obj->m_objectId;
        }
        else
        {
            Land( GetClosestLandingPad() );
        }
    }

    if( holdingPattern )
    {
        Fixed timePerUpdate = SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor();

        m_vel.RotateAroundZ( Fixed::Hundredths(2) * timePerUpdate );
        m_longitude += m_vel.x * Fixed(timePerUpdate);
        m_latitude += m_vel.y * Fixed(timePerUpdate);

        if( m_longitude <= -180 ||
            m_longitude >= 180 )
        {
            CrossSeam();
        }

        if( m_range <= 0 )
        {
            m_life = 0;
			m_lastHitByTeamId = m_teamId;
            g_app->GetWorld()->AddOutOfFueldMessage( m_objectId );            
        }
    }

    bool amIDead = MovingObject::Update();
    return amIDead;
}

void Fighter::Render2D()
{
    MovingObject::Render2D();
}

void Fighter::Render3D()
{
    MovingObject::Render3D();
}

void Fighter::RunAI()
{
    if( IsIdle() )
    {
        Land( GetClosestLandingPad() );
    }
}

int Fighter::GetAttackState()
{
    return 0;
}


void Fighter::Land( int targetId )
{
    WorldObject *target = g_app->GetWorld()->GetWorldObject(targetId);
    if( target )
    {
        SetWaypoint( target->m_longitude, target->m_latitude );
        m_isLanding = targetId;
    }
}

bool Fighter::UsingGuns()
{
    return true;
}




int Fighter::CountTargettedFighters( int targetId )
{
    int counter = 0;
    for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
    {
        if( g_app->GetWorld()->m_objects.ValidIndex(i) )
        {
            WorldObject *obj = g_app->GetWorld()->m_objects[i];
            if( obj->IsFighterClass() &&
                obj->m_teamId == m_teamId &&
                obj->m_targetObjectId == targetId )
            {
                counter++;
            }
        }
    }
    return counter;
}


int Fighter::GetTarget( Fixed range, const LList<int> *excludeIds )
{
    LList<int> farTargets;
    LList<int> closeTargets;
    for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
    {
        if( g_app->GetWorld()->m_objects.ValidIndex(i) )
        {
            WorldObject *obj = g_app->GetWorld()->m_objects[i];
            if( obj->m_teamId != TEAMID_SPECIALOBJECTS )
            {
                if( excludeIds )
                {
                    bool excluded = false;
                    for( int e = 0; e < excludeIds->Size(); ++e )
                        if( excludeIds->GetData( e ) == obj->m_objectId ) { excluded = true; break; }
                    if( excluded ) continue;
                }
                if( !g_app->GetWorld()->IsFriend( obj->m_teamId, m_teamId ) &&
                    GetAttackOdds( obj->m_type ) > 0 &&
                    obj->m_visible[m_teamId] &&
                    !g_app->GetWorld()->GetTeam( m_teamId )->m_ceaseFire[ obj->m_teamId ] )
                {
                    Fixed distanceSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, obj->m_longitude, obj->m_latitude );
                    if( distanceSqd < GetActionRangeSqd() )
                    {
                        closeTargets.PutData(obj->m_objectId );
                    }
                    else if( distanceSqd < range * range )
                    {
                        farTargets.PutData( obj->m_objectId );
                    }
                }
            }
        }
    }
    if( closeTargets.Size() > 0 )
    {
        int targetIndex = syncrand() % closeTargets.Size();
        return closeTargets[ targetIndex ];
    }
    else if( farTargets.Size() > 0 )
    {
        int targetIndex = syncrand() % farTargets.Size();
        return farTargets[ targetIndex ];
    }
    return -1;
}

int Fighter::IsValidCombatTarget( int _objectId )
{
    WorldObject *obj = g_app->GetWorld()->GetWorldObject( _objectId );
    if( !obj ) return TargetTypeInvalid;

    if( obj->IsCarrierClass() ||
        obj->IsAirbaseClass() ||
        obj->m_type == TypeTanker )
    {
        if( obj->m_teamId == m_teamId ||
            g_app->GetWorld()->GetTeam(m_teamId)->m_ceaseFire[obj->m_teamId] )
        {
            return TargetTypeLand;
        }
    }

    if( obj->IsAircraft() || obj->IsCruiseMissileClass() || obj->IsBallisticMissileClass() )
    {
        if( obj->IsAircraft() &&
            ( obj->m_teamId == m_teamId ||
              g_app->GetWorld()->GetTeam(m_teamId)->m_ceaseFire[obj->m_teamId] ) )
        {
            return TargetTypePursue;
        }
        if( m_states[0]->m_numTimesPermitted > 0 )
            return TargetTypeLaunchFighter;
        return TargetTypeInvalid;
    }

    if( obj->IsTargetableSurfaceNavy() )
    {
        if( m_currentState == 1 && m_states[1]->m_numTimesPermitted > 0 )
            return TargetTypeLaunchLACM;
        return TargetTypeInvalid;
    }
    if( obj->IsCityClass() )
    {
        if( m_currentState == 1 && m_states[1]->m_numTimesPermitted > 0 )
            return TargetTypeLaunchLACM;
        return TargetTypeInvalid;
    }
    if( !obj->IsMovingObject() && WorldObject::GetArchetypeForType(obj->m_type) == WorldObject::ArchetypeBuilding )
    {
        if( m_currentState == 1 && m_states[1]->m_numTimesPermitted > 0 )
            return TargetTypeLaunchLACM;
        return TargetTypeInvalid;
    }

    return TargetTypeInvalid;
}


bool Fighter::IsActionQueueable()
{
    return true;
}

bool Fighter::ShouldProcessActionQueue()
{
    return true;  // Process queue in both states (state 0: gun/land, state 1: LACM)
}

bool Fighter::ShouldProcessNextQueuedAction()
{
    // State 1 LACM: process when in range of head action's target (like bomber state 2)
    if( m_currentState == 1 )
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
    // State 0: don't pop until done with current target
    if( m_targetObjectId == -1 )
        return true;
    return false;
}

void Fighter::RequestAction( ActionOrder *_action )
{
    if( m_currentState == 1 )
    {
        WorldObject *target = g_app->GetWorld()->GetWorldObject( _action->m_targetObjectId );
        if( target && ( target->IsCruiseMissileClass() || target->IsBallisticMissileClass() ) )
        {
            delete _action;
            return;
        }
        if( target && target->IsAircraft() &&
            !g_app->GetWorld()->IsFriend( m_teamId, target->m_teamId ) )
        {
            delete _action;
            return;
        }
        bool wasQueueEmpty = ( m_actionQueue.Size() == 0 );
        WorldObject::RequestAction( _action );
        if( wasQueueEmpty && _action->m_targetObjectId != -1 && target )
        {
            SetWaypoint( target->m_longitude, target->m_latitude );
        }
    }
    else
    {
        m_targetObjectId = -1;
        m_isLanding = -1;
        m_isEscorting = -1;
        m_opportunityFireOnly = false;
        ClearActionQueue();
        WorldObject::RequestAction( _action );
    }
}

bool Fighter::SetWaypointOnAction()
{
    return true;
}

