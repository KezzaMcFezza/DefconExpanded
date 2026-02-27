#include "lib/universal_include.h"

#include "lib/resource/resource.h"
#include "lib/math/random_number.h"
#include "lib/language_table.h"
#include "lib/render3d/renderer_3d.h"
#include "lib/render2d/renderer_2d.h"
#include "lib/preferences.h"

#include "app/app.h"
#include "app/globals.h"
#include "app/game.h"

#include "network/network_defines.h"

#include "renderer/map_renderer.h"
#include "renderer/globe_renderer.h"

#include "world/world.h"
#include "world/airbase.h"
#include "world/aircraft_loads.h"
#include "world/team.h"
#include "world/bomber.h"
#include "world/bomber_fast.h"
#include "world/bomber_stealth.h"
#include "world/fighter.h"
#include "world/fighter_light.h"
#include "world/fighter_stealth.h"
#include "world/fighter_navystealth.h"
#include "world/nuke.h"
#include "world/tanker.h"



AirBase::AirBase()
:   WorldObject(),
    m_fighterRegenTimer(1024)
{
    SetType( TypeAirBase );
    
    strcpy( bmpImageFilename, "graphics/airbase.bmp" );

    m_radarRange = 20;
    m_selectable = false;  

    m_life = 15;
    m_nukeSupply = -1;

    m_maxFighters = 10;
    m_maxBombers = 10;
    m_maxAEW = 4;

    AddState( LANGUAGEPHRASE("state_fighter_lightlaunch"), 60, 30, 10, 25, true, 2, 3 );   // 0: FighterLight CAP
    AddState( LANGUAGEPHRASE("state_strikefighter_lightlaunch"), 60, 30, 10, 25, true, 2, 3 );  // 1: FighterLight Strike
    AddState( LANGUAGEPHRASE("state_navyfighterlaunch"), 60, 30, 10, 45, true, 3, 3 );   // 2: Fighter CAP
    AddState( LANGUAGEPHRASE("state_navystrikefighterlaunch"), 60, 30, 10, 45, true, 3, 3 );  // 3: Fighter Strike
    AddState( LANGUAGEPHRASE("state_navystealthfighterlaunch"), 60, 30, 10, 45, true, 0, 3 );   // 4: NavyStealth CAP
    AddState( LANGUAGEPHRASE("state_navystealthstrikefighterlaunch"), 60, 30, 10, 45, true, 0, 3 );   // 5: NavyStealth Strike
    AddState( LANGUAGEPHRASE("state_stealthfighterlaunch"), 60, 30, 10, 45, true, 0, 3 );   // 6: Stealth Fighter CAP
    AddState( LANGUAGEPHRASE("state_stealthstrikefighterlaunch"), 60, 30, 10, 45, true, 0, 3 );  // 7: Stealth Fighter Strike
    AddState( LANGUAGEPHRASE("state_bomberlaunch"), 60, 30, 10, 140, true, 5, 3 );   // 8: Bomber NUKE
    AddState( LANGUAGEPHRASE("state_cruisebomberlaunch"), 60, 30, 10, 140, true, 5, 3 );  // 9: Bomber ALCM
    AddState( LANGUAGEPHRASE("state_bomber_fastlaunch"), 60, 30, 10, 90, true, 0, 3 );   // 10: BomberFast NUKE
    AddState( LANGUAGEPHRASE("state_cruisebomber_fastlaunch"), 60, 30, 10, 90, true, 0, 3 );  // 11: BomberFast ALCM
    AddState( LANGUAGEPHRASE("state_stealthbomberlaunch"), 60, 30, 10, 140, true, 0, 3 );   // 12: Stealth Bomber NUKE
    AddState( LANGUAGEPHRASE("state_stealthcruisebomberlaunch"), 60, 30, 10, 140, true, 0, 3 );  // 13: Stealth Bomber ALCM
    AddState( LANGUAGEPHRASE("state_aewlaunch"), 60, 30, 10, 140, true, 2, 3 );   // 14: AEW
    AddState( LANGUAGEPHRASE("state_tankerlaunch"), 60, 30, 10, 140, true, 0, 3 );  // 15: Tanker

    m_states[1]->m_numTimesPermitted = m_states[0]->m_numTimesPermitted;
    m_states[3]->m_numTimesPermitted = m_states[2]->m_numTimesPermitted;
    m_states[5]->m_numTimesPermitted = m_states[4]->m_numTimesPermitted;
    m_states[7]->m_numTimesPermitted = m_states[6]->m_numTimesPermitted;
    m_states[9]->m_numTimesPermitted = m_states[8]->m_numTimesPermitted;
    m_states[11]->m_numTimesPermitted = m_states[10]->m_numTimesPermitted;
    m_states[13]->m_numTimesPermitted = m_states[12]->m_numTimesPermitted;

    InitialiseTimers();
}

void AirBase::SetTeamId( int teamId )
{
    WorldObject::SetTeamId( teamId );
    if ( teamId >= 0 )
    {
        Team *team = g_app->GetWorld()->GetTeam( teamId );
        if ( team && team->m_territories.Size() > 0 )
            ApplyTerritoryAircraftLoads( this, team->m_territories[0] );
    }

    for( int i = 0; i < m_states.Size(); ++i )
    {
        if( m_states[i]->m_numTimesPermitted > 0 )
        {
            m_currentState = i;
            m_stateTimer = m_states[i]->m_timeToPrepare;
            break;
        }
    }
}

void AirBase::RequestAction(ActionOrder *_action)
{
    // Accept all launch orders regardless of range. Range = fuel indicator for aircraft.
    WorldObject::RequestAction(_action);
}


void AirBase::Action( int targetObjectId, Fixed longitude, Fixed latitude )
{
    if( !CheckCurrentState() )
    {
        return;
    }

    WorldObject *obj = g_app->GetWorld()->GetWorldObject( targetObjectId );
    if( obj )
    {
        longitude = obj->m_longitude;
        latitude = obj->m_latitude;
    }

    if( m_stateTimer <= 0 )
    {
        if( m_currentState == 0 ) { if(!LaunchFighterLight( targetObjectId, longitude, latitude, 0 )) return; }
        else if( m_currentState == 1 ) { if(!LaunchFighterLight( targetObjectId, longitude, latitude, 1 )) return; }
        else if( m_currentState == 2 ) { if(!LaunchFighter( targetObjectId, longitude, latitude, 1 )) return; }
        else if( m_currentState == 3 ) { if(!LaunchFighter( targetObjectId, longitude, latitude, 2 )) return; }
        else if( m_currentState == 4 ) { if(!LaunchNavyStealthFighter( targetObjectId, longitude, latitude, 0 )) return; }
        else if( m_currentState == 5 ) { if(!LaunchNavyStealthFighter( targetObjectId, longitude, latitude, 1 )) return; }
        else if( m_currentState == 6 ) { if(!LaunchStealthFighter( targetObjectId, longitude, latitude, 0 )) return; }
        else if( m_currentState == 7 ) { if(!LaunchStealthFighter( targetObjectId, longitude, latitude, 1 )) return; }
        else if( m_currentState == 8 ) { if(!LaunchBomber( targetObjectId, longitude, latitude, 1 )) return; }
        else if( m_currentState == 9 ) { if(!LaunchBomber( targetObjectId, longitude, latitude, 2 )) return; }
        else if( m_currentState == 10 ) { if(!LaunchBomberFast( targetObjectId, longitude, latitude, 1 )) return; }
        else if( m_currentState == 11 ) { if(!LaunchBomberFast( targetObjectId, longitude, latitude, 2 )) return; }
        else if( m_currentState == 12 ) { if(!LaunchStealthBomber( targetObjectId, longitude, latitude, 1 )) return; }
        else if( m_currentState == 13 ) { if(!LaunchStealthBomber( targetObjectId, longitude, latitude, 2 )) return; }
        else if( m_currentState == 14 ) { if(!LaunchAEW( targetObjectId, longitude, latitude )) return; }
        else if( m_currentState == 15 ) { if(!LaunchTanker( targetObjectId, longitude, latitude )) return; }
        WorldObject::Action( targetObjectId, longitude, latitude );
        if( m_currentState <= 7 && ( m_currentState & 1 ) == 0 )
            m_states[m_currentState+1]->m_numTimesPermitted = m_states[m_currentState]->m_numTimesPermitted;
        else if( m_currentState >= 8 && m_currentState <= 13 && ( m_currentState & 1 ) == 0 )
            m_states[m_currentState+1]->m_numTimesPermitted = m_states[m_currentState]->m_numTimesPermitted;
        else if( m_currentState <= 7 && ( m_currentState & 1 ) == 1 )
            m_states[m_currentState-1]->m_numTimesPermitted = m_states[m_currentState]->m_numTimesPermitted;
        else if( m_currentState >= 8 && m_currentState <= 13 && ( m_currentState & 1 ) == 1 )
            m_states[m_currentState-1]->m_numTimesPermitted = m_states[m_currentState]->m_numTimesPermitted;
    }
}

void AirBase::RunAI()
{    
    if( m_aiTimer <= 0 )
    {
        int trackSyncRand = g_preferences->GetInt( PREFS_NETWORKTRACKSYNCRAND );

        m_aiTimer = m_aiSpeed;
        if(m_states[m_currentState]->m_numTimesPermitted - m_actionQueue.Size() <= 0 )
        {
            return;
        }

        if( g_app->GetWorld()->GetDefcon() < 3 )
        {
            Fixed aggressionDistance = 20;
            aggressionDistance += 5 * g_app->GetWorld()->GetTeam(m_teamId)->m_aggression;
            if( aggressionDistance > 45 )
            {
                aggressionDistance = 45;
            }
            
            Team *team = g_app->GetWorld()->GetTeam( m_teamId );
            if( g_app->GetWorld()->GetDefcon() > 1 &&
                GetFighterCount() > 2 &&
                team->m_currentState == Team::StateScouting)
            {
                if( m_currentState > 7 )
                {
                    SetState( ( m_states[0]->m_numTimesPermitted > 0 ) ? 0 : ( ( m_states[2]->m_numTimesPermitted > 0 ) ? 2 : ( ( m_states[4]->m_numTimesPermitted > 0 ) ? 4 : 6 ) ) );
                }
                for( int i = 0; i < World::NumTerritories; ++i )
                {
                    int owner = g_app->GetWorld()->GetTerritoryOwner(i);
                    if( !g_app->GetWorld()->IsFriend(m_teamId, owner) )
                    {
                        Fixed distSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, 
                                                                           g_app->GetWorld()->m_populationCenter[i].x, 
                                                                           g_app->GetWorld()->m_populationCenter[i].y );
                        if( distSqd < 50 * 50 )
                        {
                            Fixed longitude = syncsfrand(40);
                            Fixed latitude  = syncsfrand(40);

                            ActionOrder *action = new ActionOrder;
                            action->m_longitude = g_app->GetWorld()->m_populationCenter[i].x + longitude;
                            action->m_latitude = g_app->GetWorld()->m_populationCenter[i].y + latitude;
                            RequestAction( action );
                            break;
                        }
                    }
                }
            }
            else
            {
                bool eventFound = false;

                if( trackSyncRand )
                {
                    SyncRandLog( "Airbase eventlog size = %d", team->m_eventLog.Size() );
                    for( int i = 0; i < team->m_eventLog.Size(); ++i )
                    {
                        Event *event = team->m_eventLog[i];
                        SyncRandLog( "Airbase event %d : type[%d] objId[%d] action[%d] team[%d] fleet[%d] long[%2.2f] lat[%2.2f]",
                                        i, 
                                        event->m_type,
                                        event->m_objectId,
                                        event->m_actionTaken,
                                        event->m_teamId,
                                        event->m_fleetId,
                                        event->m_longitude.DoubleValue(),
                                        event->m_latitude.DoubleValue() );
                    }
                }

                for( int i = 0; i < team->m_eventLog.Size(); ++i )
                {
                    Event *event = team->m_eventLog[i];
                    if( event &&
                        (event->m_type == Event::TypeIncomingAircraft ||
                         event->m_type == Event::TypeEnemyIncursion ||
                         event->m_type == Event::TypeSubDetected ) )
                    {
                        WorldObject *obj = g_app->GetWorld()->GetWorldObject( event->m_objectId );
                        if( obj &&
                            !g_app->GetWorld()->IsFriend( m_teamId, obj->m_teamId ) &&                            
                            team->CountTargettedUnits( obj->m_objectId ) < 3 &&
                            g_app->GetWorld()->GetDistance( m_longitude, m_latitude, obj->m_longitude, obj->m_latitude ) <= aggressionDistance )
                        {
                            if( obj->IsAircraft() )
                            {
                                if( GetFighterCount() > 0 )
                                {
                                    if( m_currentState != 0 )
                                    {
                                        SetState(0);
                                    }
                                }
                                else
                                {
                                    continue;
                                }
                            }
                            else if( obj->IsSubmarine() )
                            {
                                if( GetBomberCount() > 0 )
                                {
                                    SetState(8);
                                }
                            }
                            else
                            {
                                continue;
                            }
                            eventFound = true;

                            ActionOrder *action = new ActionOrder;
                            action->m_targetObjectId = obj->m_objectId;
                            action->m_longitude = obj->m_longitude;
                            action->m_latitude = obj->m_latitude;

                            RequestAction( action );
                            
                            if( trackSyncRand )
                            {
                                SyncRandLog( "Airbase event actioned to object %d [%s]", obj->m_objectId, WorldObject::GetName(obj->m_type) );
                            }

                            event->m_actionTaken++;
                            if( event->m_actionTaken >= 2 )
                            {
                                team->DeleteEvent(i);
                            }
                            break;
                        }                        
                    }                    
                }
                
                if( trackSyncRand )
                {
                    SyncRandLog( "Airbase EventFound = %d", (int)eventFound );
                }

                FastDArray<int> targetList;
                if( !eventFound )
                {
                    for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
                    {
                        if( g_app->GetWorld()->m_objects.ValidIndex(i) )
                        {
                            WorldObject *obj = g_app->GetWorld()->m_objects[i];
                            if( !g_app->GetWorld()->IsFriend( m_teamId, obj->m_teamId ) &&
                                obj->IsMovingObject() &&
                                obj->m_visible[m_teamId] &&
                                g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, obj->m_longitude, obj->m_latitude ) <= aggressionDistance * aggressionDistance )
                            {
                                (void)targetList.PutData( obj->m_objectId );
                            }
                        }
                    }
                }

                if( trackSyncRand )
                {
                    SyncRandLog( "Airbase Targetlist Size = %d", targetList.Size() );
                }

                if( targetList.Size() > 0 )
                {
                    if( trackSyncRand )
                    {
                        for( int i = 0; i < targetList.Size(); ++i )
                        {
                            int id = targetList[i];
                            WorldObject *wobj = g_app->GetWorld()->GetWorldObject(id);
                            if( wobj )
                            {
                                SyncRandLog( "Airbase target = %d [%s]", id, WorldObject::GetName(wobj->m_type) );
                            }
                            else
                            {
                                SyncRandLog( "Airbase target = %d [not found]", id );
                            }
                        }
                    }

                    int id = syncrand() % targetList.Size();

                    WorldObject *obj = g_app->GetWorld()->GetWorldObject( targetList[id] );
                    if( obj )
                    {                        
                        ActionOrder *action = new ActionOrder;
                        action->m_targetObjectId = obj->m_objectId;
                        action->m_longitude = obj->m_longitude;
                        action->m_latitude = obj->m_latitude;

                        RequestAction( action );
                    }
                }
                else
                {
                    if( g_app->GetWorld()->GetDefcon() == 1 )
                    {
                        if( team->m_subState >= Team::SubStateLaunchBombers ||
                            team->m_currentState == Team::StatePanic )
                        {
                            if( m_currentState != 8 && m_currentState != 9 )
                            {
                                SetState(8);
                            }
                            if( m_currentState == 8 || m_currentState == 9 )
                            {
                                // launch bombers!
                                Fixed longitude = 0;
                                Fixed latitude = 0;
                                int objectId = -1;

                                Nuke::FindTarget( m_teamId, team->m_targetTeam, m_objectId, 180, &longitude, &latitude, &objectId );
                                if( longitude != 0 && latitude != 0)
                                {
                                    ActionOrder *action = new ActionOrder();
                                    action->m_targetObjectId = objectId;
                                    action->m_longitude = longitude;
                                    action->m_latitude = latitude;
                                    RequestAction( action );
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}


bool AirBase::IsActionQueueable()
{
    return true;
}

bool AirBase::UsingNukes()
{
    if( m_currentState == 2 )
    {
        return true;
    }
    return false;
}

void AirBase::NukeStrike()
{
    float fighterLossFactor = 0.5f;
    float bomberLossFactor = 0.5f;
    for( int i = 0; i <= 7; ++i )
        m_states[i]->m_numTimesPermitted = (int)( m_states[i]->m_numTimesPermitted * fighterLossFactor );
    for( int i = 8; i <= 13; ++i )
        m_states[i]->m_numTimesPermitted = (int)( m_states[i]->m_numTimesPermitted * bomberLossFactor );
}

bool AirBase::Update()
{
    m_states[1]->m_numTimesPermitted = m_states[0]->m_numTimesPermitted;
    m_states[3]->m_numTimesPermitted = m_states[2]->m_numTimesPermitted;
    m_states[5]->m_numTimesPermitted = m_states[4]->m_numTimesPermitted;
    m_states[7]->m_numTimesPermitted = m_states[6]->m_numTimesPermitted;
    m_states[9]->m_numTimesPermitted = m_states[8]->m_numTimesPermitted;
    m_states[11]->m_numTimesPermitted = m_states[10]->m_numTimesPermitted;
    m_states[13]->m_numTimesPermitted = m_states[12]->m_numTimesPermitted;

    if( m_states[m_currentState]->m_numTimesPermitted == 0 )
    {
        for( int i = 0; i < m_states.Size(); ++i )
        {
            if( m_states[i]->m_numTimesPermitted > 0 )
            {
                SetState( i );
                break;
            }
        }
    }

    // Fighter regen disabled for now
    //if( GetFighterCount() < 5 )
    //{
    //    m_fighterRegenTimer -= SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor();
    //    if( m_fighterRegenTimer <= 0 )
    //    {
    //        if( m_states[2]->m_numTimesPermitted < m_maxFighters )
    //        {
    //            m_states[2]->m_numTimesPermitted++;
    //            m_states[3]->m_numTimesPermitted++;
    //        }
    //        m_fighterRegenTimer = 1024;
    //    }
    //}
    //else
    //{
    //    m_fighterRegenTimer = 1024;
    //}

    return WorldObject::Update();
}

bool AirBase::CanLaunchBomber()
{
    return ( m_currentState >= 8 && m_currentState <= 13 &&
             m_states[m_currentState]->m_numTimesPermitted > 0 );
}

bool AirBase::CanLaunchFighter()
{
    return ( m_currentState <= 7 && m_states[m_currentState]->m_numTimesPermitted > 0 );
}

bool AirBase::CanLaunchFighterLight() { return ( ( m_currentState == 0 || m_currentState == 1 ) && m_states[m_currentState]->m_numTimesPermitted > 0 ); }
bool AirBase::CanLaunchStealthFighter() { return ( ( m_currentState == 6 || m_currentState == 7 ) && m_states[m_currentState]->m_numTimesPermitted > 0 ); }
bool AirBase::CanLaunchNavyStealthFighter() { return ( ( m_currentState == 4 || m_currentState == 5 ) && m_states[m_currentState]->m_numTimesPermitted > 0 ); }
bool AirBase::CanLaunchBomberFast() { return ( ( m_currentState == 10 || m_currentState == 11 ) && m_states[m_currentState]->m_numTimesPermitted > 0 ); }
bool AirBase::CanLaunchStealthBomber() { return ( ( m_currentState == 12 || m_currentState == 13 ) && m_states[m_currentState]->m_numTimesPermitted > 0 ); }
bool AirBase::CanLaunchAEW() { return ( m_currentState == 14 && m_states[14]->m_numTimesPermitted > 0 ); }
bool AirBase::CanLaunchTanker() { return ( m_currentState == 15 && m_states[15]->m_numTimesPermitted > 0 ); }

int AirBase::GetFighterCount()
{
    return m_states[0]->m_numTimesPermitted + m_states[2]->m_numTimesPermitted + m_states[4]->m_numTimesPermitted + m_states[6]->m_numTimesPermitted;
}

int AirBase::GetBomberCount()
{
    return m_states[8]->m_numTimesPermitted + m_states[10]->m_numTimesPermitted + m_states[12]->m_numTimesPermitted;
}

int AirBase::GetAEWCount()
{
    return m_states.Size() > 14 ? m_states[14]->m_numTimesPermitted : 0;
}

int AirBase::GetTankerCount()
{
    return m_states.Size() > 15 ? m_states[15]->m_numTimesPermitted : 0;
}

void AirBase::OnFighterLanded( int aircraftType )
{
    if( aircraftType == TypeFighterLight && m_states[0]->m_numTimesPermitted < m_maxFighters )
        { m_states[0]->m_numTimesPermitted++; m_states[1]->m_numTimesPermitted++; }
    else if( aircraftType == TypeFighter && m_states[2]->m_numTimesPermitted < m_maxFighters )
        { m_states[2]->m_numTimesPermitted++; m_states[3]->m_numTimesPermitted++; }
    else if( aircraftType == TypeFighterNavyStealth && m_states[4]->m_numTimesPermitted < m_maxFighters )
        { m_states[4]->m_numTimesPermitted++; m_states[5]->m_numTimesPermitted++; }
    else if( aircraftType == TypeFighterStealth && m_states[6]->m_numTimesPermitted < m_maxFighters )
        { m_states[6]->m_numTimesPermitted++; m_states[7]->m_numTimesPermitted++; }
}

void AirBase::OnBomberLanded( int aircraftType )
{
    if( aircraftType == TypeBomber && m_states[8]->m_numTimesPermitted < m_maxBombers )
        { m_states[8]->m_numTimesPermitted++; m_states[9]->m_numTimesPermitted++; }
    else if( aircraftType == TypeBomberFast && m_states[10]->m_numTimesPermitted < m_maxBombers )
        { m_states[10]->m_numTimesPermitted++; m_states[11]->m_numTimesPermitted++; }
    else if( aircraftType == TypeBomberStealth && m_states[12]->m_numTimesPermitted < m_maxBombers )
        { m_states[12]->m_numTimesPermitted++; m_states[13]->m_numTimesPermitted++; }
}

void AirBase::OnAEWLanded()
{
    if( m_states.Size() > 14 )
        m_states[14]->m_numTimesPermitted++;
}

void AirBase::OnTankerLanded()
{
    if( m_states.Size() > 15 )
        m_states[15]->m_numTimesPermitted++;
}

void AirBase::MaybeRemoveRandomStoredAircraft()
{
    if( syncfrand(100) >= 50 )
        return;
    // Pick random category with stock: (0,1) FighterLight, (2,3) Fighter, (4,5) NavyStealth, (6,7) Stealth,
    // (8,9) Bomber, (10,11) BomberFast, (12,13) Stealth Bomber, (14) AEW, (15) Tanker
    int primaryStates[] = { 0, 2, 4, 6, 8, 10, 12, 14, 15 };
    int numCategories = ( m_states.Size() > 15 ) ? 9 : ( ( m_states.Size() > 14 ) ? 8 : 7 );
    int available[9], numAvailable = 0;
    for( int i = 0; i < numCategories; ++i )
    {
        int idx = primaryStates[i];
        if( idx < m_states.Size() && m_states[idx]->m_numTimesPermitted > 0 )
            available[numAvailable++] = idx;
    }
    if( numAvailable == 0 )
        return;
    int pick = available[ syncrand() % numAvailable ];
    m_states[pick]->m_numTimesPermitted--;
    if( pick == 0 || pick == 2 || pick == 4 || pick == 6 || pick == 8 || pick == 10 || pick == 12 )
        m_states[pick + 1]->m_numTimesPermitted--;
}

int AirBase::GetAttackOdds( int _defenderType )
{
    if( CanLaunchFighter() )
    {
        WorldObject::Archetype defenderArchetype = WorldObject::GetArchetypeForType( _defenderType );
        int fighterType = ( m_currentState <= 1 ) ? TypeFighterLight : ( ( m_currentState <= 3 ) ? TypeFighter : ( ( m_currentState <= 5 ) ? TypeFighterNavyStealth : TypeFighterStealth ) );
        if( defenderArchetype == WorldObject::ArchetypeAircraft )
            return g_app->GetWorld()->GetAttackOdds( fighterType, _defenderType );
        if( m_currentState == 1 || m_currentState == 3 || m_currentState == 5 || m_currentState == 7 )
        {
            if( defenderArchetype == WorldObject::ArchetypeBuilding )
                return g_app->GetWorld()->GetAttackOdds( TypeLACM, _defenderType );
            WorldObject::ClassType defenderClass = WorldObject::GetClassTypeForType( _defenderType );
            if( defenderClass == WorldObject::ClassTypeCarrier || defenderClass == WorldObject::ClassTypeBattleShip ||
                defenderClass == WorldObject::ClassTypeSub )
                return g_app->GetWorld()->GetAttackOdds( TypeLACM, _defenderType );
        }
        return 0;
    }

    if( CanLaunchBomber() )
    {
        int bomberType = ( m_currentState <= 9 ) ? TypeBomber : ( ( m_currentState <= 11 ) ? TypeBomberFast : TypeBomberStealth );
        if( _defenderType == TypeCity ||
            _defenderType == TypeSilo ||
            _defenderType == TypeAirBase ||
            _defenderType == TypeRadarStation )
            return g_app->GetWorld()->GetAttackOdds( TypeNuke, _defenderType );
        return g_app->GetWorld()->GetAttackOdds( bomberType, _defenderType );
    }

    return WorldObject::GetAttackOdds( _defenderType );
}


int AirBase::IsValidCombatTarget( int _objectId )
{
    WorldObject *obj = g_app->GetWorld()->GetWorldObject( _objectId );
    if( !obj ) return TargetTypeInvalid;

    int basicCheck = WorldObject::IsValidCombatTarget( _objectId );
    if( basicCheck < TargetTypeInvalid ) return basicCheck;

    if( m_currentState == 14 )
    {
        return TargetTypeLaunchAEW;
    }

    if( m_currentState == 15 )
    {
        return TargetTypeLaunchTanker;
    }

    if( m_currentState <= 7 )
    {
        if( obj->IsAircraft() || obj->IsCruiseMissileClass() || obj->IsBallisticMissileClass() )
        {
            int fighterType = ( m_currentState <= 1 ) ? TypeFighterLight : ( ( m_currentState <= 3 ) ? TypeFighter : ( ( m_currentState <= 5 ) ? TypeFighterNavyStealth : TypeFighterStealth ) );
            if( g_app->GetWorld()->GetAttackOdds( fighterType, obj->m_type ) > 0 )
                return TargetTypeLaunchFighter;
        }
        if( m_currentState == 1 || m_currentState == 3 || m_currentState == 5 || m_currentState == 7 )
        {
            if( obj->IsTargetableSurfaceNavy() )
                return TargetTypeLaunchLACM;
            if( !obj->IsMovingObject() && WorldObject::GetArchetypeForType(obj->m_type) == WorldObject::ArchetypeBuilding )
                return TargetTypeLaunchLACM;
        }
        return TargetTypeInvalid;
    }

    if( m_currentState >= 8 && m_currentState <= 13 )
    {
        if( obj->IsAircraft() || obj->IsCruiseMissileClass() || obj->IsBallisticMissileClass() )
            return TargetTypeInvalid;
        if( !obj->IsMovingObject() )
            return TargetTypeLaunchBomber;
        if( obj->IsTargetableSurfaceNavy() )
            return TargetTypeLaunchBomber;
        if( ( m_currentState == 9 || m_currentState == 11 || m_currentState == 13 ) &&
             !obj->IsMovingObject() && WorldObject::GetArchetypeForType(obj->m_type) == WorldObject::ArchetypeBuilding )
            return TargetTypeLaunchBomber;
    }

    return TargetTypeInvalid;
}


int AirBase::IsValidMovementTarget( Fixed longitude, Fixed latitude )
{
    // For state 0/1: range is not a gate. Allow out-of-range (fuel indicator only).
    if( m_states[m_currentState]->m_numTimesPermitted - m_actionQueue.Size() <= 0 )
    {
        return TargetTypeOutOfStock;
    }

    if( m_states[m_currentState]->m_defconPermitted < g_app->GetWorld()->GetDefcon() )
    {
        return TargetTypeDefconRequired;
    }

    if( m_currentState >= 0 && m_currentState <= 7 )
        return TargetTypeLaunchFighter;

    if( m_currentState >= 8 && m_currentState <= 13 )
        return TargetTypeLaunchBomber;

    if( m_currentState == 14 )
        return TargetTypeLaunchAEW;

    if( m_currentState == 15 )
        return TargetTypeLaunchTanker;

    return TargetTypeInvalid;
}


void AirBase::Render2D()
{
    WorldObject::Render2D();

    if( m_teamId == g_app->GetWorld()->m_myTeamId ||
        g_app->GetWorld()->m_myTeamId == -1 ||
        g_app->GetGame()->m_winner != -1 )
    {   
        if( !m_states.ValidIndex( m_currentState ) ) return;
        int numInStore = m_states[m_currentState]->m_numTimesPermitted;
        int numInQueue = m_actionQueue.Size();

        Team *team = g_app->GetWorld()->GetTeam(m_teamId);
        Colour colour = team->GetTeamColour();            
        colour.m_a = 150;

        Image *bmpImage = NULL;
        switch( m_currentState )
        {
            case 0: case 1:  bmpImage = g_resource->GetImage("graphics/smalllightfighter.bmp"); break;
            case 2: case 3:  bmpImage = g_resource->GetImage("graphics/smallfighter.bmp"); break;
            case 4: case 5:  bmpImage = g_resource->GetImage("graphics/smallstealthnavyfighter.bmp"); break;
            case 6: case 7:  bmpImage = g_resource->GetImage("graphics/smallstealthfighter.bmp"); break;
            case 8: case 9:  bmpImage = g_resource->GetImage("graphics/smallbomber.bmp"); break;
            case 10: case 11: bmpImage = g_resource->GetImage("graphics/smallfastbomber.bmp"); break;
            case 12: case 13: bmpImage = g_resource->GetImage("graphics/smallstealthbomber.bmp"); break;
            case 14:         bmpImage = g_resource->GetImage("graphics/smallaew.bmp"); break;
            case 15:         bmpImage = g_resource->GetImage("graphics/smallrefueler.bmp"); break;
            default:         bmpImage = g_resource->GetImage("graphics/smallfighter.bmp"); break;
        }

        if( bmpImage )
        {
            float x = m_longitude.DoubleValue() - GetSize().DoubleValue() * 0.5f;
            float y = m_latitude.DoubleValue() - GetSize().DoubleValue() * 0.3f;    
            float size = GetSize().DoubleValue() * 0.35f;
            
            float dx = size*0.9f;

            if( team->m_territories[0] >= 3 )
            {
                x += GetSize().DoubleValue() * 0.65f;                                
                dx *= -1;
            }       

            for( int i = 0; i < numInStore; ++i )
            {
                if( i >= (numInStore-numInQueue) )
                {
                    colour.Set( 128,128,128,100 );
                }
                
                g_renderer2d->StaticSprite( bmpImage, x, y, size*0.9f, -size, colour );
                x += dx;
            }
        }
    }    
}

void AirBase::Render3D()
{
    WorldObject::Render3D();

    if( m_teamId == g_app->GetWorld()->m_myTeamId ||
        g_app->GetWorld()->m_myTeamId == -1 ||
        g_app->GetGame()->m_winner != -1 )
    {   
        if( !m_states.ValidIndex( m_currentState ) ) return;
        int numInStore = m_states[m_currentState]->m_numTimesPermitted;
        int numInQueue = m_actionQueue.Size();

        Team *team = g_app->GetWorld()->GetTeam(m_teamId);
        Colour colour = team->GetTeamColour();            
        colour.m_a = 150;

        Image *bmpImage = NULL;
        switch( m_currentState )
        {
            case 0: case 1:  bmpImage = g_resource->GetImage("graphics/smalllightfighter.bmp"); break;
            case 2: case 3:  bmpImage = g_resource->GetImage("graphics/smallfighter.bmp"); break;
            case 4: case 5:  bmpImage = g_resource->GetImage("graphics/smallstealthnavyfighter.bmp"); break;
            case 6: case 7:  bmpImage = g_resource->GetImage("graphics/smallstealthfighter.bmp"); break;
            case 8: case 9:  bmpImage = g_resource->GetImage("graphics/smallbomber.bmp"); break;
            case 10: case 11: bmpImage = g_resource->GetImage("graphics/smallfastbomber.bmp"); break;
            case 12: case 13: bmpImage = g_resource->GetImage("graphics/smallstealthbomber.bmp"); break;
            case 14:         bmpImage = g_resource->GetImage("graphics/smallaew.bmp"); break;
            case 15:         bmpImage = g_resource->GetImage("graphics/smallrefueler.bmp"); break;
            default:         bmpImage = g_resource->GetImage("graphics/smallfighter.bmp"); break;
        }

        if( bmpImage )
        {
            GlobeRenderer *globeRenderer = g_app->GetGlobeRenderer();
            if( globeRenderer )
            {
                float baseSize = GetSize3D().DoubleValue();
                float iconSize = baseSize * GLOBE_UNIT_ICON_SIZE;

                Vector3<float> basePos = globeRenderer->ConvertLongLatTo3DPosition(m_longitude.DoubleValue(), m_latitude.DoubleValue());
                Vector3<float> normal = basePos.Normalized();
                basePos = globeRenderer->GetElevatedPosition(basePos);
                
                Vector3<float> tangent1, tangent2;
                globeRenderer->GetSurfaceTangents(normal, tangent1, tangent2);

                float size3D = GetSize3D().DoubleValue();
                
                float iconSpacing = size3D * GLOBE_UNIT_ICON_SIZE * 0.9f;
                float iconSizeHalf = iconSize * 0.5f;
                
                Vector3<float> offset = -tangent1 * size3D * 0.5f - tangent2 * size3D * 0.3f;
                offset += tangent1 * iconSizeHalf - tangent2 * iconSizeHalf;

                if( team->m_territories[0] >= 3 )
                {
                    offset += tangent1 * size3D * 0.65f;
                    iconSpacing *= -1;
                }

                for( int i = 0; i < numInStore; ++i )
                {
                    if( i >= (numInStore-numInQueue) )
                    {
                        colour.Set( 128,128,128,100 );
                    }
                    
                    Vector3<float> iconPos = basePos + offset + tangent1 * (i * iconSpacing);
                    
                    g_renderer3d->StaticSprite3D( bmpImage, iconPos.x, iconPos.y, iconPos.z, 
                                                  iconSize * 0.9f, iconSize, colour, BILLBOARD_SURFACE_ALIGNED );
                }
            }
        }
    }    
}

