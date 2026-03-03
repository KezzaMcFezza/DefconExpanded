
#ifndef _included_antibm_h
#define _included_antibm_h

#include "world/gunfire.h"


class AntiBM : public GunFire
{
public:

    AntiBM( Fixed range );

    void    Render3D() override;
    void    GetCombatInterceptionPoint( WorldObject *target, Fixed *interceptLongitude, Fixed *interceptLatitude ) override;
};


#endif
