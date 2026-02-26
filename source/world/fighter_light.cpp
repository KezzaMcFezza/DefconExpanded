#include "lib/universal_include.h"
#include "lib/resource/resource.h"
#include "lib/language_table.h"

#include "app/app.h"
#include "app/globals.h"

#include "world/worldobject.h"
#include "world/fighter_light.h"
#include "world/fighter.h"


FighterLight::FighterLight()
:   Fighter()
{
    SetType( TypeFighterLight );

    strcpy( bmpImageFilename, "graphics/f16.bmp" );

    m_range = 25;

    m_states[0]->m_numTimesPermitted = 2;
    m_states[1]->m_numTimesPermitted = 1;
}
