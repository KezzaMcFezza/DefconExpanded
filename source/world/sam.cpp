#include "lib/universal_include.h"

#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/language_table.h"
#include "lib/math/random_number.h"

#include "app/app.h"
#include "app/globals.h"

#include "world/world.h"
#include "world/sam.h"


SAM::SAM()
:   WorldObject()
{
    SetType( TypeSAM );

    strcpy( bmpImageFilename, "graphics/sam.bmp" );

    m_stealthType = 100;
    m_radarRange = 20;
    m_selectable = true;

    m_currentState = 0;
    m_life = 3;

    AddState( LANGUAGEPHRASE("state_airdefense"), 60, 10, 10, 15, true, -1, 3 );

    InitialiseTimers();
}

void SAM::Action( int targetObjectId, Fixed longitude, Fixed latitude )
{
    if( !CheckCurrentState() )
    {
        return;
    }

    if( m_stateTimer <= 0 )
    {
        WorldObject *targetObject = g_app->GetWorld()->GetWorldObject(targetObjectId);
        if( targetObject )
        {
            if( targetObject->m_visible[m_teamId] &&
                g_app->GetWorld()->GetAttackOdds( m_type, targetObject->m_type ) > 0 &&
                g_app->GetWorld()->GetDistance( m_longitude, m_latitude, targetObject->m_longitude, targetObject->m_latitude) <= GetActionRange() )
            {
                m_targetObjectId = targetObjectId;
            }
            else
            {
                return;
            }
        }
        else
        {
            return;
        }
        WorldObject::Action( targetObjectId, longitude, latitude );
    }
}

bool SAM::Update()
{
    if( m_stateTimer <= 0 )
    {
        if( m_retargetTimer <= 0 )
        {
            m_retargetTimer = 10;
            AirDefense();
        }
        if( m_targetObjectId != -1 )
        {
            WorldObject *targetObject = g_app->GetWorld()->GetWorldObject(m_targetObjectId);
            if( targetObject )
            {
                if( targetObject->m_visible[m_teamId] &&
                    g_app->GetWorld()->GetAttackOdds( m_type, targetObject->m_type ) > 0 &&
                    g_app->GetWorld()->GetDistance( m_longitude, m_latitude, targetObject->m_longitude, targetObject->m_latitude) <= GetActionRange() )
                {
                    FireGun( GetActionRange() );
                    m_stateTimer = m_states[0]->m_timeToReload;
                    if( BurstFireOnFired( m_targetObjectId ) )
                    {
                        int previousId = m_targetObjectId;
                        m_targetObjectId = -1;
                        m_retargetTimer = 0;
                        AirDefense();
                        if( m_targetObjectId == -1 )
                        {
                            BurstFireAccelerateCountdowns( Fixed(10) );
                            BurstFireRemoveTarget( previousId );
                            m_targetObjectId = previousId;
                        }
                        BurstFireResetShotCount();
                    }
                }
                else
                {
                    m_targetObjectId = -1;
                    BurstFireResetShotCount();
                }
            }
            else
            {
                m_targetObjectId = -1;
                BurstFireResetShotCount();
            }
        }
    }

    return WorldObject::Update();
}

int SAM::GetTargetObjectId()
{
    return m_targetObjectId;
}

bool SAM::UsingGuns()
{
    return true;
}

int SAM::FindBestAirTarget( const LList<int> *excludeIds )
{
    Fixed actionRangeSqd = GetActionRangeSqd();
    static const int priorities[] = { WorldObject::TypeLACM, WorldObject::TypeLANM,
                                      WorldObject::TypeBomber, WorldObject::TypeFighter };
    for( int p = 0; p < 4; ++p )
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


void SAM::AirDefense()
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

    int targetId = FindBestAirTarget( fullExcluded.Size() > 0 ? &fullExcluded : nullptr );
    if( targetId != -1 )
    {
        Action( targetId, -1, -1 );
        if( team ) team->RegisterEngagedTarget( targetId );
        return;
    }

    int engagedId = FindBestAirTarget( burstExcluded.Size() > 0 ? &burstExcluded : nullptr );
    if( engagedId != -1 )
    {
        m_retargetTimer = Fixed::Hundredths(50);
    }
}

int SAM::GetAttackOdds( int _defenderType )
{
    return g_app->GetWorld()->GetAttackOdds( m_type, _defenderType );
}

int SAM::IsValidCombatTarget( int _objectId )
{
    WorldObject *obj = g_app->GetWorld()->GetWorldObject( _objectId );
    if( !obj ) return TargetTypeInvalid;

    int basicResult = WorldObject::IsValidCombatTarget( _objectId );
    if( basicResult < TargetTypeInvalid ) return basicResult;

    // SAM targets Aircraft archetype only (Fighter, Bomber, LACM, LANM)
    if( WorldObject::GetArchetypeForType( obj->m_type ) != WorldObject::ArchetypeAircraft )
        return TargetTypeInvalid;

    // Allow manual targeting of friendlies (e.g. to shoot down stray missiles)
    // AirDefense() auto-targeting still excludes friendlies via GetNearestObject.

    if( g_app->GetWorld()->GetAttackOdds( TypeSAM, obj->m_type ) > 0 )
        return TargetTypeValid;

    return TargetTypeInvalid;
}

Image *SAM::GetBmpImage( int state )
{
    return g_resource->GetImage( "graphics/sam.bmp" );
}
