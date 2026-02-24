#include "lib/universal_include.h"

#include "lib/language_table.h"

#include "app/app.h"
#include "app/globals.h"

#include "world/world.h"
#include "world/lanm.h"


LANM::LANM()
:   LACM()
{
    SetType( TypeLANM );
    strcpy( bmpImageFilename, "graphics/lacm.bmp" );
}
