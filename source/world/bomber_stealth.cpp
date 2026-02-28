#include "lib/universal_include.h"
#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/language_table.h"

#include "app/app.h"
#include "app/globals.h"

#include "world/world.h"
#include "world/bomber_stealth.h"
#include "world/bomber.h"


BomberStealth::BomberStealth()
:   Bomber()
{
    SetType( TypeBomberStealth );

    strcpy( bmpImageFilename, "graphics/stealthbomber.bmp" );

    m_stealthType = 10;

    m_nukeSupply = 1;
    m_states[0]->m_numTimesPermitted = 1;
    m_states[1]->m_numTimesPermitted = 1;
    m_states[2]->m_numTimesPermitted = 2;
}
