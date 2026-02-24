
#ifndef _included_lanm_h
#define _included_lanm_h

#include "world/lacm.h"


/** Land-Attack Nuclear Missile: LACM trajectory (cruise, can track ships), nuclear explosion on impact. */
class LANM : public LACM
{
public:
    LANM();

    int  GetExplosionIntensity() const override { return 100; }
    bool IsNuclearExplosion() const override { return true; }
};


#endif
