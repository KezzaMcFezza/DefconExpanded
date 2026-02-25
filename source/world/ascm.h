
#ifndef _included_ascm_h
#define _included_ascm_h

#include "world/silomed.h"


/** ASCM: Anti-Ship Cruise Missile battery. Silo that launches LACM at ships only.
 *  No standby mode, auto-targets ships in range, regens ammo at 1 per 15 minutes.
 *  Launch mode available at Defcon 3. */
class ASCM : public SiloMed
{
public:
    ASCM();

    void    Action          ( int targetObjectId, Fixed longitude, Fixed latitude ) override;
    bool    Update          () override;
    void    RunAI           () override;
    void    SetState        ( int state ) override;

    bool    UsingNukes      () override { return false; }
    int     GetAttackOdds   ( int _defenderType ) override;
    int     IsValidCombatTarget ( int _objectId ) override;
    int     IsValidMovementTarget( Fixed longitude, Fixed latitude ) override;

    bool    IsActionQueueable() override { return true; }
    bool    ShouldProcessActionQueue() override { return true; }

    void    Render2D       () override;
    void    Render3D       () override;
    Image   *GetBmpImage   ( int state ) override;

private:
    Fixed   m_ammoRegenTimer;   // Seconds until next +1 ammo (15 min = 900)
};


#endif
