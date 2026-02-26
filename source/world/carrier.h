
#ifndef _included_carrier_h
#define _included_carrier_h

#include "world/movingobject.h"


class Carrier : public MovingObject
{
protected:
    char bmpFighterMarkerFilename[256];

public:

    Carrier();

    void    SetTeamId           ( int teamId ) override;
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
    bool    CanLaunchFighterLight();
    bool    CanLaunchStealthFighter();
    bool    CanLaunchNavyStealthFighter();
    bool    CanLaunchBomber();
    bool    CanLaunchBomberFast();
    bool    CanLaunchStealthBomber();
    bool    CanLaunchAEW();
    int     GetFighterCount() override;
    int     GetBomberCount() override;
    int     GetAEWCount() override;
    void    OnFighterLanded( int aircraftType ) override;
    void    OnBomberLanded( int aircraftType ) override;
    void    OnAEWLanded() override;
    void    MaybeRemoveRandomStoredAircraft() override;

    int     CountIncomingFighters();

    void    LaunchScout();

    int  IsValidCombatTarget     ( int _objectId );                                      // returns TargetType...
    int  IsValidMovementTarget   ( Fixed longitude, Fixed latitude );                    //
};



#endif
