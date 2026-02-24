
#ifndef _included_nuke_h
#define _included_nuke_h

#include "world/movingobject.h"


class Nuke : public MovingObject
{
protected:
    Fixed   m_curveLatitude;    // 1 or -1: curve toward N or S pole (great circle)
    Fixed   m_prevDistanceToTarget;
    
    Fixed   m_newLongitude;             // Used to slowly turn towards new target
    Fixed   m_newLatitude;

    bool    m_targetLocked;

public:
    Nuke();

    // Impact zone graphic (estimated nuke impact area) - public for MapRenderer
    Fixed   m_curveDirection;   // 1 or -1: east/west direction for curve
    float   m_impactzone_x, m_impactzone_y, m_impactzone_w, m_impactzone_h;
    float   m_cointoss_x, m_cointoss_y;
    Fixed   m_launchposition_x, m_launchposition_y;

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

    /** Called on impact; override in subclasses (e.g. CBM) for different explosion type. */
    virtual void OnImpact();
}; 


#endif
