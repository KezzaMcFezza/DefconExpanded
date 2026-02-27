#include "lib/universal_include.h"
#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/render2d/renderer_2d.h"
#include "lib/math/vector3.h"
#include "lib/math/random_number.h"
#include "lib/language_table.h"

#include "app/app.h"
#include "app/globals.h"

#include "renderer/world_renderer.h"

#include "interface/interface.h"

#include "world/world.h"
#include "world/tanker.h"
#include "world/team.h"


Tanker::Tanker()
:   MovingObject(),
    m_refuelRange(5)
{
    SetType( TypeTanker );

    strcpy( bmpImageFilename, "graphics/kc10.bmp" );

    m_stealthType = 100;
    m_radarRange = 0;
    m_speed = Fixed::Hundredths(4);
    m_turnRate = Fixed::Hundredths(2);
    m_selectable = true;
    m_maxHistorySize = 10;
    m_range = 360;
    m_maxRange = 360;

    m_movementType = MovementTypeAir;

    m_refuelSlot[0] = -1;
    m_refuelSlot[1] = -1;

    AddState( LANGUAGEPHRASE("state_refuel"), 0, 0, 5, 0, false );

    InitialiseTimers();
}


void Tanker::Action( int targetObjectId, Fixed longitude, Fixed latitude )
{
    m_targetObjectId = -1;

    WorldObject *target = g_app->GetWorld()->GetWorldObject( targetObjectId );
    if( target &&
        target->m_teamId == m_teamId &&
        target->IsAircraftLauncher() )
    {
        Land( targetObjectId );
    }

    if( m_teamId == g_app->GetWorld()->m_myTeamId && targetObjectId == -1 )
    {
        g_app->GetWorldRenderer()->CreateAnimation( WorldRenderer::AnimationTypeActionMarker, m_objectId,
                                                    longitude.DoubleValue(), latitude.DoubleValue() );
    }

    MovingObject::Action( targetObjectId, longitude, latitude );
}

bool Tanker::Update()
{
    MoveToWaypoint();

    Fixed timePerUpdate = SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor();

    RefuelTargets( timePerUpdate );

    for( int s = 0; s < 2; ++s )
    {
        if( m_refuelSlot[s] != -1 )
        {
            WorldObject *obj = g_app->GetWorld()->GetWorldObject( m_refuelSlot[s] );
            if( !obj || !obj->IsMovingObject() || obj->m_life <= 0 )
            {
                ClearSlot( m_refuelSlot[s] );
                continue;
            }
            Fixed dist = g_app->GetWorld()->GetDistance( m_longitude, m_latitude, obj->m_longitude, obj->m_latitude );
            if( dist > m_refuelRange + Fixed(2) )
            {
                ClearSlot( m_refuelSlot[s] );
            }
        }
    }

    if( m_targetLongitude == 0 && m_targetLatitude == 0 )
    {
        if( m_range <= 0 )
        {
            m_life = 0;
            m_lastHitByTeamId = m_teamId;
            g_app->GetWorld()->AddOutOfFueldMessage( m_objectId );
        }
        else
        {
            m_longitude += m_vel.x * Fixed(timePerUpdate);
            m_latitude += m_vel.y * Fixed(timePerUpdate);
            if( m_longitude <= -180 || m_longitude >= 180 )
            {
                CrossSeam();
            }
            if( m_range <= 0 )
            {
                m_life = 0;
                m_lastHitByTeamId = m_teamId;
                g_app->GetWorld()->AddOutOfFueldMessage( m_objectId );
            }
            if( IsIdle() )
            {
                Land( GetClosestLandingPad() );
            }
        }
    }

    int bestTarget = FindBestRefuelTarget();
    if( bestTarget != -1 )
    {
        WorldObject *target = g_app->GetWorld()->GetWorldObject( bestTarget );
        if( target && target->IsMovingObject() )
        {
            bool isLarge = target->IsBomberClass() || target->IsAEWClass() ||
                           target->m_type == TypeTanker;
            if( IsSlotAvailable( isLarge ) )
            {
                AssignSlot( bestTarget, isLarge );
            }
        }
    }

    return MovingObject::Update();
}

void Tanker::RunAI()
{
    if( IsIdle() )
    {
        Land( GetClosestLandingPad() );
    }
}

void Tanker::Land( int targetId )
{
    WorldObject *target = g_app->GetWorld()->GetWorldObject( targetId );
    if( target )
    {
        SetWaypoint( target->m_longitude, target->m_latitude );
        m_isLanding = targetId;
    }
}

int Tanker::GetAttackState()
{
    return -1;
}

int Tanker::IsValidCombatTarget( int _objectId )
{
    WorldObject *obj = g_app->GetWorld()->GetWorldObject( _objectId );
    if( !obj ) return TargetTypeInvalid;

    if( obj->IsAircraftLauncher() &&
        obj->m_teamId == m_teamId )
    {
        return TargetTypeLand;
    }

    return MovingObject::IsValidCombatTarget( _objectId );
}

bool Tanker::IsSlotAvailable( bool largeSlot ) const
{
    if( largeSlot )
        return ( m_refuelSlot[0] == -1 && m_refuelSlot[1] == -1 );
    return ( m_refuelSlot[0] == -1 || m_refuelSlot[1] == -1 );
}

void Tanker::AssignSlot( int objectId, bool largeSlot )
{
    if( largeSlot )
    {
        m_refuelSlot[0] = objectId;
        m_refuelSlot[1] = objectId;
    }
    else
    {
        if( m_refuelSlot[0] == -1 )
            m_refuelSlot[0] = objectId;
        else if( m_refuelSlot[1] == -1 )
            m_refuelSlot[1] = objectId;
    }
}

void Tanker::ClearSlot( int objectId )
{
    if( m_refuelSlot[0] == objectId ) m_refuelSlot[0] = -1;
    if( m_refuelSlot[1] == objectId ) m_refuelSlot[1] = -1;
}

void Tanker::RefuelTargets( Fixed timeStep )
{
    Fixed fuelRate = Fixed(10) * timeStep;

    for( int s = 0; s < 2; ++s )
    {
        if( m_refuelSlot[s] == -1 ) continue;
        if( s == 1 && m_refuelSlot[0] == m_refuelSlot[1] ) continue;

        WorldObject *target = g_app->GetWorld()->GetWorldObject( m_refuelSlot[s] );
        if( !target || !target->IsMovingObject() ) continue;

        MovingObject *aircraft = (MovingObject *)target;
        Fixed maxRange = aircraft->m_maxRange;
        if( maxRange <= 0 ) continue;
        if( aircraft->m_range >= maxRange ) continue;

        Fixed toGive = fuelRate;
        if( aircraft->m_range + toGive > maxRange )
            toGive = maxRange - aircraft->m_range;
        if( toGive > m_range )
            toGive = m_range;
        if( toGive <= 0 ) continue;

        aircraft->m_range += toGive;
        m_range -= toGive;

        if( aircraft->GetSpeed() > m_speed )
        {
            aircraft->m_vel = aircraft->m_vel.Normalized() * m_speed;
        }
    }
}

int Tanker::FindBestRefuelTarget()
{
    int bestId = -1;
    Fixed lowestFuel = Fixed::MAX;

    for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
    {
        if( !g_app->GetWorld()->m_objects.ValidIndex(i) ) continue;
        WorldObject *obj = g_app->GetWorld()->m_objects[i];
        if( !obj || !obj->IsMovingObject() ) continue;
        if( !obj->IsAircraft() ) continue;
        if( obj->m_type == TypeLACM || obj->m_type == TypeLANM ) continue;
        if( obj->m_objectId == m_objectId ) continue;

        if( !g_app->GetWorld()->IsFriend( m_teamId, obj->m_teamId ) ) continue;

        Fixed dist = g_app->GetWorld()->GetDistance( m_longitude, m_latitude, obj->m_longitude, obj->m_latitude );
        if( dist > m_refuelRange ) continue;

        bool alreadySlotted = false;
        for( int s = 0; s < 2; ++s )
        {
            if( m_refuelSlot[s] == obj->m_objectId )
                alreadySlotted = true;
        }
        if( alreadySlotted ) continue;

        MovingObject *aircraft = (MovingObject *)obj;
        Fixed maxRange = aircraft->m_maxRange;
        if( maxRange <= 0 ) continue;
        if( aircraft->m_range >= maxRange ) continue;

        if( obj->m_type == TypeTanker )
        {
            if( aircraft->m_range >= m_range ) continue;
        }

        if( aircraft->m_range < lowestFuel )
        {
            lowestFuel = aircraft->m_range;
            bestId = obj->m_objectId;
        }
    }

    return bestId;
}
