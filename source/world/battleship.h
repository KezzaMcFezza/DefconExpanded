
#ifndef _included_battleship_h
#define _included_battleship_h

#include "world/movingobject.h"


class BattleShip : public MovingObject
{
protected:
    void    AirDefense      ();

public:

    BattleShip();

    void    RequestAction   ( ActionOrder *_action );
    void    Action          ( int targetObjectId, Fixed longitude, Fixed latitude );
    void    AcquireTargetFromAction( ActionOrder *action ) override;
    bool    Update          ();
    void    Render2D        ();
    void    Render3D        ();
    void    RunAI           ();
    int     GetAttackState  ();
    bool    UsingGuns       ();
    void    FireGun         ( Fixed range );
    int     GetTarget       ( Fixed range, const LList<int> *excludeIds = nullptr );
    int     GetBurstFireShots() const override;
    void    FleetAction     ( int targetObjectId );
    int     GetAttackPriority( int _type );
    int     GetAttackOdds   ( int _defenderType );
    bool    IsActionQueueable();
    int     IsValidCombatTarget( int _objectId );
    int     IsValidMovementTarget( Fixed longitude, Fixed latitude ) override;
    bool    IsPinging       ();
};


#endif
