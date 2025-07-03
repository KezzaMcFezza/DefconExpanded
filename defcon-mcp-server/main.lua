package.path = debug.getinfo(1, "S").source:match[[^@?(.*[\/])[^\/]-$]] .."?.lua;".. package.path

local infile
local outfile
local offset = 0
local getStateNext = true
local eventCounter = 0
local events = {}

-- Required by luabot binding.
function OnInit()
  SendChat("/name [BOT]MCP")
  SendChat("Hello! I am a bot controlled through Model Context Protocol!")
  local inputfilename = "R://input.txt"
  local outputfilename = "R://output.txt"
  if GetCommandLineArguments("inputfile") ~= nil then
    inputfilename = GetCommandLineArguments("inputfile")
  end
  if GetCommandLineArguments("outputfile") ~= nil then
    outputfilename = GetCommandLineArguments("outputfile")
  end
  infile = io.open(inputfilename, "w")
  infile:close()
  infile = io.open(inputfilename, "r")
  if not infile then
    SendChat("Could not open input file: " .. inputfilename)
  else 
    SendChat("Opened input file: " .. inputfilename)
  end
  outfile = io.open(outputfilename, "w")
  if not infile then
    SendChat("Could not open output file: " .. outputfilename)
  else 
    SendChat("Opened output file: " .. outputfilename)
  end
  main = coroutine.create(MTLTest)
  live = true
end

-- Required. 100ms execution time limit. Use it wisely.
function OnTick()
  if live ~= false then
    live, err = coroutine.resume(main)
    if live == false then DebugLog(err) end
  end
end

function OnEvent (eventtype, sourceID, targetID, unittype, longitude, latitude)
  DebugLog("Event: " .. eventtype)
  
  -- Get actual unit types for source and target
  local sourceType = "Unknown"
  local targetType = "Unknown"
  
  -- Try to get source type
  if sourceID ~= nil then
    local sourceTypeCheck = GetUnitType(sourceID)
    if sourceTypeCheck then
      sourceType = sourceTypeCheck
    end
  end
  
  -- Try to get target type
  if targetID ~= nil then
    local targetTypeCheck = GetUnitType(targetID)
    if targetTypeCheck then
      targetType = targetTypeCheck
    end
  end
  
  -- Increment event counter and store event
  eventCounter = eventCounter + 1
  local event = {
    id = eventCounter,
    type = eventtype,
    source = tostring(sourceID),
    sourceType = sourceType,
    target = tostring(targetID),
    targetType = targetType,
    longitude = tostring(longitude),
    latitude = tostring(latitude),
    time = GetGameTime()
  }
  events[eventCounter] = event
  
  -- Write event to output file for MCP server
  outfile:write("Event: " .. eventCounter .. ", " .. eventtype .. 
                ", Source: " .. tostring(sourceID) .. " (" .. sourceType .. ")" ..
                ", Target: " .. tostring(targetID) .. " (" .. targetType .. ")" ..
                ", Location: " .. tostring(longitude) .. ", " .. tostring(latitude) .. "\n")
  outfile:flush()
end

function PlaceFleetAngle(longitude, latitude, type, angle, fleetsize)
  -- angle goes from 1 to 6
  local lono, lato = GetFleetMemberOffset(fleetsize, angle)
  DebugLog("Placing fleet member at angle " .. angle .. " with offset: " .. lono .. ", " .. lato)
  PlaceFleet(longitude + lono, latitude + lato, type)
end

function attemptFleet(longitude, latitude, type1, type2, type3, type4, type5, type6)
  local a = IsValidPlacementLocation(longitude, latitude, "Sub")
  local b = IsValidTerritory(GetOwnTeamID(), longitude, latitude, true)
  local success = false
  if a == true and b == true then
    success = true
    DebugLog("Valid placement (Fleet): " .. longitude .. " / " .. latitude)
    local fleetsize = 0
    local t1 = ensureIsShipType(type1)
    local t2 = ensureIsShipType(type2)
    local t3 = ensureIsShipType(type3)
    local t4 = ensureIsShipType(type4)
    local t5 = ensureIsShipType(type5)
    local t6 = ensureIsShipType(type6)
    if t1 then
      fleetsize = fleetsize + 1
    end
    if t2 then
      fleetsize = fleetsize + 1
    end
    if t3 then
      fleetsize = fleetsize + 1
    end
    if t4 then
      fleetsize = fleetsize + 1
    end
    if t5 then
      fleetsize = fleetsize + 1
    end
    if t6 then
      fleetsize = fleetsize + 1
    end
    DebugLog("fleetSize - " .. fleetsize)
    if t1 then
      DebugLog("1 - " .. t1)
      PlaceFleetAngle(longitude, latitude, t1, 1, fleetsize)
    end
    if t2 then
      DebugLog("2 - " .. t2)
      PlaceFleetAngle(longitude, latitude, t2, 2, fleetsize)
    end
    if t3 then
      DebugLog("3 - " .. t3)
      PlaceFleetAngle(longitude, latitude, t3, 3, fleetsize)
    end
    if t4 then
      DebugLog("4 - " .. t4)
      PlaceFleetAngle(longitude, latitude, t4, 4, fleetsize)
    end
    if t5 then
      DebugLog("5 - " .. t5)
      PlaceFleetAngle(longitude, latitude, t5, 5, fleetsize)
    end
    if t6 then
      DebugLog("6 - " .. t6)
      PlaceFleetAngle(longitude, latitude, t6, 6, fleetsize)
    end
  else
    --DebugLog("Invalid placement")
    WhiteboardDraw(longitude-0.5, latitude, longitude+0.5, latitude)
    WhiteboardDraw(longitude, latitude+0.5, longitude, latitude-0.5)
  end
  return success
end

function attemptPlace(longitude, latitude, typename)
  if typename ~= "RadarStation" and typename ~= "Silo" and typename ~= "AirBase" then
    DebugLog("Attempting to place structure, but typename provided is not supported!")
    return false
  end
  local a = IsValidPlacementLocation(longitude, latitude, typename)
  local b = IsValidTerritory(GetOwnTeamID(), longitude, latitude, false)
  if a == true and b == true then
    DebugLog("Valid placement (" .. typename .. "): " .. longitude .. " / " .. latitude)
    PlaceStructure(longitude, latitude, typename)
    return true
  else
    --DebugLog("Invalid placement")
    WhiteboardDraw(longitude-0.5, latitude-0.5, longitude+0.5, latitude+0.5)
    WhiteboardDraw(longitude-0.5, latitude+0.5, longitude+0.5, latitude-0.5)
    return false
  end
end

function attemptNuke(siloID, longitude, latitude)
  local id = getThingByID("Silo", siloID, true)
  if id ~= nil then
    SetState(id, 0)
    SetActionTarget(id, null, longitude, latitude)
    WhiteboardDraw(GetLongitude(id), GetLatitude(id), longitude, latitude)
    return true
  end
  return false
end

function attemptDefensiveSilo(siloID, longitude, latitude)
  local id = getThingByID("Silo", siloID, true)
  if id ~= nil then
    SetState(id, 1)
    return true
  end
  return false
end

function attemptSetAirUnitTarget(unitID, targetID, longitude, latitude)
  local id = getThingByID(nil, unitID, true)
  if id ~= nil then
    SetActionTarget(id, targetID, longitude, latitude)
    return true
  end
  return false
end

function attemptSetAirUnitLanding(unitID, targetID)
  local id = getThingByID(nil, unitID, true)
  if id ~= nil then
    SetLandingTarget(id, targetID)
    return true
  end
  return false
end

function attemptSetSubmarineState(unitID, state)
  local id = getThingByID("Sub", unitID, true)
  if id ~= nil then
    SetState(id, state)
    return true
  end
  return false
end

function ensureIsShipType(type)
  if type == "Sub" or type == "Carrier" or type == "BattleShip" then
    DebugLog("Yep, it's good: " .. type)
    return type
  end
  return nil
end

function getThingByID(type, idstr, mineOnly)
  local ud = {}
  GetAllUnitData(ud)
  for id, unit in pairs(ud) do
    if string.sub(tostring(id), 2, -2) == idstr then
      if mineOnly == false or (mineOnly == true and unit["team"] == GetOwnTeamID()) then
        if type == nil or (type ~= nil and unit["type"] == type) then
          return id
        end
      end
    end
  end
end

function getMyFleetByID(idstr)
  local fleets = GetOwnFleets()
  for _, id in pairs(fleets) do
    if string.sub(tostring(id), 2, -2) == idstr then
      return id
    end
  end
end

function GetGameState()
  local teamid = GetOwnTeamID()
  local cityids = GetCityIDs()
  local defcon = GetDefconLevel()
  local ud = {}
  local a, b
  GetAllUnitData(ud)
  
  -- Include event counter in game state
  local totalEvents = eventCounter

  outfile:write("\n-- game state info start --:\n")
  outfile:write("\nDEFCON level: " .. defcon .. "\n")
  outfile:write("Game time (seconds): " .. GetGameTime() .. "\n")
  outfile:write("Total events: " .. totalEvents .. "\n\n")

  local teamid = GetOwnTeamID()
  local teamname = GetTeamName(teamid)
  outfile:write("Your player name: " .. teamname .. "\n")
  local teamterritories = GetTeamTerritories(teamid)
  outfile:write("Your player territories: " .. table.concat(teamterritories, ", ") .. "\n")
  local allianceId = GetAllianceID(teamid)
  outfile:write("Your alliance ID: " .. string.sub(tostring(allianceId), 2, -2) .. "\n")
  -- get allies
  local allTeamIDs = GetAllTeamIDs()

  outfile:write("Your allies: ")
  for _, id in ipairs(allTeamIDs) do
    local alliance = GetAllianceID(id)
    local first = true
    if alliance == allianceId and id ~= teamid then
      if first == false then
        outfile:write(", ")
      end
      outfile:write(GetTeamName(id))
      local territoryNames = GetTeamTerritories(id)
      if territoryNames ~= nil and #territoryNames > 0 then
        outfile:write(" (" .. table.concat(territoryNames, ", ") .. ")")
      end
      first = false
    end
  end
  outfile:write("\n")
  -- get enemy territories
  outfile:write("Your enemies: ")
  for _, id in ipairs(allTeamIDs) do
    local alliance = GetAllianceID(id)
    local first = true
    if alliance ~= allianceId then
      if first == false then
        outfile:write(", ")
      end
      outfile:write(GetTeamName(id))
      local territoryNames = GetTeamTerritories(id)
      if territoryNames ~= nil and #territoryNames > 0 then
        outfile:write(" (" .. table.concat(territoryNames, ", ") .. ")")
      end
      first = false
    end
  end
  outfile:write("\n")




  outfile:write("\nYour units and buildings currently on the map (id, typename, longitude, latitude):\n")
  for id, unit in pairs(ud) do
    if unit["team"] == teamid then
      outfile:write(string.sub(tostring(id), 2, -2) .. ", " .. tostring(unit["type"]) .. ", " .. tostring(unit["longitude"]) .. ", " .. tostring(unit["latitude"]) .. "\n")
    end
  end
  
  outfile:write("\nYour fleets (FleetID, ships with IDs and locations):\n")
  local fleets = GetOwnFleets()
  for _, fleetId in ipairs(fleets) do
    local ships = GetFleetUnits(fleetId)
    outfile:write(string.sub(tostring(fleetId), 2, -2) .. ":\n")
    for _, shipId in ipairs(ships) do
      local shipType = GetUnitType(shipId)
      local longitude = GetLongitude(shipId)
      local latitude = GetLatitude(shipId)
      outfile:write("  " .. string.sub(tostring(shipId), 2, -2) .. ", " .. shipType .. ", " .. longitude .. ", " .. latitude .. "\n")
    end
  end

  outfile:write("\nThese are the cities you must protect (longitude, latitude, population):\n")

  for _, id in ipairs(cityids) do
    if GetTeamID(id) == teamid then
      outfile:write(tostring(GetLongitude(id)) .. ", " .. tostring(GetLatitude(id)) .. ", " .. tostring(GetCityPopulation(id)) .. "\n")
    end
  end

  if GetDefconLevel() >= 4 then
    outfile:write("\nYou can still place:\n")
    if GetRemainingUnits("Silo") > 0 then
      outfile:write("Silo - " .. GetRemainingUnits("Silo") .. "\n")
    else
      outfile:write("NO Silo - do not try to place this!\n")
    end
    if GetRemainingUnits("RadarStation") > 0 then
      outfile:write("RadarStation - " .. GetRemainingUnits("RadarStation") .. "\n")
    else
      outfile:write("NO RadarStation - do not try to place this!\n")
    end
    if GetRemainingUnits("AirBase") > 0 then
      outfile:write("AirBase - " .. GetRemainingUnits("AirBase") .. "\n")
    else
      outfile:write("NO AirBase - do not try to place this!\n")
    end

    outfile:write("\nYou can still create fleets, using ships:\n")
    if GetRemainingUnits("Sub") > 0 then
      outfile:write("Sub - " .. GetRemainingUnits("Sub") .. "\n")
    else
      outfile:write("NO Sub - do not try to place this!\n")
    end
    if GetRemainingUnits("BattleShip") > 0 then
      outfile:write("BattleShip - " .. GetRemainingUnits("BattleShip") .. "\n")
    else
      outfile:write("NO BattleShip - do not try to place this!\n")
    end
    if GetRemainingUnits("Carrier") > 0 then
      outfile:write("Carrier - " .. GetRemainingUnits("Carrier") .. "\n")
    else
      outfile:write("NO Carrier - do not try to place this!\n")
    end

  end

  if GetDefconLevel() <= 3 then
    outfile:write("\nThese are the cities you must DESTROY (longitude, latitude, population):\n")
    for _, id in ipairs(cityids) do
      if GetTeamID(id) ~= teamid then
        outfile:write(tostring(GetLongitude(id)) .. ", " .. tostring(GetLatitude(id)) .. ", " .. tostring(GetCityPopulation(id)) .. "\n")
      end
    end
  end

  if GetDefconLevel() <= 1 then
    outfile:write("\nYour silos with nukes (SiloID, nukecount, state):\n")
    for id, unit in pairs(ud) do
      if unit["team"] == teamid and unit["type"] == "Silo" and GetNukeCount(id) > 0 then
        local state = unit["state"] -- 0 - icbm launch mode, 1 - air defense, 2 - n/a
        outfile:write(string.sub(tostring(id), 2, -2) .. ", " --[[ .. tostring(unit["longitude"]) .. ", " .. tostring(unit["latitude"]) .. ", " --]] .. tostring(GetNukeCount(id)) .. 
        ", " .. (state == 0 and "ICBM LAUNCH" or state == 1 and "AIR DEFENSE" or state == 2 and "N/A" or "UNKNOWN")
        .. "\n")
      end
    end

    outfile:write("\nYour silos without any nukes (SiloID, state) - HINT, you may want to set these to defensive mode!:\n")
    for id, unit in pairs(ud) do
      if unit["team"] == teamid and unit["type"] == "Silo" and GetNukeCount(id) == 0 then
        local state = unit["state"] -- 0 - icbm launch mode, 1 - air defense, 2 - n/a
        outfile:write(string.sub(tostring(id), 2, -2) ..
        ", " .. (state == 0 and "ICBM LAUNCH" or state == 1 and "AIR DEFENSE" or state == 2 and "N/A" or "UNKNOWN")
        .. "\n")
      end
    end

    outfile:write("\nNukes in flight (longitude, latitude, target longitude, target latitude, owner id, team info, state):\n")
    for id, unit in pairs(ud) do
      if unit["type"] == "Nuke" then
        local isYours = unit["team"] == teamid
        local isAlly = unit["team"] ~= teamid and GetAllianceID(unit["team"]) == GetAllianceID(teamid)
        local state = unit["state"] -- 0 - on target, 1 - disarm, 2 - n/a
        local targetLongitude, targetLatitude = GetMovementTargetLocation(id)
        outfile:write(string.sub(tostring(id), 2, -2) .. ", " .. tostring(unit["longitude"]) .. ", " .. tostring(unit["latitude"]) .. 
                      ", " .. tostring(targetLongitude) .. ", " .. tostring(targetLatitude) ..
                      ", " .. tostring(unit["team"]) .. ", " .. (isYours and "YOURS" or isAlly and "ALLY" or "ENEMY") .. ", " 
                      .. (state == 0 and "ON TARGET" or state == 1 and "DISARM" or state == 2 and "N/A" or "UNKNOWN")
                      .. "\n")
      end
    end
  end

  outfile:write("\n-- game state info end --:\n")

  outfile:flush()
end

function MTLTest()
  while true do
    coroutine.yield()
    if GetGameTick() % 100 == 0 then
      getStateNext = true
    end

    if GetGameTick() % 10 == 0 then
      -- write output
      if getStateNext == true then
        getStateNext = false
        GetGameState()
      end

      -- read input
      local file_size = infile:seek("end")
      --DebugLog("File size " .. file_size)
      if file_size < offset then
        DebugLog("File size got truncated, resetting!")
        offset = file_size
      end
      if file_size > offset then
        getStateNext = true
        infile:seek("set", offset)
        local content = infile:read(file_size - offset)
        offset = file_size
        for line in string.gmatch(content .. "\n", "(.-)\n") do
          local debuglog = string.match(line, "^DebugLog%(\"(.*)\"%)")
          if debuglog then
            DebugLog(debuglog)
          end

          local chat, corrid = string.match(line, "^SendChat%(\"(.*)\"%)%s*%-%-%s*(%d+)$")
          if chat then
            DebugLog(chat)
            SendChat(chat)
          end

          local z, x, v, corrid = string.match(line, "^LaunchNukeFromSilo%(([0-9]*), ([0-9.-]*), ([0-9.-]*)%)%s*%-%-%s*(%d+)$")
          if not z then
            z, x, v = string.match(line, "^LaunchNukeFromSilo%(([0-9]*), ([0-9.-]*), ([0-9.-]*)%)")
          end
          if z then
            local success = attemptNuke(z, x, v)
            local result = "Command result: LaunchNukeFromSilo(" .. z .. ", " .. x .. ", " .. v .. ") - " .. (success and "SUCCESS" or "FAILED")
            if corrid then result = result .. " [ID:" .. corrid .. "]" end
            outfile:write(result .. "\n")
            outfile:flush()
          end
          
          local x1, y1, x2, y2, corrid = string.match(line, "^WhiteboardDraw%(([0-9.-]*), ([0-9.-]*), ([0-9.-]*), ([0-9.-]*)%)%s*%-%-%s*(%d+)$")
          if not x1 then
            x1, y1, x2, y2 = string.match(line, "^WhiteboardDraw%(([0-9.-]*), ([0-9.-]*), ([0-9.-]*), ([0-9.-]*)%)")
          end
          if x1 and y1 and x2 and y2 then
            WhiteboardDraw(tonumber(x1), tonumber(y1), tonumber(x2), tonumber(y2))
            local result = "Command result: WhiteboardDraw(" .. x1 .. ", " .. y1 .. ", " .. x2 .. ", " .. y2 .. ") - SUCCESS"
            if corrid then result = result .. " [ID:" .. corrid .. "]" end
            outfile:write(result .. "\n")
            outfile:flush()
          end

          local d, corrid = string.match(line, "^StopLaunchingNukesFromSiloAndGoDefensive%(([0-9]*)%)%s*%-%-%s*(%d+)$")
          if not d then
            d = string.match(line, "^StopLaunchingNukesFromSiloAndGoDefensive%(([0-9]*)%)")
          end
          if d then
            local success = attemptDefensiveSilo(d)
            local result = "Command result: StopLaunchingNukesFromSiloAndGoDefensive(" .. d .. ") - " .. (success and "SUCCESS" or "FAILED")
            if corrid then result = result .. " [ID:" .. corrid .. "]" end
            outfile:write(result .. "\n")
            outfile:flush()
          end

          -- note: below command also allows non-quoted types, as that's a common LLM mistake.
          local a, b, c, corrid = string.match(line, "^PlaceStructure%(([0-9.-]*), ([0-9.-]*), \"?([a-zA-Z]*)\"?%)%s*%-%-%s*(%d+)$")
          if not a then
            a, b, c = string.match(line, "^PlaceStructure%(([0-9.-]*), ([0-9.-]*), \"?([a-zA-Z]*)\"?%)")
          end
          if a then
            local success = attemptPlace(a, b, c)
            local result = "Command result: PlaceStructure(" .. a .. ", " .. b .. ", " .. c .. ") - " .. (success and "SUCCESS" or "FAILED")
            if corrid then result = result .. " [ID:" .. corrid .. "]" end
            outfile:write(result .. "\n")
            outfile:flush()
          end

          -- note: below command also allows non-quoted types, as that's a common LLM mistake.
          local e, f, g, h, i, j, k, l, corrid = string.match(line, "^PlaceFleet%(([0-9.-]*), ([0-9.-]*), \"?([a-zA-Z]*)\"?, \"?([a-zA-Z]*)\"?, \"?([a-zA-Z]*)\"?, \"?([a-zA-Z]*)\"?, \"?([a-zA-Z]*)\"?, \"?([a-zA-Z]*)\"?%)%s*%-%-%s*(%d+)$")
          if not e then
            e, f, g, h, i, j, k, l = string.match(line, "^PlaceFleet%(([0-9.-]*), ([0-9.-]*), \"?([a-zA-Z]*)\"?, \"?([a-zA-Z]*)\"?, \"?([a-zA-Z]*)\"?, \"?([a-zA-Z]*)\"?, \"?([a-zA-Z]*)\"?, \"?([a-zA-Z]*)\"?%)")
          end
          if e then
            local success = attemptFleet(e, f, g, h, i, j, k, l)
            local result = "Command result: PlaceFleet(" .. e .. ", " .. f .. ", " .. (g or "") .. ", " .. (h or "") .. ", " .. (i or "") .. ", " .. (j or "") .. ", " .. (k or "") .. ", " .. (l or "") .. ") - " .. (success and "SUCCESS" or "FAILED")
            if corrid then result = result .. " [ID:" .. corrid .. "]" end
            outfile:write(result .. "\n")
            outfile:flush()
          end
          local e, f, g, h, i, j, k, corrid = string.match(line, "^PlaceFleet%(([0-9.-]*), ([0-9.-]*), \"?([a-zA-Z]*)\"?, \"?([a-zA-Z]*)\"?, \"?([a-zA-Z]*)\"?, \"?([a-zA-Z]*)\"?, \"?([a-zA-Z]*)\"?%)%s*%-%-%s*(%d+)$")
          if not e then
            e, f, g, h, i, j, k = string.match(line, "^PlaceFleet%(([0-9.-]*), ([0-9.-]*), \"?([a-zA-Z]*)\"?, \"?([a-zA-Z]*)\"?, \"?([a-zA-Z]*)\"?, \"?([a-zA-Z]*)\"?, \"?([a-zA-Z]*)\"?%)")
          end
          if e then
            local success = attemptFleet(e, f, g, h, i, j, k)
            local result = "Command result: PlaceFleet(" .. e .. ", " .. f .. ", " .. (g or "") .. ", " .. (h or "") .. ", " .. (i or "") .. ", " .. (j or "") .. ", " .. (k or "") .. ") - " .. (success and "SUCCESS" or "FAILED")
            if corrid then result = result .. " [ID:" .. corrid .. "]" end
            outfile:write(result .. "\n")
            outfile:flush()
          end
          local e, f, g, h, i, j, corrid = string.match(line, "^PlaceFleet%(([0-9.-]*), ([0-9.-]*), \"?([a-zA-Z]*)\"?, \"?([a-zA-Z]*)\"?, \"?([a-zA-Z]*)\"?, \"?([a-zA-Z]*)\"?%)%s*%-%-%s*(%d+)$")
          if not e then
            e, f, g, h, i, j = string.match(line, "^PlaceFleet%(([0-9.-]*), ([0-9.-]*), \"?([a-zA-Z]*)\"?, \"?([a-zA-Z]*)\"?, \"?([a-zA-Z]*)\"?, \"?([a-zA-Z]*)\"?%)")
          end
          if e then
            local success = attemptFleet(e, f, g, h, i, j)
            local result = "Command result: PlaceFleet(" .. e .. ", " .. f .. ", " .. (g or "") .. ", " .. (h or "") .. ", " .. (i or "") .. ", " .. (j or "") .. ") - " .. (success and "SUCCESS" or "FAILED")
            if corrid then result = result .. " [ID:" .. corrid .. "]" end
            outfile:write(result .. "\n")
            outfile:flush()
          end
          local e, f, g, h, i, corrid = string.match(line, "^PlaceFleet%(([0-9.-]*), ([0-9.-]*), \"?([a-zA-Z]*)\"?, \"?([a-zA-Z]*)\"?, \"?([a-zA-Z]*)\"?%)%s*%-%-%s*(%d+)$")
          if not e then
            e, f, g, h, i = string.match(line, "^PlaceFleet%(([0-9.-]*), ([0-9.-]*), \"?([a-zA-Z]*)\"?, \"?([a-zA-Z]*)\"?, \"?([a-zA-Z]*)\"?%)")
          end
          if e then
            local success = attemptFleet(e, f, g, h, i)
            local result = "Command result: PlaceFleet(" .. e .. ", " .. f .. ", " .. (g or "") .. ", " .. (h or "") .. ", " .. (i or "") .. ") - " .. (success and "SUCCESS" or "FAILED")
            if corrid then result = result .. " [ID:" .. corrid .. "]" end
            outfile:write(result .. "\n")
            outfile:flush()
          end
          local e, f, g, h, corrid = string.match(line, "^PlaceFleet%(([0-9.-]*), ([0-9.-]*), \"?([a-zA-Z]*)\"?, \"?([a-zA-Z]*)\"?%)%s*%-%-%s*(%d+)$")
          if not e then
            e, f, g, h = string.match(line, "^PlaceFleet%(([0-9.-]*), ([0-9.-]*), \"?([a-zA-Z]*)\"?, \"?([a-zA-Z]*)\"?%)")
          end
          if e then
            local success = attemptFleet(e, f, g, h)
            local result = "Command result: PlaceFleet(" .. e .. ", " .. f .. ", " .. (g or "") .. ", " .. (h or "") .. ") - " .. (success and "SUCCESS" or "FAILED")
            if corrid then result = result .. " [ID:" .. corrid .. "]" end
            outfile:write(result .. "\n")
            outfile:flush()
          end
          local e, f, g, corrid = string.match(line, "^PlaceFleet%(([0-9.-]*), ([0-9.-]*), \"?([a-zA-Z]*)\"?%)%s*%-%-%s*(%d+)$")
          if not e then
            e, f, g = string.match(line, "^PlaceFleet%(([0-9.-]*), ([0-9.-]*), \"?([a-zA-Z]*)\"?%)")
          end
          if e then
            local success = attemptFleet(e, f, g)
            local result = "Command result: PlaceFleet(" .. e .. ", " .. f .. ", " .. (g or "") .. ") - " .. (success and "SUCCESS" or "FAILED")
            if corrid then result = result .. " [ID:" .. corrid .. "]" end
            outfile:write(result .. "\n")
            outfile:flush()
          end
          
          local fleetId, x, y, corrid = string.match(line, "^MoveFleet%(([0-9]*), ([0-9.-]*), ([0-9.-]*)%)%s*%-%-%s*(%d+)$")
          if not fleetId then
            fleetId, x, y = string.match(line, "^MoveFleet%(([0-9]*), ([0-9.-]*), ([0-9.-]*)%)")
          end
          if fleetId then
            local success = false
            local fleetId_obj = getMyFleetByID(fleetId)
            if fleetId_obj then
              local fleet = GetFleetUnits(fleetId_obj)
              if fleet then
                for _, unitId in ipairs(fleet) do
                  SetMovementTarget(unitId, x, y)
                end
                success = true
              end
            end
            local result = "Command result: MoveFleet(" .. fleetId .. ", " .. x .. ", " .. y .. ") - " .. (success and "SUCCESS" or "FAILED")
            if corrid then result = result .. " [ID:" .. corrid .. "]" end
            outfile:write(result .. "\n")
            outfile:flush()
          end
          
          local unitId, targetId, x, y, corrid = string.match(line, "^SetAirUnitTarget%(([0-9]*), ([0-9]*), ([0-9.-]*), ([0-9.-]*)%)%s*%-%-%s*(%d+)$")
          if not unitId then
            unitId, targetId, x, y = string.match(line, "^SetAirUnitTarget%(([0-9]*), ([0-9]*), ([0-9.-]*), ([0-9.-]*)%)")
          end
          if unitId then
            local success = attemptSetAirUnitTarget(unitId, targetId, x, y)
            local result = "Command result: SetAirUnitTarget(" .. unitId .. ", " .. targetId .. ", " .. x .. ", " .. y .. ") - " .. (success and "SUCCESS" or "FAILED")
            if corrid then result = result .. " [ID:" .. corrid .. "]" end
            outfile:write(result .. "\n")
            outfile:flush()
          end
          
          local unitId, targetId, corrid = string.match(line, "^SetAirUnitLanding%(([0-9]*), ([0-9]*)%)%s*%-%-%s*(%d+)$")
          if not unitId then
            unitId, targetId = string.match(line, "^SetAirUnitLanding%(([0-9]*), ([0-9]*)%)")
          end
          if unitId then
            local success = attemptSetAirUnitLanding(unitId, targetId)
            local result = "Command result: SetAirUnitLanding(" .. unitId .. ", " .. targetId .. ") - " .. (success and "SUCCESS" or "FAILED")
            if corrid then result = result .. " [ID:" .. corrid .. "]" end
            outfile:write(result .. "\n")
            outfile:flush()
          end
          
          local unitId, state, corrid = string.match(line, "^SetSubmarineState%(([0-9]*), ([0-2])%)%s*%-%-%s*(%d+)$")
          if not unitId then
            unitId, state = string.match(line, "^SetSubmarineState%(([0-9]*), ([0-2])%)")
          end
          if unitId then
            local success = attemptSetSubmarineState(unitId, state)
            local result = "Command result: SetSubmarineState(" .. unitId .. ", " .. state .. ") - " .. (success and "SUCCESS" or "FAILED")
            if corrid then result = result .. " [ID:" .. corrid .. "]" end
            outfile:write(result .. "\n")
            outfile:flush()
          end
          
          local teamId, corrid = string.match(line, "^RequestCeaseFire%(([0-9]*)%)%s*%-%-%s*(%d+)$")
          if not teamId then
            teamId = string.match(line, "^RequestCeaseFire%(([0-9]*)%)")
          end
          if teamId then
            local teamId_obj = getThingByID(nil, teamId, false)
            local success = false
            if teamId_obj then
              RequestCeaseFire(teamId_obj)
              success = true
            end
            local result = "Command result: RequestCeaseFire(" .. teamId .. ") - " .. (success and "SUCCESS" or "FAILED")
            if corrid then result = result .. " [ID:" .. corrid .. "]" end
            outfile:write(result .. "\n")
            outfile:flush()
          end
          
          local teamId, corrid = string.match(line, "^RequestShareRadar%(([0-9]*)%)%s*%-%-%s*(%d+)$")
          if not teamId then
            teamId = string.match(line, "^RequestShareRadar%(([0-9]*)%)")
          end
          if teamId then
            local teamId_obj = getThingByID(nil, teamId, false)
            local success = false
            if teamId_obj then
              RequestShareRadar(teamId_obj)
              success = true
            end
            local result = "Command result: RequestShareRadar(" .. teamId .. ") - " .. (success and "SUCCESS" or "FAILED")
            if corrid then result = result .. " [ID:" .. corrid .. "]" end
            outfile:write(result .. "\n")
            outfile:flush()
          end
          
          local allianceId, corrid = string.match(line, "^RequestAlliance%(([0-9]*)%)%s*%-%-%s*(%d+)$")
          if not allianceId then
            allianceId = string.match(line, "^RequestAlliance%(([0-9]*)%)")
          end
          if allianceId then
            local allianceId_obj = getThingByID(nil, allianceId, false)
            local success = false
            if allianceId_obj then
              RequestAlliance(allianceId_obj)
              success = true
            end
            local result = "Command result: RequestAlliance(" .. allianceId .. ") - " .. (success and "SUCCESS" or "FAILED")
            if corrid then result = result .. " [ID:" .. corrid .. "]" end
            outfile:write(result .. "\n")
            outfile:flush()
          end
          
          local eventId, inFavor, corrid = string.match(line, "^SendVote%(([0-9]*), ([a-z]*)%)%s*%-%-%s*(%d+)$")
          if not eventId then
            eventId, inFavor = string.match(line, "^SendVote%(([0-9]*), ([a-z]*)%)")
          end
          if eventId then
            local eventId_obj = getThingByID(nil, eventId, false)
            local success = false
            if eventId_obj then
              SendVote(eventId_obj, inFavor == "true")
              success = true
            end
            local result = "Command result: SendVote(" .. eventId .. ", " .. inFavor .. ") - " .. (success and "SUCCESS" or "FAILED")
            if corrid then result = result .. " [ID:" .. corrid .. "]" end
            outfile:write(result .. "\n")
            outfile:flush()
          end
          

        end
      end
    end
  end
end
