#include "lib/universal_include.h"

#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/language_table.h"
#include "lib/render3d/renderer_3d.h"
#include "lib/render2d/renderer_2d.h"
#include "lib/math/random_number.h"

#include "app/app.h"
#include "app/globals.h"
#include "app/game.h"

#include "renderer/map_renderer.h"
#include "renderer/globe_renderer.h"

#include "world/world.h"
#include "world/carrier.h"
#include "world/aircraft_loads.h"
#include "world/team.h"
#include "world/fighter.h"
#include "world/fighter_navystealth.h"
#include "world/fleet.h"



Carrier::Carrier()
:   MovingObject()
{
    SetType( TypeCarrier );
    
    strcpy( bmpImageFilename, "graphics/wasp.bmp" );
    strcpy( bmpFighterMarkerFilename, "graphics/fighter.bmp" );

    m_radarRange = 15;
    m_selectable = false;  
    m_speed = Fixed::Hundredths(3);
    m_turnRate = Fixed::Hundredths(1);
    m_movementType = MovementTypeSea;
    m_maxHistorySize = 10;
    m_range = Fixed::MAX;
    m_life = 3;

    m_maxFighters = 5;
    m_maxBombers = 0;
    m_maxAEW = 2;

    m_ghostFadeTime = 150;

    AddState( LANGUAGEPHRASE("state_navyfighterlaunch"), 60, 30, 10, 45, true, 5, 3 );       // 0: Fighter CAP
    AddState( LANGUAGEPHRASE("state_navystrikefighterlaunch"), 60, 30, 10, 45, true, 5, 3 ); // 1: Fighter Strike
    AddState( LANGUAGEPHRASE("state_navystealthfighterlaunch"), 60, 30, 10, 45, true, 0, 3 ); // 2: NavyStealth CAP
    AddState( LANGUAGEPHRASE("state_navystealthstrikefighterlaunch"), 60, 30, 10, 45, true, 0, 3 ); // 3: NavyStealth Strike
    AddState( LANGUAGEPHRASE("state_aewlaunch"), 60, 30, 10, 140, true, 2, 3 );             // 4: AEW

    m_states[1]->m_numTimesPermitted = m_states[0]->m_numTimesPermitted;
    m_states[3]->m_numTimesPermitted = m_states[2]->m_numTimesPermitted;

    InitialiseTimers();
}

void Carrier::SetTeamId( int teamId )
{
    MovingObject::SetTeamId( teamId );
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

void Carrier::RequestAction(ActionOrder *_action)
{
    WorldObject::RequestAction(_action);
}


void Carrier::Action( int targetObjectId, Fixed longitude, Fixed latitude )
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
        if( m_currentState == 0 ) { if(!LaunchFighter( targetObjectId, longitude, latitude, 1 )) return; }
        else if( m_currentState == 1 ) { if(!LaunchFighter( targetObjectId, longitude, latitude, 2 )) return; }
        else if( m_currentState == 2 ) { if(!LaunchNavyStealthFighter( targetObjectId, longitude, latitude, 0 )) return; }
        else if( m_currentState == 3 ) { if(!LaunchNavyStealthFighter( targetObjectId, longitude, latitude, 1 )) return; }
        else if( m_currentState == 4 ) { if(!LaunchAEW( targetObjectId, longitude, latitude )) return; }
        MovingObject::Action( targetObjectId, longitude, latitude );
        if( m_currentState <= 3 && ( m_currentState & 1 ) == 0 )
            m_states[m_currentState+1]->m_numTimesPermitted = m_states[m_currentState]->m_numTimesPermitted;
        else if( m_currentState <= 3 && ( m_currentState & 1 ) == 1 )
            m_states[m_currentState-1]->m_numTimesPermitted = m_states[m_currentState]->m_numTimesPermitted;
    }
}

bool Carrier::Update()
{
    AppDebugAssert( m_type == WorldObject::TypeCarrier );
    m_states[1]->m_numTimesPermitted = m_states[0]->m_numTimesPermitted;
    m_states[3]->m_numTimesPermitted = m_states[2]->m_numTimesPermitted;
    MoveToWaypoint();
    return MovingObject::Update();
}


void Carrier::RunAI()
{    
    if( m_aiTimer <= 0 )
    {
        if( g_app->GetWorld()->GetDefcon() <= 3 )
        {
            m_aiTimer = m_aiSpeed;
            Team *team = g_app->GetWorld()->GetTeam( m_teamId );
            Fleet *fleet = team->GetFleet( m_fleetId );
            if( m_states[m_currentState]->m_numTimesPermitted - m_actionQueue.Size() <= 0 )
            {
                return;
            }

            if( team->m_targetsVisible >= 4 ||
                team->m_currentState >= Team::StateAssault )
            {
                LaunchScout();
            }
            else
            {
                LList<int> targets;
                int scanRange = 15.0f;
                for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
                {
                    if( g_app->GetWorld()->m_objects.ValidIndex(i) )
                    {
                        WorldObject *obj = g_app->GetWorld()->m_objects[i];
                        bool visible = obj->m_visible[m_teamId];

                        if( !g_app->GetWorld()->IsFriend( m_teamId, obj->m_teamId) &&                            
                            visible &&
                            obj->IsMovingObject() &&
                            team->CountTargettedUnits(i) < 3 &&
                            g_app->GetWorld()->GetDistance( m_longitude, m_latitude, obj->m_longitude, obj->m_latitude ) < scanRange )
                        {
                            targets.PutData(obj->m_objectId );
                        }
                    }
                }
                int targetIndex = -1;
                if( targets.Size() > 0 )
                {
                    targetIndex = targets[syncrand() % targets.Size()];
                }

                WorldObject *obj = g_app->GetWorld()->GetWorldObject( targetIndex );
                if( obj )
                {                
                    if( obj->IsAircraft() && m_stateTimer <= 0 )
                    {
                        SetState( 0 );  // Fighter CAP for intercepting aircraft
                        ActionOrder *action = new ActionOrder();
                        action->m_targetObjectId = obj->m_objectId;
                        action->m_longitude = obj->m_longitude;
                        action->m_latitude = obj->m_latitude;
                        RequestAction(action);
                    }
                }
                else
                {
                    if( GetFighterCount() > 2 )
                        LaunchScout();
                }
            }
        }
    }
}

void Carrier::LaunchScout()
{
    if( GetFighterCount() > 0 )
    {
        if( m_currentState != 0 )
        {
            SetState(0);
        }
        Team *team = g_app->GetWorld()->GetTeam( m_teamId );
        
        int territoryId = -1;
        for( int i = 0; i < World::NumTerritories; ++i )
        {
            int owner = g_app->GetWorld()->GetTerritoryOwner( i );
            if( owner != -1 &&
                !g_app->GetWorld()->IsFriend( owner, m_teamId ) )
            {
                Fixed distance = g_app->GetWorld()->GetDistance( m_longitude, m_latitude, 
                                                                g_app->GetWorld()->m_populationCenter[i].x,
                                                                g_app->GetWorld()->m_populationCenter[i].y );
                Fixed maxRange = 45 / g_app->GetWorld()->GetUnitScaleFactor();
                if( distance < maxRange )
                {
                    territoryId = i;
                    break;
                }
            }
        }

        if( territoryId != -1 )
        {
            Fixed longitude = g_app->GetWorld()->m_populationCenter[territoryId].x + syncsfrand( 60 );
            Fixed latitude = g_app->GetWorld()->m_populationCenter[territoryId].y + syncsfrand( 60 );
                                        
            ActionOrder *action = new ActionOrder();
            action->m_longitude = longitude;
            action->m_latitude = latitude;
            RequestAction( action );
        }
    }
}

bool Carrier::IsActionQueueable()
{
    return true;
}

int Carrier::FindTarget()
{
    for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
    {
        if( g_app->GetWorld()->m_objects.ValidIndex(i) )
        {
            WorldObject *obj = g_app->GetWorld()->m_objects[i];

            if( obj->m_teamId != m_teamId &&                
                g_app->GetWorld()->GetAttackOdds( WorldObject::TypeFighter, obj->m_type ) > 0 &&
                obj->m_visible[m_teamId] == true )
            {
                Fixed distance = g_app->GetWorld()->GetDistance( m_longitude, m_latitude, obj->m_longitude, obj->m_latitude);

                if( distance <= 30 )
                {
                    return obj->m_objectId;
                }
            }
        }
    }
    return -1;
}

int Carrier::GetAttackState()
{
    return -1;
}

void Carrier::Retaliate( int attackerId )
{
    Fleet *fleet = g_app->GetWorld()->GetTeam( m_teamId )->GetFleet( m_fleetId );
    if( fleet )
    {
        WorldObject *obj = g_app->GetWorld()->GetWorldObject( attackerId );
        if( obj &&
            obj->m_visible[ m_teamId ] && 
            !g_app->GetWorld()->GetTeam( m_teamId )->m_ceaseFire[ obj->m_teamId ])
        {
            if( m_states[m_currentState]->m_numTimesPermitted > 0 &&
                m_currentState <= 3 )
            {
                if( g_app->GetWorld()->GetAttackOdds( WorldObject::TypeFighter, obj->m_type ) > 0 )
                {
                    Action( attackerId, -1, -1 );

                    if( m_fleetId != -1 )
                    {
                        fleet->FleetAction( m_targetObjectId );
                    }
                }
            }
        }
    }
}

bool Carrier::UsingNukes()
{
    return false;
}


void Carrier::FleetAction( int targetObjectId )
{
    if( m_targetObjectId == -1 )
    {
        WorldObject *obj = g_app->GetWorld()->GetWorldObject( targetObjectId );
        if( obj )
        {
            if( m_states[ m_currentState ]->m_numTimesPermitted != 0 &&
                m_stateTimer <= 0 )
            {
                    if( m_currentState <= 3 )
                    {
                        if( g_app->GetWorld()->GetAttackOdds( WorldObject::TypeFighter, obj->m_type ) > 0 )
                        {
                            ActionOrder *action = new ActionOrder();
                            action->m_targetObjectId = obj->m_objectId;
                            action->m_longitude = obj->m_longitude;
                            action->m_latitude = obj->m_latitude;
                            RequestAction( action );
                        }
                    }
            }
            MovingObject::Retaliate( targetObjectId );
        }
    }
}

bool Carrier::CanLaunchBomber()
{
    return false;
}

bool Carrier::CanLaunchFighter()
{
    return ( m_currentState <= 3 && m_states[m_currentState]->m_numTimesPermitted > 0 );
}

bool Carrier::CanLaunchFighterLight() { return false; }
bool Carrier::CanLaunchStealthFighter() { return false; }
bool Carrier::CanLaunchNavyStealthFighter() { return ( ( m_currentState == 2 || m_currentState == 3 ) && m_states[m_currentState]->m_numTimesPermitted > 0 ); }
bool Carrier::CanLaunchBomberFast() { return false; }
bool Carrier::CanLaunchStealthBomber() { return false; }
bool Carrier::CanLaunchAEW() { return ( m_currentState == 4 && m_states[4]->m_numTimesPermitted > 0 ); }

int Carrier::GetFighterCount()
{
    return m_states[0]->m_numTimesPermitted + m_states[2]->m_numTimesPermitted;
}

int Carrier::GetBomberCount()
{
    return 0;
}

int Carrier::GetAEWCount()
{
    return m_states.Size() > 4 ? m_states[4]->m_numTimesPermitted : 0;
}

void Carrier::OnFighterLanded( int aircraftType )
{
    if( aircraftType == TypeFighter && m_states[0]->m_numTimesPermitted < m_maxFighters )
        { m_states[0]->m_numTimesPermitted++; m_states[1]->m_numTimesPermitted++; }
    else if( aircraftType == TypeFighterNavyStealth && m_states[2]->m_numTimesPermitted < m_maxFighters )
        { m_states[2]->m_numTimesPermitted++; m_states[3]->m_numTimesPermitted++; }
}

void Carrier::OnBomberLanded( int aircraftType )
{
}

void Carrier::OnAEWLanded()
{
    if( m_states.Size() > 4 )
        m_states[4]->m_numTimesPermitted++;
}

void Carrier::MaybeRemoveRandomStoredAircraft()
{
    if( syncfrand(100) >= 50 )
        return;
    // Pick random category with stock: (0,1) Fighter, (2,3) NavyStealth, (4) AEW
    int primaryStates[] = { 0, 2, 4 };
    int numCategories = ( m_states.Size() > 4 ) ? 3 : 2;
    int available[3], numAvailable = 0;
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
    if( pick == 0 || pick == 2 )
        m_states[pick + 1]->m_numTimesPermitted--;
}

int Carrier::GetAttackOdds( int _defenderType )
{
    if( CanLaunchFighter() )
    {
        int fighterType = ( m_currentState <= 1 ) ? TypeFighter : TypeFighterNavyStealth;
        WorldObject::Archetype defenderArchetype = WorldObject::GetArchetypeForType( _defenderType );
        if( defenderArchetype == WorldObject::ArchetypeAircraft )
            return g_app->GetWorld()->GetAttackOdds( fighterType, _defenderType );
        if( m_currentState == 1 || m_currentState == 3 )
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
    return MovingObject::GetAttackOdds( _defenderType );
}


int Carrier::IsValidCombatTarget( int _objectId )
{
    WorldObject *obj = g_app->GetWorld()->GetWorldObject( _objectId );
    if( !obj ) return TargetTypeInvalid;

    int basicCheck = WorldObject::IsValidCombatTarget( _objectId );
    if( basicCheck < TargetTypeInvalid ) return basicCheck;

    if( m_currentState == 4 )
    {
        if( obj->IsAircraftLauncher() && obj->m_teamId == m_teamId )
            return TargetTypeLaunchAEW;
        return TargetTypeLaunchAEW;
    }

    if( m_currentState <= 3 )
    {
        if( obj->IsAircraft() || obj->IsCruiseMissileClass() || obj->IsBallisticMissileClass() )
        {
            int fighterType = ( m_currentState <= 1 ) ? TypeFighter : TypeFighterNavyStealth;
            if( g_app->GetWorld()->GetAttackOdds( fighterType, obj->m_type ) > 0 )
                return TargetTypeLaunchFighter;
        }
        if( m_currentState == 1 || m_currentState == 3 )
        {
            if( obj->IsTargetableSurfaceNavy() )
                return TargetTypeLaunchLACM;
            if( !obj->IsMovingObject() && WorldObject::GetArchetypeForType(obj->m_type) == WorldObject::ArchetypeBuilding )
                return TargetTypeLaunchLACM;
        }
        return TargetTypeInvalid;
    }

    return TargetTypeInvalid;
}


int Carrier::IsValidMovementTarget( Fixed longitude, Fixed latitude )
{
    if( m_states[m_currentState]->m_numTimesPermitted - m_actionQueue.Size() <= 0 )
        return TargetTypeOutOfStock;
    if( m_states[m_currentState]->m_defconPermitted < g_app->GetWorld()->GetDefcon() )
        return TargetTypeDefconRequired;
    if( m_currentState <= 3 )
        return TargetTypeLaunchFighter;
    if( m_currentState == 4 )
        return TargetTypeLaunchAEW;
    return TargetTypeInvalid;
}


int Carrier::CountIncomingFighters()
{
    int num = 0;
    for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
    {
        if( g_app->GetWorld()->m_objects.ValidIndex(i) )
        {
            MovingObject *obj = (MovingObject *)g_app->GetWorld()->m_objects[i];
            if( obj->IsFighterClass() &&
                obj->m_teamId == m_teamId &&
                obj->m_isLanding == m_objectId )
            {
                num++;
            }
        }
    }
    return num;
}


void Carrier::Render2D()
{
    MovingObject::Render2D();

    if( m_teamId == g_app->GetWorld()->m_myTeamId ||
        g_app->GetWorld()->m_myTeamId == -1 ||
        g_app->GetGame()->m_winner != -1 )
    {   
        int numInStore = m_states[m_currentState]->m_numTimesPermitted;
        int numInQueue = m_actionQueue.Size();

        Team *team = g_app->GetWorld()->GetTeam(m_teamId);
        Colour colour = team->GetTeamColour();            
        colour.m_a = 150;

        Image *bmpImage = NULL;
        switch( m_currentState )
        {
            case 0: case 1:  bmpImage = g_resource->GetImage("graphics/smallfighter.bmp"); break;
            case 2: case 3:  bmpImage = g_resource->GetImage("graphics/smallstealthnavyfighter.bmp"); break;
            case 4:          bmpImage = g_resource->GetImage("graphics/smallaew.bmp"); break;
            default:         bmpImage = g_resource->GetImage("graphics/smallfighter.bmp"); break;
        }

        Fixed predictionTime = Fixed::FromDouble(g_predictionTime) * g_app->GetWorld()->GetTimeScaleFactor();
        float predictedLongitude = (m_longitude + m_vel.x * predictionTime).DoubleValue();
        float predictedLatitude = (m_latitude + m_vel.y * predictionTime).DoubleValue(); 

        if( bmpImage )
        {
            float x = predictedLongitude - GetSize().DoubleValue() * 0.85f;
            float y = predictedLatitude - GetSize().DoubleValue() * 0.25f; 
            float size = GetSize().DoubleValue() * 0.35f;
            
            float dx = size*0.9f;

            if( team->m_territories[0] >= 3 )
            {
                x += GetSize().DoubleValue()*1.4f;                                
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

void Carrier::Render3D()
{
    MovingObject::Render3D();

    if( m_teamId == g_app->GetWorld()->m_myTeamId ||
        g_app->GetWorld()->m_myTeamId == -1 ||
        g_app->GetGame()->m_winner != -1 )
    {   
        int numInStore = m_states[m_currentState]->m_numTimesPermitted;
        int numInQueue = m_actionQueue.Size();

        Team *team = g_app->GetWorld()->GetTeam(m_teamId);
        Colour colour = team->GetTeamColour();            
        colour.m_a = 150;

        Image *bmpImage = NULL;
        switch( m_currentState )
        {
            case 0: case 1:  bmpImage = g_resource->GetImage("graphics/smallfighter.bmp"); break;
            case 2: case 3:  bmpImage = g_resource->GetImage("graphics/smallstealthnavyfighter.bmp"); break;
            case 4:          bmpImage = g_resource->GetImage("graphics/smallaew.bmp"); break;
            default:         bmpImage = g_resource->GetImage("graphics/smallfighter.bmp"); break;
        }

        Fixed predictionTime = Fixed::FromDouble(g_predictionTime) * g_app->GetWorld()->GetTimeScaleFactor();
        float predictedLongitude = (m_longitude + m_vel.x * predictionTime).DoubleValue();
        float predictedLatitude = (m_latitude + m_vel.y * predictionTime).DoubleValue(); 

        if( bmpImage )
        {
            GlobeRenderer *globeRenderer = g_app->GetGlobeRenderer();
            if( globeRenderer )
            {
                float baseSize = GetSize3D().DoubleValue();
                float iconSize = baseSize * GLOBE_UNIT_ICON_SIZE;

                Vector3<float> basePos = globeRenderer->ConvertLongLatTo3DPosition(predictedLongitude, predictedLatitude);
                Vector3<float> normal = basePos.Normalized();
                basePos = globeRenderer->GetElevatedPosition(basePos);
                
                Vector3<float> tangent1, tangent2;
                globeRenderer->GetSurfaceTangents(normal, tangent1, tangent2);

                float size3D = GetSize3D().DoubleValue();
                
                float iconSpacing = size3D * GLOBE_UNIT_ICON_SIZE * 0.9f;
                float iconSizeHalf = iconSize * 0.5f;
                
                Vector3<float> offset = -tangent1 * size3D * 0.85f - tangent2 * size3D * 0.25f;
                offset += tangent1 * iconSizeHalf - tangent2 * iconSizeHalf;

                if( team->m_territories[0] >= 3 )
                {
                    offset += tangent1 * size3D * 1.4f;
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

