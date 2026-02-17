
#ifndef _included_nuke_h
#define _included_nuke_h

#include "world/movingobject.h"


class Nuke : public MovingObject
{
protected:
    Fixed   m_curveDirection;
    Fixed   m_prevDistanceToTarget;
    
    Fixed   m_newLongitude;             // Used to slowly turn towards new target
    Fixed   m_newLatitude;

    bool    m_targetLocked;

public:
    Nuke();

    // moved total distance to public section to be used by the 3d trail rendering
    
    Fixed   m_totalDistance;  
    Fixed   GetTotalDistance() const { return m_totalDistance; } // 

    void    Action          ( int targetObjectId, Fixed longitude, Fixed latitude );
    bool    Update          ();
    void    Render2D        ();
    void    Render3D        ();

    void    SetWaypoint     ( Fixed longitude, Fixed latitude );
    
    static void FindTarget  ( int team, int targetTeam, int launchedBy, Fixed range, Fixed *longitude, Fixed *latitude );
    static void FindTarget  ( int team, int targetTeam, int launchedBy, Fixed range, Fixed *longitude, Fixed *latitude, int *objectId );
    static int CountTargetedNukes( int teamId, Fixed longitude, Fixed latitude );

    void    CeaseFire       ( int teamId );
    int     IsValidMovementTarget( Fixed longitude, Fixed latitude );

    void    LockTarget();
}; 


#endif
