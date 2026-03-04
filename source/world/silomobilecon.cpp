#include "lib/universal_include.h"

#include "lib/resource/resource.h"
#include "lib/string_utils.h"
#include "lib/language_table.h"

#include "app/app.h"
#include "app/globals.h"

#include "world/world.h"
#include "world/silo.h"
#include "world/silomobile.h"
#include "world/silomobilecon.h"
#include "world/team.h"


SiloMobileCon::SiloMobileCon()
:   SiloMobile()
{
    SetType( TypeSiloMobileCon );
    m_nukeSupply = -1;   // CBM, not nukes - do not use m_nukeSupply
    // Conventional missiles available at Defcon 3
    if( m_states.Size() > 1 )
        m_states[1]->m_defconPermitted = 3;
    

    Fixed gameScale = World::GetUnitScaleFactor();
    Fixed range45 = Fixed( 45 ) / gameScale;
    m_states[0]->m_actionRange = range45;
    m_states[1]->m_actionRange = range45;
    m_states[1]->m_stateName = newStr( LANGUAGEPHRASE("state_silomednuke") );
}


Fixed SiloMobileCon::GetNukeLaunchRange() const
{
    Fixed gameScale = World::GetUnitScaleFactor();
    return Fixed( 45 ) / gameScale;
}

void SiloMobileCon::Action( int targetObjectId, Fixed longitude, Fixed latitude )
{
    if( !CheckCurrentState() )
        return;

    if( m_stateTimer <= 0 )
    {
        if( m_currentState == 1 )
        {
            g_app->GetWorld()->LaunchCBM( m_teamId, m_objectId, longitude, latitude, GetNukeLaunchRange(), targetObjectId );
            for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
            {
                Team *team = g_app->GetWorld()->m_teams[i];
                m_visible[team->m_teamId] = true;
                m_lastSeenTime[team->m_teamId] = m_ghostFadeTime;
                m_seen[team->m_teamId] = true;
            }
            m_states[0]->m_numTimesPermitted = m_states[1]->m_numTimesPermitted;
        }
        WorldObject::Action( targetObjectId, longitude, latitude );
    }
}


int SiloMobileCon::GetAttackOdds( int _defenderType )
{
    if( ( m_currentState == 0 || m_currentState == 1 ) &&
        m_states[1]->m_numTimesPermitted > 0 )
    {
        return g_app->GetWorld()->GetAttackOdds( TypeCBM, _defenderType );
    }
    return 0;
}


// When there is a valid surface target (building or ship) under the cursor, CBM is launched at that
// target (targetObjectId passed); CBM will track and intercept moving ships. See CBM::Update().
int SiloMobileCon::IsValidCombatTarget( int _objectId )
{
    WorldObject *obj = g_app->GetWorld()->GetWorldObject( _objectId );
    if( !obj ) return TargetTypeInvalid;

    int basicResult = WorldObject::IsValidCombatTarget( _objectId );
    if( basicResult < TargetTypeInvalid ) return basicResult;

    bool isFriend = g_app->GetWorld()->IsFriend( m_teamId, obj->m_teamId );
    int attackOdds = g_app->GetWorld()->GetAttackOdds( TypeCBM, obj->m_type );
    if( ( m_currentState == 0 || m_currentState == 1 ) &&
        m_states[1]->m_numTimesPermitted > 0 &&
        ( attackOdds > 0 || ( isFriend && !obj->IsMovingObject() ) ) )
    {
        Fixed distSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, obj->m_longitude, obj->m_latitude );
        Fixed rangeSqd = GetNukeLaunchRange() * GetNukeLaunchRange();
        if( distSqd > rangeSqd ) return TargetTypeOutOfRange;
        return TargetTypeLaunchCBM;
    }
    return TargetTypeInvalid;
}


// Launch anywhere on map (like nuke) when no unit under cursor; otherwise prefer surface target via combat.
int SiloMobileCon::IsValidMovementTarget( Fixed longitude, Fixed latitude )
{
    if( latitude > 100 || latitude < -100 )
        return TargetTypeInvalid;
    if( ( m_currentState == 0 || m_currentState == 1 ) &&
        m_states[1]->m_numTimesPermitted > 0 )
    {
        Fixed distSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, longitude, latitude );
        Fixed rangeSqd = GetNukeLaunchRange() * GetNukeLaunchRange();
        if( distSqd > rangeSqd ) return TargetTypeOutOfRange;
        return TargetTypeLaunchCBM;
    }
    return TargetTypeInvalid;
}
