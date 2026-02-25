
#ifndef _included_silomed_h
#define _included_silomed_h

#include "world/silo.h"


/** Silo with MRBM range (45). */
class SiloMed : public Silo
{
public:
    SiloMed();

    Fixed GetNukeLaunchRange() const override;

    int  IsValidCombatTarget ( int _objectId ) override;
    int  IsValidMovementTarget( Fixed longitude, Fixed latitude ) override;
};


#endif
