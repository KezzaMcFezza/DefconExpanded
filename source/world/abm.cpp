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

    Team *team = g_app->GetWorld()->GetTeam( m_teamId );
    if( team ) team->RegisterEngagedTarget( m_targetObjectId );
}

Image *ABM::GetBmpImage( int state )
{
    (void)state;
    return g_resource->GetImage( "graphics/abm.bmp" );
}


int ABM::FindBestBallisticTarget( const LList<int> *excludeIds )
{
    Fixed actionRangeSqd = GetActionRangeSqd();
    static const int priorities[] = { WorldObject::TypeNuke, WorldObject::TypeCBM };
    for( int p = 0; p < 2; ++p )
    {
        int id = g_app->GetWorld()->GetNearestObject( m_teamId, m_longitude, m_latitude,
                                                       priorities[p], true, excludeIds );
        WorldObject *obj = g_app->GetWorld()->GetWorldObject( id );
        if( obj && g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude,
                                                       obj->m_longitude, obj->m_latitude ) <= actionRangeSqd )
        {
            return obj->m_objectId;
        }
    }
    return -1;
}


void ABM::AirDefense()
{
    if( g_app->GetWorld()->GetDefcon() > 3 )
        return;

    Team *team = g_app->GetWorld()->GetTeam( m_teamId );

    LList<int> burstExcluded;
    BurstFireGetExcludedIds( burstExcluded );

    LList<int> fullExcluded;
    for( int i = 0; i < burstExcluded.Size(); ++i )
        fullExcluded.PutData( burstExcluded[i] );
    if( team ) team->GetRecentlyEngagedIds( fullExcluded );

    int targetId = FindBestBallisticTarget( fullExcluded.Size() > 0 ? &fullExcluded : nullptr );
    if( targetId != -1 )
    {
        Action( targetId, -1, -1 );
        if( team ) team->RegisterEngagedTarget( targetId );
        return;
    }

    int engagedId = FindBestBallisticTarget( burstExcluded.Size() > 0 ? &burstExcluded : nullptr );
    if( engagedId != -1 )
    {
        m_retargetTimer = Fixed::Hundredths(50);
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
