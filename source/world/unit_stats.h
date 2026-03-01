#ifndef _included_unit_stats_h
#define _included_unit_stats_h

class WorldObject;

// Applies territory-specific stat overrides (life, speed, turn rate, stealth,
// and per-state prepare/reload/action-range) based on the team's primary territory.
// Called from SetTeamId after construction is complete.
void ApplyTerritoryStatOverrides( WorldObject *unit, int territory );

#endif
