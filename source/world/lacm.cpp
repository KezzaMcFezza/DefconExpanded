#include "lib/universal_include.h"

#include "lib/language_table.h"
#include "lib/sound/soundsystem.h"
#include "lib/math/math_utils.h"

#include <math.h>

#include "lib/math/vector3.h"
#include "lib/math/random_number.h"

#include "app/app.h"
#include "app/globals.h"

#include "renderer/map_renderer.h"
#include "renderer/world_renderer.h"
#include "renderer/globe_renderer.h"

#include "world/world.h"
#include "world/lacm.h"
#include "world/team.h"
#include "world/city.h"


LACM::LACM()
:   MovingObject(),
    m_totalDistance(0),
    m_curveDirection(0),
    m_curveLatitude(0),
    m_prevDistanceToTarget( Fixed::MAX ),
    m_newLongitude(0),
    m_newLatitude(0),
    m_targetLocked(false),
    m_origin(-1),
    m_initspeed(0),
    m_missTime( Fixed(15) ),
    m_targetIsShip(false)
{
    SetType( TypeLACM );

    strcpy( bmpImageFilename, "graphics/lacm.bmp" );

    m_stealthType = 66;
    m_radarRange = 0;
    m_speed = Fixed::Hundredths(10);
    m_initspeed = m_speed;
    m_selectable = true;
    m_maxHistorySize = 32;
    m_range = Fixed::MAX;
    m_turnRate = Fixed::Hundredths(1);
    m_movementType = MovementTypeAir;

    AddState( LANGUAGEPHRASE("state_ontarget"), 0, 0, 0, Fixed::MAX, false );
    AddState( LANGUAGEPHRASE("state_disarm"), 100, 0, 0, 0, false );
}


void LACM::Action( int targetObjectId, Fixed longitude, Fixed latitude )
{
    // Guided by Update() when target is ship; no in-flight retargeting for static
}


void LACM::SetWaypoint( Fixed longitude, Fixed latitude )
{
    if( m_targetLocked )
    {
        return;
    }

    if( latitude > 100 || latitude < -100 )
    {
        return;
    }

    ClearWaypoints();

    // Handle longitude wrap-around for shortest path
    while (longitude < -180)
    {
        longitude += 360;
    }
    while (longitude > 180)
    {
        longitude -= 360;
    }

    // Choose shortest path (across date line or direct)
    Fixed directDiff = (longitude - m_longitude).abs();
    Fixed seamDiff = 360 - directDiff;

    if (seamDiff < directDiff)
    {
        if (longitude > m_longitude)
        {
            longitude -= 360;
        }
        else
        {
            longitude += 360;
        }
    }

    m_targetLongitude = longitude;
    m_targetLatitude = latitude;

    // Calculate distance with latitude correction factor (for anti-building curve)
    Vector3<Fixed> target( m_targetLongitude, m_targetLatitude, 0 );
    Vector3<Fixed> pos( m_longitude, m_latitude, 0 );

    Fixed latFactor = Fixed(90) - ((m_latitude.abs() + m_targetLatitude.abs()) / 2);
    if (latFactor < Fixed(15))
    {
        latFactor = Fixed(15);
    }

    Fixed dx = (target.x - pos.x) * (Fixed(90) / latFactor);
    Fixed dy = target.y - pos.y;

    m_totalDistance = Fixed::FromDouble(sqrt((dx*dx + dy*dy).DoubleValue()));

    if( m_targetLongitude >= m_longitude )
    {
        m_curveDirection = 1;
    }
    else
    {
        m_curveDirection = -1;
    }

    if ((m_latitude + m_targetLatitude) >= 0)
    {
        m_curveLatitude = 1;
    }
    else
    {
        m_curveLatitude = -1;
    }
}


bool LACM::Update()
{
    // Disarmed?
    if( m_currentState == 1 && m_stateTimer <= 0 )
    {
        if( m_teamId == g_app->GetWorld()->m_myTeamId )
        {
            char message[64];
            strcpy( message, LANGUAGEPHRASE("message_disarmed") );
            g_app->GetWorld()->AddWorldMessage( m_longitude, m_latitude, m_teamId, message, WorldMessage::TypeObjectState );
        }
        return true;
    }

    WorldObject *targetObj = ( m_targetObjectId != -1 ) ? g_app->GetWorld()->GetWorldObject( m_targetObjectId ) : 0;
    bool isShipTarget = targetObj && ( targetObj->IsCarrierClass() || targetObj->IsBattleShipClass() ||
                                       ( targetObj->IsSubmarine() && !targetObj->IsHiddenFrom() ) );
    bool shipPursuit = isShipTarget || m_targetIsShip;

    if( shipPursuit )
    {
        // Mode 1: Anti-ship - track moving target (LANM/LASM style)
        m_targetIsShip = true;
        if( targetObj )
        {
            m_targetLongitude = targetObj->m_longitude;
            m_targetLatitude = targetObj->m_latitude;

            Fixed currentDistance = g_app->GetWorld()->GetDistance( m_longitude, m_latitude,
                                                                     targetObj->m_longitude, targetObj->m_latitude );
            if( m_prevDistanceToTarget < Fixed::MAX )
            {
                Fixed timePerUpdate = SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor();
                Fixed tolerance = Fixed::Hundredths(1);
                if( currentDistance > m_prevDistanceToTarget + tolerance )
                {
                    m_missTime -= timePerUpdate;
                    if( m_missTime <= 0 )
                    {
                        g_app->GetWorld()->CreateExplosion( m_teamId, m_longitude, m_latitude, 30 );
#ifdef TOGGLE_SOUND
                        g_soundSystem->TriggerEvent( SoundObjectId(m_objectId), "Detonate" );
#endif
                        return true;
                    }
                }
            }
            m_prevDistanceToTarget = currentDistance;
            CombatIntercept( targetObj );
        }
        else
        {
            // Target already dead: fly to last known position
            Fixed currentDistance = g_app->GetWorld()->GetDistance( m_longitude, m_latitude,
                                                                     m_targetLongitude, m_targetLatitude );
            if( m_prevDistanceToTarget < Fixed::MAX )
            {
                Fixed timePerUpdate = SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor();
                Fixed tolerance = Fixed::Hundredths(1);
                if( currentDistance > m_prevDistanceToTarget + tolerance )
                {
                    m_missTime -= timePerUpdate;
                    if( m_missTime <= 0 )
                    {
                        g_app->GetWorld()->CreateExplosion( m_teamId, m_longitude, m_latitude, 30 );
#ifdef TOGGLE_SOUND
                        g_soundSystem->TriggerEvent( SoundObjectId(m_objectId), "Detonate" );
#endif
                        return true;
                    }
                }
            }
            m_prevDistanceToTarget = currentDistance;
        }

        bool reachedTarget = MoveToWaypointAntiShip();
        if( reachedTarget )
        {
            if( targetObj )
            {
                // Impact on ship
                int randomChance = syncfrand(100).IntValue();
                Fixed attackOdds = g_app->GetWorld()->GetAttackOdds( TypeLACM, targetObj->m_type, m_origin );
                if( Fixed(randomChance) < attackOdds )
                {
                    targetObj->m_life--;
                    targetObj->m_life = (targetObj->m_life > 0) ? targetObj->m_life : 0;
                    if( targetObj->m_life == 0 && m_origin != -1 )
                    {
                        WorldObject *origin = g_app->GetWorld()->GetWorldObject( m_origin );
                        if( origin )
                        {
                            origin->SetTargetObjectId(-1);
                            origin->m_isRetaliating = false;
                            targetObj->m_lastHitByTeamId = origin->m_teamId;
                        }
                    }
                }
                g_app->GetWorld()->CreateExplosion( m_teamId, m_longitude, m_latitude, 30, targetObj->m_teamId );
                targetObj->Retaliate( m_origin );
            }
            else
            {
                g_app->GetWorld()->CreateExplosion( m_teamId, m_longitude, m_latitude, 30 );
            }
#ifdef TOGGLE_SOUND
            g_soundSystem->TriggerEvent( SoundObjectId(m_objectId), "Detonate" );
#endif
            return true;
        }
        return MovingObject::Update();
    }

    // Mode 2: Anti-building - shortest-path globe movement (LACM/Nuke style)
    m_targetIsShip = false;

    Fixed timePerUpdate = SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor();

    Vector3<Fixed> target( m_targetLongitude, m_targetLatitude, 0 );
    Vector3<Fixed> pos( m_longitude, m_latitude, 0 );
    Vector3<Fixed> front = (target - pos).Normalise();

    Fixed remainingDistance = (target - pos).Mag();
    Fixed fractionDistance = m_totalDistance > 0 ? 1 - remainingDistance / m_totalDistance : Fixed::Hundredths(50);

    Fixed latCurveFactor = Fixed(5) * m_latitude.abs() / Fixed(90);
    if (latCurveFactor < Fixed(3))
    {
        latCurveFactor = Fixed(3);
    }
    front.RotateAroundZ(Fixed::PI / (latCurveFactor * m_curveDirection * m_curveLatitude));
    front.RotateAroundZ(fractionDistance * (-m_curveDirection * m_curveLatitude * Fixed::PI / latCurveFactor));

    if (pos.y > 85)
    {
        Fixed extremeFractionNorth = (pos.y - 85) / 50;
        Clamp(extremeFractionNorth, Fixed(0), Fixed(1));
        front.y *= (1 - extremeFractionNorth);
        front.Normalise();
    }
    else if (pos.y < -85)
    {
        Fixed extremeFractionSouth = ((-pos.y) - 85) / 50;
        Clamp(extremeFractionSouth, Fixed(0), Fixed(1));
        front.y *= (1 - extremeFractionSouth);
        front.Normalise();
    }

    Fixed latitudeFactor = Fixed(90) - m_latitude.abs();
    if (latitudeFactor < Fixed(10))
    {
        latitudeFactor = Fixed(10);
    }
    Fixed speedMultiplier = Fixed(1) + (((Fixed(90)/latitudeFactor)-Fixed(1))*Fixed(2)/Fixed(8));
    if (speedMultiplier > Fixed(3))
    {
        speedMultiplier = Fixed(3);
    }

    m_vel = front * (m_speed * speedMultiplier);

    Fixed newLongitude = m_longitude + m_vel.x * timePerUpdate;
    Fixed newLatitude  = m_latitude  + m_vel.y * timePerUpdate;

    if( newLongitude <= -180 ||
        newLongitude >= 180 )
    {
        m_longitude = newLongitude;
        CrossSeam();
        newLongitude = m_longitude;
    }

    Fixed newDistance = g_app->GetWorld()->GetDistance( newLongitude, newLatitude, m_targetLongitude, m_targetLatitude);

    if( newDistance < 2 &&
        newDistance >= remainingDistance )
    {
        m_targetLongitude = 0;
        m_targetLatitude = 0;
        m_vel.Zero();
        g_app->GetWorld()->CreateExplosion( m_teamId, m_longitude, m_latitude, 30 );
#ifdef TOGGLE_SOUND
        g_soundSystem->TriggerEvent( SoundObjectId(m_objectId), "Detonate" );
#endif
        return true;
    }
    else
    {
        m_range -= ( m_vel * Fixed(timePerUpdate) ).Mag();
        if( m_range <= 0 )
        {
            m_life = 0;
            m_lastHitByTeamId = -1;
            g_app->GetWorld()->AddOutOfFueldMessage( m_objectId );
        }
        m_longitude = newLongitude;
        m_latitude = newLatitude;
    }

    return MovingObject::Update();
}


bool LACM::MoveToWaypointAntiShip()
{
    Fixed distToTarget = g_app->GetWorld()->GetDistance( m_longitude, m_latitude, m_targetLongitude, m_targetLatitude );
    Fixed timePerUpdate = SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor();

    Fixed newLongitude, newLatitude, newDistance;
    CalculateNewPositionAntiShip( &newLongitude, &newLatitude, &newDistance );

    if( m_longitude <= -180 || m_longitude >= 180 )
    {
        CrossSeam();
        CalculateNewPositionAntiShip( &newLongitude, &newLatitude, &newDistance );
    }

    if( newDistance <= distToTarget && newDistance < m_initspeed )
    {
        ClearWaypoints();
        m_vel.Zero();
        return true;
    }

    m_range -= Vector3<Fixed>( m_vel.x * Fixed(timePerUpdate), m_vel.y * Fixed(timePerUpdate), 0 ).Mag();
    if( m_range <= 0 )
    {
        m_life = 0;
        m_lastHitByTeamId = -1;
        g_app->GetWorld()->AddOutOfFueldMessage( m_objectId );
    }
    m_longitude = newLongitude;
    m_latitude = newLatitude;
    return false;
}


void LACM::CalculateNewPositionAntiShip( Fixed *newLongitude, Fixed *newLatitude, Fixed *newDistance )
{
    World::SanitiseTargetLongitude( m_longitude, m_targetLongitude );
    Vector3<Fixed> targetDir = (Vector3<Fixed>( m_targetLongitude, m_targetLatitude, 0 ) -
                                Vector3<Fixed>( m_longitude, m_latitude, 0 )).Normalise();

    Fixed latitudeFactor = Fixed(90) - m_latitude.abs();
    if (latitudeFactor < Fixed(10)) latitudeFactor = Fixed(10);
    Fixed speedMultiplier = Fixed(1) + (((Fixed(90)/latitudeFactor)-Fixed(1))*Fixed(2)/Fixed(8));
    if (speedMultiplier > Fixed(3)) speedMultiplier = Fixed(3);

    Fixed timePerUpdate = SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor();
    // LACM is slower than gunfire but ships are much slower than aircraft - use higher turn rate
    // than before (35 vs 15) so the missile can track moving ships; gunfire uses 80 for aircraft.
    Fixed lacmTurnRate = Fixed::Hundredths(35);
    Fixed lacmSpeed = m_initspeed;
    Fixed factor1 = lacmTurnRate * timePerUpdate / 10;
    Fixed factor2 = 1 - factor1;
    m_vel = ( targetDir * factor1 ) + ( m_vel * factor2 );
    m_vel.Normalise();
    m_vel *= lacmSpeed * speedMultiplier;

    *newLongitude = m_longitude + m_vel.x * timePerUpdate;
    *newLatitude = m_latitude + m_vel.y * timePerUpdate;
    *newDistance = g_app->GetWorld()->GetDistance( *newLongitude, *newLatitude, m_targetLongitude, m_targetLatitude );
}


void LACM::Render2D()
{
    MovingObject::Render2D();
}


void LACM::Render3D()
{
    MovingObject::Render3D();
}


void LACM::LockTarget()
{
    m_targetLocked = true;
}


void LACM::FindTarget( int team, int targetTeam, int launchedBy, Fixed range, Fixed *longitude, Fixed *latitude )
{
    int objectId = -1;
    LACM::FindTarget( team, targetTeam, launchedBy, range, longitude, latitude, &objectId );
}


void LACM::FindTarget( int team, int targetTeam, int launchedBy, Fixed range, Fixed *longitude, Fixed *latitude, int *objectId )
{
    WorldObject *launcher = g_app->GetWorld()->GetWorldObject(launchedBy);
    if( !launcher )
        return;

    Fixed rangeSqd = range * range;
    LList<int> validTargets;

    for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
    {
        if( !g_app->GetWorld()->m_objects.ValidIndex(i) ) continue;
        WorldObject *obj = g_app->GetWorld()->m_objects[i];
        if( obj->m_teamId != targetTeam || !obj->m_seen[team] || obj->IsMovingObject() )
            continue;
        Fixed distanceSqd = g_app->GetWorld()->GetDistanceSqd( launcher->m_longitude, launcher->m_latitude, obj->m_longitude, obj->m_latitude );
        if( distanceSqd > rangeSqd )
            continue;
        int numTargetedLACM = CountTargetedLACM( team, obj->m_longitude, obj->m_latitude );
        if( ( obj->IsRadarClass() && numTargetedLACM < 2 ) ||
            ( !obj->IsRadarClass() && numTargetedLACM < 4 ) )
        {
            validTargets.PutData( obj->m_objectId );
        }
    }

    if( validTargets.Size() > 0 )
    {
        int targetId = syncrand() % validTargets.Size();
        WorldObject *obj = g_app->GetWorld()->GetWorldObject( validTargets[ targetId ] );
        if( obj )
        {
            *longitude = obj->m_longitude;
            *latitude = obj->m_latitude;
            *objectId = obj->m_objectId;
            return;
        }
    }

    int maxPop = 0;
    *objectId = -1;

    for( int i = 0; i < g_app->GetWorld()->m_cities.Size(); ++i )
    {
        if( !g_app->GetWorld()->m_cities.ValidIndex(i) ) continue;
        City *city = g_app->GetWorld()->m_cities[i];
        if( g_app->GetWorld()->IsFriend( city->m_teamId, team ) )
            continue;
        if( g_app->GetWorld()->GetDistanceSqd( city->m_longitude, city->m_latitude, launcher->m_longitude, launcher->m_latitude ) > rangeSqd )
            continue;
        int numTargetedLACM = CountTargetedLACM( team, city->m_longitude, city->m_latitude );
        int estimatedPop = City::GetEstimatedPopulation( team, i, numTargetedLACM );
        if( estimatedPop > maxPop )
        {
            maxPop = estimatedPop;
            *longitude = city->m_longitude;
            *latitude = city->m_latitude;
        }
    }
}


int LACM::CountTargetedLACM( int teamId, Fixed longitude, Fixed latitude )
{
    int targetedLACM = 0;
    for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
    {
        if( !g_app->GetWorld()->m_objects.ValidIndex(i) ) continue;
        WorldObject *obj = g_app->GetWorld()->m_objects[i];
        if( obj->m_type != WorldObject::TypeLACM )
            continue;
        MovingObject *mov = (MovingObject *)obj;
        Fixed targetLongitude = mov->m_targetLongitude;
        if( targetLongitude > 180 ) targetLongitude -= 360;
        else if( targetLongitude < -180 ) targetLongitude += 360;
        if( obj->m_teamId == teamId &&
            targetLongitude == longitude &&
            mov->m_targetLatitude == latitude )
        {
            targetedLACM++;
        }
    }
    return targetedLACM;
}
