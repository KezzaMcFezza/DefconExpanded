#include "lib/universal_include.h"

#include "lib/math/vector3.h"
#include "lib/language_table.h"

#include "app/app.h"
#include "app/globals.h"

#include "renderer/world_renderer.h"
#include "renderer/map_renderer.h"

#include "world/world.h"
#include "world/fighter.h"


Fighter::Fighter()
:   MovingObject()
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

    AddState( LANGUAGEPHRASE("state_attack"), 60, 20, 5, 10, true );

    InitialiseTimers();
}


void Fighter::AcquireTargetFromAction( ActionOrder *action )
{
    if( m_currentState != 0 || action->m_targetObjectId == -1 ) return;
    WorldObject *target = g_app->GetWorld()->GetWorldObject( action->m_targetObjectId );
    if( target &&
        target->m_visible[m_teamId] &&
        g_app->GetWorld()->GetAttackOdds( m_type, target->m_type ) > 0 )
    {
        m_targetObjectId = action->m_targetObjectId;
    }
}

void Fighter::Action( int targetObjectId, Fixed longitude, Fixed latitude )
{   
    m_targetObjectId = -1;
    m_opportunityFireOnly = false;

    if( m_currentState == 0 )
    {
        //SetWaypoint( longitude, latitude );
        WorldObject *target = g_app->GetWorld()->GetWorldObject( targetObjectId );
        if( target )
        {
            if( target->m_visible[m_teamId] &&
                g_app->GetWorld()->GetAttackOdds( m_type, target->m_type ) > 0)
            {
                m_targetObjectId = targetObjectId;
            }
        }
    }

    if( m_teamId == g_app->GetWorld()->m_myTeamId &&
        targetObjectId == -1 )
    {
        g_app->GetWorldRenderer()->CreateAnimation( WorldRenderer::AnimationTypeActionMarker, m_objectId,
												  longitude.DoubleValue(), latitude.DoubleValue() );
    }

    MovingObject::Action( targetObjectId, longitude, latitude );
}

bool Fighter::Update()
{   
    bool arrived = MoveToWaypoint();
    bool holdingPattern = false;

    if( m_targetObjectId != -1 )
    {
        WorldObject *targetObject = g_app->GetWorld()->GetWorldObject(m_targetObjectId);  
        if( targetObject )
        {
            if( targetObject->m_teamId == m_teamId )
            {
                if( targetObject->IsAircraftLauncher() )
                {
                    Land( m_targetObjectId );
                }
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
                            FireGun( GetActionRange() );
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
        // On patrol: opportunity fire at hostiles in range without changing course
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
                    FireGun( GetActionRange() );
                    m_stateTimer = m_states[ m_currentState ]->m_timeToReload;
                    BurstFireOnFired( targetId );
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

    if( IsIdle() )
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


int Fighter::IsValidCombatTarget( int _objectId )
{
    WorldObject *obj = g_app->GetWorld()->GetWorldObject( _objectId );
    if( !obj ) return TargetTypeInvalid;

    if( obj->m_type == TypeCarrier || 
        obj->m_type == TypeAirBase )
    {
        if( obj->m_teamId == m_teamId )
        {
            return TargetTypeLand;
        }
    }

    return MovingObject::IsValidCombatTarget( _objectId );
}


bool Fighter::SetWaypointOnAction()
{
    return true;
}

