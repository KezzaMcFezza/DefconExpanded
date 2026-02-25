
#ifndef _included_silomobilecon_h
#define _included_silomobilecon_h

#include "world/silomobile.h"


/** Mobile silo that launches CBM (conventional ballistic) instead of nuke. */
class SiloMobileCon : public SiloMobile
{
public:
    SiloMobileCon();

    void Action( int targetObjectId, Fixed longitude, Fixed latitude ) override;
    int  GetAttackOdds( int _defenderType ) override;
    int  IsValidCombatTarget( int _objectId ) override;
    int  IsValidMovementTarget( Fixed longitude, Fixed latitude ) override;
    bool UsingNukes() override { return false; }
    bool UsesConventionalBallistic() override { return true; }
};


#endif
