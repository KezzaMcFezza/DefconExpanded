
#ifndef _included_fighter_h
#define _included_fighter_h

#include "world/movingobject.h"


class Fighter : public MovingObject
{  
public:
    bool    m_playerSetWaypoint;
    bool    m_opportunityFireOnly;   // Target acquired via patrol opportunity fire; engage but don't pursue

public:
    Fighter();

    void    Render2D        ();
    void    Render3D        ();
    
    void    Action          ( int targetObjectId, Fixed longitude, Fixed latitude );
    void    AcquireTargetFromAction( ActionOrder *action ) override;
    bool    Update          ();

	void	RunAI			();

    int     GetAttackState  ();

    void    Land            ( int targetId );
    bool    UsingGuns       ();
    int     CountTargettedFighters( int targetId );

    int     IsValidCombatTarget( int _objectId );                                      // returns TargetType...
    bool    SetWaypointOnAction();
};


#endif
