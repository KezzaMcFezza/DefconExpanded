#include "lib/universal_include.h"
#include "world/carrier_light.h"
#include "world/carrier.h"

CarrierLight::CarrierLight()
:   Carrier()
{
    SetType( WorldObject::TypeCarrierLight );
}
