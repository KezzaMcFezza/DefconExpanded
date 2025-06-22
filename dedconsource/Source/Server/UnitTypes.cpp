/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007-2025 Manuel Moos
 *  *
 *  */

 #include "UnitTypes.h"
 #include "Main/Log.h"
 #include <sstream>
 #include <string>
 #include <map>
 #include <ctime>
 #include <vector>
 
 // Debug flag to print detailed unit tracking information
 static bool debugUnitTracking = true;
 
 // Define unit types array with their names
 const UnitTypeInfo UNIT_TYPES[] = {
     {UNIT_TYPE_SILO, "silo"},
     {UNIT_TYPE_RADAR, "radar"},
     {UNIT_TYPE_SUBMARINE, "submarine"},
     {UNIT_TYPE_BATTLESHIP, "battleship"},
     {UNIT_TYPE_AIRBASE, "airbase"},
     {UNIT_TYPE_CARRIER, "carrier"},
     {-1, "unknown"} // Sentinel value
 };
 
 // Current state tracking - maps object IDs to their last known unit type
 std::map<int, int> unitTypeMap;
 
 // Map to track all unit objects by their ID and the message type they were observed in
 struct UnitTrackingInfo {
     int objectId;
     int unitType;
     int teamId;
     int clientId;
     std::string lastMessageType;
     time_t lastSeen;
 };
 
 std::map<int, UnitTrackingInfo> allUnits;
 
 // Get unit name from type ID
 const char* GetUnitTypeName(int typeId) {
     for (int i = 0; UNIT_TYPES[i].typeId != -1; i++) {
         if (UNIT_TYPES[i].typeId == typeId) {
             return UNIT_TYPES[i].name;
         }
     }
     return "unknown";
 }
 
 // Get descriptive state name based on unit type and state value
 const char* GetUnitStateDescription(int unitType, int state) {
     // Based on the unit type, interpret the state value
     switch(unitType) {
         case UNIT_TYPE_CARRIER:
             switch(state) {
                 case CARRIER_STATE_FIGHTER: return "fighter launch mode";
                 case CARRIER_STATE_BOMBER: return "bomber launch mode";
                 case CARRIER_STATE_ANTI_SUBMARINE: return "anti-submarine mode";
                 default: return "unknown carrier state";
             }
         
         case UNIT_TYPE_SUBMARINE:
             switch(state) {
                 case SUBMARINE_STATE_PASSIVE: return "passive sonar mode";
                 case SUBMARINE_STATE_ACTIVE: return "active sonar mode";
                 case SUBMARINE_STATE_NUKE_LAUNCH: return "nuke launch mode (surfaced)";
                 default: return "unknown submarine state";
             }
             
         case UNIT_TYPE_SILO:
             switch(state) {
                 case SILO_STATE_ICBM: return "ICBM launch mode";
                 case SILO_STATE_AIR_DEFENSE: return "air defense mode";
                 default: return "unknown silo state";
             }
             
         case UNIT_TYPE_AIRBASE:
             switch(state) {
                 case AIRBASE_STATE_FIGHTER: return "fighter launch mode";
                 case AIRBASE_STATE_BOMBER: return "bomber launch mode";
                 default: return "unknown airbase state";
             }
             
         default:
             return "unknown state";
     }
 }
 
 // Log debug information about unit actions
 void LogUnitDebugInfo(const char* actionType, int objectId, int teamId, int clientId, const char* additionalInfo) {
     if (Log::GetLevel() >= 3) {
         Log::Out() << "UNIT DEBUG: " << actionType 
                   << " ObjectID=" << objectId
                   << " Team=" << teamId
                   << " Client=" << clientId
                   << " " << additionalInfo << "\n";
     }
 }
 
 // Function to manually register unit type for an object ID (used when we don't catch placement)
 void ManuallyRegisterUnit(int objectId, int unitType, int teamId, int clientId) {
     if (unitTypeMap.find(objectId) != unitTypeMap.end()) {
         return; // Already registered
     }
     
     unitTypeMap[objectId] = unitType;
     
     // Also update the all units tracking
     UnitTrackingInfo info;
     info.objectId = objectId;
     info.unitType = unitType;
     info.teamId = teamId;
     info.clientId = clientId;
     info.lastMessageType = "manual_registration";
     info.lastSeen = time(nullptr);
     allUnits[objectId] = info;
     
     if (debugUnitTracking) {
         Log::Out() << "MANUAL REGISTRATION: ObjectID=" << objectId 
                   << " as " << GetUnitTypeName(unitType) 
                   << " for Team=" << teamId 
                   << ", Client=" << clientId << "\n";
     }
 }
 
 // Store the unit type when we detect a placement
 void StoreUnitType(int objectId, int unitType) {
     // Track in the main unit type map
     unitTypeMap[objectId] = unitType;
     
     // Update allUnits tracking as well
     if (allUnits.find(objectId) != allUnits.end()) {
         allUnits[objectId].unitType = unitType;
         allUnits[objectId].lastSeen = time(nullptr);
         allUnits[objectId].lastMessageType = "unit_type_update";
     } else {
         UnitTrackingInfo info;
         info.objectId = objectId;
         info.unitType = unitType;
         info.teamId = -1;  // Unknown at this point
         info.clientId = -1; // Unknown at this point
         info.lastMessageType = "new_unit_type";
         info.lastSeen = time(nullptr);
         allUnits[objectId] = info;
     }
     
     if (debugUnitTracking) {
         Log::Out() << "UNIT TRACKING: Stored objectID=" << objectId 
                   << " as unit type=" << unitType 
                   << " (" << GetUnitTypeName(unitType) << ")\n";
     }
 }
 
 // Get the unit type for an object ID
 int GetStoredUnitType(int objectId) {
     auto it = unitTypeMap.find(objectId);
     if (it != unitTypeMap.end()) {
         return it->second;
     }
     return -1;  // Unknown type
 }
 
 // Structure to track unit lifetime events
 struct UnitLifetimeEvent {
     int objectId;
     int teamId;
     int clientId;
     int unitType;
     time_t timestamp;
     std::string eventType;  // "created", "state_change", "moved", "destroyed"
     std::string details;
 };
 
 // Map to track unit lifetime events for analysis
 std::map<int, std::vector<UnitLifetimeEvent>> unitLifetimeEvents;
 
 // Track when a unit is potentially destroyed
 void TrackPotentialDestruction(int objectId, int teamId, int clientId, const char* reason) {
     int unitType = GetStoredUnitType(objectId);
     
     // Format timestamp for logging
     time_t now = time(nullptr);
     
     // Create lifetime event
     UnitLifetimeEvent event;
     event.objectId = objectId;
     event.teamId = teamId;
     event.clientId = clientId;
     event.unitType = unitType;
     event.timestamp = now;
     event.eventType = "potential_destruction";
     event.details = reason;
     
     // Add to unit's lifetime events
     unitLifetimeEvents[objectId].push_back(event);
     
     // Log for detection
     if (Log::GetLevel() >= 2) {
         struct tm *timeinfo = localtime(&now);
         char timestamp[20];
         strftime(timestamp, sizeof(timestamp), "%H:%M:%S", timeinfo);
         
         Log::Out() << "POTENTIAL DESTRUCTION: " << timestamp
                   << " ObjectID=" << objectId
                   << " Team=" << teamId
                   << " Client=" << clientId
                   << " UnitType=" << GetUnitTypeName(unitType)
                   << " Reason=" << reason << "\n";
     }
 }
 
 // Check for potential destruction patterns
 void CheckDestructionPatterns(int objectId, int teamId, int clientId, int state) {
     int unitType = GetStoredUnitType(objectId);
     if (unitType < 0) return;
     
     // Looking for patterns that might indicate destruction
     const auto& events = unitLifetimeEvents[objectId];
     
     // This is where we would add pattern detection logic based on observations
     
     // For now, let's track some suspicious state changes
     
     // Potential pattern: a state change without any previous events could indicate a destruction
     if (events.empty()) {
         TrackPotentialDestruction(objectId, teamId, clientId, "Unexpected state change with no previous events");
     }
     
     // Other patterns to explore
     switch (unitType) {
         case UNIT_TYPE_SUBMARINE:
             // Maybe a submarine going to nuke launch mode and then disappearing indicates destruction?
             if (state == SUBMARINE_STATE_NUKE_LAUNCH) {
                 TrackPotentialDestruction(objectId, teamId, clientId, "Submarine entered nuke launch mode");
             }
             break;
             
         case UNIT_TYPE_CARRIER:
         case UNIT_TYPE_BATTLESHIP:
             // Naval units might have specific destruction patterns to watch for
             break;
             
         case UNIT_TYPE_SILO:
         case UNIT_TYPE_RADAR:
             // Fixed installations might have different destruction patterns
             break;
     }
 }
 
 // Log a confirmed unit destruction
 void LogUnitDestruction(int objectId, int teamId, int clientId, const char* reason) {
     int unitType = GetStoredUnitType(objectId);
     
     // Format timestamp for logging
     time_t now = time(nullptr);
     struct tm *timeinfo = localtime(&now);
     char timestamp[20];
     strftime(timestamp, sizeof(timestamp), "%H:%M:%S", timeinfo);
     
     // Write to event log
     Log::Event() << "UNIT_DESTROYED "
                 << timestamp << " "
                 << clientId << " "
                 << teamId << " "
                 << objectId << " "
                 << GetUnitTypeName(unitType) << " "
                 << reason << "\n";
     
     // Also output to console at appropriate log level
     if (Log::GetLevel() >= 1) {
         Log::Out() << "Unit destroyed: Client " << clientId
                   << " (Team " << teamId
                   << ") lost " << GetUnitTypeName(unitType)
                   << " (ID " << objectId
                   << ") - " << reason << "\n";
     }
     
     // Create lifetime event
     UnitLifetimeEvent event;
     event.objectId = objectId;
     event.teamId = teamId;
     event.clientId = clientId;
     event.unitType = unitType;
     event.timestamp = now;
     event.eventType = "destroyed";
     event.details = reason;
     
     // Add to unit's lifetime events
     unitLifetimeEvents[objectId].push_back(event);
 }
 
 // Log a unit state change with descriptive information
 void LogUnitStateChange(int objectId, int teamId, int clientId, int state) {
     int unitType = GetStoredUnitType(objectId);
     
     // If we don't know the unit type, try to guess based on state patterns
     if (unitType < 0) {
         if (state == 0 || state == 1) {
             // Could be silo, carrier, submarine, or airbase
             if (objectId >= 10) {
                 // Higher object IDs are more likely to be silos
                 unitType = UNIT_TYPE_SILO;
                 ManuallyRegisterUnit(objectId, unitType, teamId, clientId);
             } else if (objectId >= 6 && objectId <= 9) {
                 // Middle range object IDs are more likely to be carriers
                 unitType = UNIT_TYPE_CARRIER;
                 ManuallyRegisterUnit(objectId, unitType, teamId, clientId);
             }
         } else if (state == 2) {
             // State 2 is used by carriers and submarines
             unitType = UNIT_TYPE_CARRIER; // Guess carrier
             ManuallyRegisterUnit(objectId, unitType, teamId, clientId);
         }
     }
     
     // Format timestamp for logging
     time_t now = time(nullptr);
     struct tm *timeinfo = localtime(&now);
     char timestamp[20];
     strftime(timestamp, sizeof(timestamp), "%H:%M:%S", timeinfo);
     
     // Get unit type name and state description
     std::string unitTypeName = GetUnitTypeName(unitType);
     std::string stateDesc = GetUnitStateDescription(unitType, state);
     
     // Update tracking info
     if (allUnits.find(objectId) != allUnits.end()) {
         allUnits[objectId].teamId = teamId;
         allUnits[objectId].clientId = clientId;
         allUnits[objectId].lastSeen = now;
         allUnits[objectId].lastMessageType = "state_change";
     } else {
         UnitTrackingInfo info;
         info.objectId = objectId;
         info.unitType = unitType;
         info.teamId = teamId;
         info.clientId = clientId;
         info.lastMessageType = "state_change";
         info.lastSeen = now;
         allUnits[objectId] = info;
     }
     
     // Create lifetime event
     UnitLifetimeEvent event;
     event.objectId = objectId;
     event.teamId = teamId;
     event.clientId = clientId;
     event.unitType = unitType;
     event.timestamp = now;
     event.eventType = "state_change";
     event.details = stateDesc;
     
     // Add to unit's lifetime events
     unitLifetimeEvents[objectId].push_back(event);
     
     // Write to event log
     Log::Event() << "UNIT_STATE_CHANGE "
                 << timestamp << " "
                 << clientId << " "
                 << teamId << " "
                 << objectId << " "
                 << unitTypeName << " "
                 << state << " "
                 << stateDesc << "\n";
     
     // Also output to console at appropriate log level
     if (Log::GetLevel() >= 1) {
         Log::Out() << "Unit state change: Client " << clientId
                   << " (Team " << teamId
                   << ") changed " << unitTypeName
                   << " (ID " << objectId
                   << ") to " << stateDesc << "\n";
     }
 }
 
 // Add a function to display all tracked units (for debugging)
 void PrintAllTrackedUnits() {
     Log::Out() << "=== TRACKED UNITS (" << allUnits.size() << " total) ===\n";
     
     for (const auto& pair : allUnits) {
         const UnitTrackingInfo& info = pair.second;
         struct tm *timeinfo = localtime(&info.lastSeen);
         char timestamp[20];
         strftime(timestamp, sizeof(timestamp), "%H:%M:%S", timeinfo);
         
         Log::Out() << "ObjectID=" << info.objectId
                   << " Type=" << GetUnitTypeName(info.unitType)
                   << " Team=" << info.teamId
                   << " Client=" << info.clientId
                   << " LastSeen=" << timestamp
                   << " LastMsg=" << info.lastMessageType
                   << "\n";
     }
     
     Log::Out() << "==============================\n";
 }
 
 // Initialize unit tracking at server start
 void InitializeUnitTracking() {
     unitTypeMap.clear();
     unitLifetimeEvents.clear();
     allUnits.clear();
     
     if (Log::GetLevel() >= 1) {
         Log::Out() << "Unit tracking system initialized\n";
     }
 }