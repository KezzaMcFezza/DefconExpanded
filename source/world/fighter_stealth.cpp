#include "lib/universal_include.h"
#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/language_table.h"

#include "app/app.h"
#include "app/globals.h"

#include "world/worldobject.h"
#include "world/fighter_stealth.h"
#include "world/fighter.h"


FighterStealth::FighterStealth()
:   Fighter()
{
    SetType( TypeFighterStealth );

    strcpy( bmpImageFilename, "graphics/f22.bmp" );

    m_stealthType = 10;

    m_states[0]->m_numTimesPermitted = 2;
}
