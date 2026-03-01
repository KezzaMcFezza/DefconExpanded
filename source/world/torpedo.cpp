#include "lib/universal_include.h"

#include "lib/resource/image.h"
#include "lib/render/renderer.h"
#include "lib/render2d/renderer_2d.h"
#include "lib/render3d/renderer_3d.h"
#include "lib/math/vector3.h"

#include "app/app.h"
#include "app/globals.h"

#include "world/world.h"
#include "world/torpedo.h"


Torpedo::Torpedo( Fixed range )
:   GunFire( range )
{
    m_useFixedSpeed = true;
    m_speed = Fixed::Hundredths(6);
    m_speed /= g_app->GetWorld()->GetUnitScaleFactor();
    m_turnRate = Fixed::Hundredths(80);
    m_turnRate /= g_app->GetWorld()->GetUnitScaleFactor();
    m_maxHistorySize = -1;
    m_movementType = MovementTypeSea;
}
