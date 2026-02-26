#include "lib/universal_include.h"
#include "world/unit_graphics.h"
#include "world/worldobject.h"
#include "world/world.h"

#include <cstring>

namespace
{
    struct TerritoryOverride
    {
        int territory;
        int unitType;
        const char *graphic;
    };

    // Territory-based overrides from MERGE_PLAN.md
    // USA uses generic (no overrides). Format: graphics/name.bmp
    static const TerritoryOverride s_overrides[] =
    {
        // China
        { World::TerritoryChina, WorldObject::TypeFighterLight,      "graphics/j10.bmp" },
        { World::TerritoryChina, WorldObject::TypeFighter,           "graphics/flanker.bmp" },
        { World::TerritoryChina, WorldObject::TypeFighterNavyStealth,"graphics/j35.bmp" },
        { World::TerritoryChina, WorldObject::TypeFighterStealth,    "graphics/j20.bmp" },
        { World::TerritoryChina, WorldObject::TypeBomber,            "graphics/h6.bmp" },
        { World::TerritoryChina, WorldObject::TypeBattleShip,        "graphics/type55.bmp" },
        { World::TerritoryChina, WorldObject::TypeBattleShip2,       "graphics/type52d.bmp" },
        { World::TerritoryChina, WorldObject::TypeBattleShip3,       "graphics/type54a.bmp" },
        { World::TerritoryChina, WorldObject::TypeCarrierSuper,      "graphics/fujian.bmp" },
        { World::TerritoryChina, WorldObject::TypeCarrier,           "graphics/kuznetsov.bmp" },
        { World::TerritoryChina, WorldObject::TypeCarrierLight,      "graphics/kuznetsov.bmp" },
        { World::TerritoryChina, WorldObject::TypeCarrierLHD,        "graphics/type075.bmp" },

        // Russia
        { World::TerritoryRussia, WorldObject::TypeFighterLight,     "graphics/mig29.bmp" },
        { World::TerritoryRussia, WorldObject::TypeFighter,          "graphics/flanker.bmp" },
        { World::TerritoryRussia, WorldObject::TypeFighterStealth,   "graphics/su57.bmp" },
        { World::TerritoryRussia, WorldObject::TypeBomber,           "graphics/tu95.bmp" },
        { World::TerritoryRussia, WorldObject::TypeBomberFast,       "graphics/tu22m.bmp" },
        { World::TerritoryRussia, WorldObject::TypeBattleShip,       "graphics/slava.bmp" },
        { World::TerritoryRussia, WorldObject::TypeBattleShip2,      "graphics/udaloy.bmp" },
        { World::TerritoryRussia, WorldObject::TypeBattleShip3,      "graphics/admiralgrigorovich.bmp" },
        { World::TerritoryRussia, WorldObject::TypeCarrier,          "graphics/kuznetsov.bmp" },
        { World::TerritoryRussia, WorldObject::TypeCarrierLight,     "graphics/kuznetsov.bmp" },

        // NATO
        { World::TerritoryNATO, WorldObject::TypeFighter,            "graphics/rafale.bmp" },
        { World::TerritoryNATO, WorldObject::TypeBattleShip2,        "graphics/horizon.bmp" },
        { World::TerritoryNATO, WorldObject::TypeBattleShip3,        "graphics/fremm.bmp" },
        { World::TerritoryNATO, WorldObject::TypeCarrier,            "graphics/degaulle.bmp" },
        { World::TerritoryNATO, WorldObject::TypeCarrierLight,       "graphics/queen.bmp" },

        // India
        { World::TerritoryIndia, WorldObject::TypeFighter,           "graphics/rafale.bmp" },
        { World::TerritoryIndia, WorldObject::TypeFighterLight,      "graphics/flanker.bmp" },
        { World::TerritoryIndia, WorldObject::TypeCarrier,           "graphics/kuznetsov.bmp" },

        // Pakistan
        { World::TerritoryPakistan, WorldObject::TypeFighterLight,   "graphics/j10.bmp" },

        // Japan (roster uses CarrierLight for Izumo-class)
        { World::TerritoryJapan, WorldObject::TypeCarrier,           "graphics/izumo.bmp" },
        { World::TerritoryJapan, WorldObject::TypeCarrierLight,      "graphics/izumo.bmp" },

        // Ukraine
        { World::TerritoryUkraine, WorldObject::TypeFighterLight,    "graphics/mig29.bmp" },
        { World::TerritoryUkraine, WorldObject::TypeFighter,         "graphics/flanker.bmp" },

        // Indonesia
        { World::TerritoryIndonesia, WorldObject::TypeFighter,       "graphics/flanker.bmp" },

        // North Korea
        { World::TerritoryNorthKorea, WorldObject::TypeFighterLight, "graphics/mig29.bmp" },
        { World::TerritoryNorthKorea, WorldObject::TypeFighter,      "graphics/flanker.bmp" },

        // Iran
        { World::TerritoryIran, WorldObject::TypeFighterLight,       "graphics/mig29.bmp" },
        { World::TerritoryIran, WorldObject::TypeFighter,            "graphics/f14.bmp" },

        // Saudi
        { World::TerritorySaudi, WorldObject::TypeFighter,           "graphics/rafale.bmp" },

        // Egypt
        { World::TerritoryEgypt, WorldObject::TypeFighter,           "graphics/rafale.bmp" },

        // Vietnam
        { World::TerritoryVietnam, WorldObject::TypeFighter,         "graphics/flanker.bmp" },

        // Neutrals (America, Africa, Europe, Asia)
        { World::TerritoryNeutralAmerica, WorldObject::TypeFighter,  "graphics/flanker.bmp" },
        { World::TerritoryNeutralAfrica,  WorldObject::TypeFighter,  "graphics/flanker.bmp" },
        { World::TerritoryNeutralEurope,  WorldObject::TypeFighter,  "graphics/flanker.bmp" },
        { World::TerritoryNeutralAsia,    WorldObject::TypeFighter,  "graphics/flanker.bmp" },
    };

    static const int s_numOverrides = sizeof(s_overrides) / sizeof(s_overrides[0]);
}

const char *GetUnitGraphicForTerritory( int territory, int unitType, const char *defaultPath )
{
    for ( int i = 0; i < s_numOverrides; ++i )
    {
        if ( s_overrides[i].territory == territory && s_overrides[i].unitType == unitType )
            return s_overrides[i].graphic;
    }
    return defaultPath;
}
