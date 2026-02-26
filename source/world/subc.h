#ifndef _included_subc_h
#define _included_subc_h

#include "world/sub.h"

class SubC : public Sub
{
    WorldObject *FindTarget      ( const LList<int> *excludeIds = nullptr );
    bool         IsSafeTarget    ( Fleet *_fleet );

public:

    SubC();

    void    Action          ( int targetObjectId, Fixed longitude, Fixed latitude );
    bool    Update          ();
    void    RunAI           ();
    int     GetAttackState  ();

    bool    UsingNukes      () override;
    bool    UsingLACM       ();
    void    SetState        ( int state );
    bool    IsActionQueueable();
    bool    IsGroupable     ( int unitType );
    void    RequestAction   ( ActionOrder *_action );

    int     GetAttackOdds   ( int _defenderType );
    int     IsValidCombatTarget     ( int _objectId );
    int     IsValidMovementTarget   ( Fixed longitude, Fixed latitude );
};


#endif
