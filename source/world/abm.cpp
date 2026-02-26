#include "lib/universal_include.h"

#include "lib/resource/resource.h"
#include "lib/resource/image.h"

#include "app/app.h"
#include "app/globals.h"

#include "world/world.h"
#include "world/abm.h"
#include "world/antibm.h"


ABM::ABM()
:   SAM()
{
    SetType( TypeABM );
    strcpy( bmpImageFilename, "graphics/abm.bmp" );
}

void ABM::FireGun( Fixed range )
{
    WorldObject *targetObject = g_app->GetWorld()->GetWorldObject(m_targetObjectId);
    AppAssert( targetObject );
    AntiBM *bullet = new AntiBM( range );
    bullet->SetPosition( m_longitude, m_latitude );
    bullet->SetTargetObjectId( targetObject->m_objectId );
    bullet->SetTeamId( m_teamId );
    bullet->m_origin = m_objectId;
    Fixed interceptLongitude, interceptLatitude;
    bullet->GetCombatInterceptionPoint( targetObject, &interceptLongitude, &interceptLatitude );
    bullet->SetWaypoint( interceptLongitude, interceptLatitude );
    bullet->SetInitialVelocityTowardWaypoint();
    bullet->m_distanceToTarget = g_app->GetWorld()->GetDistance( m_longitude, m_latitude, interceptLongitude, interceptLatitude );
    bullet->m_attackOdds = g_app->GetWorld()->GetAttackOdds( m_type, targetObject->m_type, m_objectId );
    (void)g_app->GetWorld()->m_gunfire.PutData( bullet );
}

Image *ABM::GetBmpImage( int state )
{
    (void)state;
    return g_resource->GetImage( "graphics/abm.bmp" );
}


void ABM::AirDefense()
{
    if( g_app->GetWorld()->GetDefcon() > 3 )
        return;

    LList<int> excluded;
    BurstFireGetExcludedIds( excluded );
    const LList<int> *excludePtr = excluded.Size() > 0 ? &excluded : nullptr;
    Fixed actionRangeSqd = GetActionRangeSqd();

    // ABM targets ballistic missiles only (Nuke, then CBM)
    int nukeId = g_app->GetWorld()->GetNearestObject( m_teamId, m_longitude, m_latitude, WorldObject::TypeNuke, true, excludePtr );
    WorldObject *nuke = g_app->GetWorld()->GetWorldObject( nukeId );
    if( nuke && g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, nuke->m_longitude, nuke->m_latitude ) <= actionRangeSqd )
    {
        Action( nuke->m_objectId, -1, -1 );
        return;
    }

    int cbmId = g_app->GetWorld()->GetNearestObject( m_teamId, m_longitude, m_latitude, WorldObject::TypeCBM, true, excludePtr );
    WorldObject *cbm = g_app->GetWorld()->GetWorldObject( cbmId );
    if( cbm && g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, cbm->m_longitude, cbm->m_latitude ) <= actionRangeSqd )
    {
        Action( cbm->m_objectId, -1, -1 );
    }
}


int ABM::GetAttackOdds( int _defenderType )
{
    return g_app->GetWorld()->GetAttackOdds( m_type, _defenderType );
}


int ABM::IsValidCombatTarget( int _objectId )
{
    WorldObject *obj = g_app->GetWorld()->GetWorldObject( _objectId );
    if( !obj ) return TargetTypeInvalid;

    int basicResult = WorldObject::IsValidCombatTarget( _objectId );
    if( basicResult < TargetTypeInvalid ) return basicResult;

    // ABM targets BallisticMissile archetype only (Nuke, CBM)
    if( WorldObject::GetArchetypeForType( obj->m_type ) != WorldObject::ArchetypeBallisticMissile )
        return TargetTypeInvalid;

    // Allow manual targeting of friendlies (e.g. to shoot down stray missiles)
    // AirDefense() auto-targeting still excludes friendlies via GetNearestObject.

    if( g_app->GetWorld()->GetAttackOdds( TypeABM, obj->m_type ) > 0 )
        return TargetTypeValid;

    return TargetTypeInvalid;
}
