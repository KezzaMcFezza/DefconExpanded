#include "lib/universal_include.h"
#include "lib/resource/resource.h"
#include "lib/language_table.h"

#include "app/app.h"
#include "app/globals.h"

#include "world/world.h"
#include "world/bomber_fast.h"
#include "world/bomber.h"


BomberFast::BomberFast()
:   Bomber()
{
    SetType( TypeBomberFast );

    strcpy( bmpImageFilename, "graphics/b1b.bmp" );

    m_stealthType = 100;
    m_states[0]->m_numTimesPermitted = 1;
    m_states[1]->m_numTimesPermitted = 1;
    m_speed = Fixed::Hundredths(10);
    m_range = 90;
}
