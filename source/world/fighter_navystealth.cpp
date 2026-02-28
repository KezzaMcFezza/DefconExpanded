#include "lib/universal_include.h"
#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/language_table.h"

#include "app/app.h"
#include "app/globals.h"

#include "world/worldobject.h"
#include "world/fighter_navystealth.h"
#include "world/fighter.h"

FighterNavyStealth::FighterNavyStealth()
:   Fighter()
{
    SetType( TypeFighterNavyStealth );

    strcpy( bmpImageFilename, "graphics/f35c.bmp" );

    m_stealthType = 30;

    m_states[0]->m_numTimesPermitted = 2;
}
