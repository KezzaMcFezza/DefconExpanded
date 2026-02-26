#ifndef _included_territory_roster_h
#define _included_territory_roster_h

#include "world/world.h"
#include "world/worldobject.h"

// Territory roster: which building and navy units each territory can place.
// Placement window uses team->m_territories[0] (first territory) for roster.
// Unit counts are multiplied by territoriesPerTeam (unchanged).

struct TerritoryRosterSlot
{
    int unitType;   // WorldObject::Type* or -1 for empty
};

// Get building roster for a territory. Fills slots[row][col] = unitType.
// Max 6 rows, 2 cols. Returns number of rows used.
int GetTerritoryBuildingRoster( int territoryId, TerritoryRosterSlot slots[6][2] );

// Get navy roster for a territory. Fills slots[row][col] = unitType.
// Max 6 rows, 2 cols. Returns number of rows used.
int GetTerritoryNavyRoster( int territoryId, TerritoryRosterSlot slots[6][2] );

// True if this unit type is in the territory's building roster
bool TerritoryHasBuildingType( int territoryId, int unitType );

// True if this unit type is in the territory's navy roster
bool TerritoryHasNavyType( int territoryId, int unitType );

#endif
