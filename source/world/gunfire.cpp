#include "lib/universal_include.h"


#include "lib/resource/image.h"
#include "lib/render/renderer.h"
#include "lib/render2d/renderer_2d.h"
#include "lib/render3d/renderer_3d.h"
#include "lib/math/vector3.h"
#include "lib/math/random_number.h"
#include "lib/math/math_utils.h"
 
#include "app/app.h"
#include "app/globals.h"

#include "renderer/map_renderer.h"
#include "renderer/globe_renderer.h"

#include "world/world.h"
#include "world/gunfire.h"
#include "world/team.h"


GunFire::GunFire( Fixed range )
:   MovingObject(),
    m_useFixedSpeed(true),
    m_origin(-1),
    m_distanceToTarget(0)
{
    m_stealthType = 100;  // normal
    m_range = range;
    m_speed = Fixed::Hundredths(30);

    m_turnRate = Fixed::Hundredths(30);
    m_maxHistorySize = -1;
    m_movementType = MovementTypeAir;
    
    strcpy( bmpImageFilename, "graphics/laser.bmp" );

    m_speed /= g_app->GetWorld()->GetUnitScaleFactor();
    m_turnRate /= g_app->GetWorld()->GetUnitScaleFactor();
}

bool GunFire::Update()
{
    WorldObject *obj = g_app->GetWorld()->GetWorldObject( m_targetObjectId );
    if( obj )
    {
        CombatIntercept( obj );
    }
    else
    {
        //m_range = 0.0f;
        // Fly in a straight line for remainder
        m_targetLongitude = m_longitude + m_vel.x * 10;
        m_targetLatitude = m_latitude + m_vel.y * 10;
    }

    //
    // Update history  
    
    m_historyTimer -= SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor() / 10;
    if( m_historyTimer <= 0 )
    {
        if( m_longitude > -180 ||
            m_longitude < 180 )
        {
            m_history.PutDataAtStart( new Vector3<Fixed>(m_longitude, m_latitude, 0) );
            m_historyTimer = Fixed::Hundredths(10);
        }
    }

    m_maxHistorySize = 10;
    
    while( m_maxHistorySize != -1 && 
           m_history.ValidIndex(m_maxHistorySize) )
    {
        delete m_history[m_maxHistorySize];
        m_history.RemoveData(m_maxHistorySize);
    }

    return MoveToWaypoint();
}

void GunFire::Render2D()
{
    Fixed predictionTime = Fixed::FromDouble(g_predictionTime) * g_app->GetWorld()->GetTimeScaleFactor();
    float predictedLongitude = (m_longitude + m_vel.x * Fixed(predictionTime)).DoubleValue();
    float predictedLatitude = (m_latitude + m_vel.y * Fixed(predictionTime)).DoubleValue(); 
    float size = 2;
    if( g_app->GetMapRenderer() && g_app->GetMapRenderer()->GetZoomFactor() <= 0.25f )
    {
        size *= g_app->GetMapRenderer()->GetZoomFactor() * 4;
    }
    float angle = atan( -m_vel.x.DoubleValue() / m_vel.y.DoubleValue() );
    if( m_vel.y < 0 ) angle += M_PI;

    Team *team = g_app->GetWorld()->GetTeam(m_teamId);
    Colour colour = team->GetTeamColour();            
    float maxSize = 10;

    for( int i = 1; i < m_history.Size(); ++i )
    {
        Vector3<float> lastPos, thisPos;
		
		lastPos = *m_history[i-1];
        thisPos = *m_history[i];

        if( lastPos.x < -170 && thisPos.x > 170 )
        {
            thisPos.x = -180 - ( 180 - thisPos.x );
        }

        if( lastPos.x > 170 && thisPos.x < -170 )
        {
            thisPos.x = 180 + ( 180 - fabs(thisPos.x) );
        }

        Vector3<float> diff = thisPos - lastPos;        
        colour.m_a = 255 - 255 * (float) i / (float) maxSize;
        g_renderer2d->Line( lastPos.x, lastPos.y, thisPos.x, thisPos.y, colour );
    }

    if( m_history.Size() > 0 )
    {
        Vector3<float> lastPos, thisPos;
		
		lastPos = *m_history[ 0 ];
        thisPos = Vector3<float>( predictedLongitude, predictedLatitude, 0 );
        
        if( lastPos.x < -170 && thisPos.x > 170 )
        {
            thisPos.x = -180 - ( 180 - thisPos.x );
        }

        if( lastPos.x > 170 && thisPos.x < -170 )
        {
            thisPos.x = 180 + ( 180 - fabs(thisPos.x) );
        }

        colour.m_a = 255;
        g_renderer2d->Line( lastPos.x, lastPos.y, thisPos.x, thisPos.y, colour );
    }

    g_renderer2d->Line( predictedLongitude, predictedLatitude, 
                                predictedLongitude-m_vel.x.DoubleValue(), 
                                predictedLatitude-m_vel.y.DoubleValue(), colour );
}

void GunFire::Render3D()
{
    Fixed predictionTime = Fixed::FromDouble(g_predictionTime) * g_app->GetWorld()->GetTimeScaleFactor();
    float predictedLongitude = (m_longitude + m_vel.x * Fixed(predictionTime)).DoubleValue();
    float predictedLatitude = (m_latitude + m_vel.y * Fixed(predictionTime)).DoubleValue(); 

    Team *team = g_app->GetWorld()->GetTeam(m_teamId);
    Colour colour = team->GetTeamColour();            
    float maxSize = 10;
    
    float gameScale = World::GetGameScale().DoubleValue();
    if( gameScale < 0.01f ) gameScale = 1.0f;
    float lineWidth = 1.0f / gameScale;
    if( lineWidth < 0.25f ) lineWidth = 0.25f;
    
    GlobeRenderer *globeRenderer = g_app->GetGlobeRenderer();
    
    for( int i = 1; i < m_history.Size(); ++i )
    {
        Vector3<float> lastPos, thisPos;
        
        lastPos = *m_history[i-1];
        thisPos = *m_history[i];

        if( lastPos.x < -170 && thisPos.x > 170 )
        {
            thisPos.x = -180 - ( 180 - thisPos.x );
        }

        if( lastPos.x > 170 && thisPos.x < -170 )
        {
            thisPos.x = 180 + ( 180 - fabs(thisPos.x) );
        }

        Vector3<float> lastPos3D = globeRenderer->ConvertLongLatTo3DPosition(lastPos.x, lastPos.y);
        Vector3<float> thisPos3D = globeRenderer->ConvertLongLatTo3DPosition(thisPos.x, thisPos.y);
        
        Vector3<float> lastNormal = lastPos3D;
        lastNormal.Normalise();
        lastPos3D += lastNormal * GLOBE_ELEVATION;
        
        Vector3<float> thisNormal = thisPos3D;
        thisNormal.Normalise();
        thisPos3D += thisNormal * GLOBE_ELEVATION;
        
        colour.m_a = 255 - 255 * (float) i / (float) maxSize;
        g_renderer3d->Line3D( lastPos3D.x, lastPos3D.y, lastPos3D.z, 
                              thisPos3D.x, thisPos3D.y, thisPos3D.z, colour, lineWidth );
    }

    if( m_history.Size() > 0 )
    {
        Vector3<float> lastPos, thisPos;
        
        lastPos = *m_history[ 0 ];
        thisPos = Vector3<float>( predictedLongitude, predictedLatitude, 0 );
        
        if( lastPos.x < -170 && thisPos.x > 170 )
        {
            thisPos.x = -180 - ( 180 - thisPos.x );
        }

        if( lastPos.x > 170 && thisPos.x < -170 )
        {
            thisPos.x = 180 + ( 180 - fabs(thisPos.x) );
        }

        Vector3<float> lastPos3D = globeRenderer->ConvertLongLatTo3DPosition(lastPos.x, lastPos.y);
        Vector3<float> thisPos3D = globeRenderer->ConvertLongLatTo3DPosition(thisPos.x, thisPos.y);
        
        Vector3<float> lastNormal = lastPos3D;
        lastNormal.Normalise();
        lastPos3D += lastNormal * GLOBE_ELEVATION;
        
        Vector3<float> thisNormal = thisPos3D;
        thisNormal.Normalise();
        thisPos3D += thisNormal * GLOBE_ELEVATION;
        
        colour.m_a = 255;
        g_renderer3d->Line3D( lastPos3D.x, lastPos3D.y, lastPos3D.z, 
                              thisPos3D.x, thisPos3D.y, thisPos3D.z, colour, lineWidth );
    }

    Vector3<float> endPos = Vector3<float>( predictedLongitude-m_vel.x.DoubleValue(), 
                                             predictedLatitude-m_vel.y.DoubleValue(), 0 );
    Vector3<float> startPos3D = globeRenderer->ConvertLongLatTo3DPosition(predictedLongitude, predictedLatitude);
    Vector3<float> endPos3D = globeRenderer->ConvertLongLatTo3DPosition(endPos.x, endPos.y);
    
    Vector3<float> startNormal = startPos3D;
    startNormal.Normalise();
    startPos3D += startNormal * GLOBE_ELEVATION;
    
    Vector3<float> endNormal = endPos3D;
    endNormal.Normalise();
    endPos3D += endNormal * GLOBE_ELEVATION;
    
    g_renderer3d->Line3D( startPos3D.x, startPos3D.y, startPos3D.z, 
                          endPos3D.x, endPos3D.y, endPos3D.z, colour, lineWidth );
}

bool GunFire::MoveToWaypoint()
{
    Fixed distToTarget = g_app->GetWorld()->GetDistance( m_longitude, m_latitude, m_targetLongitude, m_targetLatitude);
    Fixed timePerUpdate = SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor();

    Fixed newLongitude;
    Fixed newLatitude;
    Fixed newDistance;

    CalculateNewPosition( &newLongitude, &newLatitude, &newDistance );

    // if the unit has reached the edge of the map, move it to the other side and update all nessecery information
    if( m_longitude <= -180 ||
        m_longitude >= 180 )
    {
        CrossSeam();
        CalculateNewPosition( &newLongitude, &newLatitude, &newDistance );
    }

    if( newDistance <= distToTarget && 
        newDistance < Fixed::Hundredths(50) &&
        m_targetLongitudeAcrossSeam == 0 )
    {
        ClearWaypoints();
        m_vel.Zero();

        return true;
    }
    else
    {
        m_range -= Vector3<Fixed>( m_vel.x * Fixed(timePerUpdate), m_vel.y * Fixed(timePerUpdate), 0 ).Mag();
        m_longitude = newLongitude;
        m_latitude = newLatitude;
        m_distanceToTarget -= Fixed(timePerUpdate) * m_vel.Mag();
    }

    return false;    
}

void GunFire::CalculateNewPosition( Fixed *newLongitude, Fixed *newLatitude, Fixed *newDistance )
{
    Fixed wayLong = m_targetLongitude;
    Fixed wayLat = m_targetLatitude;
    World::SanitiseTargetLongitude( m_longitude, wayLong );
    Vector3<Fixed> targetDir = (Vector3<Fixed>( wayLong, wayLat, 0 ) -
                                Vector3<Fixed>( m_longitude, m_latitude, 0 )).Normalise();    
    
    Fixed distance = (Vector3<Fixed>( wayLong, wayLat, 0 ) -
                      Vector3<Fixed>( m_longitude, m_latitude, 0 )).Mag();

    if( !m_useFixedSpeed )
    {
        m_speed = distance / 50;
        Fixed minSpeed = Fixed::Hundredths(40) / g_app->GetWorld()->GetUnitScaleFactor();
        Fixed maxSpeed = 2 / g_app->GetWorld()->GetUnitScaleFactor();
        Clamp( m_speed, minSpeed, maxSpeed );
    }

    targetDir * m_speed;

    Fixed timePerUpdate = SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor();
    
    Fixed factor1 = m_turnRate * timePerUpdate / 10;
    Fixed factor2 = 1 - factor1;
    m_vel = ( targetDir * factor1 ) + ( m_vel * factor2 );
    m_vel.Normalise();
    m_vel *= m_speed;

    // Latitude speed: near poles, same real speed = more map degrees per tick
    Fixed latitudeFactor = Fixed(90) - m_latitude.abs();
    if( latitudeFactor < Fixed(10) ) latitudeFactor = Fixed(10);
    Fixed speedMult = Fixed(1) + (((Fixed(90) / latitudeFactor) - Fixed(1)) * Fixed(2) / Fixed(8));
    if( speedMult > Fixed(3) ) speedMult = Fixed(3);
    m_vel *= speedMult;

    *newLongitude = m_longitude + m_vel.x * timePerUpdate;
    *newLatitude = m_latitude + m_vel.y * timePerUpdate;
    *newDistance =g_app->GetWorld()->GetDistance( *newLongitude, *newLatitude, m_targetLongitude, m_targetLatitude );

}


void GunFire::SetInitialVelocityTowardWaypoint()
{
    Fixed wayLong = m_targetLongitude;
    Fixed wayLat = m_targetLatitude;
    World::SanitiseTargetLongitude( m_longitude, wayLong );
    Vector3<Fixed> targetDir = (Vector3<Fixed>( wayLong, wayLat, 0 ) -
                                Vector3<Fixed>( m_longitude, m_latitude, 0 )).Normalise();
    Fixed magSq = targetDir.x * targetDir.x + targetDir.y * targetDir.y;
    if( magSq > Fixed::Hundredths(1) )
    {
        m_vel = targetDir * m_speed;
    }
    else
    {
        m_vel.Zero();
    }
}


void GunFire::Impact()
{
    WorldObject *targetObject = g_app->GetWorld()->GetWorldObject(m_targetObjectId);
    if( targetObject )
    {
        if( targetObject->IsSubmarine() &&
            targetObject->m_currentState == 3 )
        {
            m_attackOdds *= 2;
        }
        int randomChance = syncfrand(100).IntValue();
        if( Fixed(randomChance) < m_attackOdds )
        {
            targetObject->m_life--;               
            targetObject->m_life = max( targetObject->m_life, 0 );
            if( targetObject->m_life == 0 )
            {
                g_app->GetWorld()->CreateExplosion( m_teamId, targetObject->m_longitude, targetObject->m_latitude, 30, targetObject->m_teamId );

                WorldObject *origin = g_app->GetWorld()->GetWorldObject(m_origin);
                if( origin )
                {
                    origin->SetTargetObjectId(-1);
                    //g_app->GetWorld()->AddDestroyedMessage( m_origin, m_targetObjectId );
                    origin->m_isRetaliating = false;
					targetObject->m_lastHitByTeamId = origin->m_teamId;
                }
            }
        }
        g_app->GetWorld()->CreateExplosion( m_teamId, m_longitude, m_latitude, 10, targetObject->m_teamId );
        targetObject->Retaliate( m_origin );
    }
}
