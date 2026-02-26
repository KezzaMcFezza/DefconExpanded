
#ifndef _included_sub_h
#define _included_sub_h

#include "world/movingobject.h"


class Sub : public MovingObject
{    
protected:
    bool    m_hidden;

    WorldObject *FindTarget      ( const LList<int> *excludeIds = nullptr );
    bool         IsSafeTarget    ( Fleet *_fleet );

public:

    Sub();

    void    Render2D        ();
    void    Render3D        ();

    void    Action          ( int targetObjectId, Fixed longitude, Fixed latitude );
    void    AcquireTargetFromAction( ActionOrder *action ) override;
    bool    IsHiddenFrom    ();
    bool    Update          ();
    void    RunAI           ();
    int     GetAttackState  ();
    bool    IsIdle          ();
    bool    UsingGuns       ();
    bool    UsingNukes      ();
    bool    ShouldProcessActionQueue();
    void    SetState        ( int state );
    void    ChangePosition  ();
    bool    IsActionQueueable();
    bool    IsGroupable     ( int unitType );
    bool    IsPinging       ();
    void    FleetAction     ( int targetObjectId );

    void    RequestAction   ( ActionOrder *_action );

    void    FireGun                 ( Fixed range ) override;
    int     GetBurstFireShots       () const;
    int     GetAttackOdds           ( int _defenderType );
    int     IsValidCombatTarget     ( int _objectId );
    int     IsValidMovementTarget   ( Fixed longitude, Fixed latitude );
};


#endif
