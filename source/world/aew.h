
#ifndef _included_aew_h
#define _included_aew_h

#include "world/movingobject.h"


class AEW : public MovingObject
{
public:
    AEW();

    void    Action  ( int targetObjectId, Fixed longitude, Fixed latitude );
    bool    Update  ();
    void    RunAI   ();

    void    Land            ( int targetId );

    int     GetAttackState  ();
    int     IsValidCombatTarget( int _objectId );
};


#endif
