#include "lib/universal_include.h"
#include "world/carrier_lhd.h"
#include "world/carrier.h"

CarrierLHD::CarrierLHD()
:   Carrier()
{
    SetType( WorldObject::TypeCarrierLHD );
    strcpy( bmpImageFilename, "graphics/type075.bmp" );
    m_life = 2;
}
