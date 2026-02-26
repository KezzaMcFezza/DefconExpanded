#include "lib/universal_include.h"
#include "world/territory_roster.h"
#include "world/world.h"

static void ClearSlots( TerritoryRosterSlot slots[6][2] )
{
    for( int r = 0; r < 6; ++r )
        for( int c = 0; c < 2; ++c )
            slots[r][c].unitType = -1;
}

int GetTerritoryBuildingRoster( int territoryId, TerritoryRosterSlot slots[6][2] )
{
    ClearSlots( slots );
    int rowsUsed = 0;

    switch( territoryId )
    {
        case World::TerritoryUSA:
            slots[0][0].unitType = WorldObject::TypeAirBase;    slots[0][1].unitType = WorldObject::TypeSilo;
            slots[1][0].unitType = WorldObject::TypeAirBase2;   slots[1][1].unitType = WorldObject::TypeASCM;
            slots[2][0].unitType = WorldObject::TypeAirBase3;   slots[2][1].unitType = -1;
            slots[3][0].unitType = WorldObject::TypeRadarEW;    slots[3][1].unitType = WorldObject::TypeSAM;
            slots[4][0].unitType = WorldObject::TypeRadarStation; slots[4][1].unitType = WorldObject::TypeABM;
            rowsUsed = 5; break;

        case World::TerritoryNATO:
            slots[0][0].unitType = WorldObject::TypeAirBase;    slots[0][1].unitType = -1;
            slots[1][0].unitType = WorldObject::TypeAirBase2;   slots[1][1].unitType = WorldObject::TypeASCM;
            slots[2][0].unitType = WorldObject::TypeRadarEW;    slots[2][1].unitType = WorldObject::TypeSAM;
            slots[3][0].unitType = WorldObject::TypeRadarStation; slots[3][1].unitType = WorldObject::TypeABM;
            rowsUsed = 4; break;

        case World::TerritoryRussia:
        case World::TerritoryChina:
            slots[0][0].unitType = WorldObject::TypeAirBase;    slots[0][1].unitType = WorldObject::TypeAirBase2;
            slots[1][0].unitType = WorldObject::TypeSilo;       slots[1][1].unitType = WorldObject::TypeSiloMed;
            slots[2][0].unitType = WorldObject::TypeSiloMobile; slots[2][1].unitType = WorldObject::TypeSiloMobileCon;
            slots[3][0].unitType = WorldObject::TypeRadarEW;    slots[3][1].unitType = WorldObject::TypeRadarStation;
            slots[4][0].unitType = WorldObject::TypeSAM;        slots[4][1].unitType = WorldObject::TypeABM;
            slots[5][0].unitType = WorldObject::TypeASCM;       slots[5][1].unitType = -1;
            rowsUsed = 6; break;

        case World::TerritoryIndia:
            slots[0][0].unitType = WorldObject::TypeAirBase;    slots[0][1].unitType = -1;
            slots[1][0].unitType = WorldObject::TypeSiloMed;    slots[1][1].unitType = -1;
            slots[2][0].unitType = WorldObject::TypeSiloMobile; slots[2][1].unitType = WorldObject::TypeSiloMobileCon;
            slots[3][0].unitType = WorldObject::TypeSAM;        slots[3][1].unitType = WorldObject::TypeASCM;
            slots[4][0].unitType = WorldObject::TypeRadarStation; slots[4][1].unitType = -1;
            rowsUsed = 5; break;

        case World::TerritoryPakistan:
            slots[0][0].unitType = WorldObject::TypeAirBase;    slots[0][1].unitType = -1;
            slots[1][0].unitType = WorldObject::TypeSiloMed;    slots[1][1].unitType = -1;
            slots[2][0].unitType = WorldObject::TypeSiloMobileCon; slots[2][1].unitType = -1;
            slots[3][0].unitType = WorldObject::TypeASCM;       slots[3][1].unitType = -1;
            slots[4][0].unitType = WorldObject::TypeSAM;        slots[4][1].unitType = -1;
            slots[5][0].unitType = WorldObject::TypeRadarStation; slots[5][1].unitType = -1;
            rowsUsed = 6; break;

        case World::TerritoryJapan:
            slots[0][0].unitType = WorldObject::TypeAirBase;    slots[0][1].unitType = -1;
            slots[1][0].unitType = WorldObject::TypeABM;        slots[1][1].unitType = -1;
            slots[2][0].unitType = WorldObject::TypeSAM;        slots[2][1].unitType = -1;
            slots[3][0].unitType = WorldObject::TypeASCM;       slots[3][1].unitType = -1;
            slots[4][0].unitType = WorldObject::TypeRadarEW;    slots[4][1].unitType = -1;
            slots[5][0].unitType = WorldObject::TypeRadarStation; slots[5][1].unitType = -1;
            rowsUsed = 6; break;

        case World::TerritoryKorea:
            slots[0][0].unitType = WorldObject::TypeAirBase;    slots[0][1].unitType = -1;
            slots[1][0].unitType = WorldObject::TypeSiloMobileCon; slots[1][1].unitType = -1;
            slots[2][0].unitType = WorldObject::TypeASCM;       slots[2][1].unitType = -1;
            slots[3][0].unitType = WorldObject::TypeSAM;        slots[3][1].unitType = -1;
            slots[4][0].unitType = WorldObject::TypeABM;        slots[4][1].unitType = -1;
            slots[5][0].unitType = WorldObject::TypeRadarEW;    slots[5][1].unitType = WorldObject::TypeRadarStation;
            rowsUsed = 6; break;

        case World::TerritoryAustralia:
            slots[0][0].unitType = WorldObject::TypeAirBase;    slots[0][1].unitType = -1;
            slots[1][0].unitType = WorldObject::TypeASCM;       slots[1][1].unitType = -1;
            slots[2][0].unitType = WorldObject::TypeSAM;        slots[2][1].unitType = -1;
            slots[3][0].unitType = WorldObject::TypeABM;        slots[3][1].unitType = -1;
            slots[4][0].unitType = WorldObject::TypeRadarEW;    slots[4][1].unitType = -1;
            slots[5][0].unitType = WorldObject::TypeRadarStation; slots[5][1].unitType = -1;
            rowsUsed = 6; break;

        default:
            // Fallback: use USA-style roster for unfinished/neutral territories
            slots[0][0].unitType = WorldObject::TypeAirBase;    slots[0][1].unitType = WorldObject::TypeSilo;
            slots[1][0].unitType = WorldObject::TypeAirBase2;   slots[1][1].unitType = WorldObject::TypeASCM;
            slots[2][0].unitType = WorldObject::TypeRadarEW;    slots[2][1].unitType = WorldObject::TypeSAM;
            slots[3][0].unitType = WorldObject::TypeRadarStation; slots[3][1].unitType = WorldObject::TypeABM;
            rowsUsed = 4; break;
    }
    return rowsUsed;
}

int GetTerritoryNavyRoster( int territoryId, TerritoryRosterSlot slots[6][2] )
{
    ClearSlots( slots );
    int rowsUsed = 0;

    switch( territoryId )
    {
        case World::TerritoryUSA:
            slots[0][0].unitType = WorldObject::TypeCarrierSuper; slots[0][1].unitType = -1;
            slots[1][0].unitType = WorldObject::TypeCarrierLight; slots[1][1].unitType = -1;
            slots[2][0].unitType = WorldObject::TypeBattleShip; slots[2][1].unitType = WorldObject::TypeSub;
            slots[3][0].unitType = WorldObject::TypeBattleShip2; slots[3][1].unitType = WorldObject::TypeSubG;
            slots[4][0].unitType = WorldObject::TypeBattleShip3; slots[4][1].unitType = WorldObject::TypeSubC;
            rowsUsed = 5; break;

        case World::TerritoryNATO:
            slots[0][0].unitType = WorldObject::TypeCarrier;    slots[0][1].unitType = -1;
            slots[1][0].unitType = WorldObject::TypeCarrierLight;   slots[1][1].unitType = -1;
            slots[2][0].unitType = -1;                          slots[2][1].unitType = WorldObject::TypeSub;
            slots[3][0].unitType = WorldObject::TypeBattleShip2; slots[3][1].unitType = WorldObject::TypeSubC;
            slots[4][0].unitType = WorldObject::TypeBattleShip3; slots[4][1].unitType = WorldObject::TypeSubK;
            rowsUsed = 5; break;

        case World::TerritoryRussia:
            slots[0][0].unitType = WorldObject::TypeCarrier;    slots[0][1].unitType = WorldObject::TypeSub;
            slots[1][0].unitType = WorldObject::TypeBattleShip; slots[1][1].unitType = WorldObject::TypeSubG;
            slots[2][0].unitType = WorldObject::TypeBattleShip2; slots[2][1].unitType = WorldObject::TypeSubC;
            slots[3][0].unitType = WorldObject::TypeBattleShip3; slots[3][1].unitType = WorldObject::TypeSubK;
            rowsUsed = 4; break;

        case World::TerritoryChina:
            slots[0][0].unitType = WorldObject::TypeCarrierSuper; slots[0][1].unitType = -1;
            slots[1][0].unitType = WorldObject::TypeCarrier;     slots[1][1].unitType = WorldObject::TypeCarrierLight;
            slots[2][0].unitType = WorldObject::TypeBattleShip;  slots[2][1].unitType = WorldObject::TypeSub;
            slots[3][0].unitType = WorldObject::TypeBattleShip2; slots[3][1].unitType = WorldObject::TypeSubG;
            slots[4][0].unitType = WorldObject::TypeBattleShip3; slots[4][1].unitType = WorldObject::TypeSubC;
            slots[5][0].unitType = WorldObject::TypeCarrierLHD;  slots[5][1].unitType = WorldObject::TypeSubK;
            rowsUsed = 6; break;

        case World::TerritoryIndia:
            slots[0][0].unitType = WorldObject::TypeCarrier;    slots[0][1].unitType = -1;
            slots[1][0].unitType = WorldObject::TypeBattleShip2; slots[1][1].unitType = -1;
            slots[2][0].unitType = WorldObject::TypeBattleShip3; slots[2][1].unitType = -1;
            slots[3][0].unitType = WorldObject::TypeSub;        slots[3][1].unitType = -1;
            slots[4][0].unitType = WorldObject::TypeSubC;       slots[4][1].unitType = -1;
            slots[5][0].unitType = WorldObject::TypeSubK;       slots[5][1].unitType = -1;
            rowsUsed = 6; break;

        case World::TerritoryPakistan:
            slots[0][0].unitType = WorldObject::TypeBattleShip2; slots[0][1].unitType = -1;
            slots[1][0].unitType = WorldObject::TypeBattleShip3; slots[1][1].unitType = -1;
            slots[2][0].unitType = WorldObject::TypeSubK;       slots[2][1].unitType = -1;
            rowsUsed = 3; break;

        case World::TerritoryJapan:
        case World::TerritoryKorea:
            slots[0][0].unitType = WorldObject::TypeCarrierLight;   slots[0][1].unitType = -1;
            slots[1][0].unitType = WorldObject::TypeBattleShip2; slots[1][1].unitType = -1;
            slots[2][0].unitType = WorldObject::TypeBattleShip3; slots[2][1].unitType = -1;
            slots[3][0].unitType = WorldObject::TypeSubK;       slots[3][1].unitType = -1;
            rowsUsed = 4; break;

        case World::TerritoryAustralia:
            slots[0][0].unitType = WorldObject::TypeCarrierLight;   slots[0][1].unitType = -1;
            slots[1][0].unitType = WorldObject::TypeBattleShip2; slots[1][1].unitType = -1;
            slots[2][0].unitType = WorldObject::TypeBattleShip3; slots[2][1].unitType = -1;
            slots[3][0].unitType = WorldObject::TypeSubC;       slots[3][1].unitType = -1;
            slots[4][0].unitType = WorldObject::TypeSubK;       slots[4][1].unitType = -1;
            rowsUsed = 5; break;

        default:
            slots[0][0].unitType = WorldObject::TypeCarrier;    slots[0][1].unitType = WorldObject::TypeSub;
            slots[1][0].unitType = WorldObject::TypeBattleShip; slots[1][1].unitType = WorldObject::TypeSubG;
            slots[2][0].unitType = WorldObject::TypeBattleShip2; slots[2][1].unitType = WorldObject::TypeSubC;
            slots[3][0].unitType = WorldObject::TypeBattleShip3; slots[3][1].unitType = WorldObject::TypeSubK;
            rowsUsed = 4; break;
    }
    return rowsUsed;
}

bool TerritoryHasBuildingType( int territoryId, int unitType )
{
    TerritoryRosterSlot slots[6][2];
    GetTerritoryBuildingRoster( territoryId, slots );
    for( int r = 0; r < 6; ++r )
        for( int c = 0; c < 2; ++c )
            if( slots[r][c].unitType == unitType ) return true;
    return false;
}

bool TerritoryHasNavyType( int territoryId, int unitType )
{
    TerritoryRosterSlot slots[6][2];
    GetTerritoryNavyRoster( territoryId, slots );
    for( int r = 0; r < 6; ++r )
        for( int c = 0; c < 2; ++c )
            if( slots[r][c].unitType == unitType ) return true;
    return false;
}
