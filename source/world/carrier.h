
#ifndef _included_carrier_h
#define _included_carrier_h

#include "world/movingobject.h"


class Carrier : public MovingObject
{
protected:
    char bmpFighterMarkerFilename[256];

public:

    Carrier();

    void    Render2D            ();
    void    Render3D            ();

    void    RequestAction       (ActionOrder *_action);
    void    Action              ( int targetObjectId, Fixed longitude, Fixed latitude );
    bool    Update              ();
    void    RunAI               ();
    bool    IsActionQueueable   ();
    int     FindTarget          ();
    int     GetAttackState      ();

    void    Retaliate           ( int attackerId );
    bool    UsingNukes          ();

    void    FleetAction     ( int targetObjectId );

    int     GetAttackOdds   ( int _defenderType );
    
    bool    CanLaunchFighter();
    bool    CanLaunchBomber();

    int     CountIncomingFighters();

    void    LaunchScout();

    int  IsValidCombatTarget     ( int _objectId );                                      // returns TargetType...
    int  IsValidMovementTarget   ( Fixed longitude, Fixed latitude );                    //
};



#endif
