#include "lib/universal_include.h"
#include "world/carrier_super.h"
#include "world/carrier.h"
#include "world/world.h"

CarrierSuper::CarrierSuper()
:   Carrier()
{
    SetType( WorldObject::TypeCarrierSuper );
    strcpy( bmpImageFilename, "graphics/carrier.bmp" );
      
    m_speed = Fixed::Hundredths(4) / World::GetUnitScaleFactor();
    m_life = 5;
}
