#ifndef _included_aircraft_loads_h
#define _included_aircraft_loads_h

class WorldObject;

// Applies territory-specific aircraft loads to airbases and carriers.
// Call when unit gets a team (primary territory = team->m_territories[0]).
void ApplyTerritoryAircraftLoads( WorldObject *obj, int primaryTerritory );

#endif
