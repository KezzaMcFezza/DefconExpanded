
#ifndef _included_airbase_h
#define _included_airbase_h

#include "world/worldobject.h"
#include "lib/math/fixed.h"


class AirBase : public WorldObject
{
protected:
    Fixed m_fighterRegenTimer;

public:
    AirBase();

    void SetTeamId( int teamId ) override;
    void Render2D();
    void Render3D();

    void RequestAction( ActionOrder *_order );
    void Action( int targetObjectId, Fixed longitude, Fixed latitude );
    void RunAI();
    bool IsActionQueueable();
    bool UsingNukes();
    void NukeStrike();
    bool Update();
    bool CanLaunchFighter();
    bool CanLaunchFighterLight();
    bool CanLaunchStealthFighter();
    bool CanLaunchBomber();
    bool CanLaunchBomberFast();
    bool CanLaunchStealthBomber();
    bool CanLaunchAEW();
    bool CanLaunchNavyStealthFighter();
    int GetFighterCount() override;
    int GetBomberCount() override;
    int GetAEWCount() override;
    void OnFighterLanded( int aircraftType ) override;
    void OnBomberLanded( int aircraftType ) override;
    void OnAEWLanded() override;
    void MaybeRemoveRandomStoredAircraft() override;
    
    int  GetAttackOdds           ( int _defenderType );

    int  IsValidCombatTarget     ( int _objectId );                                      // returns TargetType...
    int  IsValidMovementTarget   ( Fixed longitude, Fixed latitude );                    //
};



#endif
