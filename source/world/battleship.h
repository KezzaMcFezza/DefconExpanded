
#ifndef _included_battleship_h
#define _included_battleship_h

#include "world/movingobject.h"


class BattleShip : public MovingObject
{

public:    

    BattleShip();

    void    Render2D        ();
    void    Render3D        ();

    void    Action          ( int targetObjectId, Fixed longitude, Fixed latitude ); 
    bool    Update          (); 
    void    RunAI           ();
    int     GetAttackState();
    bool    UsingGuns       ();
    int     GetTarget       ( Fixed range );
    void    FleetAction     ( int targetObjectId );
    int     GetAttackPriority( int _type );
};


#endif
