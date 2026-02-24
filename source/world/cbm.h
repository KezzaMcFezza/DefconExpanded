
#ifndef _included_cbm_h
#define _included_cbm_h

#include "world/nuke.h"


/** Conventional Ballistic Missile: Nuke trajectory, cruise-missile (conventional) explosion.
 *  Can track a non-flying moving target (ship) and recalculate impact point. */
class CBM : public Nuke
{
public:
    CBM();

    void    OnImpact() override;
    bool    Update() override;
};


#endif
