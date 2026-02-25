
#ifndef _included_abm_h
#define _included_abm_h

#include "world/sam.h"


/** ABM: Anti-ballistic missile. Inherits SAM but targets only BallisticMissile archetype (Nuke, CBM). */
class ABM : public SAM
{
public:
    ABM();

    void    AirDefense() override;
    int     GetAttackOdds( int _defenderType ) override;
    int     IsValidCombatTarget( int _objectId ) override;
    Image  *GetBmpImage( int state ) override;
};


#endif
