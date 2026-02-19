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
    m_nukeTargetLongitude(0),
    m_nukeTargetLatitude(0),
    m_bombingRun(false)
{
    SetType( TypeBomber );

    strcpy( bmpImageFilename, "graphics/bomber.bmp" );

    m_radarRange = 5;
    m_speed = Fixed::Hundredths(5);
    m_turnRate = Fixed::Hundredths(1);
    m_selectable = true;
    m_maxHistorySize = 10;
    m_range = 140;

    m_nukeSupply = 1;

    m_movementType = MovementTypeAir;

    AddState( LANGUAGEPHRASE("state_airtoseamissile"), 60, 30, 10, 20, true, -1, 3 );
    AddState( LANGUAGEPHRASE("state_bombernuke"), 240, 120, 5, 25, true, 2, 1 );

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
        return;
    }

    m_nukeTargetLongitude = 0;
    m_nukeTargetLatitude = 0;

    WorldObject *target = g_app->GetWorld()->GetWorldObject( targetObjectId );
    if( target )
    {
        if( target->m_type == WorldObject::TypeBattleShip ||
            target->m_type == WorldObject::TypeCarrier ||
            target->m_type == WorldObject::TypeSub )
        {
            if( m_currentState != 0 )
            {
                SetState(0);
            }
        }

        if( target->m_type == WorldObject::TypeCity ||
            target->m_type == WorldObject::TypeSilo ||
            target->m_type == WorldObject::TypeRadarStation ||
            target->m_type == WorldObject::TypeAirBase )
        {
            if( m_currentState != 1 &&
                m_states[1]->m_numTimesPermitted > 0 )
            {
                m_bombingRun = true;
                SetNukeTarget( longitude, latitude );
            }
        }
    }


    if( m_currentState == 0 )
    {    
        WorldObject *target = g_app->GetWorld()->GetWorldObject( targetObjectId );
        if( target &&
            target->m_visible[m_teamId] &&
            g_app->GetWorld()->GetAttackOdds( m_type, target->m_type ) > 0)
        {
            m_targetObjectId = targetObjectId;
        }
    }
    else if( m_currentState == 1 )
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
            MovingObject::Action( targetObjectId, longitude, latitude );
            g_app->GetWorld()->LaunchNuke( m_teamId, m_objectId, longitude, latitude, nukeRange );
            m_nukeTargetLongitude = 0;
            m_nukeTargetLatitude = 0;
            m_bombingRun = true;

            int remainingAfterQueue = m_states[1]->m_numTimesPermitted - m_actionQueue.Size();
            if( remainingAfterQueue > 0 && m_actionQueue.Size() == 0 && targetObjectId != -1 )
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
                SetNukeTarget( longitude, latitude );
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

    bool arrived = MoveToWaypoint();

    // If we arrived at waypoint but have a queued attack or pending nuke, keep waypoint set so we orbit until reload allows firing
    if( arrived && m_currentState == 1 && m_states[1]->m_numTimesPermitted > 0 )
    {
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
        else if( m_nukeTargetLongitude != 0 && m_nukeTargetLatitude != 0 )
        {
            orbitLon = m_nukeTargetLongitude;
            orbitLat = m_nukeTargetLatitude;
        }
        if( orbitLon != 0 || orbitLat != 0 )
        {
            SetWaypoint( orbitLon, orbitLat );
        }
    }

    //
    // Do we move ?
    
    Vector3<Fixed> oldVel = m_vel;

    bool hasTarget = false;
    if( m_targetObjectId != -1 )
    {
        if( m_currentState == 0 )
        {
            WorldObject *targetObject = g_app->GetWorld()->GetWorldObject(m_targetObjectId);
            if( targetObject )
            {
                if( targetObject->m_teamId == m_teamId )
                {
                    if( targetObject->m_type == WorldObject::TypeCarrier ||
                        targetObject->m_type == WorldObject::TypeAirBase )
                    {
                        SetWaypoint( targetObject->m_longitude, targetObject->m_latitude );
                        Land( m_targetObjectId );
                    }
                    m_targetObjectId = -1;
                }
                else
                {
                    if( m_targetLongitude == 0 && m_targetLatitude == 0 )
                    {
                        SetWaypoint( targetObject->m_longitude, targetObject->m_latitude );
                    }

                    if( targetObject->m_visible[ m_teamId ] )
                    {
                        hasTarget = true;
                        if(m_stateTimer <= 0)
                        {
                            Fixed distanceSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, targetObject->m_longitude, targetObject->m_latitude);
                            if( distanceSqd <= GetActionRangeSqd() )
                            {
                                FireGun( GetActionRange() );
                                m_stateTimer = m_states[0]->m_timeToReload;
                            }
                        }
                    }
                }
            }
            else
            {
                m_targetObjectId = -1;
            }
        }            
    }

    if( m_currentState == 0 &&
        !hasTarget &&
        m_retargetTimer <= 0 &&
        !m_bombingRun &&
        m_isLanding == -1 )
    {
        m_retargetTimer = 10;
        m_targetObjectId = -1;
        WorldObject *obj = g_app->GetWorld()->GetWorldObject( GetTarget(25) );
        if( obj )
        {
            m_targetObjectId = obj->m_objectId;
        }
    }

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
        if( m_nukeTargetLongitude != 0 && m_nukeTargetLatitude != 0 )
        {
            if( m_stateTimer == 0 )
            {
                Fixed distanceSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, m_nukeTargetLongitude, m_nukeTargetLatitude );
                if( distanceSqd <= GetActionRangeSqd() )
                {
                    Action( -1, m_nukeTargetLongitude, m_nukeTargetLatitude );
                    m_bombingRun = true;
                }
            }
        }
    }

    if( m_bombingRun && m_states[1]->m_numTimesPermitted > 0 &&
        g_app->GetWorld()->GetDefcon() == 1 &&
        m_currentState != 1 )
    {
        SetState(1);
    }

    bool hasQueuedAttack = ( m_currentState == 1 ) && ( m_actionQueue.Size() > 0 );
    bool hasAmmoAndWaypoint = ( m_currentState == 1 ) && ( m_states[1]->m_numTimesPermitted > 0 ) &&
        ( m_targetLongitude != 0 || m_targetLatitude != 0 );
    if( ( m_bombingRun && m_states[1]->m_numTimesPermitted == 0 ) ||
        ( IsIdle() && !hasQueuedAttack && !hasAmmoAndWaypoint ) )
    {
        Land( GetClosestLandingPad() );
    }

    if( m_currentState == 1 )
    {
        if( m_states[1]->m_numTimesPermitted == 0 )
        {
            SetState(0);
        }
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
        for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
        {
            if( g_app->GetWorld()->m_objects.ValidIndex(i) )
            {
                WorldObject *obj = g_app->GetWorld()->m_objects[i];
                if( obj )
                {
                    if( g_app->GetWorld()->GetAttackOdds( m_type, obj->m_type ) > 0 &&
                        !g_app->GetWorld()->IsFriend( m_teamId, obj->m_teamId ) &&                        
                        obj->m_visible[m_teamId] &&
                        g_app->GetWorld()->GetDistance( m_longitude, m_latitude, obj->m_longitude, obj->m_latitude ) < m_range - 15 )
                    {
                        SetState(0);
                        Action( obj->m_targetObjectId, obj->m_longitude, obj->m_latitude );
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
    if( m_currentState == 1 )
    {
        return true;
    }
    else
    {
        return false;
    }
}

void Bomber::SetNukeTarget( Fixed longitude, Fixed latitude )
{
    m_nukeTargetLongitude = longitude;
    m_nukeTargetLatitude = latitude;
}

void Bomber::Retaliate( int attackerId )
{
    if( g_app->GetWorld()->GetTeam( m_teamId )->m_type == Team::TypeAI )
    {
        if( m_currentState == 1 &&
            m_states[1]->m_numTimesPermitted > 0 &&
            m_stateTimer == 0)
        {
            WorldObject *obj = g_app->GetWorld()->GetWorldObject( attackerId );
            if( obj )
            {
                if( obj->m_type == WorldObject::TypeSilo &&
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

void Bomber::SetState( int state )
{
    MovingObject::SetState( state );
    if( m_currentState != 1 )
    {
        m_bombingRun = false;
    }
}


int Bomber::GetAttackOdds( int _defenderType )
{
    if( m_states[1]->m_numTimesPermitted > 0 )
    {
        if( _defenderType == TypeCity ||
            _defenderType == TypeSilo ||
            _defenderType == TypeAirBase ||
            _defenderType == TypeRadarStation )
        {
            return g_app->GetWorld()->GetAttackOdds( TypeNuke, _defenderType );
        }
    }

    return MovingObject::GetAttackOdds( _defenderType );
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

    if( obj->m_type == TypeCarrier || 
        obj->m_type == TypeAirBase )
    {
        if( obj->m_teamId == m_teamId )
        {
            return TargetTypeLand;
        }
    }


    bool isFriend = g_app->GetWorld()->IsFriend( m_teamId, obj->m_teamId );    
    if( !isFriend )
    {       
        // Nuke mode: static targets only (empty, city, building). Never moving.
        if( !obj->IsMovingObject() &&
            m_states[1]->m_numTimesPermitted > 0 )
        {
            return TargetTypeLaunchNuke;
        }
    }

    return TargetTypeInvalid;
}


bool Bomber::IsActionQueueable()
{
    return ( m_currentState == 1 );  // nuke mode only in DefconExpanded
}


void Bomber::RequestAction( ActionOrder *_action )
{
    if( m_currentState == 1 )
    {
        WorldObject *target = g_app->GetWorld()->GetWorldObject( _action->m_targetObjectId );
        if( target && ( target->IsAircraft() || target->IsMissileClass() ) )
        {
            delete _action;
            return;
        }
        // Only set waypoint to the new target if it becomes our current (first) order.
        // When adding to an existing queue, keep flying to the first target until we launch.
        bool wasQueueEmpty = ( m_actionQueue.Size() == 0 );
        WorldObject::RequestAction( _action );
        if( wasQueueEmpty )
        {
            SetWaypoint( _action->m_longitude, _action->m_latitude );
        }
    }
    else
    {
        WorldObject::RequestAction( _action );
    }
}


void Bomber::CeaseFire( int teamId )
{
    if( m_currentState == 1 && m_actionQueue.Size() > 0 )
    {
        ClearActionQueue();
        Land( GetClosestLandingPad() );
    }
    if( m_nukeTargetLongitude != 0 && m_nukeTargetLatitude != 0 )
    {
        if( g_app->GetWorldRenderer()->IsValidTerritory( teamId, m_nukeTargetLongitude, m_nukeTargetLatitude, false ) )
        {
            m_nukeTargetLongitude = 0;
            m_nukeTargetLatitude = 0;
            m_bombingRun = false;
            Land( GetClosestLandingPad() );
        }
    }
    WorldObject::CeaseFire( teamId );
}


void Bomber::Render2D()
{
    MovingObject::Render2D();

    if( (m_teamId == g_app->GetWorld()->m_myTeamId ||
        g_app->GetWorld()->m_myTeamId == -1 ||
        g_app->GetGame()->m_winner != -1 ) &&
        m_states[1]->m_numTimesPermitted > 0 )
    {
        Fixed predictionTime = Fixed::FromDouble(g_predictionTime) * g_app->GetWorld()->GetTimeScaleFactor();
        float predictedLongitude = (m_longitude + m_vel.x * predictionTime).DoubleValue();
        float predictedLatitude = (m_latitude + m_vel.y * predictionTime).DoubleValue(); 
    
        float angle = atan( -m_vel.x.DoubleValue() / m_vel.y.DoubleValue() );
        if( m_vel.y < 0 ) angle += M_PI;
    
        Team *team = g_app->GetWorld()->GetTeam(m_teamId);
        Colour colour = team->GetTeamColour();            
        colour.m_a = 150;

        Image *bmpImage = g_resource->GetImage( "graphics/smallnuke.bmp", 0.0625f, 0.10f );

        float size = GetSize().DoubleValue() * 0.4f;
        
        g_renderer2d->RotatingSprite( bmpImage, predictedLongitude, 
                                  predictedLatitude,
                                  size/2, 
                                  size/2, 
                                  colour, 
                                  angle );
    }
}

void Bomber::Render3D()
{
    MovingObject::Render3D();

    if( (m_teamId == g_app->GetWorld()->m_myTeamId ||
        g_app->GetWorld()->m_myTeamId == -1 ||
        g_app->GetGame()->m_winner != -1 ) &&
        m_states[1]->m_numTimesPermitted > 0 )
    {
        Fixed predictionTime = Fixed::FromDouble(g_predictionTime) * g_app->GetWorld()->GetTimeScaleFactor();
        float predictedLongitude = (m_longitude + m_vel.x * predictionTime).DoubleValue();
        float predictedLatitude = (m_latitude + m_vel.y * predictionTime).DoubleValue(); 
    
        float angle = atan( -m_vel.x.DoubleValue() / m_vel.y.DoubleValue() );
        if( m_vel.y < 0 ) angle += M_PI;
    
        Team *team = g_app->GetWorld()->GetTeam(m_teamId);
        Colour colour = team->GetTeamColour();            
        colour.m_a = 150;

        Image *bmpImage = g_resource->GetImage( "graphics/smallnuke.bmp", 0.0625f, 0.10f );

        GlobeRenderer *globeRenderer = g_app->GetGlobeRenderer();
        if( globeRenderer && bmpImage )
        {
            float size = GetSize3D().DoubleValue();
            float iconSize = size * 0.4f;

            Vector3<float> spritePos = globeRenderer->ConvertLongLatTo3DPosition(predictedLongitude, predictedLatitude);
            Vector3<float> normal = spritePos;
            normal.Normalise();
            spritePos += normal * GLOBE_ELEVATION;
            
            g_renderer3d->RotatingSprite3D( bmpImage, spritePos.x, spritePos.y, spritePos.z,
                                            iconSize, iconSize, colour, angle, BILLBOARD_SURFACE_ALIGNED );
        }
    }
}

bool Bomber::SetWaypointOnAction()
{
    return true;
}
