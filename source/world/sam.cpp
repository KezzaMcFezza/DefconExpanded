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
    m_life = 5;

    AddState( LANGUAGEPHRASE("state_airdefense"), 60, 20, 10, 30, true, -1, 3 );

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

void SAM::AirDefense()
{
    if( g_app->GetWorld()->GetDefcon() > 3 )
        return;

    // SAM targets Aircraft archetype only (LACM, LANM, Bomber, Fighter); ABM handles ballistic.
    LList<int> excluded;
    BurstFireGetExcludedIds( excluded );
    const LList<int> *excludePtr = excluded.Size() > 0 ? &excluded : nullptr;
    Fixed actionRangeSqd = GetActionRangeSqd();

    // Priority 1: LACM (cruise missiles)
    int lacmId = g_app->GetWorld()->GetNearestObject( m_teamId, m_longitude, m_latitude, WorldObject::TypeLACM, true, excludePtr );
    WorldObject *lacm = g_app->GetWorld()->GetWorldObject( lacmId );
    if( lacm && g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, lacm->m_longitude, lacm->m_latitude ) <= actionRangeSqd )
    {
        Action( lacm->m_objectId, -1, -1 );
        return;
    }

    // Priority 2: LANM
    int lanmId = g_app->GetWorld()->GetNearestObject( m_teamId, m_longitude, m_latitude, WorldObject::TypeLANM, true, excludePtr );
    WorldObject *lanm = g_app->GetWorld()->GetWorldObject( lanmId );
    if( lanm && g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, lanm->m_longitude, lanm->m_latitude ) <= actionRangeSqd )
    {
        Action( lanm->m_objectId, -1, -1 );
        return;
    }

    // Priority 3: Bombers
    int bomberId = g_app->GetWorld()->GetNearestObject( m_teamId, m_longitude, m_latitude, WorldObject::TypeBomber, true, excludePtr );
    WorldObject *bomber = g_app->GetWorld()->GetWorldObject( bomberId );
    if( bomber && g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, bomber->m_longitude, bomber->m_latitude ) <= actionRangeSqd )
    {
        Action( bomber->m_objectId, -1, -1 );
        return;
    }

    // Priority 4: Fighters
    int fighterId = g_app->GetWorld()->GetNearestObject( m_teamId, m_longitude, m_latitude, WorldObject::TypeFighter, true, excludePtr );
    WorldObject *fighter = g_app->GetWorld()->GetWorldObject( fighterId );
    if( fighter && g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, fighter->m_longitude, fighter->m_latitude ) <= actionRangeSqd )
    {
        Action( fighter->m_objectId, -1, -1 );
        return;
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
