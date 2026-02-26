#include "lib/universal_include.h"

#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/language_table.h"
#include "lib/math/random_number.h"

#include "app/app.h"
#include "app/globals.h"
#include "app/game.h"

#include "renderer/map_renderer.h"

#include "interface/interface.h"

#include "world/world.h"
#include "world/battleship.h"
#include "world/gunfire.h"
#include "world/depthcharge.h"
#include "world/fleet.h"
#include "world/lacm.h"


BattleShip::BattleShip()
:   MovingObject()
{
    SetType( TypeBattleShip );

    strcpy( bmpImageFilename, "graphics/tico.bmp" );

    m_stealthType = 100;
    m_radarRange = 10;
    m_speed = Fixed::Hundredths(3);
    m_turnRate = Fixed::Hundredths(1);
    m_selectable = true;
    m_maxHistorySize = 10;
    m_range = Fixed::MAX;
    m_movementType = MovementTypeSea;
    m_life = 3;

    m_ghostFadeTime = 150;

    // 0: Air Defense (vs aircraft and missiles)
    AddState( LANGUAGEPHRASE("state_airdefense"), 60, 10, 10, 30, true, -1, 3 );
    // 1: LACM Launch (LACM vs buildings, LACM anti-ship vs surface ships)
    AddState( LANGUAGEPHRASE("state_sublacm"), 60, 6, 10, 45, true, 40, 3 );
    // 2: Anti-Sub (depth charges)
    AddState( LANGUAGEPHRASE("state_antisub"), 120, 30, 10, 5, false, -1, 3 );
    // 3: Naval Gun (vs surface ships only)
    AddState( LANGUAGEPHRASE("state_navaldefense"), 60, 20, 10, 10, true, -1, 3 );

    InitialiseTimers();
}

void BattleShip::Action( int targetObjectId, Fixed longitude, Fixed latitude )
{
    if( !CheckCurrentState() )
    {
        return;
    }

    m_targetObjectId = -1;

    if( m_currentState == 0 )
    {
        // Air defense: set target only
        WorldObject *target = g_app->GetWorld()->GetWorldObject( targetObjectId );
        if( target && target->m_visible[m_teamId] &&
            g_app->GetWorld()->GetAttackOdds( m_type, target->m_type, m_objectId ) > 0 )
        {
            m_targetObjectId = targetObjectId;
        }
        return;
    }

    if( m_currentState == 1 )
    {
        // LACM mode: launch LACM (building, ship, or map position)
        if( m_stateTimer <= 0 )
        {
            WorldObject *targetObj = ( targetObjectId != -1 ) ? g_app->GetWorld()->GetWorldObject( targetObjectId ) : nullptr;
            if( targetObj && targetObj->m_life <= 0 )
            {
                return;
            }
            bool isShipTarget = ( targetObj &&
                targetObj->IsNavy() && ( !targetObj->IsSubmarine() || !targetObj->IsHiddenFrom() ) );
            if( isShipTarget )
            {
                g_app->GetWorld()->LaunchCruiseMissile( m_teamId, m_objectId, longitude, latitude, Fixed(90), targetObjectId );
            }
            else
            {
                g_app->GetWorld()->LaunchCruiseMissile( m_teamId, m_objectId, longitude, latitude, Fixed(90), -1 );
            }
            Fleet *fleet = g_app->GetWorld()->GetTeam( m_teamId )->GetFleet( m_fleetId );
            if( fleet ) fleet->FleetAction( targetObjectId );
        }
        WorldObject::Action( targetObjectId, longitude, latitude );
        return;
    }

    if( m_currentState == 2 )
    {
        // Anti-sub: set target only
        WorldObject *target = g_app->GetWorld()->GetWorldObject( targetObjectId );
        if( target && ( target->m_visible[m_teamId] || target->m_seen[m_teamId] ) &&
            target->IsSubmarine() &&
            g_app->GetWorld()->GetAttackOdds( m_type, target->m_type, m_objectId ) > 0 )
        {
            m_targetObjectId = targetObjectId;
        }
        return;
    }

    if( m_currentState == 3 )
    {
        // Naval gun: set target only
        WorldObject *target = g_app->GetWorld()->GetWorldObject( targetObjectId );
        if( target && target->m_visible[m_teamId] &&
            target->IsNavy() && ( !target->IsSubmarine() || !target->IsHiddenFrom() ) &&
            g_app->GetWorld()->GetAttackOdds( m_type, target->m_type, m_objectId ) > 0 )
        {
            m_targetObjectId = targetObjectId;
        }
        return;
    }
}

void BattleShip::AcquireTargetFromAction( ActionOrder *action )
{
    if( action->m_targetObjectId == -1 ) return;
    if( m_currentState == 0 )
    {
        WorldObject *target = g_app->GetWorld()->GetWorldObject( action->m_targetObjectId );
        if( target && target->m_visible[m_teamId] &&
            ( target->IsAircraft() || target->IsBallisticMissileClass() ) &&
            g_app->GetWorld()->GetAttackOdds( m_type, target->m_type, m_objectId ) > 0 )
        {
            m_targetObjectId = action->m_targetObjectId;
        }
    }
    else if( m_currentState == 2 )
    {
        WorldObject *target = g_app->GetWorld()->GetWorldObject( action->m_targetObjectId );
        if( target && ( target->m_visible[m_teamId] || target->m_seen[m_teamId] ) &&
            target->IsSubmarine() &&
            g_app->GetWorld()->GetAttackOdds( m_type, target->m_type, m_objectId ) > 0 )
        {
            m_targetObjectId = action->m_targetObjectId;
        }
    }
    else if( m_currentState == 3 )
    {
        WorldObject *target = g_app->GetWorld()->GetWorldObject( action->m_targetObjectId );
        if( target && target->m_visible[m_teamId] &&
            target->IsNavy() && ( !target->IsSubmarine() || !target->IsHiddenFrom() ) &&
            g_app->GetWorld()->GetAttackOdds( m_type, target->m_type, m_objectId ) > 0 )
        {
            m_targetObjectId = action->m_targetObjectId;
        }
    }
}

bool BattleShip::Update()
{
    AppDebugAssert( m_type == WorldObject::TypeBattleShip );
    MoveToWaypoint();

    Fleet *fleet = g_app->GetWorld()->GetTeam( m_teamId )->GetFleet( m_fleetId );
    if( fleet )
    {
        if( m_currentState == 0 )
        {
            // Air defense: auto-acquire and fire at air/missile targets
            if( m_stateTimer <= 0 && m_retargetTimer <= 0 )
            {
                m_retargetTimer = 10;
                AirDefense();
            }
            if( m_targetObjectId != -1 )
            {
                WorldObject *targetObject = g_app->GetWorld()->GetWorldObject( m_targetObjectId );
                if( targetObject && targetObject->m_visible[m_teamId] &&
                    g_app->GetWorld()->GetAttackOdds( m_type, targetObject->m_type, m_objectId ) > 0 &&
                    g_app->GetWorld()->GetDistance( m_longitude, m_latitude, targetObject->m_longitude, targetObject->m_latitude ) <= GetActionRange() )
                {
                    if( m_stateTimer <= 0 )
                    {
                        FireGun( GetActionRange() );
                        m_stateTimer = m_states[0]->m_timeToReload;
                        if( BurstFireOnFired( m_targetObjectId ) )
                        {
                            LList<int> excluded;
                            BurstFireGetExcludedIds( excluded );
                            int newId = GetTarget( GetActionRange(), &excluded );
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
                }
                else
                {
                    m_targetObjectId = -1;
                    BurstFireResetShotCount();
                }
            }
        }
        else if( m_currentState == 1 )
        {
            // LACM: queue processed by base Update()
        }
        else if( m_currentState == 2 )
        {
            // Anti-sub: Ping when reload ready; fire depth charge at target in range
            bool attackPossible = false;
            if( m_stateTimer <= 0 )
            {
                Ping();
                m_stateTimer = m_states[2]->m_timeToReload;
                attackPossible = true;
            }
            if( attackPossible && fleet->m_currentState != Fleet::FleetStatePassive && m_targetObjectId != -1 )
            {
                WorldObject *targetObject = g_app->GetWorld()->GetWorldObject( m_targetObjectId );
                if( targetObject && ( targetObject->m_visible[m_teamId] || targetObject->m_seen[m_teamId] ) )
                {
                    Fixed distanceSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, targetObject->m_longitude, targetObject->m_latitude );
                    if( distanceSqd <= GetActionRangeSqd() )
                    {
                        FireGun( GetActionRange() );
                        if( BurstFireOnFired( m_targetObjectId ) )
                        {
                            LList<int> excluded;
                            BurstFireGetExcludedIds( excluded );
                            int newId = GetTarget( GetActionRange(), &excluded );
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
                }
                else
                {
                    m_targetObjectId = -1;
                    BurstFireResetShotCount();
                }
            }
        }
        else if( m_currentState == 3 )
        {
            // Naval gun: retarget and fire at surface ships
            bool hasTarget = false;
            if( m_targetObjectId != -1 )
            {
                WorldObject *targetObject = g_app->GetWorld()->GetWorldObject( m_targetObjectId );
                if( targetObject && targetObject->m_visible[m_teamId] &&
                    targetObject->IsNavy() && ( !targetObject->IsSubmarine() || !targetObject->IsHiddenFrom() ) )
                {
                    Fixed distance = g_app->GetWorld()->GetDistance( m_longitude, m_latitude, targetObject->m_longitude, targetObject->m_latitude );
                    if( distance <= GetActionRange() )
                    {
                        hasTarget = true;
                        if( m_stateTimer <= 0 )
                        {
                            FireGun( GetActionRange() );
                            m_stateTimer = m_states[3]->m_timeToReload;
                            fleet->FleetAction( m_targetObjectId );
                            if( BurstFireOnFired( m_targetObjectId ) )
                            {
                                LList<int> excluded;
                                BurstFireGetExcludedIds( excluded );
                                int newId = GetTarget( 10, &excluded );
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
                    }
                }
            }
            if( !hasTarget && m_retargetTimer <= 0 )
            {
                m_targetObjectId = -1;
                BurstFireResetShotCount();
                if( g_app->GetWorld()->GetDefcon() < 4 )
                {
                    m_retargetTimer = 10;
                    LList<int> excluded;
                    BurstFireGetExcludedIds( excluded );
                    int tid = GetTarget( 10, &excluded );
                    if( tid != -1 )
                    {
                        m_targetObjectId = tid;
                    }
                }
            }
        }
    }

    return MovingObject::Update();
}

void BattleShip::Render2D()
{
    MovingObject::Render2D();
}

void BattleShip::Render3D()
{
    MovingObject::Render3D();
}

void BattleShip::RunAI()
{
    if( m_aiTimer <= 0 )
    {
        m_aiTimer = m_aiSpeed;
        Team *team = g_app->GetWorld()->GetTeam( m_teamId );
        Fleet *fleet = team->GetFleet( m_fleetId );
        if( !fleet ) return;

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

        // LACM ammo exhausted: switch to naval gun
        if( m_states.Size() > 1 && m_currentState == 1 &&
            m_states[1]->m_numTimesPermitted - m_actionQueue.Size() <= 0 )
        {
            SetState(3);
        }

        // LACM allowed at Defcon 3 - submarine-style launch logic
        if( g_app->GetWorld()->GetDefcon() <= 3 &&
            m_states.Size() > 1 )
        {
            Fixed maxRange = 40;
            Fixed maxRangeSqd = 40 * 40;
            bool haveLacmAmmo = ( m_states[1]->m_numTimesPermitted - m_actionQueue.Size() > 0 );

            // In air defense with no air target but enemy ships in range: switch to LACM and launch
            if( m_currentState == 0 && m_targetObjectId == -1 && haveLacmAmmo )
            {
                WorldObject *nearestShip = nullptr;
                Fixed nearestSqd = Fixed::MAX;
                for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
                {
                    if( !g_app->GetWorld()->m_objects.ValidIndex(i) ) continue;
                    WorldObject *obj = g_app->GetWorld()->m_objects[i];
                    if( g_app->GetWorld()->IsFriend( obj->m_teamId, m_teamId ) ) continue;
                    if( team->m_ceaseFire[obj->m_teamId] ) continue;
                    if( !obj->m_seen[m_teamId] ) continue;
                    bool isShipTarget = obj->IsNavy() && ( !obj->IsSubmarine() || !obj->IsHiddenFrom() );
                    if( !isShipTarget ) continue;
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
                    SetState(1);
                    ActionOrder *action = new ActionOrder();
                    action->m_longitude = nearestShip->m_longitude;
                    action->m_latitude = nearestShip->m_latitude;
                    action->m_targetObjectId = nearestShip->m_objectId;
                    RequestAction( action );
                    return;
                }
            }

            if( fleet->IsInNukeRange() )
            {
                if( team->m_maxTargetsSeen >= 4 ||
                    ( team->m_currentState >= Team::StateAssault && m_stateTimer == 0 ) )
                {
                    if( !haveLacmAmmo )
                        return;
                    int targetsInRange = 0;
                    for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
                    {
                        if( g_app->GetWorld()->m_objects.ValidIndex(i) )
                        {
                            WorldObject *obj = g_app->GetWorld()->m_objects[i];
                            if( !obj->IsMovingObject() &&
                                obj->m_seen[m_teamId] &&
                                !g_app->GetWorld()->IsFriend( obj->m_teamId, m_teamId ) &&
                                g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, obj->m_longitude, obj->m_latitude ) < maxRangeSqd )
                            {
                                targetsInRange++;
                            }
                        }
                    }

                    Fixed longitude = 0;
                    Fixed latitude = 0;
                    int objectId = -1;
                    int targetTeam = team->m_targetTeam;
                    if( g_app->GetGame()->m_victoryTimer > 0 )
                    {
                        for( int i = 0; i < World::NumTerritories; ++i )
                        {
                            int owner = g_app->GetWorld()->GetTerritoryOwner(i);
                            if( owner != -1 &&
                                !g_app->GetWorld()->IsFriend( owner, m_teamId ) )
                            {
                                Fixed popCenterLong = g_app->GetWorld()->m_populationCenter[i].x;
                                Fixed popCenterLat = g_app->GetWorld()->m_populationCenter[i].y;
                                if( g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, popCenterLong, popCenterLat ) < maxRangeSqd )
                                {
                                    targetTeam = g_app->GetWorld()->GetTerritoryOwner(i);
                                    break;
                                }
                            }
                        }
                    }

                    LACM::FindTarget( m_teamId, targetTeam, m_objectId, maxRange, &longitude, &latitude, &objectId );
                    if( ( longitude != 0 && latitude != 0 ) &&
                        ( targetsInRange >= 4 || ( team->m_currentState >= Team::StateAssault ) ) )
                    {
                        if( haveLacmAmmo )
                        {
                            if( m_currentState != 1 )
                                SetState(1);
                            ActionOrder *action = new ActionOrder();
                            action->m_longitude = longitude;
                            action->m_latitude = latitude;
                            action->m_targetObjectId = objectId;
                            RequestAction( action );
                        }
                    }
                    else if( m_actionQueue.Size() == 0 )
                    {
                        SetState(0);
                    }
                }
            }
            else if( m_actionQueue.Size() == 0 )
            {
                if( m_currentState != 0 )
                    SetState(0);
            }

            // Multiple battleships: one should be anti-sub if fleet is not already firing guns/missiles
            if( fleet->IsInFleetClass( WorldObject::ClassTypeBattleShip ) )
            {
                int battleshipCount = 0;
                int designatedAntiSubId = -1;
                bool anyBattleshipFiringOrLaunching = false;
                for( int i = 0; i < fleet->m_fleetMembers.Size(); ++i )
                {
                    if( !fleet->m_fleetMembers.ValidIndex(i) ) continue;
                    WorldObject *obj = g_app->GetWorld()->GetWorldObject( fleet->m_fleetMembers[i] );
                    if( !obj || !obj->IsBattleShipClass() ) continue;
                    battleshipCount++;
                    if( ( obj->m_currentState == 1 && obj->m_actionQueue.Size() > 0 ) ||
                        ( obj->m_currentState == 2 && obj->m_targetObjectId != -1 ) ||
                        ( obj->m_currentState == 3 && obj->m_targetObjectId != -1 ) )
                        anyBattleshipFiringOrLaunching = true;
                    if( designatedAntiSubId == -1 || obj->m_objectId < designatedAntiSubId )
                        designatedAntiSubId = obj->m_objectId;
                }
                if( battleshipCount >= 2 && !anyBattleshipFiringOrLaunching && designatedAntiSubId != -1 &&
                    m_objectId == designatedAntiSubId && m_currentState == 0 )
                {
                    SetState(2);
                }
            }
        }
    }
}

int BattleShip::GetAttackState()
{
    return m_currentState;
}

bool BattleShip::UsingGuns()
{
    return ( m_currentState == 0 || m_currentState == 2 || m_currentState == 3 );
}

void BattleShip::FireGun( Fixed range )
{
    WorldObject *targetObject = g_app->GetWorld()->GetWorldObject( m_targetObjectId );
    AppAssert( targetObject );
    if( m_currentState == 2 )
    {
        DepthCharge *bullet = new DepthCharge( range );
        bullet->SetPosition( m_longitude, m_latitude );
        bullet->SetTargetObjectId( targetObject->m_objectId );
        bullet->SetWaypoint( targetObject->m_longitude, targetObject->m_latitude );
        bullet->SetTeamId( m_teamId );
        bullet->m_origin = m_objectId;
        bullet->m_distanceToTarget = g_app->GetWorld()->GetDistance( m_longitude, m_latitude, targetObject->m_longitude, targetObject->m_latitude );
        bullet->m_attackOdds = g_app->GetWorld()->GetAttackOdds( m_type, targetObject->m_type, m_objectId );
        (void)g_app->GetWorld()->m_gunfire.PutData( bullet );
    }
    else
    {
        GunFire *bullet = new GunFire( range );
        bullet->SetPosition( m_longitude, m_latitude );
        bullet->SetTargetObjectId( targetObject->m_objectId );
        bullet->SetTeamId( m_teamId );
        bullet->m_origin = m_objectId;
        Fixed interceptLongitude, interceptLatitude;
        bullet->GetCombatInterceptionPoint( targetObject, &interceptLongitude, &interceptLatitude );
        bullet->SetWaypoint( interceptLongitude, interceptLatitude );
        bullet->SetInitialVelocityTowardWaypoint();
        bullet->m_distanceToTarget = g_app->GetWorld()->GetDistance( m_longitude, m_latitude, interceptLongitude, interceptLatitude );
        bullet->m_attackOdds = g_app->GetWorld()->GetAttackOdds( m_type, targetObject->m_type, m_objectId );
        (void)g_app->GetWorld()->m_gunfire.PutData( bullet );
    }
}

bool BattleShip::IsPinging()
{
    return ( m_currentState == 2 );
}

void BattleShip::AirDefense()
{
    if( g_app->GetWorld()->GetDefcon() > 3 ) return;
    Fixed actionRangeSqd = GetActionRangeSqd();
    WorldObject *best = NULL;
    LList<int> excluded;
    BurstFireGetExcludedIds( excluded );

    for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
    {
        if( !g_app->GetWorld()->m_objects.ValidIndex(i) ) continue;
        WorldObject *obj = g_app->GetWorld()->m_objects[i];
        if( !obj->IsAircraft() && !obj->IsBallisticMissileClass() ) continue;
        if( g_app->GetWorld()->IsFriend( obj->m_teamId, m_teamId ) ) continue;
        if( excluded.Size() > 0 )
        {
            bool excludedTarget = false;
            for( int e = 0; e < excluded.Size(); ++e )
                if( excluded.GetData( e ) == obj->m_objectId ) { excludedTarget = true; break; }
            if( excludedTarget ) continue;
        }
        Fixed distSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, obj->m_longitude, obj->m_latitude );
        if( distSqd > actionRangeSqd ) continue;
        if( g_app->GetWorld()->GetAttackOdds( m_type, obj->m_type, m_objectId ) <= 0 ) continue;
        if( !best || GetAttackPriority( best->m_type ) > GetAttackPriority( obj->m_type ) )
            best = obj;
    }
    if( best )
        Action( best->m_objectId, -1, -1 );
}

int BattleShip::GetTarget( Fixed range, const LList<int> *excludeIds )
{
    Team *team = g_app->GetWorld()->GetTeam( m_teamId );
    WorldObject *currentTarget = NULL;
    Fixed rangeSqd = range * range;

    for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
    {
        if( !g_app->GetWorld()->m_objects.ValidIndex(i) ) continue;
        WorldObject *obj = g_app->GetWorld()->m_objects[i];
        if( obj->m_teamId == TEAMID_SPECIALOBJECTS ) continue;
        if( g_app->GetWorld()->IsFriend( m_teamId, obj->m_teamId ) ) continue;
        if( team->m_ceaseFire[ obj->m_teamId ] ) continue;
        if( !obj->m_visible[m_teamId] ) continue;
        if( g_app->GetWorld()->GetAttackOdds( m_type, obj->m_type, m_objectId ) <= 0 ) continue;
        if( excludeIds )
        {
            bool excluded = false;
            for( int e = 0; e < excludeIds->Size(); ++e )
                if( excludeIds->GetData( e ) == obj->m_objectId ) { excluded = true; break; }
            if( excluded ) continue;
        }
        if( g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, obj->m_longitude, obj->m_latitude ) >= rangeSqd ) continue;

        if( m_currentState == 0 )
        {
            if( !obj->IsAircraft() && !obj->IsBallisticMissileClass() ) continue;
        }
        else if( m_currentState == 2 )
        {
            if( !obj->IsSubmarine() ) continue;
        }
        else if( m_currentState == 3 )
        {
            if( !obj->IsNavy() || ( obj->IsSubmarine() && obj->IsHiddenFrom() ) ) continue;
        }
        else
        {
            continue;
        }

        bool validNewTarget = !currentTarget ||
            GetAttackPriority( currentTarget->m_type ) > GetAttackPriority( obj->m_type );
        if( validNewTarget )
        {
            currentTarget = obj;
        }
    }
    return currentTarget ? currentTarget->m_objectId : -1;
}

int BattleShip::GetBurstFireShots() const
{
    return ( m_currentState == 2 ) ? 3 : 5;
}

void BattleShip::FleetAction( int targetObjectId )
{
    if( m_currentState == 1 ) return;
    if( m_targetObjectId != -1 ) return;
    WorldObject *obj = g_app->GetWorld()->GetWorldObject( targetObjectId );
    if( !obj || !obj->m_visible[ m_teamId ] ) return;
    if( g_app->GetWorld()->GetAttackOdds( m_type, obj->m_type, m_objectId ) <= 0 ) return;
    if( m_currentState == 2 && !obj->IsSubmarine() ) return;
    if( m_currentState == 3 && ( !obj->IsNavy() || ( obj->IsSubmarine() && obj->IsHiddenFrom() ) ) ) return;
    m_targetObjectId = targetObjectId;
}

int BattleShip::GetAttackPriority( int _type )
{
    if( m_currentState == 0 )
    {
        // Air defense: nukes first, then LACM, then aircraft
        if( _type == WorldObject::TypeNuke ) return 1;
        if( _type == WorldObject::TypeLACM ) return 2;
        if( _type == WorldObject::TypeBomber ) return 3;
        if( _type == WorldObject::TypeFighter ) return 4;
        return 6;
    }
    if( m_currentState == 2 )
    {
        return ( WorldObject::GetClassTypeForType( _type ) == WorldObject::ClassTypeSub ) ? 1 : 6;
    }
    if( m_currentState == 3 )
    {
        if( WorldObject::GetClassTypeForType( _type ) == WorldObject::ClassTypeSub ) return 1;
        if( WorldObject::GetClassTypeForType( _type ) == WorldObject::ClassTypeCarrier ) return 2;
        if( WorldObject::GetClassTypeForType( _type ) == WorldObject::ClassTypeBattleShip ) return 3;
        return 6;
    }
    return 6;
}

int BattleShip::GetAttackOdds( int _defenderType )
{
    WorldObject::Archetype defArchetype = WorldObject::GetArchetypeForType( _defenderType );
    WorldObject::ClassType defClass = WorldObject::GetClassTypeForType( _defenderType );

    if( m_currentState == 0 )
    {
        if( defArchetype != WorldObject::ArchetypeAircraft && defArchetype != WorldObject::ArchetypeBallisticMissile )
            return 0;
    }
    else if( m_currentState == 1 )
    {
        if( defArchetype == WorldObject::ArchetypeAircraft || defArchetype == WorldObject::ArchetypeBallisticMissile )
            return 0;
        if( defArchetype == WorldObject::ArchetypeBuilding || defArchetype == WorldObject::ArchetypeNavy )
            return g_app->GetWorld()->GetAttackOdds( WorldObject::TypeLACM, _defenderType );
        return 0;
    }
    else if( m_currentState == 2 )
    {
        if( defClass != WorldObject::ClassTypeSub ) return 0;
    }
    else if( m_currentState == 3 )
    {
        if( defArchetype != WorldObject::ArchetypeNavy ) return 0;
    }
    return g_app->GetWorld()->GetAttackOdds( m_type, _defenderType );
}

bool BattleShip::IsActionQueueable()
{
    return ( m_currentState == 1 );
}

void BattleShip::RequestAction( ActionOrder *_action )
{
    if( m_currentState == 1 )
    {
        if( _action->m_targetObjectId != -1 )
        {
            WorldObject *target = g_app->GetWorld()->GetWorldObject( _action->m_targetObjectId );
            if( target && ( target->IsAircraft() || target->IsBallisticMissileClass() ) )
            {
                delete _action;
                return;
            }
        }
        if( g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, _action->m_longitude, _action->m_latitude ) < GetActionRangeSqd() )
        {
            WorldObject::RequestAction( _action );
        }
        else
        {
            delete _action;
        }
    }
    else
    {
        MovingObject::RequestAction( _action );
    }
}

int BattleShip::IsValidCombatTarget( int _objectId )
{
    WorldObject *obj = g_app->GetWorld()->GetWorldObject( _objectId );
    if( !obj ) return WorldObject::TargetTypeInvalid;
    int basicResult = MovingObject::IsValidCombatTarget( _objectId );
    if( basicResult < WorldObject::TargetTypeInvalid ) return basicResult;

    if( m_currentState == 0 )
    {
        if( !obj->IsAircraft() && !obj->IsBallisticMissileClass() ) return WorldObject::TargetTypeInvalid;
        Fixed distanceSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, obj->m_longitude, obj->m_latitude );
        if( distanceSqd > GetActionRangeSqd() ) return WorldObject::TargetTypeOutOfRange;
        if( g_app->GetWorld()->GetAttackOdds( m_type, obj->m_type, m_objectId ) <= 0 ) return WorldObject::TargetTypeInvalid;
        return WorldObject::TargetTypeValid;
    }
    if( m_currentState == 1 )
    {
        if( obj->IsAircraft() || obj->IsBallisticMissileClass() ) return WorldObject::TargetTypeInvalid;
        Fixed distanceSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, obj->m_longitude, obj->m_latitude );
        if( distanceSqd >= GetActionRangeSqd() ) return WorldObject::TargetTypeOutOfRange;
        if( obj->IsNavy() && ( !obj->IsSubmarine() || !obj->IsHiddenFrom() ) )
            return WorldObject::TargetTypeValid;
        if( !obj->IsMovingObject() )
            return WorldObject::TargetTypeLaunchLACM;
        return WorldObject::TargetTypeInvalid;
    }
    if( m_currentState == 2 )
    {
        if( !obj->IsSubmarine() ) return WorldObject::TargetTypeInvalid;
        Fixed distanceSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, obj->m_longitude, obj->m_latitude );
        if( distanceSqd > GetActionRangeSqd() ) return WorldObject::TargetTypeOutOfRange;
        return WorldObject::TargetTypeValid;
    }
    if( m_currentState == 3 )
    {
        if( !obj->IsNavy() || ( obj->IsSubmarine() && obj->IsHiddenFrom() ) ) return WorldObject::TargetTypeInvalid;
        Fixed distanceSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, obj->m_longitude, obj->m_latitude );
        if( distanceSqd > GetActionRangeSqd() ) return WorldObject::TargetTypeOutOfRange;
        if( g_app->GetWorld()->GetAttackOdds( m_type, obj->m_type, m_objectId ) <= 0 ) return WorldObject::TargetTypeInvalid;
        return WorldObject::TargetTypeValid;
    }
    return WorldObject::TargetTypeInvalid;
}


int BattleShip::IsValidMovementTarget( Fixed longitude, Fixed latitude )
{
    if( m_currentState == 1 )
    {
        bool haveLacmAmmo = ( m_states[1]->m_numTimesPermitted - m_actionQueue.Size() > 0 );
        if( haveLacmAmmo && latitude <= 100 && latitude >= -100 )
        {
            Fixed distanceSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, longitude, latitude );
            if( distanceSqd <= GetActionRangeSqd() )
            {
                return WorldObject::TargetTypeLaunchLACM;
            }
        }
    }
    return MovingObject::IsValidMovementTarget( longitude, latitude );
}
