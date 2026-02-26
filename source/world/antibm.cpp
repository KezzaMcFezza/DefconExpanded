#include "lib/universal_include.h"

#include "lib/resource/image.h"
#include "lib/render/renderer.h"
#include "lib/render2d/renderer_2d.h"
#include "lib/render3d/renderer_3d.h"
#include "lib/math/vector3.h"

#include "app/app.h"
#include "app/globals.h"

#include "world/world.h"
#include "world/antibm.h"
#include "world/movingobject.h"


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

    // Base lead: 95-110% - ballistic missiles accelerate in terminal descent so constant-velocity
    // extrapolation undershoots; we need extra lead to compensate for target speeding up.
    Fixed closeDist = Fixed(2);
    Fixed farDist   = Fixed(20);
    Fixed leadRange = farDist - closeDist;
    Fixed distFactor = ( distanceMag - closeDist ) / leadRange;
    if( distFactor < 0 ) distFactor = 0;
    if( distFactor > 1 ) distFactor = 1;
    Fixed leadFactor = Fixed::Hundredths(95) + distFactor * Fixed::Hundredths(15);

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
