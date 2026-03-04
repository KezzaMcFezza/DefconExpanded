
#ifndef _included_cbm_h
#define _included_cbm_h

#include "world/nuke.h"


/** Conventional Ballistic Missile: Nuke trajectory, cruise-missile (conventional) explosion.
 *  Can track a non-flying moving target (ship) and recalculate impact point.
 *  Uses miss time (like LACM) instead of fuel range - self-destructs when target lost or overshooting. */
class CBM : public Nuke
{
    int   m_origin;    // Launcher object ID for Retaliate / GetAttackOdds
    Fixed m_missTime;  // Self-destruct when target lost or overshooting for too long (like LACM)
public:
    CBM();

    void    SetOrigin( int origin ) { m_origin = origin; }
    void    OnImpact() override;
    bool    Update() override;
};


#endif
