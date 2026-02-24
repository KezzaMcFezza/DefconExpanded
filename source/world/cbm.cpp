#include "lib/universal_include.h"

#include "lib/language_table.h"
#include "lib/sound/soundsystem.h"
#include "lib/math/math_utils.h"
#include "lib/math/vector3.h"

#include "app/app.h"
#include "app/globals.h"

#include "world/world.h"
#include "world/cbm.h"


CBM::CBM()
:   Nuke()
{
    SetType( TypeCBM );
    strcpy( bmpImageFilename, "graphics/nuke.bmp" );
}


void CBM::OnImpact()
{
    g_app->GetWorld()->CreateExplosion( m_teamId, m_longitude, m_latitude, 30, -1, false );
}


bool CBM::Update()
{
    // Track non-flying moving target (ship): recalculate waypoint so we land on predicted position
    if( m_targetObjectId != -1 && !m_targetLocked )
    {
        WorldObject *target = g_app->GetWorld()->GetWorldObject( m_targetObjectId );
        if( target && target->m_life > 0 &&
            ( target->IsCarrierClass() || target->IsBattleShipClass() ||
              ( target->IsSubmarine() && !target->IsHiddenFrom() ) ) )
        {
            Vector3<Fixed> pos( m_longitude, m_latitude, 0 );
            Vector3<Fixed> tpos( target->m_longitude, target->m_latitude, 0 );
            Fixed remainingDist = ( tpos - pos ).Mag();
            Fixed timePerUpdate = SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor();
            Fixed speed = m_speed;
            Fixed latitudeFactor = Fixed(90) - m_latitude.abs();
            if( latitudeFactor < Fixed(10) ) latitudeFactor = Fixed(10);
            Fixed speedMultiplier = Fixed(1) + (((Fixed(90) / latitudeFactor) - Fixed(1)) * Fixed(2) / Fixed(8));
            if( speedMultiplier > Fixed(3) ) speedMultiplier = Fixed(3);
            speed *= speedMultiplier;
            Fixed timeToImpact = ( speed > 0 && remainingDist > 0 ) ? remainingDist / speed : timePerUpdate * Fixed(10);

            Fixed predLon = target->m_longitude + target->m_vel.x * timeToImpact;
            Fixed predLat = target->m_latitude + target->m_vel.y * timeToImpact;
            SetWaypoint( predLon, predLat );
        }
    }

    return Nuke::Update();
}
