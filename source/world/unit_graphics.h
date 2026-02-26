#ifndef _included_unit_graphics_h
#define _included_unit_graphics_h

// Returns the graphics path for a unit type given the team's primary territory.
// Uses MERGE_PLAN territory-based unit graphics; falls back to defaultPath when no override.
// defaultPath: e.g. from WorldRenderer m_imageFiles or object bmpImageFilename.
const char *GetUnitGraphicForTerritory( int territory, int unitType, const char *defaultPath );

#endif
