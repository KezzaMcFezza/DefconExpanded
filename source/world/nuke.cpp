#include "lib/universal_include.h"

#include "lib/language_table.h"
#include "lib/sound/soundsystem.h"
#include "lib/math/math_utils.h"

#include "lib/math/vector3.h"
#include "lib/math/random_number.h"
#include "lib/debug/profiler.h"

#include "app/app.h"
#include "app/globals.h"

#include "renderer/world_renderer.h"
#include "renderer/map_renderer.h"
#include "renderer/globe_renderer.h"

#include "world/world.h"
#include "world/nuke.h"
#include "world/team.h"
#include "world/city.h"
#include "world/bomber.h"


Nuke::Nuke()
:   MovingObject(),
    m_prevDistanceToTarget( Fixed::MAX ),
    m_newLongitude(0),
    m_newLatitude(0),
    m_targetLocked(false),
    m_impactzone_x(0.0f), m_impactzone_y(0.0f), m_impactzone_w(0.0f), m_impactzone_h(0.0f),
    m_cointoss_x(1.0f), m_cointoss_y(1.0f),
    m_launchposition_x(0), m_launchposition_y(0)
{
    SetType( TypeNuke );

    m_stealthType = 200;  // early2 - very visible
    strcpy( bmpImageFilename, "graphics/nuke.bmp" );

    m_radarRange = 0;
    m_speed = Fixed::Hundredths(20);
    m_selectable = true;
    m_maxHistorySize = -1;
    m_range = Fixed::MAX;
    m_turnRate = Fixed::Hundredths(1);
    m_movementType = MovementTypeAir;

    AddState( LANGUAGEPHRASE("state_ontarget"), 0, 0, 0, Fixed::MAX, false );
    AddState( LANGUAGEPHRASE("state_disarm"), 100, 0, 0, 0, false );

    m_impactzone_w = (syncfrand(50).DoubleValue() + 10) / 100.0f;
    m_impactzone_h = (syncfrand(50).DoubleValue() + 10) / 100.0f;
    m_impactzone_x = (syncfrand(50).DoubleValue() + 10) / 100.0f;
    m_impactzone_y = (syncfrand(50).DoubleValue() + 10) / 100.0f;
    m_cointoss_x = 1.0f;
    m_cointoss_y = 1.0f;
    if (syncfrand(2).IntValue() < 1) m_cointoss_x = -1.0f;
    if (syncfrand(2).IntValue() < 1) m_cointoss_y = -1.0f;
}


void Nuke::Action( int targetObjectId, Fixed longitude, Fixed latitude )
{
    //m_newLongitude = longitude;
    //m_newLatitude = latitude;
}

void Nuke::SetWaypoint( Fixed longitude, Fixed latitude )
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

    // Choose shortest path (across date line or direct)
    while( longitude < -180 ) longitude += 360;
    while( longitude > 180 ) longitude -= 360;

    Fixed directDiff = (longitude - m_longitude).abs();
    Fixed seamDiff = 360 - directDiff;
    if( seamDiff < directDiff )
    {
        if( longitude > m_longitude )
            longitude -= 360;
        else
            longitude += 360;
    }

    m_targetLongitude = longitude;
    m_targetLatitude = latitude;
    m_launchposition_x = m_longitude;
    m_launchposition_y = m_latitude;

    // Latitude-corrected distance for projection (great circle approximation on flat map)
    Vector3<Fixed> target( m_targetLongitude, m_targetLatitude, 0 );
    Vector3<Fixed> pos( m_longitude, m_latitude, 0 );
    Fixed latFactor = Fixed(90) - ((m_latitude.abs() + m_targetLatitude.abs()) / 2);
    if( latFactor < Fixed(15) ) latFactor = Fixed(15);
    Fixed dx = (target.x - pos.x) * (Fixed(90) / latFactor);
    Fixed dy = target.y - pos.y;
    m_totalDistance = Fixed::FromDouble(sqrt((dx*dx + dy*dy).DoubleValue()));

    // Curve direction (east/west)
    m_curveDirection = (m_targetLongitude >= m_longitude) ? Fixed(1) : Fixed(-1);

    // Hemisphere: curve toward N or S pole for great circle path
    m_curveLatitude = ((m_latitude + m_targetLatitude) >= 0) ? Fixed(1) : Fixed(-1);
}


bool Nuke::Update()
{
    //
    // Are we disarmed?

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


    //
    // Is our waypoint changing?

    Fixed timePerUpdate = SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor();
    
    if( m_newLongitude != 0 || m_newLatitude != 0 )
    {
        Fixed factor1 = m_turnRate * timePerUpdate * 2;
        Fixed factor2 = 1 - factor1;
        m_targetLongitude = ( m_newLongitude * factor1 ) + ( m_targetLongitude * factor2 );
        m_targetLatitude = ( m_newLatitude * factor1 ) + ( m_targetLatitude * factor2 );

        if( ( m_targetLongitude - m_newLongitude ).abs() < 1 &&
			( m_targetLatitude - m_newLatitude ).abs() < 1 )
        {
            SetWaypoint( m_newLongitude, m_newLatitude );
            m_newLongitude = 0;
            m_newLatitude = 0;
        }
    }


    //
    // Move towards target along great circle path (shortest distance on globe)

    Vector3<Fixed> target( m_targetLongitude, m_targetLatitude, 0 );
    Vector3<Fixed> pos( m_longitude, m_latitude, 0 );
    Vector3<Fixed> front = (target - pos).Normalise();

    Fixed remainingDistance = (target - pos).Mag();
    Fixed fractionDistance = m_totalDistance > 0 ? 1 - remainingDistance / m_totalDistance : Fixed::Hundredths(50);

    // Great circle curve: rotate direction toward pole based on hemisphere
    Fixed latCurveFactor = Fixed(5) * m_latitude.abs() / Fixed(90);
    if( latCurveFactor < Fixed(3) ) latCurveFactor = Fixed(3);
    front.RotateAroundZ( Fixed::PI / (latCurveFactor * m_curveDirection * m_curveLatitude) );
    front.RotateAroundZ( fractionDistance * (-m_curveDirection * m_curveLatitude * Fixed::PI / latCurveFactor) );

    // Latitude speed: near poles, same real speed = more map degrees per tick
    Fixed latitudeFactor = Fixed(90) - m_latitude.abs();
    if( latitudeFactor < Fixed(10) ) latitudeFactor = Fixed(10);
    Fixed speedMultiplier = Fixed(1) + (((Fixed(90) / latitudeFactor) - Fixed(1)) * Fixed(2) / Fixed(8));
    if( speedMultiplier > Fixed(3) ) speedMultiplier = Fixed(3);

    m_vel = front * (m_speed * speedMultiplier);

    Fixed newLongitude = m_longitude + m_vel.x * timePerUpdate;
    Fixed newLatitude = m_latitude + m_vel.y * timePerUpdate;
    Fixed newDistance = g_app->GetWorld()->GetDistance( newLongitude, newLatitude, m_targetLongitude, m_targetLatitude);

    if( newLongitude <= -180 ||
        newLongitude >= 180 )
    {
        m_longitude = newLongitude;
        CrossSeam();

        newLongitude = m_longitude;
        newDistance = g_app->GetWorld()->GetDistance( newLongitude, newLatitude, m_targetLongitude, m_targetLatitude );
    }

    if( newDistance < 2 &&
        newDistance >= remainingDistance )
    {
        m_targetLongitude = 0;
        m_targetLatitude = 0;
        m_vel.Zero();
        OnImpact();
#ifdef TOGGLE_SOUND
        g_soundSystem->TriggerEvent( SoundObjectId(m_objectId), "Detonate" );
#endif
        return true;
    }
    else
    {
        m_range -= Vector3<Fixed>( m_vel.x * Fixed(timePerUpdate), m_vel.y * Fixed(timePerUpdate), 0 ).Mag();
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

void Nuke::Render2D()
{
    MovingObject::Render2D();
}

void Nuke::Render3D()
{
    if (g_app->IsGlobeMode()) {
        GlobeRenderer *globeRenderer = g_app->GetGlobeRenderer();
        if (globeRenderer && globeRenderer->ShouldUse3DNukeTrajectories()) {
            return;
        }
    }
    
    MovingObject::Render3D();
}

void Nuke::FindTarget( int team, int targetTeam, int launchedBy, Fixed range, Fixed *longitude, Fixed *latitude )
{
    int objectId = -1;
    Nuke::FindTarget( team, targetTeam, launchedBy, range, longitude, latitude, &objectId );
}

void Nuke::FindTarget( int team, int targetTeam, int launchedBy, Fixed range, Fixed *longitude, Fixed *latitude, int *objectId )
{
    START_PROFILE("Nuke::FindTarget");
    
    WorldObject *launcher = g_app->GetWorld()->GetWorldObject(launchedBy);
    if( !launcher ) 
    {
        END_PROFILE("Nuke::FindTarget");
        return;
    }

    LList<int> validTargets;
        
    for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
    {
        if( g_app->GetWorld()->m_objects.ValidIndex(i) )
        {
            WorldObject *obj = g_app->GetWorld()->m_objects[i];
            if( obj->m_teamId == targetTeam &&
                obj->m_seen[team] &&
                !obj->IsMovingObject() )
            {
                Fixed distanceSqd = g_app->GetWorld()->GetDistanceSqd( launcher->m_longitude, launcher->m_latitude, obj->m_longitude, obj->m_latitude);
                if( distanceSqd <= range * range )
                {                    
                    int numTargetedNukes = CountTargetedNukes( team, obj->m_longitude, obj->m_latitude );
                    
                    if( (obj->IsRadarClass() && numTargetedNukes < 2 ) ||
                        (!obj->IsRadarClass() && numTargetedNukes < 4 ) )
                    {
                        validTargets.PutData(obj->m_objectId);
                    }
                }
            }
        }
    }

    if( validTargets.Size() > 0 )
    {
        int targetId = syncrand() % validTargets.Size();
        int objIndex = validTargets[ targetId ];
        WorldObject *obj = g_app->GetWorld()->GetWorldObject(objIndex);
        if( obj )
        {
            *longitude = obj->m_longitude;
            *latitude = obj->m_latitude;
            *objectId = obj->m_objectId;
            END_PROFILE("Nuke::FindTarget");
            return;
        }
    }

    Team *friendlyTeam = g_app->GetWorld()->GetTeam( team );

    int maxPop = 500000;        // Don't bother hitting cities with less than 0.5M survivors

    for( int i = 0; i < g_app->GetWorld()->m_cities.Size(); ++i )
    {
        if( g_app->GetWorld()->m_cities.ValidIndex(i) )
        {
            City *city = g_app->GetWorld()->m_cities[i];

            if( !g_app->GetWorld()->IsFriend( city->m_teamId, team) && 
				g_app->GetWorld()->GetDistanceSqd( city->m_longitude, city->m_latitude, launcher->m_longitude, launcher->m_latitude) <= range * range)               
            {
                int numTargetedNukes = CountTargetedNukes(team, city->m_longitude, city->m_latitude);
                int estimatedPop = City::GetEstimatedPopulation( team, i, numTargetedNukes );
                if( estimatedPop > maxPop )
                {
                    maxPop = estimatedPop;
                    *longitude = city->m_longitude;
                    *latitude = city->m_latitude;
                    *objectId = -1;
                }
            }
        }
    }
    
    END_PROFILE("Nuke::FindTarget");
}

int Nuke::CountTargetedNukes( int teamId, Fixed longitude, Fixed latitude )
{
    int targetedNukes = 0;
    for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
    {
        if( g_app->GetWorld()->m_objects.ValidIndex(i) )
        {
            if( g_app->GetWorld()->m_objects[i]->IsNuke() )
            {
                MovingObject *obj = (MovingObject *)g_app->GetWorld()->m_objects[i];
                Fixed targetLongitude = obj->m_targetLongitude;
                if( targetLongitude > 180 )
                {
                    targetLongitude -= 360;
                }
                else if( targetLongitude < -180 )
                {
                    targetLongitude += 360;
                }

                if( obj->m_teamId == teamId &&
                    targetLongitude == longitude &&
                    obj->m_targetLatitude == latitude )
                {
                    ++targetedNukes;
                }
            }
            else if( g_app->GetWorld()->m_objects[i]->IsBomberClass() )
            {
                Bomber *obj = (Bomber *)g_app->GetWorld()->m_objects[i];
                if( obj->m_teamId == teamId &&
                    obj->GetNukeTargetLongitude() == longitude &&
                    obj->GetNukeTargetLatitude() == latitude )
                {
                    ++targetedNukes;
                }
            }
            else if( g_app->GetWorld()->m_objects[i]->IsSubmarine() ||
                     g_app->GetWorld()->m_objects[i]->IsSiloClass() )
            {
                WorldObject *obj = g_app->GetWorld()->m_objects[i];
                bool inNukeMode = false;
                if( obj->IsSiloClass() )
                    inNukeMode = ( obj->m_currentState == 0 || obj->m_currentState == 1 );
                else if( obj->IsSubmarine() )
                    inNukeMode = ( obj->m_currentState == 2 || obj->m_currentState == 3 );
                if( obj->m_teamId == teamId && inNukeMode )
                {
                    for( int j = 0; j < obj->m_actionQueue.Size(); ++j )
                    {
                        if( obj->m_actionQueue[j]->m_longitude == longitude &&
                            obj->m_actionQueue[j]->m_latitude == latitude )
                        {
                            targetedNukes++;
                        }
                    }
                }
            }
            else if( g_app->GetWorld()->m_objects[i]->IsAircraftLauncher() )
            {
                WorldObject *obj = g_app->GetWorld()->m_objects[i];
                if( obj->m_teamId == teamId &&
                    obj->m_currentState == 1 )
                {
                    for( int j = 0; j < obj->m_actionQueue.Size(); ++j )
                    {
                        if( obj->m_actionQueue[j]->m_longitude == longitude &&
                            obj->m_actionQueue[j]->m_latitude == latitude )
                        {
                            targetedNukes++;
                        }
                    }
                }
            }
        }
    }
    return targetedNukes;
}


void Nuke::OnImpact()
{
    g_app->GetWorld()->CreateExplosion( m_teamId, m_longitude, m_latitude, 100, -1, true );
}

void Nuke::CeaseFire( int teamId )
{
    if( g_app->GetWorldRenderer()->IsValidTerritory( teamId, m_targetLongitude, m_targetLatitude, false ) )
    {
        SetState(1);
    }
}

int Nuke::IsValidMovementTarget( Fixed longitude, Fixed latitude )
{
    return TargetTypeInvalid;
}

void Nuke::LockTarget()
{
    m_targetLocked = true;
}