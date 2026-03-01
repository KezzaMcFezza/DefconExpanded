
#ifndef _included_tanker_h
#define _included_tanker_h

#include "world/movingobject.h"


class Tanker : public MovingObject
{
protected:
    int     m_refuelSlot[2];        // Object IDs occupying small slots (-1 = empty)
    Fixed   m_refuelRange;          // Action range for refueling

public:
    Tanker();

    void    Action  ( int targetObjectId, Fixed longitude, Fixed latitude );
    bool    Update  ();
    void    RunAI   ();
    void    Render2D();
    void    Render3D();

    void    Land            ( int targetId );

    int     GetAttackState  ();
    int     IsValidCombatTarget( int _objectId );

    bool    IsSlotAvailable ( bool largeSlot ) const;
    void    AssignSlot      ( int objectId, bool largeSlot );
    void    ManualAssignSlot( int objectId, bool largeSlot );
    void    ClearSlot       ( int objectId );
    void    RefuelTargets   ( Fixed timeStep );
    int     FindBestRefuelTarget();
    bool    IsRefuelingTarget( int objectId ) const;
    void    RefuelAssign    ( int targetObjectId );
    Fixed   GetRefuelRange  () const;
};


#endif
