#include "lib/universal_include.h"

#include "lib/language_table.h"

#include "world/radarstation.h"



RadarStation::RadarStation()
:   WorldObject()
{
    SetType( TypeRadarStation );
    
    strcpy( bmpImageFilename, "graphics/radarstation.bmp" );

    m_radarRange = 20;
    m_selectable = false;  
    
    m_life = 2;
    
    AddState( LANGUAGEPHRASE("state_radar"), 0, 0, 20, 0, false );

}
