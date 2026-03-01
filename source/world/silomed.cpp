#include "lib/universal_include.h"

#include "lib/resource/resource.h"
#include "lib/language_table.h"

#include "app/app.h"
#include "app/globals.h"

#include "world/world.h"
#include "world/silo.h"
#include "world/silomed.h"


SiloMed::SiloMed()
:   Silo()
{
    SetType( TypeSiloMed );
    strcpy( bmpImageFilename, "graphics/silo.bmp" );

    m_life = 15;
    m_nukeSupply = 6;
    m_states[0]->m_numTimesPermitted = 6;
    m_states[1]->m_numTimesPermitted = 6;

    Fixed gameScale = World::GetUnitScaleFactor();
    Fixed range90 = Fixed( 90 ) / gameScale;
    m_states[0]->m_actionRange = range90;
    m_states[1]->m_actionRange = range90;
}

Fixed SiloMed::GetNukeLaunchRange() const
{
    Fixed gameScale = World::GetUnitScaleFactor();
    return Fixed( 45 ) / gameScale;
}

int SiloMed::IsValidCombatTarget( int _objectId )
{
    int result = Silo::IsValidCombatTarget( _objectId );
    if( result < WorldObject::TargetTypeInvalid ) return result;
    if( result != WorldObject::TargetTypeLaunchNuke ) return result;

    WorldObject *obj = g_app->GetWorld()->GetWorldObject( _objectId );
    if( !obj ) return WorldObject::TargetTypeInvalid;

    Fixed distSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, obj->m_longitude, obj->m_latitude );
    Fixed rangeSqd = GetNukeLaunchRange() * GetNukeLaunchRange();
    if( distSqd > rangeSqd ) return WorldObject::TargetTypeOutOfRange;
    return result;
}

int SiloMed::IsValidMovementTarget( Fixed longitude, Fixed latitude )
{
    int result = Silo::IsValidMovementTarget( longitude, latitude );
    if( result < WorldObject::TargetTypeInvalid ) return result;
    if( result != WorldObject::TargetTypeLaunchNuke ) return result;

    Fixed distSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, longitude, latitude );
    Fixed rangeSqd = GetNukeLaunchRange() * GetNukeLaunchRange();
    if( distSqd > rangeSqd ) return WorldObject::TargetTypeOutOfRange;
    return result;
}
