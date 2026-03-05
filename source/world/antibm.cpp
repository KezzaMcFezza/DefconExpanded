#include "lib/universal_include.h"

#include "lib/resource/image.h"
#include "lib/render/renderer.h"
#include "lib/render2d/renderer_2d.h"
#include "lib/render3d/renderer_3d.h"
#include "lib/math/vector3.h"

#include "app/app.h"
#include "app/globals.h"

#include "renderer/globe_renderer.h"
#include "world/world.h"
#include "world/antibm.h"
#include "world/movingobject.h"
#include "world/team.h"


AntiBM::AntiBM( Fixed range )
:   GunFire( range )
{
    m_useFixedSpeed = true;
    m_speed = Fixed::Hundredths(60);
    m_speed /= g_app->GetWorld()->GetUnitScaleFactor();
    m_turnRate = Fixed::Hundredths(60);
    m_turnRate /= g_app->GetWorld()->GetUnitScaleFactor();
    m_maxHistorySize = 10;
    m_movementType = MovementTypeAir;

    strcpy( bmpImageFilename, "graphics/laser.bmp" );
}

void AntiBM::Render3D()
{
    float gameScale = World::GetGameScale().DoubleValue();
    if( gameScale < 0.01f ) gameScale = 1.0f;
    float lineWidth = 1.0f / gameScale;
    if( lineWidth < 0.25f ) lineWidth = 0.25f;
    
    GlobeRenderer *globeRenderer = g_app->GetGlobeRenderer();
    if( !g_app->IsGlobeMode() || !globeRenderer || !globeRenderer->ShouldUse3DNukeTrajectories() )
    {
        GunFire::Render3D();
        return;
    }

    WorldObject *origin = g_app->GetWorld()->GetWorldObject( m_origin );
    if( !origin )
    {
        GunFire::Render3D();
        return;
    }

    float launchLon = origin->m_longitude.DoubleValue();
    float launchLat = origin->m_latitude.DoubleValue();
    float targetLon = m_targetLongitude.DoubleValue();
    float targetLat = m_targetLatitude.DoubleValue();
    Fixed totalDistance = g_app->GetWorld()->GetDistance(
        origin->m_longitude, origin->m_latitude, m_targetLongitude, m_targetLatitude );

    if( totalDistance <= 0 )
    {
        GunFire::Render3D();
        return;
    }

    Fixed predictionTime = Fixed::FromDouble(g_predictionTime) * g_app->GetWorld()->GetTimeScaleFactor();
    float predictedLon = (m_longitude + m_vel.x * predictionTime).DoubleValue();
    float predictedLat = (m_latitude + m_vel.y * predictionTime).DoubleValue();

    Team *team = g_app->GetWorld()->GetTeam(m_teamId);
    Colour colour = team ? team->GetTeamColour() : Colour(255, 255, 255, 255);
    float maxSize = 10;

    for( int i = 1; i < m_history.Size(); ++i )
    {
        Vector3<Fixed> lastPos = *m_history[i-1];
        Vector3<Fixed> thisPos = *m_history[i];
        float lastX = lastPos.x.DoubleValue(), lastY = lastPos.y.DoubleValue();
        float thisX = thisPos.x.DoubleValue(), thisY = thisPos.y.DoubleValue();

        if( lastX < -170 && thisX > 170 )
            thisX = -180 - ( 180 - thisX );
        if( lastX > 170 && thisX < -170 )
            thisX = 180 + ( 180 - fabsf(thisX) );

        Vector3<float> lastPos3D = globeRenderer->CalculateBallisticProjectile3DPosition(
            launchLon, launchLat, targetLon, targetLat, lastX, lastY, totalDistance );
        Vector3<float> thisPos3D = globeRenderer->CalculateBallisticProjectile3DPosition(
            launchLon, launchLat, targetLon, targetLat, thisX, thisY, totalDistance );

        colour.m_a = (unsigned char)(255 - 255 * (float)i / maxSize);
        g_renderer3d->Line3D( lastPos3D.x, lastPos3D.y, lastPos3D.z,
                              thisPos3D.x, thisPos3D.y, thisPos3D.z, colour, lineWidth );
    }

    if( m_history.Size() > 0 )
    {
        Vector3<Fixed> lastPos = *m_history[0];
        float lastX = lastPos.x.DoubleValue(), lastY = lastPos.y.DoubleValue();

        if( lastX < -170 && predictedLon > 170 )
            predictedLon = -180 - ( 180 - predictedLon );
        if( lastX > 170 && predictedLon < -170 )
            predictedLon = 180 + ( 180 - fabsf(predictedLon) );

        Vector3<float> lastPos3D = globeRenderer->CalculateBallisticProjectile3DPosition(
            launchLon, launchLat, targetLon, targetLat, lastX, lastY, totalDistance );
        Vector3<float> thisPos3D = globeRenderer->CalculateBallisticProjectile3DPosition(
            launchLon, launchLat, targetLon, targetLat, predictedLon, predictedLat, totalDistance );

        colour.m_a = 255;
        g_renderer3d->Line3D( lastPos3D.x, lastPos3D.y, lastPos3D.z,
                              thisPos3D.x, thisPos3D.y, thisPos3D.z, colour, lineWidth );
    }

    float endX = predictedLon - m_vel.x.DoubleValue();
    float endY = predictedLat - m_vel.y.DoubleValue();
    Vector3<float> startPos3D = globeRenderer->CalculateBallisticProjectile3DPosition(
        launchLon, launchLat, targetLon, targetLat, predictedLon, predictedLat, totalDistance );
    Vector3<float> endPos3D = globeRenderer->CalculateBallisticProjectile3DPosition(
        launchLon, launchLat, targetLon, targetLat, endX, endY, totalDistance );

    g_renderer3d->Line3D( startPos3D.x, startPos3D.y, startPos3D.z,
                          endPos3D.x, endPos3D.y, endPos3D.z, colour, lineWidth );
}

void AntiBM::GetCombatInterceptionPoint( WorldObject *target, Fixed *interceptLongitude, Fixed *interceptLatitude )
{
    // AntiBM has its own interception math (speed 60, turn rate 60) - separate from GunFire which
    // is tuned for speed 30 / turn rate 30. ABM only targets ballistic missiles.
    if( !target->IsBallisticMissileClass() )
    {
        MovingObject::GetCombatInterceptionPoint( target, interceptLongitude, interceptLatitude );
        return;
    }

    Vector3<Fixed> targetVel = target->m_vel;
    Fixed targetLongitude = target->m_longitude;
    World::SanitiseTargetLongitude( m_longitude, targetLongitude );
    Vector3<Fixed> distance( targetLongitude - m_longitude, target->m_latitude - m_latitude, 0 );

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

    if( timeLeft < 0 ) timeLeft = 0;

    // Base lead: 85-95% - nukes are slower now; old 95-110% was for faster missiles with terminal
    // acceleration. Reduced lead prevents ABM overshooting slower ballistic targets.
    Fixed closeDist = Fixed(2);
    Fixed farDist   = Fixed(20);
    Fixed leadRange = farDist - closeDist;
    Fixed distFactor = ( distanceMag - closeDist ) / leadRange;
    if( distFactor < 0 ) distFactor = 0;
    if( distFactor > 1 ) distFactor = 1;
    Fixed leadFactor = Fixed::Hundredths(85) + distFactor * Fixed::Hundredths(10);

    // Reduce lead as latitude increases: steeper curve so extreme polar lat gets strong reduction.
    // Quadratic: most drop happens near poles. 100% at equator, ~40% at poles.
    Fixed targetLatAbs = target->m_latitude.abs();
    Fixed latNorm = targetLatAbs / Fixed(90);
    Fixed poleScale = Fixed(1) - latNorm * latNorm * Fixed::Hundredths(60);
    if( poleScale < Fixed::Hundredths(40) ) poleScale = Fixed::Hundredths(40);
    leadFactor = leadFactor * poleScale;

    timeLeft = timeLeft * leadFactor;

    *interceptLongitude = targetLongitude + timeLeft * targetVel.x;
    *interceptLatitude  = target->m_latitude + timeLeft * targetVel.y;
    World::SanitiseTargetLongitude( m_longitude, *interceptLongitude );
}
