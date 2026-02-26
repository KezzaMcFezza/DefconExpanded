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
#include "world/nuke.h"



AirBase::AirBase()
:   WorldObject(),
    m_fighterRegenTimer(1024)
{
    SetType( TypeAirBase );
    
    strcpy( bmpImageFilename, "graphics/airbase.bmp" );

    m_radarRange = 20;
    m_selectable = false;  

    m_life = 15;
    m_nukeSupply = -1;  // Infinite resupply - bombers that land get full nukes on relaunch

    m_maxFighters = 10;
    m_maxBombers = 10;

    AddState( LANGUAGEPHRASE("state_fighterlaunch"), 120, 120, 10, 45, true, 5, 3 );   // 0: CAP
    AddState( LANGUAGEPHRASE("state_strikefighterlaunch"), 120, 120, 10, 45, true, 5, 3 );  // 1: Strike
    AddState( LANGUAGEPHRASE("state_bomberlaunch"), 120, 120, 10, 140, true, 5, 3 );  // 2: Nuke
    AddState( LANGUAGEPHRASE("state_cruisebomberlaunch"), 120, 120, 10, 140, true, 5, 3 );  // 3: LACM

    m_states[3]->m_numTimesPermitted = m_states[2]->m_numTimesPermitted;

    InitialiseTimers();
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
        if( m_currentState == 0 || m_currentState == 1 )
        {
            int fighterMode = ( m_currentState == 0 ) ? 1 : 2;  // 1=CAP, 2=Strike
            if(!LaunchFighter( targetObjectId, longitude, latitude, fighterMode ))
            {
                return;
            }
        }
        else if( m_currentState == 2 )
        {
            if(!LaunchBomber( targetObjectId, longitude, latitude, 1 ))
            {
                return;
            }
        }
        else if( m_currentState == 3 )
        {
            if(!LaunchBomber( targetObjectId, longitude, latitude, 2 ))
            {
                return;
            }
        }
        WorldObject::Action( targetObjectId, longitude, latitude );
        if( m_currentState == 2 || m_currentState == 3 )
            m_states[3]->m_numTimesPermitted = m_states[2]->m_numTimesPermitted;
        if( m_currentState == 0 || m_currentState == 1 )
        {
            int c = m_states[0]->m_numTimesPermitted;
            if( (int)m_states[1]->m_numTimesPermitted < c ) c = m_states[1]->m_numTimesPermitted;
            m_states[0]->m_numTimesPermitted = m_states[1]->m_numTimesPermitted = c;
        }
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
                m_states[0]->m_numTimesPermitted > 2 &&
                team->m_currentState == Team::StateScouting)
            {
                if( m_currentState != 0 )
                {
                    SetState(0);
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
                                if( m_states[0]->m_numTimesPermitted > 0 )
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
                                if( m_states[2]->m_numTimesPermitted > 0 )
                                {
                                    SetState(2);
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
                            if( m_currentState != 2 )
                            {
                                SetState(2);
                            }
                            if( m_currentState == 2 )
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
    // Reduce remaining aircraft

    float fighterLossFactor = 0.5f;
    float bomberLossFactor = 0.5f;

    m_states[0]->m_numTimesPermitted *= fighterLossFactor;
    m_states[1]->m_numTimesPermitted *= fighterLossFactor;
    m_states[2]->m_numTimesPermitted *= bomberLossFactor;
    m_states[3]->m_numTimesPermitted *= bomberLossFactor;
    // No m_nukeSupply - airbases don't store nukes
}

bool AirBase::Update()
{
    m_states[3]->m_numTimesPermitted = m_states[2]->m_numTimesPermitted;
    m_states[1]->m_numTimesPermitted = m_states[0]->m_numTimesPermitted;
    int altState = ( m_currentState < 2 ) ? 2 : 0;
    if( m_states[m_currentState]->m_numTimesPermitted == 0 &&
        m_states[altState]->m_numTimesPermitted > 0 )
    {
        SetState( altState );
    }

    if( m_states[0]->m_numTimesPermitted < 5 )
    {
        m_fighterRegenTimer -= SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor();
        if( m_fighterRegenTimer <= 0 )
        {
            m_states[0]->m_numTimesPermitted ++;
            m_fighterRegenTimer = 1024;
        }
    }
    else
    {
        m_fighterRegenTimer = 1024;
    }

    return WorldObject::Update();
}

bool AirBase::CanLaunchBomber()
{
    if( ( m_currentState == 2 || m_currentState == 3 ) &&
        m_states[m_currentState]->m_numTimesPermitted > 0 )
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool AirBase::CanLaunchFighter()
{
    if( ( m_currentState == 0 || m_currentState == 1 ) &&
        m_states[m_currentState]->m_numTimesPermitted > 0 )
    {
        return true;
    }
    else
    {
        return false;
    }
}


int AirBase::GetAttackOdds( int _defenderType )
{
    if( CanLaunchFighter() )
    {
        WorldObject::Archetype defenderArchetype = WorldObject::GetArchetypeForType( _defenderType );
        if( defenderArchetype == WorldObject::ArchetypeAircraft )
            return g_app->GetWorld()->GetAttackOdds( TypeFighter, _defenderType );
        if( m_currentState == 1 )  // Strike mode
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
        if( _defenderType == TypeCity ||
            _defenderType == TypeSilo ||
            _defenderType == TypeAirBase ||
            _defenderType == TypeRadarStation )
        {
            return g_app->GetWorld()->GetAttackOdds( TypeNuke, _defenderType );
        }
        else
        {
            return g_app->GetWorld()->GetAttackOdds( TypeBomber, _defenderType );
        }
    }

    return WorldObject::GetAttackOdds( _defenderType );
}


int AirBase::IsValidCombatTarget( int _objectId )
{
    WorldObject *obj = g_app->GetWorld()->GetWorldObject( _objectId );
    if( !obj ) return TargetTypeInvalid;

    int basicCheck = WorldObject::IsValidCombatTarget( _objectId );
    if( basicCheck < TargetTypeInvalid ) return basicCheck;

    // Range is not a launch gate - allow out-of-range for UI. Aircraft will fly toward target. Allow allies.
    if( m_currentState == 0 )  // CAP
    {
        if( obj->IsAircraft() || obj->IsCruiseMissileClass() || obj->IsBallisticMissileClass() )
        {
            int attackOdds = g_app->GetWorld()->GetAttackOdds( TypeFighter, obj->m_type );
            if( attackOdds > 0 )
                return TargetTypeLaunchFighter;
        }
        return TargetTypeInvalid;
    }

    if( m_currentState == 1 )  // Strike
    {
        if( obj->IsAircraft() || obj->IsCruiseMissileClass() || obj->IsBallisticMissileClass() )
        {
            int attackOdds = g_app->GetWorld()->GetAttackOdds( TypeFighter, obj->m_type );
            if( attackOdds > 0 )
                return TargetTypeLaunchFighter;
        }
        else if( obj->IsCarrierClass() || obj->IsBattleShipClass() || ( obj->IsSubmarine() && !obj->IsHiddenFrom() ) )
            return TargetTypeLaunchLACM;
        else if( !obj->IsMovingObject() && WorldObject::GetArchetypeForType(obj->m_type) == WorldObject::ArchetypeBuilding )
            return TargetTypeLaunchLACM;
        return TargetTypeInvalid;
    }

    if( m_currentState == 2 )  // Bomber Nuke
    {
        if( obj->IsAircraft() || obj->IsCruiseMissileClass() || obj->IsBallisticMissileClass() )
            return TargetTypeInvalid;
        if( !obj->IsMovingObject() )
            return TargetTypeLaunchBomber;
        if( obj->IsCarrierClass() || obj->IsBattleShipClass() || ( obj->IsSubmarine() && !obj->IsHiddenFrom() ) )
            return TargetTypeLaunchBomber;
    }

    if( m_currentState == 3 )  // Bomber LACM
    {
        if( obj->IsAircraft() || obj->IsCruiseMissileClass() || obj->IsBallisticMissileClass() )
            return TargetTypeInvalid;
        if( obj->IsCarrierClass() || obj->IsBattleShipClass() || ( obj->IsSubmarine() && !obj->IsHiddenFrom() ) )
            return TargetTypeLaunchBomber;
        if( !obj->IsMovingObject() && WorldObject::GetArchetypeForType(obj->m_type) == WorldObject::ArchetypeBuilding )
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

    if( m_currentState == 0 || m_currentState == 1 )
    {
        return TargetTypeLaunchFighter;
    }

    if( m_currentState == 2 || m_currentState == 3 )
    {
        return TargetTypeLaunchBomber;
    }

    return TargetTypeInvalid;
}


void AirBase::Render2D()
{
    WorldObject::Render2D();

    if( m_teamId == g_app->GetWorld()->m_myTeamId ||
        g_app->GetWorld()->m_myTeamId == -1 ||
        g_app->GetGame()->m_winner != -1 )
    {   
        int numInStore = m_states[m_currentState]->m_numTimesPermitted;
        int numInQueue = m_actionQueue.Size();

        Team *team = g_app->GetWorld()->GetTeam(m_teamId);
        Colour colour = team->GetTeamColour();            
        colour.m_a = 150;

        Image *bmpImage = g_resource->GetImage("graphics/smallbomber.bmp");
        if( m_currentState == 0 || m_currentState == 1 ) bmpImage = g_resource->GetImage("graphics/smallfighter.bmp" );

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
        int numInStore = m_states[m_currentState]->m_numTimesPermitted;
        int numInQueue = m_actionQueue.Size();

        Team *team = g_app->GetWorld()->GetTeam(m_teamId);
        Colour colour = team->GetTeamColour();            
        colour.m_a = 150;

        Image *bmpImage = g_resource->GetImage("graphics/smallbomber.bmp");
        if( m_currentState == 0 || m_currentState == 1 ) bmpImage = g_resource->GetImage("graphics/smallfighter.bmp" );

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

