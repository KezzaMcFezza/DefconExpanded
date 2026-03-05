#include "lib/universal_include.h"


#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/math/math_utils.h"
#include "lib/math/random_number.h"
#include "lib/sound/soundsystem.h"
#include "lib/preferences.h"
#include "lib/render3d/renderer_3d.h"

#include "app/globals.h"
#include "app/app.h"
#include "app/game.h"

#include "renderer/world_renderer.h"
#include "renderer/map_renderer.h"
#include "renderer/globe_renderer.h"

#include "world/tanker.h"


#include "interface/interface.h"

#include "world/world.h"
#include "world/movingobject.h"
#include "world/airbase.h"
#include "world/fleet.h"


MovingObject::MovingObject()
:   WorldObject(),
    m_speed(0),
    m_historyTimer(0),
    m_maxHistorySize(10),
    m_targetLongitude(0),
    m_targetLatitude(0),
    m_movementType(MovementTypeLand),
    m_range(0),
    m_maxRange(0),
    m_targetNodeId(-1),
    m_finalTargetLongitude(0),
    m_finalTargetLatitude(0),
    m_pathCalcTimer(1),
    m_targetLongitudeAcrossSeam(0),
    m_targetLatitudeAcrossSeam(0),
    m_blockHistory(false),
    m_isLanding(-1),
    m_isEscorting(-1),
    m_turning(false),
    m_angleTurned(0)
{
}

MovingObject::~MovingObject()
{
    m_history.EmptyAndDelete();
    //m_movementBlips.EmptyAndDelete();
}


void MovingObject::InitialiseTimers()
{
    WorldObject::InitialiseTimers();

    Fixed gameScale = World::GetUnitScaleFactor();
    m_speed /= gameScale;
    //m_range /= gameScale;
    m_maxRange = m_range;
}


bool MovingObject::Update()
{
    //
    // Update history


    m_historyTimer -= SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor() / 10;
    if( m_historyTimer <= 0 )
    {
        m_history.PutDataAtStart( new Vector3<Fixed>(m_longitude, m_latitude, 0) );
        m_historyTimer = 2; 

        GlobeRenderer *globeRenderer = g_app->GetGlobeRenderer();
        if( globeRenderer && globeRenderer->ShouldUse3DNukeTrajectories() )
        {
            if( IsBallisticMissileClass() )
            {
                m_historyTimer = 1;
            }
        }
    }
    

    while( m_maxHistorySize != -1 && 
           m_history.ValidIndex(m_maxHistorySize) )
    {
        delete m_history[m_maxHistorySize];
        m_history.RemoveData(m_maxHistorySize);
    }


    for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
    {
        Team *team = g_app->GetWorld()->m_teams[i];
        m_lastSeenTime[team->m_teamId] -= g_app->GetWorld()->GetTimeScaleFactor() * SERVER_ADVANCE_PERIOD ;
        if( m_lastSeenTime[team->m_teamId] < 0 ) m_lastSeenTime[team->m_teamId] = 0;
    }

    if( m_isLanding != -1 )
    {
        WorldObject *home = g_app->GetWorld()->GetWorldObject( m_isLanding );
        if( home )
        {
            if( home->m_type == TypeTanker &&
                g_app->GetWorld()->IsFriend( m_teamId, home->m_teamId ) )
            {
                Land( m_isLanding );
            }
            else if( IsBomberClass() && home->IsCarrierClass() )
            {
                Land( GetClosestLandingPad() );
            }
            else if( IsFighterClass() && ( m_type == TypeFighterLight || m_type == TypeFighterStealth ) &&
                     home->IsCarrierClass() )
            {
                // Stealth and light fighters cannot land on carriers
                Land( GetClosestLandingPad() );
            }
            else if( ( IsFighterClass() &&
                home->GetFighterCount() >= home->m_maxFighters ) ||
                ( IsBomberClass() &&
                home->m_maxBombers > 0 &&
                home->GetBomberCount() >= home->m_maxBombers ) ||
                ( IsAEWClass() &&
                home->m_maxAEW > 0 &&
                home->GetAEWCount() >= home->m_maxAEW ))
            {
                Land( GetClosestLandingPad() );
            }            
            else if( g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, home->m_longitude, home->m_latitude ) < ( Fixed( 2 ) / World::GetGameScale() ) * ( Fixed( 2 ) / World::GetGameScale() ) )
            {
                if( home->m_teamId == m_teamId )
                {
                    bool landed = false;
                    if( IsFighterClass() )
                    {
                        if( home->GetFighterCount() < home->m_maxFighters )
                        {
                            home->OnFighterLanded( m_type );
                            landed = true;
                        }
                    }
                    else if( IsBomberClass() )
                    {
                        if( home->m_maxBombers > 0 && home->GetBomberCount() < home->m_maxBombers )
                        {
                            home->OnBomberLanded( m_type );
                            if( home->m_nukeSupply >= 0 )
                                home->m_nukeSupply += m_states[1]->m_numTimesPermitted;
                            landed = true;
                        }
                    }
                    else if( m_type == WorldObject::TypeTanker )
                    {
                        if( home->IsAirbaseClass() )
                        {
                            AirBase *airbase = (AirBase *)home;
                            airbase->OnTankerLanded();
                            landed = true;
                        }
                    }
                    else if( IsAEWClass() )
                    {
                        if( home->m_maxAEW > 0 && home->GetAEWCount() < home->m_maxAEW )
                        {
                            home->OnAEWLanded();
                            landed = true;
                        }
                    }
                    if( landed )
                    {
                        m_life = 0;
						m_lastHitByTeamId = -1;
                    }
                    else
                    {
                        Land( GetClosestLandingPad() );
                    }
                }
            }
            else if( home->IsMovingObject() &&
                home->m_vel.MagSquared() > 0 )
            {
                Land( m_isLanding );
            }
        }
        else
        {
            m_isLanding = -1;
        }
    }

    if( m_isEscorting != -1 )
    {
        WorldObject *escortTarget = g_app->GetWorld()->GetWorldObject( m_isEscorting );
        if( escortTarget && escortTarget->m_life > 0 )
        {
            int savedEscort = m_isEscorting;
            SetWaypoint( escortTarget->m_longitude, escortTarget->m_latitude );
            m_isEscorting = savedEscort;
        }
        else
        {
            m_isEscorting = -1;
        }
    }

    for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
    {
        Team *team = g_app->GetWorld()->m_teams[i];
        if( m_lastSeenTime[team->m_teamId] <= 0 )
        {
            m_seen[team->m_teamId] = false;
        }
    }
 
    return WorldObject::Update();
}

bool MovingObject::IsValidPosition ( Fixed longitude, Fixed latitude )
{
    bool validWaypoint = false;

    if( m_movementType == MovementTypeLand && !g_app->GetWorldRenderer()->IsValidTerritory( -1, longitude, latitude, false ) ) validWaypoint = true;
    if( m_movementType == MovementTypeSea && g_app->GetWorldRenderer()->IsValidTerritory( -1, longitude, latitude, true ) ) validWaypoint = true;
    if( m_movementType == MovementTypeAir ) validWaypoint = true;

    return validWaypoint;
}

void MovingObject::SetWaypoint( Fixed longitude, Fixed latitude )
{
    if( latitude > 100 || latitude < -100 )
    {
        return;
    }

    // stop the unit before setting a new course
    ClearWaypoints();
       
    if( m_movementType == MovementTypeAir )
    {
        Fixed directDistanceSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, longitude, latitude, true);
        Fixed distanceAcrossSeamSqd = g_app->GetWorld()->GetDistanceAcrossSeamSqd( m_longitude, m_latitude, longitude, latitude);

        if( distanceAcrossSeamSqd < directDistanceSqd )
        {
            Fixed targetSeamLatitude;
            Fixed targetSeamLongitude;
            g_app->GetWorld()->GetSeamCrossLatitude( Vector3<Fixed>( longitude, latitude, 0 ), 
                                                     Vector3<Fixed>(m_longitude, m_latitude, 0), 
                                                     &targetSeamLongitude, &targetSeamLatitude);
            if(targetSeamLongitude < 0)
            {
                longitude -= 360;
            }
            else 
            {
                longitude += 360;
            }
        }

    }
    m_targetLongitude = longitude;
    m_targetLatitude = latitude;
}


char *HashDouble( double value, char *buffer );


void FFClamp( Fixed &f, unsigned long long clamp )
{
    union {
        double d;
        unsigned long long l;
    } value;

    value.d = f.DoubleValue();
    value.l &= ~(unsigned long long)clamp;

    f = Fixed::FromDouble( value.d );
}



//void MovingObject::CalculateNewPosition( Fixed *newLongitude, Fixed *newLatitude, Fixed *newDistance )
//{
//    if( m_longitude > -180 && m_longitude < 0 && m_targetLongitude > 180 )
//    {
//        m_targetLongitude -= 360;
//    }
//    else if( m_longitude < 180 && m_longitude > 0 && m_targetLongitude < -180 )
//    {
//        m_targetLongitude += 360;
//    }
//    
//    Vector3<Fixed> targetDir = (Vector3<Fixed>( m_targetLongitude, m_targetLatitude, 0 ) -
//								Vector3<Fixed>( m_longitude, m_latitude, 0 )).Normalise();
//    Vector3<Fixed> originalTargetDir = targetDir;
//
//    Fixed timePerUpdate = SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor();
//    
//    Fixed factor1 = m_turnRate * timePerUpdate / 10;
//    Fixed factor2 = 1 - factor1;
//
//    {
//        char buffer1[32];
//        char buffer2[32];
//        SyncRandLog( "a: %s %s\n", HashDouble( m_vel.x.DoubleValue(), buffer1 ), HashDouble( m_vel.y.DoubleValue(), buffer2 ) );
//    }
//
//    m_vel = ( targetDir * factor1 ) + ( m_vel * factor2 );
//
//    FFClamp(m_vel.x, 0xF);
//    FFClamp(m_vel.y, 0xF);
//    FFClamp(m_vel.z, 0xF);
//
//
//    {
//        char buffer1[32];
//        char buffer2[32];
//        char buffer3[32];
//        char buffer4[32];
//        char buffer5[32];
//        char buffer6[32];
//        SyncRandLog( "a1: %s %s %s %s %s %s\n", 
//                            HashDouble( factor1.DoubleValue(), buffer3 ),
//                            HashDouble( factor2.DoubleValue(), buffer4 ),
//                            HashDouble( targetDir.x.DoubleValue(), buffer5 ),
//                            HashDouble( targetDir.y.DoubleValue(), buffer6 ),
//                            HashDouble( m_vel.x.DoubleValue(), buffer1 ), 
//                            HashDouble( m_vel.y.DoubleValue(), buffer2 ) );
//    }
//
//    //m_vel.Normalise();
//
//    Fixed lenSqrd = m_vel.x*m_vel.x + m_vel.y*m_vel.y + m_vel.z*m_vel.z;
//    if (lenSqrd > 0)
//    {
//        Fixed len = sqrt(lenSqrd);
//        Fixed invLen = 1 / len;
//
//        {
//            char buffer1[32];
//            char buffer2[32];
//            char buffer3[32];
//            SyncRandLog( "a2: %s %s %s\n", 
//                            HashDouble( lenSqrd.DoubleValue(), buffer1 ), 
//                            HashDouble( len.DoubleValue(), buffer2 ),
//                            HashDouble( invLen.DoubleValue(), buffer3 ) );
//        }
//
//        m_vel.x *= invLen;
//        m_vel.y *= invLen;
//        m_vel.z *= invLen;
//    }
//    else
//    {
//        m_vel.x = m_vel.y = 0;
//        m_vel.z = 1;
//    }
//
//    FFClamp(m_vel.x, 0xFF);
//    FFClamp(m_vel.y, 0xFF);
//    FFClamp(m_vel.z, 0xFF);
//
//    {
//        char buffer1[32];
//        char buffer2[32];
//        SyncRandLog( "b: %s %s\n", HashDouble( m_vel.x.DoubleValue(), buffer1 ), HashDouble( m_vel.y.DoubleValue(), buffer2 ) );
//    }
//
//	Fixed dotProduct = originalTargetDir * m_vel;
//    Fixed angle = acos( dotProduct );
//
//    if( dotProduct < Fixed::FromDouble(-0.98) )
//    {
//        m_turning = true;
//    }
//
//    if( m_turning )
//    {
//        Fixed turn = (angle / 50) * timePerUpdate;
//        m_vel.RotateAroundZ( turn );
//        m_vel.Normalise();
//        m_angleTurned += turn;
//        if( turn > Fixed::Hundredths(12) )
//        {
//            m_turning = false;
//            m_angleTurned = 0;
//        }
//    }
//
//    m_vel *= m_speed;
//
//    FFClamp(m_vel.x, 0xFFF);
//    FFClamp(m_vel.y, 0xFFF);
//    FFClamp(m_vel.z, 0xFFF);
//
//    {
//        char buffer1[32];
//        char buffer2[32];
//        SyncRandLog( "c: %s %s\n", HashDouble( m_vel.x.DoubleValue(), buffer1 ), HashDouble( m_vel.y.DoubleValue(), buffer2 ) );
//    }
//
//    *newLongitude = m_longitude + m_vel.x * Fixed(timePerUpdate);
//    *newLatitude = m_latitude + m_vel.y * Fixed(timePerUpdate);
//    *newDistance = g_app->GetWorld()->GetDistance( *newLongitude, *newLatitude, m_targetLongitude, m_targetLatitude );
//}


void MovingObject::CalculateNewPosition( Fixed *newLongitude, Fixed *newLatitude, Fixed *newDistance )
{
    if( m_longitude > -180 && m_longitude < 0 && m_targetLongitude > 180 )
    {
        m_targetLongitude -= 360;
    }
    else if( m_longitude < 180 && m_longitude > 0 && m_targetLongitude < -180 )
    {
        m_targetLongitude += 360;
    }

    Vector3<Fixed> targetDir = (Vector3<Fixed>( m_targetLongitude, m_targetLatitude, 0 ) -
                                Vector3<Fixed>( m_longitude, m_latitude, 0 )).Normalise();
    Vector3<Fixed> originalTargetDir = targetDir;

    Fixed timePerUpdate = SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor();

    Fixed factor1 = m_turnRate * timePerUpdate / 10;
    if( factor1 > 1 ) factor1 = 1;
    Fixed factor2 = 1 - factor1;

    m_vel = ( targetDir * factor1 ) + ( m_vel * factor2 );
    m_vel.Normalise();

    Fixed dotProduct = originalTargetDir * m_vel;

    if( dotProduct < Fixed::FromDouble(-0.98) )
    {
        m_turning = true;
    }

    if( m_turning )
    {
        Fixed angle = acos( dotProduct );
        Fixed turn = (angle / 50) * timePerUpdate;
        m_vel.RotateAroundZ( turn );
        m_vel.Normalise();
        m_angleTurned += turn;
        if( turn > Fixed::Hundredths(12) )
        {
            m_turning = false;
            m_angleTurned = 0;
        }
    }

    m_vel *= m_speed;

    // Latitude-based speed: map is a projection, so near poles same real distance spans more map degrees.
    // Units should appear to move faster (cross more map per tick) to simulate constant real speed.
    Fixed latitudeFactor = Fixed(90) - m_latitude.abs();
    if( latitudeFactor < Fixed(10) ) latitudeFactor = Fixed(10);
    Fixed speedMult = Fixed(1) + (((Fixed(90) / latitudeFactor) - Fixed(1)) * Fixed(2) / Fixed(8));
    if( speedMult > Fixed(3) ) speedMult = Fixed(3);
    m_vel *= speedMult;

    *newLongitude = m_longitude + m_vel.x * Fixed(timePerUpdate);
    *newLatitude = m_latitude + m_vel.y * Fixed(timePerUpdate);
    *newDistance = g_app->GetWorld()->GetDistance( *newLongitude, *newLatitude, m_targetLongitude, m_targetLatitude );
}


bool MovingObject::MoveToWaypoint()
{
    Fixed timePerUpdate = SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor();
    m_range -= Vector3<Fixed>( m_vel.x * Fixed(timePerUpdate), m_vel.y * Fixed(timePerUpdate), 0 ).Mag();
    if( m_targetLongitude != 0 &&
        m_targetLatitude != 0 )
    {
        Fixed distToTarget = g_app->GetWorld()->GetDistance( m_longitude, m_latitude, m_targetLongitude, m_targetLatitude);

        Fixed newLongitude;
        Fixed newLatitude;
        Fixed newDistance;

        Fixed savedSpeed = m_speed;
        if( m_isLanding != -1 )
        {
            WorldObject *landTarget = g_app->GetWorld()->GetWorldObject( m_isLanding );
            if( landTarget && landTarget->m_type == TypeTanker && landTarget->IsMovingObject() &&
                g_app->GetWorld()->IsFriend( m_teamId, landTarget->m_teamId ) )
            {
                Tanker *tanker = (Tanker *)landTarget;
                if( tanker->IsRefuelingTarget( m_objectId ) &&
                    m_speed > tanker->m_speed )
                {
                    m_speed = tanker->m_speed;
                }
            }
        }
        CalculateNewPosition( &newLongitude, &newLatitude, &newDistance );
        m_speed = savedSpeed;

        // if the unit has reached the edge of the map, move it to the other side and update all nessecery information
        if( newLongitude <= -180 ||
            newLongitude >= 180 )
        {
            m_longitude = newLongitude;
            CrossSeam();
            newLongitude = m_longitude;
            newDistance = g_app->GetWorld()->GetDistance( newLongitude, newLatitude, m_targetLongitude, m_targetLatitude );
        }

        // Aircraft overshoot: don't clear waypoints - apply the move so they fly past and circle back
        bool aircraftOvershoot = ( m_movementType == MovementTypeAir && newDistance < 3 && newDistance > distToTarget );
        if( aircraftOvershoot )
        {
            if( m_range <= 0 )
            {
                m_life = 0;
                m_lastHitByTeamId = m_teamId;
                g_app->GetWorld()->AddOutOfFueldMessage( m_objectId );
            }
            m_longitude = newLongitude;
            m_latitude = newLatitude;
            return false;   // not arrived - keep waypoint so we turn and circle back
        }

        // Aircraft with queued attack or bomber in nuke mode with ammo: don't clear waypoints on arrival - keeps flying/orbiting until reload allows firing
        bool hasQueue = m_actionQueue.Size() > 0;
        bool bomberWithAmmo = ( IsBomberClass() && m_states.ValidIndex( m_currentState ) &&
            m_currentState == 1 &&
            m_states[m_currentState]->m_numTimesPermitted != -1 &&
            m_states[m_currentState]->m_numTimesPermitted - (int)m_actionQueue.Size() > 0 );
        bool aircraftWithQueue = ( m_movementType == MovementTypeAir && ( hasQueue || bomberWithAmmo ) );
        if( aircraftWithQueue && newDistance < timePerUpdate * m_speed * Fixed::Hundredths(50) )
        {
            m_longitude = newLongitude;
            m_latitude = newLatitude;
            return true;   // arrived but waypoint stays set - we're in position to fire when reload allows
        }

        if( (newDistance < timePerUpdate * m_speed * Fixed::Hundredths(50)) ||
            (m_movementType == MovementTypeSea &&
             newDistance < 1 &&
             newDistance > distToTarget))
        {
            ClearWaypoints();
            if( m_movementType == MovementTypeSea )
            {
                m_vel.Zero();
            }
            return true;
        }
        else
        {
            if( m_range <= 0 )
            {
                m_life = 0;
				m_lastHitByTeamId = m_teamId;
                g_app->GetWorld()->AddOutOfFueldMessage( m_objectId );
            }
            m_longitude = newLongitude;
            m_latitude = newLatitude;
        }
        return false;
    }
    return true;
    
}

void MovingObject::CrossSeam()
{
    if( m_longitude >= 180 )
    {
        Fixed amountCrossed = m_longitude - 180;
        m_longitude = -180 + amountCrossed;
    }
    else if( m_longitude <= -180 )
    {
        Fixed amountCrossed = m_longitude + 180;
        m_longitude = 180 + amountCrossed;
    }

    if( m_targetLongitude > 180 )
    {
        m_targetLongitude -= 360;
    }
    else if( m_targetLongitude < -180 )
    {
        m_targetLongitude += 360;
    }

    if( !IsBallisticMissileClass() )
    {
        // ballistic missiles have their own separate calculation for this
        Fixed seamDistanceSqd =  g_app->GetWorld()->GetDistanceAcrossSeamSqd( m_longitude, m_latitude, m_targetLongitude, m_targetLatitude );
        Fixed directDistanceSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, m_targetLongitude, m_targetLatitude, true );

        if( seamDistanceSqd < directDistanceSqd )
        {
            // target has crossed seam when it shouldnt have (possible while turning )
            if( m_targetLongitude < 0 )
            {
                m_targetLongitude += 360;
            }
            else
            {
                m_targetLongitude -= 360;
            }
        }
    }
}

void MovingObject::Render2D()
{
    if( g_preferences->GetInt( PREFS_GRAPHICS_TRAILS ) == 1 )
    {
        RenderHistory2D();
    }

    if( m_movementType == MovementTypeAir )
    {
        float angle = (float)atan2( (double)(-m_vel.x.DoubleValue()), (double)m_vel.y.DoubleValue() );
        if( IsBallisticMissileClass() ) angle += (float)M_PI;  // nuke sprite faces opposite way
        
        float clampedPrediction = (g_predictionTime > 0.0f) ? g_predictionTime : 0.0f;
        Fixed predictionTime = Fixed::FromDouble(clampedPrediction) * g_app->GetWorld()->GetTimeScaleFactor();
        float predictedLongitude = (m_longitude + m_vel.x * Fixed(predictionTime)).DoubleValue();
        float predictedLatitude = (m_latitude + m_vel.y * Fixed(predictionTime)).DoubleValue();
        float size = GetSize().DoubleValue();

        Team *team = g_app->GetWorld()->GetTeam(m_teamId);
        Colour colour = team->GetTeamColour();
        colour.m_a = 255;
        
        Image *bmpImage = g_resource->GetImage( GetResolvedBmpImageFilename() );

        Image *outlineImage = GetOutlineImage();
        if( outlineImage )
        {
            Colour white( 255, 255, 255, 255 );
            float outlineX = predictedLongitude - size * 0.5f;
            float outlineY = predictedLatitude + size * 0.5f;
            float outlineW = size;
            float outlineH = -size;
            g_renderer2d->StaticSprite( outlineImage, outlineX, outlineY, outlineW, outlineH, white );
        }

        g_renderer2d->RotatingSprite( bmpImage,
                          predictedLongitude,
                          predictedLatitude,
                          size/2,
                          size/2,
                          colour,
                          angle );

        colour.Set(255,255,255,255);
        int highlightId = g_app->GetWorldRenderer()->GetCurrentHighlightId();
        for( int i = 0; i < 2; ++i )
        {
            if( i == 1 && highlightId != -1 && g_app->GetWorldRenderer()->IsSelected( highlightId ) )
                break;
            bool drawHighlight = ( i == 0 && g_app->GetWorldRenderer()->IsSelected( m_objectId ) ) ||
                                ( i == 1 && highlightId == m_objectId );

            if( drawHighlight )
            {
                bmpImage = g_resource->GetImage( GetBmpBlurFilename() );
                g_renderer2d->RotatingSprite( bmpImage,
                                predictedLongitude,
                                predictedLatitude,
                                size/2,
                                size/2,
                                colour,
                                angle );
            }
            colour.m_a *= 0.5f;
        }
    }
    else
    {
        WorldObject::Render2D();
    }
}

void MovingObject::Render3D()
{
    if( IsBallisticMissileClass() && g_app->IsGlobeMode() )
    {
        GlobeRenderer *globeRenderer = g_app->GetGlobeRenderer();
        if( globeRenderer && globeRenderer->ShouldUse3DNukeTrajectories() )
        {
            return;
        }
    }

    if( g_preferences->GetInt( PREFS_GRAPHICS_TRAILS ) == 1 )
    {
        RenderHistory3D();
    }

    if( m_movementType == MovementTypeAir )
    {
        float angle = atan( -m_vel.x.DoubleValue() / m_vel.y.DoubleValue() );
        if( m_vel.y < 0 ) angle += M_PI;
        
        float clampedPrediction = (g_predictionTime > 0.0f) ? g_predictionTime : 0.0f;
        Fixed predictionTime = Fixed::FromDouble(clampedPrediction) * g_app->GetWorld()->GetTimeScaleFactor();
        float predictedLongitude = (m_longitude + m_vel.x * Fixed(predictionTime)).DoubleValue();
        float predictedLatitude = (m_latitude + m_vel.y * Fixed(predictionTime)).DoubleValue();
        float size = GetSize3D().DoubleValue();

        Team *team = g_app->GetWorld()->GetTeam(m_teamId);
        Colour colour = team->GetTeamColour();
        colour.m_a = 255;
        
        Image *bmpImage = g_resource->GetImage( GetResolvedBmpImageFilename() );

        GlobeRenderer *globeRenderer = g_app->GetGlobeRenderer();
        
        Vector3<float> spritePos = globeRenderer->ConvertLongLatTo3DPosition(predictedLongitude, predictedLatitude);
        Vector3<float> normal = spritePos;
        normal.Normalise();
        spritePos += normal * GLOBE_ELEVATION;

        Image *outlineImage3d = GetOutlineImage();
        if( outlineImage3d )
        {
            Colour white( 255, 255, 255, 255 );
            g_renderer3d->StaticSprite3D( outlineImage3d, spritePos.x, spritePos.y, spritePos.z,
                                          size * 0.5f, size * 0.5f, white, BILLBOARD_SURFACE_ALIGNED );
        }
        
        g_renderer3d->RotatingSprite3D( bmpImage, spritePos.x, spritePos.y, spritePos.z,
                                        size, size, colour, angle, BILLBOARD_SURFACE_ALIGNED );

        colour.Set(255,255,255,255);
        int highlightId = g_app->GetWorldRenderer()->GetCurrentHighlightId();
        for( int i = 0; i < 2; ++i )
        {
            if( i == 1 && highlightId != -1 && g_app->GetWorldRenderer()->IsSelected( highlightId ) )
                break;
            bool drawHighlight = ( i == 0 && g_app->GetWorldRenderer()->IsSelected( m_objectId ) ) ||
                                ( i == 1 && highlightId == m_objectId );

            if( drawHighlight )
            {
                char *blurFile = GetBmpBlurFilename();
                if( g_resource->HasImage( blurFile ) )
                {
                    bmpImage = g_resource->GetImage( blurFile );
                    g_renderer3d->RotatingSprite3D( bmpImage, spritePos.x, spritePos.y, spritePos.z,
                                                    size, size, colour, angle, BILLBOARD_SURFACE_ALIGNED );
                }
            }
            colour.m_a *= 0.5f;
        }
    }
    else
    {
        WorldObject::Render3D();
    }
}

void MovingObject::RenderHistory2D()
{
    float clampedPrediction = (g_predictionTime > 0.0f) ? g_predictionTime : 0.0f;
    Fixed predictionTime = Fixed::FromDouble(clampedPrediction) * g_app->GetWorld()->GetTimeScaleFactor();
    float predictedLongitude = (m_longitude + m_vel.x * Fixed(predictionTime)).DoubleValue();
    float predictedLatitude = (m_latitude + m_vel.y * Fixed(predictionTime)).DoubleValue(); 

    int maxSize = m_history.Size();
    
    float zf = g_app->GetMapRenderer() ? g_app->GetMapRenderer()->GetZoomFactor() : 1.0f;
    float gameScale = World::GetGameScale().DoubleValue();
    if( gameScale < 0.01f ) gameScale = 1.0f;
    float trailMult = 2.0f - zf;
    int trailMin = (int)(2.0f * trailMult + 0.5f);
    int sizeCap = (int)(160.0f * zf * trailMult / gameScale);
    if( sizeCap < trailMin ) sizeCap = trailMin;

    if( g_app->GetGame()->GetOptionValue("GameMode") == GAMEMODE_BIGWORLD )
    {
        switch( m_type )
        {
            case TypeNuke:
            {
                int nukeCap = (int)(24.0f * zf * trailMult / gameScale);
                if( nukeCap < trailMin ) nukeCap = trailMin;
                sizeCap = nukeCap;
                break;
            }

            case TypeBattleShip:
            case TypeBattleShip2:
            case TypeBattleShip3:
            case TypeCarrier:
            case TypeCarrierLight:
            case TypeCarrierSuper:
            case TypeCarrierLHD:
            case TypeSub:
            case TypeSubG:
            case TypeSubC:
            case TypeSubK:
            case TypeFighter:
            case TypeFighterLight:
            case TypeFighterStealth:
            case TypeFighterNavyStealth:
            case TypeBomber:
            case TypeBomberFast:
            case TypeBomberStealth:
            case TypeAEW:
            case TypeTanker:
            case TypeLACM:
            case TypeLANM:
            case TypeCBM:
            case TypeABM:
                break;

            default:
                return;
        }
    }

    maxSize = ( maxSize > sizeCap ? sizeCap : maxSize );
        
    Team *team = g_app->GetWorld()->GetTeam(m_teamId);
    Colour colour;
    if( team )
    {
        colour = team->GetTeamColour();
    }
    else
    {
        colour = COLOUR_SPECIALOBJECTS;
    }

    int myTeamId = g_app->GetWorld()->m_myTeamId;
    if( m_teamId != myTeamId &&
        myTeamId != -1 &&
        myTeamId < g_app->GetWorld()->m_teams.Size() &&
        g_app->GetWorld()->GetTeam(myTeamId)->m_type != Team::TypeUnassigned )
    {
        maxSize = min( maxSize, 4 ); 
    }

    if( m_history.Size() > 0 )
    {
        Vector3<float> lastPos( predictedLongitude, predictedLatitude, 0 );
        int numSteps = maxSize * 2;

        for( int i = 0; i < numSteps; ++i )
        {
            float histIdx = (float)i * 0.5f;
            int idx0 = (int)histIdx;
            int idx1 = (idx0 + 1 < maxSize) ? idx0 + 1 : idx0;
            float frac = histIdx - (float)idx0;

            Vector3<float> pos0( m_history[idx0]->x.DoubleValue(), m_history[idx0]->y.DoubleValue(), 0.0f );
            Vector3<float> pos1( m_history[idx1]->x.DoubleValue(), m_history[idx1]->y.DoubleValue(), 0.0f );
            Vector3<float> thisPos = pos0 * (1.0f - frac) + pos1 * frac;

            if( lastPos.x < -170 && thisPos.x > 170 )       thisPos.x = -180 - ( 180 - thisPos.x );        
            if( lastPos.x > 170 && thisPos.x < -170 )       thisPos.x = 180 + ( 180 - fabs(thisPos.x) );        

            Vector3<float> diff = thisPos - lastPos;
            lastPos += diff * 0.1f;
            colour.m_a = 255 - 255 * histIdx / (float) maxSize;

            g_renderer2d->Line( lastPos.x, lastPos.y, thisPos.x, thisPos.y, colour );
            lastPos = thisPos;
        }
    }
}

void MovingObject::RenderHistory3D()
{
    if (IsBallisticMissileClass() && g_app->IsGlobeMode()) {
        GlobeRenderer *globeRenderer = g_app->GetGlobeRenderer();
        if (globeRenderer && globeRenderer->ShouldUse3DNukeTrajectories()) {
            return;
        }
    }

    float clampedPrediction = (g_predictionTime > 0.0f) ? g_predictionTime : 0.0f;
    Fixed predictionTime = Fixed::FromDouble(clampedPrediction) * g_app->GetWorld()->GetTimeScaleFactor();
    float predictedLongitude = (m_longitude + m_vel.x * Fixed(predictionTime)).DoubleValue();
    float predictedLatitude = (m_latitude + m_vel.y * Fixed(predictionTime)).DoubleValue();

    int maxSize = m_history.Size();
    
    float zf3d = g_app->GetMapRenderer() ? g_app->GetMapRenderer()->GetZoomFactor() : 1.0f;
    float gameScale3d = World::GetGameScale().DoubleValue();
    if( gameScale3d < 0.01f ) gameScale3d = 1.0f;
    float trailMult3d = 2.0f - zf3d;
    int trailMin3d = (int)(2.0f * trailMult3d + 0.5f);
    int sizeCap = (int)(160.0f * zf3d * trailMult3d / gameScale3d);
    if( sizeCap < trailMin3d ) sizeCap = trailMin3d;

    if( g_app->GetGame()->GetOptionValue("GameMode") == GAMEMODE_BIGWORLD )
    {
        switch( m_type )
        {
            case TypeNuke:
            {
                int nukeCap3d = (int)(24.0f * zf3d * trailMult3d / gameScale3d);
                if( nukeCap3d < trailMin3d ) nukeCap3d = trailMin3d;
                sizeCap = nukeCap3d;
                break;
            }

            case TypeBattleShip:
            case TypeBattleShip2:
            case TypeBattleShip3:
            case TypeCarrier:
            case TypeCarrierLight:
            case TypeCarrierSuper:
            case TypeCarrierLHD:
            case TypeSub:
            case TypeSubG:
            case TypeSubC:
            case TypeSubK:
            case TypeFighter:
            case TypeFighterLight:
            case TypeFighterStealth:
            case TypeFighterNavyStealth:
            case TypeBomber:
            case TypeBomberFast:
            case TypeBomberStealth:
            case TypeAEW:
            case TypeTanker:
            case TypeLACM:
            case TypeLANM:
            case TypeCBM:
            case TypeABM:
                break;

            default:
                return;
        }
    }

    maxSize = ( maxSize > sizeCap ? sizeCap : maxSize );
        
    Team *team = g_app->GetWorld()->GetTeam(m_teamId);
    Colour colour;
    if( team )
    {
        colour = team->GetTeamColour();
    }
    else
    {
        colour = COLOUR_SPECIALOBJECTS;
    }

    int myTeamId = g_app->GetWorld()->m_myTeamId;
    if( m_teamId != myTeamId &&
        myTeamId != -1 &&
        myTeamId < g_app->GetWorld()->m_teams.Size() &&
        g_app->GetWorld()->GetTeam(myTeamId)->m_type != Team::TypeUnassigned )
    {
        maxSize = min( maxSize, 4 );
    }

    if( m_history.Size() > 0 )
    {
        GlobeRenderer *globeRenderer = g_app->GetGlobeRenderer();
        Vector3<float> lastPos2D( predictedLongitude, predictedLatitude, 0 );
        Vector3<float> lastPos3D = globeRenderer->ConvertLongLatTo3DPosition(lastPos2D.x, lastPos2D.y);
        Vector3<float> lastNormal = lastPos3D;
        lastNormal.Normalise();
        lastPos3D += lastNormal * GLOBE_ELEVATION;

        int numSteps = maxSize * 2;

        for( int i = 0; i < numSteps; ++i )
        {
            float histIdx = (float)i * 0.5f;
            int idx0 = (int)histIdx;
            int idx1 = (idx0 + 1 < maxSize) ? idx0 + 1 : idx0;
            float frac = histIdx - (float)idx0;

            Vector3<float> pos0_2D( m_history[idx0]->x.DoubleValue(), m_history[idx0]->y.DoubleValue(), 0.0f );
            Vector3<float> pos1_2D( m_history[idx1]->x.DoubleValue(), m_history[idx1]->y.DoubleValue(), 0.0f );
            Vector3<float> thisPos2D = pos0_2D * (1.0f - frac) + pos1_2D * frac;

            if( lastPos2D.x < -170 && thisPos2D.x > 170 )       thisPos2D.x = -180 - ( 180 - thisPos2D.x );
            if( lastPos2D.x > 170 && thisPos2D.x < -170 )       thisPos2D.x = 180 + ( 180 - fabs(thisPos2D.x) );

            Vector3<float> diff = thisPos2D - lastPos2D;
            lastPos2D += diff * 0.1f;

            Vector3<float> lineStart3D = globeRenderer->ConvertLongLatTo3DPosition(lastPos2D.x, lastPos2D.y);
            Vector3<float> lineEnd3D = globeRenderer->ConvertLongLatTo3DPosition(thisPos2D.x, thisPos2D.y);

            Vector3<float> startNormal = lineStart3D;
            startNormal.Normalise();
            lineStart3D += startNormal * GLOBE_ELEVATION;
            Vector3<float> endNormal = lineEnd3D;
            endNormal.Normalise();
            lineEnd3D += endNormal * GLOBE_ELEVATION;

            colour.m_a = 255 - 255 * histIdx / (float) maxSize;

            g_renderer3d->Line3D( lineStart3D.x, lineStart3D.y, lineStart3D.z,
                                  lineEnd3D.x, lineEnd3D.y, lineEnd3D.z, colour );
            lastPos2D = thisPos2D;
        }
    }
}


int MovingObject::GetAttackState()
{
    return -1;
}

bool MovingObject::IsIdle()
{
    if( m_targetLongitude == 0 &&
        m_targetLatitude  == 0 &&
        m_targetObjectId == -1)
        return true;
    else return false;
}

void MovingObject::RenderGhost2D( int teamId )
{
    if( m_lastSeenTime[teamId] != 0)
    {
        if( m_movementType == MovementTypeAir ||
            m_movementType == MovementTypeSea )
        {		
            Fixed predictionTime = m_ghostFadeTime - m_lastSeenTime[teamId];
            predictionTime += Fixed::FromDouble(g_predictionTime) * g_app->GetWorld()->GetTimeScaleFactor();
            float predictedLongitude = (m_lastKnownPosition[teamId].x + m_lastKnownVelocity[teamId].x * predictionTime).DoubleValue();
            float predictedLatitude = (m_lastKnownPosition[teamId].y + m_lastKnownVelocity[teamId].y * predictionTime).DoubleValue();

            float size = GetSize().DoubleValue();       
            float thisSize = size;

            int transparency = int(255 * ( m_lastSeenTime[teamId] / m_ghostFadeTime ).DoubleValue() );
            Colour col = Colour(150, 150, 150, transparency);
            float angle = 0.0f;

            if( m_movementType == MovementTypeAir )            
            {
                angle = atan( -m_lastKnownVelocity[teamId].x.DoubleValue() / m_lastKnownVelocity[teamId].y.DoubleValue() );
                if( m_lastKnownVelocity[teamId].y < 0 ) angle += M_PI;
                size *= 0.5f;
                thisSize *= 0.5f;
            }
            else
            {
                Team *team = g_app->GetWorld()->GetTeam(m_teamId);
                if( team->m_territories[0] >= 3 )
                {
                    thisSize = size*-1;
                }       
            }

            Image *bmpImage = g_resource->GetImage( GetResolvedBmpImageFilename() );
            g_renderer2d->RotatingSprite( bmpImage, predictedLongitude, predictedLatitude, thisSize, size, col, angle);
        }
        else
        {
            WorldObject::RenderGhost2D( teamId );
        }
    }
}

void MovingObject::RenderGhost3D( int teamId )
{
    if( m_lastSeenTime[teamId] != 0)
    {
        GlobeRenderer *globeRenderer = g_app->GetGlobeRenderer();
        if( globeRenderer )
        {
            if( m_movementType == MovementTypeAir ||
                m_movementType == MovementTypeSea )
            {		
                Fixed predictionTime = m_ghostFadeTime - m_lastSeenTime[teamId];
                predictionTime += Fixed::FromDouble(g_predictionTime) * g_app->GetWorld()->GetTimeScaleFactor();
                float predictedLongitude = (m_lastKnownPosition[teamId].x + m_lastKnownVelocity[teamId].x * predictionTime).DoubleValue();
                float predictedLatitude = (m_lastKnownPosition[teamId].y + m_lastKnownVelocity[teamId].y * predictionTime).DoubleValue();

                float size = GetSize3D().DoubleValue();

                int transparency = int(255 * ( m_lastSeenTime[teamId] / m_ghostFadeTime ).DoubleValue() );
                Colour col = Colour(150, 150, 150, transparency);
                float angle = 0.0f;

                Vector3<float> objPos = globeRenderer->ConvertLongLatTo3DPosition(predictedLongitude, predictedLatitude);
                Vector3<float> normal = objPos;
                normal.Normalise();
                float elevation = GLOBE_ELEVATION;
                Vector3<float> renderPos = objPos + normal * elevation;

                if( m_movementType == MovementTypeAir )            
                {
                    angle = atan( -m_lastKnownVelocity[teamId].x.DoubleValue() / m_lastKnownVelocity[teamId].y.DoubleValue() );
                    if( m_lastKnownVelocity[teamId].y < 0 ) angle += M_PI;
                    size *= 0.5f;
                }

                Image *bmpImage = g_resource->GetImage( GetResolvedBmpImageFilename() );
                float spriteSize = size * 2.0f;

                if( m_movementType == MovementTypeAir )
                {
                    g_renderer3d->RotatingSprite3D( bmpImage, renderPos.x, renderPos.y, renderPos.z, 
                                                     spriteSize, spriteSize, col, angle, BILLBOARD_SURFACE_ALIGNED );
                }
                else
                {
                    g_renderer3d->StaticSprite3D( bmpImage, renderPos.x, renderPos.y, renderPos.z, 
                                                   spriteSize, spriteSize, col, BILLBOARD_SURFACE_ALIGNED );
                }
            }
            else
            {
                WorldObject::RenderGhost3D( teamId );
            }
        }
    }
}

void MovingObject::Land( int targetId )
{
}

void MovingObject::ClearWaypoints()
{
    m_targetLongitude = 0;
    m_targetLatitude = 0;
    m_finalTargetLongitude = 0;
    m_finalTargetLatitude = 0;
    m_targetLongitudeAcrossSeam = 0;
    m_targetLatitudeAcrossSeam = 0;
    m_targetNodeId = -1;
    m_isLanding = -1;
    m_isEscorting = -1;
//    m_movementBlips.EmptyAndDelete();
}


int MovingObject::IsValidMovementTarget( Fixed longitude, Fixed latitude )
{
    if( latitude > 100 || latitude < -100 )
    {
        return TargetTypeInvalid;
    }

    //
    // Are we part of a fleet?

    Fleet *fleet = g_app->GetWorld()->GetTeam(m_teamId)->GetFleet( m_fleetId );
    if( fleet )
    {
        return( fleet->IsValidFleetPosition( longitude, latitude ) );
    }


    Fixed rangeSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, longitude, latitude );      
    if( rangeSqd > m_range * m_range )
    {
        return TargetTypeOutOfRange;
    }

    bool validPosition = IsValidPosition( longitude, latitude );

    if( validPosition )
    {
        return TargetTypeValid;
    }
    else
    {
        return TargetTypeInvalid;
    }
}


void MovingObject::AutoLand()
{
    if( m_isLanding == -1 )
    {
        int target = GetClosestLandingPad();

        if( target != -1 )
        {
            WorldObject *landingPad = g_app->GetWorld()->GetWorldObject(target);
            if( m_range - 15 < g_app->GetWorld()->GetDistance( m_longitude, m_latitude, landingPad->m_longitude, landingPad->m_latitude) )
            {
                Land( target );
            }
        }
    }
}


int MovingObject::GetClosestLandingPad()
{
    int nearestId = -1;
    int nearestWithNukesId = -1;

    Fixed nearestSqd = Fixed::MAX;
    Fixed nearestWithNukesSqd = Fixed::MAX;

    //
    // Look for any carrier or airbase with room
    // Favour objects with nukes if we are a bomber

    for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
    {
        if( g_app->GetWorld()->m_objects.ValidIndex(i) )
        {
            WorldObject *obj = g_app->GetWorld()->m_objects[i];
            if( obj->m_teamId == m_teamId )
            {
                if( obj->IsAircraftLauncher() )
                {
                    // Stealth and light fighters cannot land on carriers - only airbases
                    if( IsFighterClass() && ( m_type == TypeFighterLight || m_type == TypeFighterStealth ) &&
                        obj->IsCarrierClass() )
                        continue;

                    int roomInside = 0;
                    if( IsFighterClass() ) roomInside = obj->m_maxFighters - obj->GetFighterCount();
                    if( IsBomberClass() )
                    {
                        if( obj->m_maxBombers <= 0 )
                            roomInside = 0;
                        else
                            roomInside = obj->m_maxBombers - obj->GetBomberCount();
                    }
                    if( IsAEWClass() )
                    {
                        if( obj->m_maxAEW <= 0 )
                            roomInside = 0;
                        else
                            roomInside = obj->m_maxAEW - obj->GetAEWCount();
                    }

                    if( roomInside > 0 )
                    {
                        Fixed distSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, obj->m_longitude, obj->m_latitude );
                        if( distSqd < nearestSqd )
                        {
                            nearestSqd = distSqd;
                            nearestId = obj->m_objectId;
                        }

                        if( IsBomberClass() && 
                            ( obj->m_nukeSupply > 0 || obj->m_nukeSupply == -1 ) &&
                            distSqd < nearestWithNukesSqd )
                        {
                            nearestWithNukesSqd = distSqd;
                            nearestId = obj->m_objectId;
                        }
                    }
                }
            }
        }
    }

    if( IsFighterClass() && nearestId != -1 )
        return nearestId;

    if( IsAEWClass() && nearestId != -1 )
        return nearestId;

    if( IsBomberClass() && nearestWithNukesId != -1 &&
        nearestWithNukesSqd < m_range * m_range )
        return nearestWithNukesId;

    if( IsBomberClass() && nearestId != -1 )
        return nearestId;

    return -1;
}


//int MovingObject::GetClosestLandingPad()
//{
//    int type = -1;
//    int cap = 0;
//    if( m_type == TypeFighter )
//    {
//        type = 0;
//    }
//    else if( m_type == TypeBomber )
//    {
//        type = 1;
//    }
//
//    float carrierDistance = 9999.9f;
//    float airbaseDistance = 9999.9f;
//    int nearestCarrier = -1;
//    int nearestAirbase = -1;
//
//    for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
//    {
//        if( g_app->GetWorld()->m_objects.ValidIndex(i) )
//        {
//            WorldObject *obj = g_app->GetWorld()->m_objects[i];
//            if( obj->m_teamId == m_teamId )
//            {
//                if( obj->m_type == WorldObject::TypeCarrier ||
//                    obj->m_type == WorldObject::TypeAirBase )
//                {
//                    if( type == 0 )
//                    {
//                        cap = obj->m_maxFighters;
//                    }
//                    else
//                    {
//                        cap = obj->m_maxBombers;
//                    }
//
//                    if( obj->m_states[type]->m_numTimesPermitted < cap )
//                    {
//                        float dist = g_app->GetWorld()->GetDistance( m_longitude, m_latitude, obj->m_longitude, obj->m_latitude );
//                        if( obj->m_type == WorldObject::TypeCarrier )
//                        {
//                            if( dist < carrierDistance )
//                            {
//                                carrierDistance = dist;
//                                nearestCarrier = obj->m_objectId;
//                            }
//                        }
//                        else
//                        {
//                            if( dist < airbaseDistance )
//                            {
//                                airbaseDistance = dist;
//                                nearestAirbase = obj->m_objectId;
//                            }
//                        }
//                    }
//                }
//            }
//        }
//    }
//
//    int target = -1;
//    float distance = 99999.9f;
//
//    WorldObject *base = g_app->GetWorld()->GetWorldObject(nearestAirbase);
//    if( base )
//    {
//        distance = g_app->GetWorld()->GetDistance( m_longitude, m_latitude, base->m_longitude, base->m_latitude );
//        target = nearestAirbase;
//    }
//
//    
//    WorldObject *carrier = g_app->GetWorld()->GetWorldObject(nearestCarrier);
//    if( carrier )
//    {
//        if( g_app->GetWorld()->GetDistance( m_longitude, m_latitude, carrier->m_longitude, carrier->m_latitude ) < distance &&
//            carrier->m_states[type]->m_numTimesPermitted < cap )
//        {
//            target = nearestCarrier;
//        }
//    }
//
//    return target;
//}

void MovingObject::Retaliate( int attackerId )
{
    WorldObject *obj = g_app->GetWorld()->GetWorldObject( attackerId );
    if( obj && !g_app->GetWorld()->IsFriend( m_teamId, attackerId ) &&
        !g_app->GetWorld()->GetTeam( m_teamId )->m_ceaseFire[ obj->m_teamId ])
    {
        if( UsingGuns() && m_targetObjectId == -1 )
        {            
            if( g_app->GetWorld()->GetAttackOdds( m_type, obj->m_type ) > 0 )
            {
                m_targetObjectId = attackerId;
                m_isRetaliating = true;
            }

            if( m_fleetId != -1 )
            {
                g_app->GetWorld()->GetTeam( m_teamId )->m_fleets[ m_fleetId ]->FleetAction( m_targetObjectId );
            }
        }
        if( obj->IsSubmarine() )
        {
            Fleet *fleet = g_app->GetWorld()->GetTeam( m_teamId )->GetFleet( m_fleetId );
            if( fleet )
            {
                fleet->CounterSubs();
            }
        }
    }

}

void MovingObject::Ping()
{   
    int animId = g_app->GetWorldRenderer()->CreateAnimation( WorldRenderer::AnimationTypeSonarPing, m_objectId,
														   m_longitude.DoubleValue(), m_latitude.DoubleValue() );
    AnimatedIcon *icon = g_app->GetWorldRenderer()->GetAnimations()[animId];
    ((SonarPing *)icon)->m_teamId = m_teamId;
    
    if( g_app->GetWorld()->m_myTeamId == -1 ||
        g_app->GetWorld()->m_myTeamId == m_teamId ||
        g_app->GetWorldRenderer()->CanRenderEverything() ||
        g_app->GetWorld()->IsVisible( m_stealthType, m_longitude, m_latitude, g_app->GetWorld()->m_myTeamId) )
    {
#ifdef TOGGLE_SOUND
        g_soundSystem->TriggerEvent( SoundObjectId(m_objectId), "SonarPing" );
#endif
    }


    Fleet *fleet = g_app->GetWorld()->GetTeam( m_teamId )->GetFleet( m_fleetId );
    if( fleet == NULL ||
        fleet->m_currentState == Fleet::FleetStateAggressive )
    {
        Fixed pingRangeSqd = GetActionRangeSqd();
        if( IsSubmarine() && m_states.Size() > 0 )
        {
            Fixed r0 = m_states[0]->m_actionRange;
            pingRangeSqd = r0 * r0 * 4;  // active sonar: 2x action range
        }

        for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
        {
            if( g_app->GetWorld()->m_objects.ValidIndex(i) )
            {
                MovingObject *obj = (MovingObject *)g_app->GetWorld()->m_objects[i];
                if( obj->IsMovingObject() &&
                    obj->m_movementType == MovementTypeSea &&
                    obj->m_objectId != m_objectId &&
                    !obj->m_visible[ m_teamId ] &&
                    m_teamId != obj->m_teamId )
                {
                    Fixed distanceSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, obj->m_longitude, obj->m_latitude );

                    if( distanceSqd <= pingRangeSqd )
                    {
                        if( obj->m_movementType == MovementTypeSea )
                        {
                            for( int j = 0; j < g_app->GetWorld()->m_teams.Size(); ++j )
                            {
                                Team *team = g_app->GetWorld()->m_teams[j];
                                if( g_app->GetWorld()->IsFriend(team->m_teamId, m_teamId ) )
                                {
                                    obj->m_lastSeenTime[team->m_teamId] = obj->m_ghostFadeTime;
                                    obj->m_lastKnownPosition[team->m_teamId] = Vector3<Fixed>( obj->m_longitude, obj->m_latitude, 0 );                                    
                                    //obj->m_visible[team->m_teamId] = (obj->m_type != WorldObject::TypeSub );
                                    obj->m_seen[team->m_teamId] = true;
                                    obj->m_lastKnownVelocity[team->m_teamId].Zero();
                                }
                            }
                        }

                        if( IsSubmarine() )
                        {
                            for( int j = 0; j < g_app->GetWorld()->m_teams.Size(); ++j )
                            {
                                Team *team = g_app->GetWorld()->m_teams[j];
                                if( g_app->GetWorld()->IsVisible( m_stealthType, m_longitude, m_latitude, team->m_teamId ) )
                                {
                                    m_seen[team->m_teamId] = true;
                                    m_lastSeenTime[team->m_teamId] = m_ghostFadeTime;
                                    m_lastKnownPosition[team->m_teamId] = Vector3<Fixed>( m_longitude, m_latitude, 0 );
                                    m_lastKnownVelocity[team->m_teamId].Zero();
                                }
                            }
                        }

                        if( !g_app->GetWorld()->GetTeam( m_teamId )->m_ceaseFire[ obj->m_teamId ] )
                        {
                            if( m_targetObjectId == -1 )
                            {
                                m_targetObjectId = obj->m_objectId;                  
                            }
                            if( g_app->GetWorld()->GetTeam( m_teamId )->m_type == Team::TypeAI )
                            {
                                if( obj->IsSubmarine() )
                                {
                                    g_app->GetWorld()->GetTeam( m_teamId )->AddEvent( Event::TypeSubDetected, obj->m_objectId, obj->m_teamId, obj->m_fleetId, obj->m_longitude, obj->m_latitude );
                                }
                                else
                                {
                                    g_app->GetWorld()->GetTeam( m_teamId )->AddEvent( Event::TypeEnemyIncursion, obj->m_objectId, obj->m_teamId, obj->m_fleetId, obj->m_longitude, obj->m_latitude );
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

void MovingObject::PassivePing()
{
    // Passive sonar: no visible ping, no sound, no self-reveal. Detects non-hidden ships (surface + surfaced subs) at 1.5x action range.
    Fleet *fleet = g_app->GetWorld()->GetTeam( m_teamId )->GetFleet( m_fleetId );
    if( fleet == NULL ||
        fleet->m_currentState == Fleet::FleetStateAggressive )
    {
        Fixed passiveRangeSqd = ( m_states.Size() > 0 ) ? ( m_states[0]->m_actionRange * m_states[0]->m_actionRange * Fixed::FromDouble(2.25) ) : GetActionRangeSqd();  // 1.5x = 2.25

        for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
        {
            if( !g_app->GetWorld()->m_objects.ValidIndex(i) )
                continue;
            MovingObject *obj = (MovingObject *)g_app->GetWorld()->m_objects[i];

            Fixed distanceSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, obj->m_longitude, obj->m_latitude );
            if( distanceSqd > passiveRangeSqd )
                continue;
            if( !obj->IsMovingObject() ||
                obj->m_movementType != MovementTypeSea ||
                ( obj->IsSubmarine() && obj->IsHiddenFrom() ) ||  // skip hidden (submerged) subs
                obj->m_objectId == m_objectId ||
                obj->m_visible[ m_teamId ] ||
                m_teamId == obj->m_teamId )
                continue;

            for( int j = 0; j < g_app->GetWorld()->m_teams.Size(); ++j )
            {
                Team *team = g_app->GetWorld()->m_teams[j];
                if( g_app->GetWorld()->IsFriend(team->m_teamId, m_teamId ) )
                {
                    obj->m_lastSeenTime[team->m_teamId] = obj->m_ghostFadeTime;
                    obj->m_lastKnownPosition[team->m_teamId] = Vector3<Fixed>( obj->m_longitude, obj->m_latitude, 0 );
                    obj->m_seen[team->m_teamId] = true;
                    obj->m_lastKnownVelocity[team->m_teamId].Zero();
                }
            }

            if( !g_app->GetWorld()->GetTeam( m_teamId )->m_ceaseFire[ obj->m_teamId ] )
            {
                if( m_targetObjectId == -1 )
                    m_targetObjectId = obj->m_objectId;
                if( g_app->GetWorld()->GetTeam( m_teamId )->m_type == Team::TypeAI )
                {
                    if( obj->IsSubmarine() )
                        g_app->GetWorld()->GetTeam( m_teamId )->AddEvent( Event::TypeSubDetected, obj->m_objectId, obj->m_teamId, obj->m_fleetId, obj->m_longitude, obj->m_latitude );
                    else
                        g_app->GetWorld()->GetTeam( m_teamId )->AddEvent( Event::TypeEnemyIncursion, obj->m_objectId, obj->m_teamId, obj->m_fleetId, obj->m_longitude, obj->m_latitude );
                }
            }
        }
    }
}

void MovingObject::SetSpeed( Fixed speed )
{
    m_speed = speed;
}

int MovingObject::GetTarget( Fixed range, const LList<int> *excludeIds )
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
                    g_app->GetWorld()->GetAttackOdds( m_type, obj->m_type ) > 0 &&
                    ( obj->m_visible[m_teamId] || ( obj->IsBuilding() && obj->m_seen[m_teamId] ) ) &&
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


char *MovingObject::LogState()
{
    char longitude[64];
    char latitude[64];
    char velX[64];
    char velY[64];
    char timer[64];
    char retarget[64];
    char ai[64];
    char speed[64];
    char targetLong[64];
    char targetLat[64];
    char seenTime[64];

    static char s_result[10240];
    snprintf( s_result, 10240, "obj[%d] [%10s] team[%d] fleet[%d] long[%s] lat[%s] velX[%s] velY[%s] state[%d] target[%d] life[%d] timer[%s] retarget[%s] ai[%s] speed[%s] targetNode[%d] targetLong[%s] targetLat[%s]",
                m_objectId,
                GetName(m_type),
                m_teamId,
                m_fleetId,
                HashDouble( m_longitude.DoubleValue(), longitude ),
                HashDouble( m_latitude.DoubleValue(), latitude ),
                HashDouble( m_vel.x.DoubleValue(), velX ),
                HashDouble( m_vel.y.DoubleValue(), velY ),
                m_currentState,
                m_targetObjectId,
                m_life,
                HashDouble( m_stateTimer.DoubleValue(), timer ),
                HashDouble( m_retargetTimer.DoubleValue(), retarget ),
                HashDouble( m_aiTimer.DoubleValue(), ai ),
                HashDouble( m_speed.DoubleValue(), speed ),
                m_targetNodeId,
                HashDouble( m_targetLongitude.DoubleValue(), targetLong ),
                HashDouble( m_targetLatitude.DoubleValue(), targetLat ) );

    for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
    {
        Team *team = g_app->GetWorld()->m_teams[i];
        char thisTeam[512];
        sprintf( thisTeam, "\n\tTeam %d visible[%d] seen[%d] pos[%s %s] vel[%s %s] seen[%s] state[%d]",
            team->m_teamId,
            static_cast<int>(static_cast<bool>(m_visible[team->m_teamId])),
            static_cast<int>(static_cast<bool>(m_seen[team->m_teamId])),
            HashDouble( m_lastKnownPosition[team->m_teamId].x.DoubleValue(), longitude ),
            HashDouble( m_lastKnownPosition[team->m_teamId].y.DoubleValue(), latitude ),
            HashDouble( m_lastKnownVelocity[team->m_teamId].x.DoubleValue(), velX ),
            HashDouble( m_lastKnownVelocity[team->m_teamId].y.DoubleValue(), velY ),
            HashDouble( m_lastSeenTime[team->m_teamId].DoubleValue(), seenTime ),
            m_lastSeenState[team->m_teamId] );

        strcat( s_result, thisTeam );
    }

    return s_result;
}


void MovingObject::CombatIntercept( WorldObject *target )
{
    if( target->m_visible[m_teamId] || ( target->IsBuilding() && target->m_seen[m_teamId] ) )
    {
        Fixed interceptLongitude, interceptLatitude;
        GetCombatInterceptionPoint( target, &interceptLongitude, &interceptLatitude );
        SetWaypoint( interceptLongitude, interceptLatitude );
    }
    else
    {
        SetWaypoint( target->m_lastKnownPosition[m_teamId].x, target->m_lastKnownPosition[m_teamId].y );
    }
}

void MovingObject::GetCombatInterceptionPoint( WorldObject *target, Fixed *interceptLongitude, Fixed *interceptLatitude )
{
    Fixed timeLimit = Fixed::MAX;
    Vector3<Fixed> targetVel = target->m_vel;

    Fixed targetLongitude = target->m_longitude;
    World::SanitiseTargetLongitude( m_longitude, targetLongitude );

    Vector3<Fixed> distance( targetLongitude - m_longitude, target->m_latitude - m_latitude, 0 );

    // Nukes follow curved paths - use m_vel (instantaneous tangent). Aircraft can change path -
    // use waypoint-derived velocity with timeLimit so we don't over-predict.
    if( target->IsMovingObject() )
    {
        MovingObject *movingTarget = (MovingObject *)target;
        bool useCurvedVelocity = target->IsBallisticMissileClass();
        if( !useCurvedVelocity && ( movingTarget->m_targetLatitude != 0 || movingTarget->m_targetLongitude != 0 ) )
        {
            Fixed targetTargetLongitude = movingTarget->m_targetLongitude;
            World::SanitiseTargetLongitude( targetLongitude, targetTargetLongitude );
            targetVel.x = targetTargetLongitude - targetLongitude;
            targetVel.y = movingTarget->m_targetLatitude - movingTarget->m_latitude;
            targetVel.z = 0;
            Fixed distSqd = targetVel.x * targetVel.x + targetVel.y * targetVel.y;
            if( distSqd > 0 )
            {
                Fixed dist = sqrt( distSqd );
                timeLimit = dist / movingTarget->m_speed;
                targetVel.x /= timeLimit;
                targetVel.y /= timeLimit;
            }
            else
            {
                targetVel = target->m_vel;
            }
        }
    }

    Fixed targetSpeedSqd = targetVel.x * targetVel.x + targetVel.y * targetVel.y;
    Fixed thisSpeedSqd = m_speed * m_speed;
    if( targetSpeedSqd >= thisSpeedSqd * Fixed::Hundredths(90) )
        targetSpeedSqd = thisSpeedSqd * Fixed::Hundredths(90);

    Fixed distanceMagSqd = distance.x * distance.x + distance.y * distance.y;
    Fixed distanceMag = sqrt( distanceMagSqd );
    Fixed projectileSpeed = m_speed;
    Fixed speedDiff = thisSpeedSqd - targetSpeedSqd;
    if( projectileSpeed <= Fixed::Hundredths(1) || speedDiff <= Fixed::Hundredths(1) )
    {
        *interceptLongitude = targetLongitude;
        *interceptLatitude  = target->m_latitude;
        return;
    }

    Fixed timeLeft = distanceMag / projectileSpeed;
    const int maxIter = 16;
    const Fixed epsilon = Fixed::Hundredths(1);

    for( int iter = 0; iter < maxIter; ++iter )
    {
        Fixed predLong = targetLongitude + timeLeft * targetVel.x;
        Fixed predLat = target->m_latitude + timeLeft * targetVel.y;
        World::SanitiseTargetLongitude( m_longitude, predLong );
        Fixed distToPred = g_app->GetWorld()->GetDistance( m_longitude, m_latitude, predLong, predLat );
        Fixed tNew = distToPred / projectileSpeed;
        Fixed delta = ( tNew > timeLeft ) ? ( tNew - timeLeft ) : ( timeLeft - tNew );
        timeLeft = tNew;
        if( delta < epsilon )
            break;
    }

    if( timeLeft > timeLimit ) timeLeft = timeLimit;
    if( timeLeft < 0 ) timeLeft = 0;

    if( target->IsBallisticMissileClass() )
    {
        // Ballistic missiles follow curved paths; linear extrapolation over-predicts. Clamp lead
        // so we aim closer to current position and meet the curve instead of overshooting.
        Fixed closeDist = Fixed(2);
        Fixed farDist   = Fixed(20);
        Fixed leadRange = farDist - closeDist;
        Fixed distFactor = ( distanceMag - closeDist ) / leadRange;
        if( distFactor < 0 ) distFactor = 0;
        if( distFactor > 1 ) distFactor = 1;
        Fixed leadFactor = Fixed::Hundredths(72) + distFactor * Fixed::Hundredths(18);
        timeLeft = timeLeft * leadFactor;
    }
    else if( m_type == WorldObject::TypeLACM &&
             target->IsTargetableSurfaceNavy() )
    {
        // LACM anti-ship: missile is slower than gunfire and has slower turn rate. Ships have
        // low linear speed but can change course quickly - use conservative lead narrowing
        // so we don't overshoot when the ship dodges.
        Fixed distNear = Fixed(5);
        Fixed distFar  = Fixed(50);   // LACM engagements span longer range
        Fixed distRange = distFar - distNear;
        Fixed distFactor = ( distanceMag - distNear ) / distRange;
        if( distFactor < 0 ) distFactor = 0;
        if( distFactor > 1 ) distFactor = 1;
        Fixed turnDelayFactor = Fixed(1) + distFactor * Fixed::Hundredths(30);  // up to 30% extra lead when far
        timeLeft = timeLeft * turnDelayFactor;

        // Ships can dodge by turning quickly; aim closer to current position (88-96% lead).
        Fixed closeDist = Fixed(2);
        Fixed farDist   = Fixed(50);
        Fixed leadRange = farDist - closeDist;
        Fixed distLeadFactor = ( distanceMag - closeDist ) / leadRange;
        if( distLeadFactor < 0 ) distLeadFactor = 0;
        if( distLeadFactor > 1 ) distLeadFactor = 1;
        Fixed leadFactor = Fixed::Hundredths(88) + distLeadFactor * Fixed::Hundredths(8);
        timeLeft = timeLeft * leadFactor;
    }
    else
    {
        // Turn delay: gunfire has limited turn rate, slight extra lead when far. Reduced from 40%
        // to avoid overshooting - bullet re-aims every frame so over-compensation causes misses.
        Fixed distNear = Fixed(5);
        Fixed distFar  = Fixed(25);
        Fixed distRange = distFar - distNear;
        Fixed distFactor = ( distanceMag - distNear ) / distRange;
        if( distFactor < 0 ) distFactor = 0;
        if( distFactor > 1 ) distFactor = 1;
        Fixed turnDelayFactor = Fixed(1) + distFactor * Fixed::Hundredths(12);
        timeLeft = timeLeft * turnDelayFactor;

        // Lead narrowing for aircraft.
        Fixed targetSpeed = sqrt( targetSpeedSqd );
        Fixed speedRatio = ( projectileSpeed > Fixed::Hundredths(1) ) ? ( targetSpeed / projectileSpeed ) : Fixed(0);
        Fixed closeDist = Fixed(2);
        Fixed farDist   = Fixed(25);
        Fixed leadRange = farDist - closeDist;
        Fixed distLeadFactor = ( distanceMag - closeDist ) / leadRange;
        if( distLeadFactor < 0 ) distLeadFactor = 0;
        if( distLeadFactor > 1 ) distLeadFactor = 1;
        Fixed leadFactor = Fixed::Hundredths(92) + distLeadFactor * Fixed::Hundredths(8);
        if( speedRatio > Fixed::Hundredths(50) )
            leadFactor = Fixed::Hundredths(96) + distLeadFactor * Fixed::Hundredths(4);
        timeLeft = timeLeft * leadFactor;
    }

    // Reduce lead at high latitude (linear lon/lat extrapolation over-predicts at poles).
    // 84% at poles.
    Fixed targetLatAbs = target->m_latitude.abs();
    Fixed latNorm = targetLatAbs / Fixed(90);
    Fixed poleScale = Fixed(1) - latNorm * latNorm * Fixed::Hundredths(16);
    if( poleScale < Fixed::Hundredths(84) ) poleScale = Fixed::Hundredths(84);
    timeLeft = timeLeft * poleScale;

    *interceptLongitude = targetLongitude + timeLeft * targetVel.x;
    *interceptLatitude  = target->m_latitude + timeLeft * targetVel.y;
    World::SanitiseTargetLongitude( m_longitude, *interceptLongitude );
}

