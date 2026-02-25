#include "lib/universal_include.h"

#include "lib/language_table.h"
#include "lib/sound/soundsystem.h"
#include "lib/math/math_utils.h"
#include "lib/math/vector3.h"
#include "lib/math/random_number.h"

#include "app/app.h"
#include "app/globals.h"

#include "world/world.h"
#include "world/cbm.h"


CBM::CBM()
:   Nuke(),
    m_origin(-1)
{
    SetType( TypeCBM );
    strcpy( bmpImageFilename, "graphics/nuke.bmp" );
}


void CBM::OnImpact()
{
    // Ship target: damage directly (CreateExplosion intensity 30 does not damage units).
    // Explosion at m_longitude/m_latitude (already set to target position by Nuke::Update).
    if( m_targetObjectId != -1 )
    {
        WorldObject *targetObj = g_app->GetWorld()->GetWorldObject( m_targetObjectId );
        if( targetObj && targetObj->m_life > 0 &&
            ( targetObj->IsCarrierClass() || targetObj->IsBattleShipClass() ||
              ( targetObj->IsSubmarine() && !targetObj->IsHiddenFrom() ) ) )
        {
            int randomChance = syncfrand(100).IntValue();
            Fixed attackOdds = g_app->GetWorld()->GetAttackOdds( TypeCBM, targetObj->m_type, m_origin );
            if( Fixed(randomChance) < attackOdds )
            {
                targetObj->m_life -= 3;
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
            g_app->GetWorld()->CreateExplosion( m_teamId, m_longitude, m_latitude, 30, targetObj->m_teamId, false );
            targetObj->Retaliate( m_origin );
            return;
        }
    }
    g_app->GetWorld()->CreateExplosion( m_teamId, m_longitude, m_latitude, 30, -1, false );
}


bool CBM::Update()
{
    // Waypoint = target position (like LACM). Direction uses curved ballistic path + interception
    // lead. See Nuke for m_directionLead - CBM sets it so we steer slightly ahead of the waypoint
    // when the target is moving, while impact is always at the waypoint (= ship).
    if( m_targetObjectId != -1 && !m_targetLocked )
    {
        WorldObject *target = g_app->GetWorld()->GetWorldObject( m_targetObjectId );
        if( target &&
            ( target->IsCarrierClass() || target->IsBattleShipClass() ||
              ( target->IsSubmarine() && !target->IsHiddenFrom() ) ) )
        {
            Fixed waypointLon = target->m_longitude;
            Fixed waypointLat = target->m_latitude;
            UpdateWaypoint( waypointLon, waypointLat );

            // Interception lead: bias direction toward where target will be (for moving targets).
            // Waypoint stays the target; lead adjusts steering only.
            if( target->m_life > 0 )
            {
                Fixed interceptLon, interceptLat;
                GetCombatInterceptionPoint( target, &interceptLon, &interceptLat );
                Fixed leadFactor = Fixed::Hundredths(40);  // 40% toward intercept
                SetDirectionLead( (interceptLon - waypointLon) * leadFactor,
                                 (interceptLat - waypointLat) * leadFactor );
            }
            else
            {
                SetDirectionLead( 0, 0 );
            }

            // Overshoot: we crossed over the target. Impact at waypoint.
            Fixed distToTarget = g_app->GetWorld()->GetDistance( m_longitude, m_latitude,
                waypointLon, waypointLat );
            Fixed tolerance = Fixed::Hundredths(1);
            if( m_prevDistanceToTarget < Fixed::MAX &&
                distToTarget > m_prevDistanceToTarget + tolerance &&
                distToTarget < 1 )
            {
                m_longitude = waypointLon;
                m_latitude  = waypointLat;
                m_targetLongitude = 0;
                m_targetLatitude = 0;
                m_vel.Zero();
                SetDirectionLead( 0, 0 );
                OnImpact();
#ifdef TOGGLE_SOUND
                g_soundSystem->TriggerEvent( SoundObjectId(m_objectId), "Detonate" );
#endif
                return true;
            }
            m_prevDistanceToTarget = distToTarget;
        }
        else
        {
            SetDirectionLead( 0, 0 );
        }
    }

    return Nuke::Update();
}
