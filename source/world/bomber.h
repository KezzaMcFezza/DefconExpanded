
#ifndef _included_bomber_h
#define _included_bomber_h

#include "world/movingobject.h"


class Bomber : public MovingObject
{
public:
    bool    m_bombingRun;   // set to true if the bomber is launched to nuke a ground target, in which case it will land after firing

    Fixed   GetNukeTargetLongitude();
    Fixed   GetNukeTargetLatitude();

public:

    Bomber();

    void    Render2D        ();
    void    Render3D        ();

    void    Action          ( int targetObjectId, Fixed longitude, Fixed latitude );
    Fixed   GetActionRange  ();
    bool    Update          ();
    void    RunAI           ();
    void    Land            ( int targetId );
    bool    UsingNukes      ();
    bool    ShouldProcessActionQueue();
    void    Retaliate       ( int attackerId );
    bool    CanSetState     ( int state );
    void    SetState        ( int state );

    int     GetAttackOdds       ( int _defenderType );
    int     IsValidCombatTarget ( int _objectId );

    bool    IsActionQueueable       ();
    bool    ShouldProcessNextQueuedAction();
    void    RequestAction      ( ActionOrder *_action );

    void    CeaseFire       ( int teamId );
    bool    SetWaypointOnAction();
};

#endif
