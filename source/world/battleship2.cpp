#include "lib/universal_include.h"
#include "world/battleship2.h"
#include "world/battleship.h"

BattleShip2::BattleShip2()
:   BattleShip()
{
    SetType( WorldObject::TypeBattleShip2 );
    strcpy( bmpImageFilename, "graphics/battleship.bmp" );
}
