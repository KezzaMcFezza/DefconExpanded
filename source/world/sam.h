
#ifndef _included_sam_h
#define _included_sam_h

#include "world/worldobject.h"

class SAM : public WorldObject
{
public:
    SAM();

    void    Action          ( int targetObjectId, Fixed longitude, Fixed latitude );
    bool    Update          ();
    int     GetTargetObjectId();
    bool    UsingGuns       ();
    void    AirDefense      ();

    int     GetAttackOdds       ( int _defenderType );
    int     IsValidCombatTarget ( int _objectId );

    virtual Image   *GetBmpImage    ( int state );
};


#endif
