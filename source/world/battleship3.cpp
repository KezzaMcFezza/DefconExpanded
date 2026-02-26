#include "lib/universal_include.h"
#include "world/battleship3.h"
#include "world/battleship.h"

BattleShip3::BattleShip3()
:   BattleShip()
{
    SetType( WorldObject::TypeBattleShip3 );
    strcpy( bmpImageFilename, "graphics/lcs.bmp" );
}
