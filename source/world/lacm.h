
#ifndef _included_lacm_h
#define _included_lacm_h

#include "world/movingobject.h"


class LACM : public MovingObject
{
public:
    LACM();

    void    Action              ( int targetObjectId, Fixed longitude, Fixed latitude );
    bool    Update              ();
    void    Render2D            ();
    void    Render3D            ();

    void    SetWaypoint         ( Fixed longitude, Fixed latitude );

    void    LockTarget          ();

    void    SetOrigin           ( int origin ) { m_origin = origin; }

    static void FindTarget      ( int team, int targetTeam, int launchedBy, Fixed range, Fixed *longitude, Fixed *latitude );
    static void FindTarget      ( int team, int targetTeam, int launchedBy, Fixed range, Fixed *longitude, Fixed *latitude, int *objectId );
    static int  CountTargetedLACM( int teamId, Fixed longitude, Fixed latitude );

protected:
    // Anti-building mode (LACM-style): shortest-path globe movement
    Fixed   m_totalDistance;
    Fixed   m_curveDirection;
    Fixed   m_curveLatitude;
    Fixed   m_prevDistanceToTarget;

    Fixed   m_newLongitude;
    Fixed   m_newLatitude;

    bool    m_targetLocked;

    // Anti-ship mode (LASM-style): tracking moving target
    int     m_origin;
    Fixed   m_initspeed;
    Fixed   m_missTime;         // self-destruct when target lost for too long
    bool    m_targetIsShip;     // true when tracking a ship

    bool    MoveToWaypointAntiShip();
    void    CalculateNewPositionAntiShip( Fixed *newLongitude, Fixed *newLatitude, Fixed *newDistance );
};


#endif
