/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007-2025 Manuel Moos
 *  *
 *  */

 #ifndef DEDCON_UNITTYPES_INCLUDED
 #define DEDCON_UNITTYPES_INCLUDED
 
 // Unit type information structure
 struct UnitTypeInfo {
     int typeId;
     const char* name;
 };
 
 // Unit type constant definitions
 #define UNIT_TYPE_SILO         2
 #define UNIT_TYPE_RADAR        3
 #define UNIT_TYPE_SUBMARINE    6
 #define UNIT_TYPE_BATTLESHIP   7
 #define UNIT_TYPE_AIRBASE      8
 #define UNIT_TYPE_CARRIER      11
 
 // Unit state constants
 // Carrier states
 #define CARRIER_STATE_FIGHTER         0
 #define CARRIER_STATE_BOMBER          1
 #define CARRIER_STATE_ANTI_SUBMARINE  2

 // Submarine states
 #define SUBMARINE_STATE_PASSIVE       0
 #define SUBMARINE_STATE_ACTIVE        1
 #define SUBMARINE_STATE_NUKE_LAUNCH   2

 // Silo states
 #define SILO_STATE_ICBM               0
 #define SILO_STATE_AIR_DEFENSE        1

 // Airbase states
 #define AIRBASE_STATE_FIGHTER         0
 #define AIRBASE_STATE_BOMBER          1

 // Array of unit types and their names
 extern const UnitTypeInfo UNIT_TYPES[];
 
 // Get unit name from type ID
 const char* GetUnitTypeName(int typeId);
 
 // Get state description based on unit type and state value
 const char* GetUnitStateDescription(int unitType, int state);
 
 // Debug function to help identify unit destruction commands
 void LogUnitDebugInfo(const char* actionType, int objectId, int teamId, int clientId, const char* additionalInfo = "");
 
 // Store the unit type when we detect a placement
 void StoreUnitType(int objectId, int unitType);
 
 // Get the unit type for an object ID
 int GetStoredUnitType(int objectId);
 
 // Log a unit state change with descriptive information
 void LogUnitStateChange(int objectId, int teamId, int clientId, int state);
 
 // Check for potential unit destruction patterns
 void CheckDestructionPatterns(int objectId, int teamId, int clientId, int state);

 // Log a confirmed unit destruction
 void LogUnitDestruction(int objectId, int teamId, int clientId, const char* reason);

 // Track when a unit is potentially destroyed
 void TrackPotentialDestruction(int objectId, int teamId, int clientId, const char* reason);
 
 // Initialize unit tracking at server start
 void InitializeUnitTracking();

 void ManuallyRegisterUnit(int objectId, int unitType, int teamId, int clientId);
 
 #endif // DEDCON_UNITTYPES_INCLUDED