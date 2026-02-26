
#ifndef _included_fighter_h
#define _included_fighter_h

#include "world/movingobject.h"


class Fighter : public MovingObject
{  
public:
    bool    m_playerSetWaypoint;
    bool    m_opportunityFireOnly;   // Target acquired via patrol opportunity fire; engage but don't pursue
    bool    m_lacmLoadout;           // true = Strike (2 air attack, 2 LACM), false = CAP (6 air attack, 0 LACM)

public:
    Fighter();

    void    Render2D        ();
    void    Render3D        ();

    int     GetAttackOdds   ( int _defenderType );
    
    void    Action          ( int targetObjectId, Fixed longitude, Fixed latitude );
    void    AcquireTargetFromAction( ActionOrder *action ) override;
    bool    Update          ();

	void	RunAI			();

    int     GetAttackState  ();

    void    Land            ( int targetId );
    bool    UsingGuns       ();
    int     CountTargettedFighters( int targetId );

    int     GetTarget       ( Fixed range, const LList<int> *excludeIds = nullptr ) override;
    int     IsValidCombatTarget( int _objectId );                                      // returns TargetType...
    bool    SetWaypointOnAction();
    bool    IsActionQueueable();
    bool    ShouldProcessActionQueue() override;
    bool    ShouldProcessNextQueuedAction();
    void    RequestAction( ActionOrder *_action ) override;
};


#endif
