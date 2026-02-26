#include "lib/universal_include.h"
#include "world/radar_ew.h"

RadarEW::RadarEW()
:   RadarStation()
{
    SetType( WorldObject::TypeRadarEW );
    strcpy( bmpImageFilename, "graphics/pavepaws.bmp" );

    m_life = 3;
}
