#ifndef _included_subk_h
#define _included_subk_h

#include "world/sub.h"

class SubK : public Sub
{
    WorldObject *FindTarget      ( const LList<int> *excludeIds = nullptr );
    bool         IsSafeTarget    ( Fleet *_fleet );

public:

    SubK();

    void    Action          ( int targetObjectId, Fixed longitude, Fixed latitude );
    bool    Update          ();
    void    RunAI           ();
    int     GetAttackState  ();
    bool    UsingNukes      () override;
    void    SetState        ( int state );
    bool    IsActionQueueable();
    bool    IsGroupable     ( int unitType );
    void    RequestAction   ( ActionOrder *_action );

    int     GetAttackOdds   ( int _defenderType );
    int     IsValidCombatTarget     ( int _objectId );
    int     IsValidMovementTarget   ( Fixed longitude, Fixed latitude );
};


#endif
