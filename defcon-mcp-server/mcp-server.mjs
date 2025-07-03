import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";
import { StreamableHTTPServerTransport } from "@modelcontextprotocol/sdk/server/streamableHttp.js";
import express from "express";
import fs from "fs";
import { z } from "zod";
import { randomUUID } from "crypto";
import os from "os";
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';

// File paths - platform specific
const isWindows = os.platform() === 'win32';
let INPUTFILE = isWindows ? "R:\\input.txt" : "/mnt/r/input.txt";
let OUTPUTFILE = isWindows ? "R:\\output.txt" : "/mnt/r/output.txt";
// set from cli argument --inputfile=... and --outputfile=...
if (process.argv.length > 2) {
  process.argv.forEach((arg) => {
    if (arg.startsWith("--inputfile=")) {
      INPUTFILE = arg.split("=")[1];
    } else if (arg.startsWith("--outputfile=")) {
      OUTPUTFILE = arg.split("=")[1];
    }
  });
}

// Game state tracking
let lastGameState = "";
let defconLevel = 5;
let PROMPTSETTING;
let PROMPTFILE;
let lastCorrelationId = 1000; // Starting correlation ID

// various constants
const SUCCESS_READ_TIMEOUT = 1000; // in ms

// Initialize the MCP server
const server = new McpServer({
  name: "DEFCON-LLM-Bot",
  version: "1.0.0",
  description: "MCP Server for DEFCON game AI using LLMs"
});

// Load the initial conversation settings
try {
  const __filename = fileURLToPath(import.meta.url);
  const __dirname = dirname(__filename);
  PROMPTFILE = JSON.parse(fs.readFileSync(join(__dirname, "initial_convo.json")).toString());
  updatePromptSetting();
} catch (error) {
  console.error("Failed to load initial_convo.json:", error);
  process.exit(1);
}

function updatePromptSetting() {
  PROMPTSETTING = PROMPTFILE[`defcon_${defconLevel}`] || PROMPTFILE[`defcon_5`];
}

// Initialize the game files
function initializeGameFiles() {
  try {
    fs.unlinkSync(INPUTFILE);
  } catch (e) { /* File might not exist */ }
  fs.writeFileSync(INPUTFILE, "");
}

// Read game state from output file
async function readGameState() {
  try {
    const stats = await fs.promises.stat(OUTPUTFILE);
    const fileContent = await fs.promises.readFile(OUTPUTFILE, 'utf8');
    
    // Extract the latest game state between the last occurrence of start and end markers
    const lastStartIndex = fileContent.lastIndexOf("-- game state info start --:");
    const lastEndIndex = fileContent.lastIndexOf("-- game state info end --:");
    
    if (lastStartIndex !== -1 && lastEndIndex !== -1 && lastStartIndex < lastEndIndex) {
      const latestGameState = fileContent.substring(lastStartIndex, lastEndIndex + "-- game state info end --:".length);
      
      // Extract DEFCON level
      const match = latestGameState.match(/DEFCON level: (\d+)/);
      if (match) {
        defconLevel = parseInt(match[1]);
        updatePromptSetting();
      }
      
      lastGameState = latestGameState;
      return latestGameState;
    }
    
    // If no markers found, return the whole content (fallback)
    lastGameState = fileContent;
    return fileContent;
  } catch (error) {
    console.error("Error reading game state:", error);
    return "Error reading game state";
  }
}

// Write commands to the input file
function writeCommandToGame(command) {
  try {
    fs.writeFileSync(INPUTFILE, command + "\n", { flag: 'a' });
    return true;
  } catch (error) {
    console.error("Error writing command to game:", error);
    return false;
  }
}

// MCP Resources

// Game state resource
server.resource(
  "game-state",
  "defcon://game-state",
  async (uri) => {
    const gameState = await readGameState();
    return {
      contents: [{
        uri: uri.href,
        text: gameState
      }]
    };
  }
);

// DEFCON level resource
server.resource(
  "defcon-level",
  "defcon://level",
  async (uri) => {
    return {
      contents: [{
        uri: uri.href,
        text: `Current DEFCON level: ${defconLevel}`
      }]
    };
  }
);

// Fleet information resource
server.resource(
  "fleet-info",
  "defcon://fleet-info",
  async (uri) => {
    const gameState = await readGameState();
    // Extract fleet information from game state
    const fleetInfoMatch = gameState.match(/Your fleets \(FleetID, ships with IDs and locations\):\n([\s\S]*?)(?:\n\n|$)/);
    const fleetInfo = fleetInfoMatch ? fleetInfoMatch[1] : "No fleet information available";
    
    return {
      contents: [{
        uri: uri.href,
        text: fleetInfo
      }]
    };
  }
);

// Air unit information resource
server.resource(
  "air-units",
  "defcon://air-units",
  async (uri) => {
    const gameState = await readGameState();
    // Extract air unit information from game state
    const airUnitInfo = gameState.match(/Your units and buildings[\s\S]*?((?:Bomber|Fighter)[\s\S]*?)(?:\n\n|$)/);
    const airUnits = airUnitInfo ? airUnitInfo[1] : "No air units available";
    
    return {
      contents: [{
        uri: uri.href,
        text: airUnits
      }]
    };
  }
);

// Game events resource
server.resource(
  "events",
  "defcon://events/:fromId?",
  async (uri) => {
    const fromId = parseInt(uri.pathname.split('/')[2] || 0);
    
    try {
      const gameState = await fs.promises.readFile(OUTPUTFILE, 'utf8');
      const eventRegex = /Event: (\d+), ([^,]+), Source: ([^(]+) \(([^)]+)\), Target: ([^(]+) \(([^)]+)\), Location: ([^,]+), ([^,\n]+)/g;
      
      const events = [];
      let match;
      while ((match = eventRegex.exec(gameState)) !== null) {
        const eventId = parseInt(match[1]);
        if (eventId > fromId) {
          events.push({
            id: eventId,
            type: match[2],
            source: match[3].trim(),
            sourceType: match[4],
            target: match[5].trim(),
            targetType: match[6],
            longitude: match[7],
            latitude: match[8]
          });
        }
      }
      
      const eventsText = events.length > 0 ? 
        events.map(e => `Event ${e.id}: ${e.type}, Source: ${e.source} (${e.sourceType}), Target: ${e.target} (${e.targetType}), Location: ${e.longitude}, ${e.latitude}`).join('\n') : 
        "No events found";
      
      return {
        contents: [{
          uri: uri.href,
          text: `Events after ID ${fromId}:\n\n${eventsText}`
        }]
      };
    } catch (error) {
      return {
        contents: [{
          uri: uri.href,
          text: `Error retrieving events: ${error.message}`
        }]
      };
    }
  }
);

// MCP Tools

// Debug log tool
server.tool(
  "debug-log", "Logs a message to the game's debug console",
  { 
    message: z.string().describe("Message to log to the debug console"),
    correlationId: z.number().optional().describe("Optional ID to correlate this command with its result")
  },
  async ({ message, correlationId }) => {
    // Auto-generate correlation ID if not provided
    if (correlationId === undefined) {
      correlationId = ++lastCorrelationId;
    }
    
    let command = `DebugLog("${message}") -- ${correlationId}`;
    const success = writeCommandToGame(command);
    
    return {
      content: [{ 
        type: "text", 
        text: success ? `Debug log sent: ${message} with correlation ID: ${correlationId}` : "Failed to send debug log" 
      }],
      correlationId: correlationId
    };
  }
);

// Send chat message tool
server.tool(
  "send-chat", "Sends a chat message visible to all players in the game",
  { 
    message: z.string().describe("Chat message to send to all players"),
    correlationId: z.number().optional().describe("Optional ID to correlate this command with its result")
  },
  async ({ message, correlationId }) => {
    // Auto-generate correlation ID if not provided
    if (correlationId === undefined) {
      correlationId = ++lastCorrelationId;
    }
    
    let command = `SendChat("${message}") -- ${correlationId}`;
    const success = writeCommandToGame(command);
    
    return {
      content: [{ 
        type: "text", 
        text: success ? `Chat message sent: ${message} with correlation ID: ${correlationId}` : "Failed to send chat message" 
      }],
      correlationId: correlationId
    };
  }
);

// Place structure tool
server.tool(
  "place-structure", "Places a military structure (Silo, RadarStation, or AirBase) at the specified coordinates. IMPORTANT: Check the response to verify if placement was successful!",
  { 
    longitude: z.number().describe("Longitude coordinate for structure placement (-180 to 180)"), 
    latitude: z.number().describe("Latitude coordinate for structure placement (-90 to 90)"), 
    type: z.enum(["RadarStation", "Silo", "AirBase"]).describe("Type of structure to place"),
    correlationId: z.number().optional().describe("Optional ID to correlate this command with its result"),
    skipVerification: z.boolean().optional().default(false).describe("Set to true to skip automatic verification (faster for batch operations)")
  },
  async ({ longitude, latitude, type, correlationId, skipVerification }) => {
    // Auto-generate correlation ID if not provided
    if (correlationId === undefined) {
      correlationId = ++lastCorrelationId;
    }
    
    let command = `PlaceStructure(${longitude}, ${latitude}, "${type}") -- ${correlationId}`;
    const success = writeCommandToGame(command);
    
    if (!success) {
      return {
        content: [{ 
          type: "text", 
          text: "Failed to place structure" 
        }],
        correlationId: correlationId
      };
    }
    
    // Skip verification if requested
    if (skipVerification) {
      return {
        content: [{ 
          type: "text", 
          text: `Structure placement attempted: ${type} at ${longitude}, ${latitude} with correlation ID: ${correlationId}\n\nVerification skipped. Use get-command-results tool to check the result later.`
        }],
        correlationId: correlationId
      };
    }
    
    // Wait a short time for the game to process the command
    await new Promise(resolve => setTimeout(resolve, SUCCESS_READ_TIMEOUT));
    
    // Check if the command was successful
    const result = await getCommandResult(correlationId);
    
    return {
      content: [{ 
        type: "text", 
        text: `Structure placement ${result.found ? (result.success ? "succeeded" : "failed") : "attempted"}: ${type} at ${longitude}, ${latitude} with correlation ID: ${correlationId}${result.found ? "\n\nResult: " + result.result : "\n\nIMPORTANT: You must use the get-command-results tool to verify if this placement was successful!"}`
      }],
      correlationId: correlationId,
      success: result.found ? result.success : undefined
    };
  }
);

// Whiteboard draw tool
server.tool(
  "whiteboard-draw", "Draws a line on the game's whiteboard between two coordinate points",
  { 
    longitude1: z.number().describe("Starting longitude coordinate for the line"), 
    latitude1: z.number().describe("Starting latitude coordinate for the line"), 
    longitude2: z.number().describe("Ending longitude coordinate for the line"), 
    latitude2: z.number().describe("Ending latitude coordinate for the line"),
    correlationId: z.number().optional().describe("Optional ID to correlate this command with its result")
  },
  async ({ longitude1, latitude1, longitude2, latitude2, correlationId }) => {
    // Auto-generate correlation ID if not provided
    if (correlationId === undefined) {
      correlationId = ++lastCorrelationId;
    }
    
    let command = `WhiteboardDraw(${longitude1}, ${latitude1}, ${longitude2}, ${latitude2}) -- ${correlationId}`;
    const success = writeCommandToGame(command);
    
    return {
      content: [{ 
        type: "text", 
        text: success ? `Whiteboard line drawn from (${longitude1}, ${latitude1}) to (${longitude2}, ${latitude2}) with correlation ID: ${correlationId}` : "Failed to draw on whiteboard" 
      }],
      correlationId: correlationId
    };
  }
);

// Whiteboard clear tool
server.tool(
  "whiteboard-clear", "Clears all drawings from the game's whiteboard",
  {
    correlationId: z.number().optional().describe("Optional ID to correlate this command with its result")
  },
  async ({ correlationId }) => {
    // Auto-generate correlation ID if not provided
    if (correlationId === undefined) {
      correlationId = ++lastCorrelationId;
    }
    
    let command = `WhiteboardClear() -- ${correlationId}`;
    const success = writeCommandToGame(command);
    
    return {
      content: [{ 
        type: "text", 
        text: success ? `Whiteboard cleared with correlation ID: ${correlationId}` : "Failed to clear whiteboard" 
      }],
      correlationId: correlationId
    };
  }
);

// Place fleet tool
server.tool(
  "place-fleet", "Places a fleet of ships at the specified coordinates. You can specify up to 6 ships. IMPORTANT: Check the response to verify if placement was successful!",
  { 
    longitude: z.number().describe("Longitude coordinate for fleet placement (-180 to 180)"), 
    latitude: z.number().describe("Latitude coordinate for fleet placement (-90 to 90)"), 
    ship1: z.enum(["Sub", "Carrier", "BattleShip"]).describe("Type of first ship to place"),
    ship2: z.enum(["Sub", "Carrier", "BattleShip"]).optional().describe("Type of second ship to place"),
    ship3: z.enum(["Sub", "Carrier", "BattleShip"]).optional().describe("Type of third ship to place"),
    ship4: z.enum(["Sub", "Carrier", "BattleShip"]).optional().describe("Type of fourth ship to place"),
    ship5: z.enum(["Sub", "Carrier", "BattleShip"]).optional().describe("Type of fifth ship to place"),
    ship6: z.enum(["Sub", "Carrier", "BattleShip"]).optional().describe("Type of sixth ship to place"),
    correlationId: z.number().optional().describe("Optional ID to correlate this command with its result"),
    skipVerification: z.boolean().optional().default(false).describe("Set to true to skip automatic verification (faster for batch operations)")
  },
  async ({ longitude, latitude, ship1, ship2, ship3, ship4, ship5, ship6, correlationId, skipVerification }) => {
    // Auto-generate correlation ID if not provided
    if (correlationId === undefined) {
      correlationId = ++lastCorrelationId;
    }
    
    // Build the command with only the ships that were provided
    let ships = [ship1];
    if (ship2) ships.push(ship2);
    if (ship3) ships.push(ship3);
    if (ship4) ships.push(ship4);
    if (ship5) ships.push(ship5);
    if (ship6) ships.push(ship6);
    
    let shipsStr = ships.map(ship => `"${ship}"`).join(", ");
    let command = `PlaceFleet(${longitude}, ${latitude}, ${shipsStr}) -- ${correlationId}`;
    const success = writeCommandToGame(command);
    
    if (!success) {
      return {
        content: [{ 
          type: "text", 
          text: "Failed to place fleet" 
        }],
        correlationId: correlationId
      };
    }
    
    // Skip verification if requested
    if (skipVerification) {
      return {
        content: [{ 
          type: "text", 
          text: `Fleet placement attempted at ${longitude}, ${latitude} with ships: ${ships.join(", ")} and correlation ID: ${correlationId}\n\nVerification skipped. Use get-command-results tool to check the result later.`
        }],
        correlationId: correlationId
      };
    }
    
    // Wait a short time for the game to process the command
    await new Promise(resolve => setTimeout(resolve, SUCCESS_READ_TIMEOUT));
    
    // Check if the command was successful
    const result = await getCommandResult(correlationId);
    
    return {
      content: [{ 
        type: "text", 
        text: `Fleet placement ${result.found ? (result.success ? "succeeded" : "failed") : "attempted"} at ${longitude}, ${latitude} with ships: ${ships.join(", ")} and correlation ID: ${correlationId}${result.found ? "\n\nResult: " + result.result : "\n\nIMPORTANT: You must use the get-command-results tool to verify if this placement was successful!"}`
      }],
      correlationId: correlationId,
      success: result.found ? result.success : undefined
    };
  }
);

// Launch nuke tool
server.tool(
  "launch-nuke", "Launches a nuclear missile from a specified silo to target coordinates. IMPORTANT: Check the response to verify if launch was successful!",
  { 
    siloId: z.string().describe("ID of the silo to launch the nuke from"), 
    targetLongitude: z.number().describe("Target longitude coordinate for the nuclear strike"), 
    targetLatitude: z.number().describe("Target latitude coordinate for the nuclear strike"),
    correlationId: z.number().optional().describe("Optional ID to correlate this command with its result"),
    skipVerification: z.boolean().optional().default(false).describe("Set to true to skip automatic verification (faster for batch operations)")
  },
  async ({ siloId, targetLongitude, targetLatitude, correlationId, skipVerification }) => {
    // Auto-generate correlation ID if not provided
    if (correlationId === undefined) {
      correlationId = ++lastCorrelationId;
    }
    
    let command = `LaunchNukeFromSilo(${siloId}, ${targetLongitude}, ${targetLatitude}) -- ${correlationId}`;
    const success = writeCommandToGame(command);
    
    if (!success) {
      return {
        content: [{ 
          type: "text", 
          text: "Failed to launch nuke" 
        }],
        correlationId: correlationId
      };
    }
    
    // Skip verification if requested
    if (skipVerification) {
      return {
        content: [{ 
          type: "text", 
          text: `Nuke launch attempted from silo ${siloId} to (${targetLongitude}, ${targetLatitude}) with correlation ID: ${correlationId}\n\nVerification skipped. Use get-command-results tool to check the result later.`
        }],
        correlationId: correlationId
      };
    }
    
    // Wait a short time for the game to process the command
    await new Promise(resolve => setTimeout(resolve, SUCCESS_READ_TIMEOUT));
    
    // Check if the command was successful
    const result = await getCommandResult(correlationId);
    
    return {
      content: [{ 
        type: "text", 
        text: `Nuke launch ${result.found ? (result.success ? "succeeded" : "failed") : "attempted"} from silo ${siloId} to (${targetLongitude}, ${targetLatitude}) with correlation ID: ${correlationId}${result.found ? "\n\nResult: " + result.result : "\n\nIMPORTANT: You must use the get-command-results tool to verify if this launch was successful!"}`
      }],
      correlationId: correlationId,
      success: result.found ? result.success : undefined
    };
  }
);

// Move fleet tool
server.tool(
  "move-fleet", "Moves all ships in a fleet to the specified coordinates. IMPORTANT: Check the response to verify if the command was successful!",
  { 
    fleetId: z.string().describe("ID of the fleet to move"),
    longitude: z.number().describe("Target longitude coordinate to move the fleet to"),
    latitude: z.number().describe("Target latitude coordinate to move the fleet to"),
    correlationId: z.number().optional().describe("Optional ID to correlate this command with its result"),
    skipVerification: z.boolean().optional().default(false).describe("Set to true to skip automatic verification (faster for batch operations)")
  },
  async ({ fleetId, longitude, latitude, correlationId, skipVerification }) => {
    // Auto-generate correlation ID if not provided
    if (correlationId === undefined) {
      correlationId = ++lastCorrelationId;
    }
    
    let command = `MoveFleet(${fleetId}, ${longitude}, ${latitude}) -- ${correlationId}`;
    const success = writeCommandToGame(command);
    
    if (!success) {
      return {
        content: [{ 
          type: "text", 
          text: "Failed to move fleet" 
        }],
        correlationId: correlationId
      };
    }
    
    // Skip verification if requested
    if (skipVerification) {
      return {
        content: [{ 
          type: "text", 
          text: `Fleet ${fleetId} movement attempted to (${longitude}, ${latitude}) with correlation ID: ${correlationId}\n\nVerification skipped. Use get-command-results tool to check the result later.`
        }],
        correlationId: correlationId
      };
    }
    
    // Wait a short time for the game to process the command
    await new Promise(resolve => setTimeout(resolve, SUCCESS_READ_TIMEOUT));
    
    // Check if the command was successful
    const result = await getCommandResult(correlationId);
    
    return {
      content: [{ 
        type: "text", 
        text: `Fleet ${fleetId} movement ${result.found ? (result.success ? "succeeded" : "failed") : "attempted"} to (${longitude}, ${latitude}) with correlation ID: ${correlationId}${result.found ? "\n\nResult: " + result.result : "\n\nIMPORTANT: You must use the get-command-results tool to verify if this movement was successful!"}`
      }],
      correlationId: correlationId,
      success: result.found ? result.success : undefined
    };
  }
);

// Set silo to defensive mode tool
server.tool(
  "set-silo-defensive", "Sets a silo to defensive mode to shoot down incoming nuclear missiles. IMPORTANT: Check the response to verify if the command was successful!",
  { 
    siloId: z.string().describe("ID of the silo to set to defensive mode"),
    correlationId: z.number().optional().describe("Optional ID to correlate this command with its result"),
    skipVerification: z.boolean().optional().default(false).describe("Set to true to skip automatic verification (faster for batch operations)")
  },
  async ({ siloId, correlationId, skipVerification }) => {
    // Auto-generate correlation ID if not provided
    if (correlationId === undefined) {
      correlationId = ++lastCorrelationId;
    }
    
    let command = `StopLaunchingNukesFromSiloAndGoDefensive(${siloId}) -- ${correlationId}`;
    const success = writeCommandToGame(command);
    
    if (!success) {
      return {
        content: [{ 
          type: "text", 
          text: "Failed to set silo to defensive mode" 
        }],
        correlationId: correlationId
      };
    }
    
    // Skip verification if requested
    if (skipVerification) {
      return {
        content: [{ 
          type: "text", 
          text: `Silo ${siloId} attempted to set to defensive mode with correlation ID: ${correlationId}\n\nVerification skipped. Use get-command-results tool to check the result later.`
        }],
        correlationId: correlationId
      };
    }
    
    // Wait a short time for the game to process the command
    await new Promise(resolve => setTimeout(resolve, SUCCESS_READ_TIMEOUT));
    
    // Check if the command was successful
    const result = await getCommandResult(correlationId);
    
    return {
      content: [{ 
        type: "text", 
        text: `Silo ${siloId} ${result.found ? (result.success ? "successfully set" : "failed to set") : "attempted to set"} to defensive mode with correlation ID: ${correlationId}${result.found ? "\n\nResult: " + result.result : "\n\nIMPORTANT: You must use the get-command-results tool to verify if this command was successful!"}`
      }],
      correlationId: correlationId,
      success: result.found ? result.success : undefined
    };
  }
);

// Set air unit target tool
server.tool(
  "set-air-target", "Sets the target for an air unit (bomber or fighter)",
  { 
    unitId: z.string().describe("ID of the air unit"),
    targetId: z.string().optional().describe("ID of the target unit (if targeting a unit)"),
    longitude: z.number().optional().describe("Target longitude coordinate (if targeting a location)"),
    latitude: z.number().optional().describe("Target latitude coordinate (if targeting a location)"),
    correlationId: z.number().optional().describe("Optional ID to correlate this command with its result"),
    skipVerification: z.boolean().optional().default(false).describe("Set to true to skip automatic verification")
  },
  async ({ unitId, targetId, longitude, latitude, correlationId, skipVerification }) => {
    if (correlationId === undefined) {
      correlationId = ++lastCorrelationId;
    }
    
    let command = `SetAirUnitTarget(${unitId}, ${targetId || "nil"}, ${longitude || 0}, ${latitude || 0}) -- ${correlationId}`;
    const success = writeCommandToGame(command);
    
    if (!success) {
      return {
        content: [{ type: "text", text: "Failed to set air unit target" }],
        correlationId: correlationId
      };
    }
    
    if (skipVerification) {
      return {
        content: [{ 
          type: "text", 
          text: `Air unit ${unitId} target setting attempted with correlation ID: ${correlationId}\n\nVerification skipped.`
        }],
        correlationId: correlationId
      };
    }
    
    await new Promise(resolve => setTimeout(resolve, SUCCESS_READ_TIMEOUT));
    const result = await getCommandResult(correlationId);
    
    return {
      content: [{ 
        type: "text", 
        text: `Air unit ${unitId} target ${result.found ? (result.success ? "successfully set" : "failed to set") : "attempted to set"} with correlation ID: ${correlationId}${result.found ? "\n\nResult: " + result.result : "\n\nUse get-command-results to verify success."}`
      }],
      correlationId: correlationId,
      success: result.found ? result.success : undefined
    };
  }
);

// Set air unit landing tool
server.tool(
  "set-air-landing", "Sets the landing target for an air unit",
  { 
    unitId: z.string().describe("ID of the air unit"),
    targetId: z.string().describe("ID of the landing target (airbase or carrier)"),
    correlationId: z.number().optional().describe("Optional ID to correlate this command with its result"),
    skipVerification: z.boolean().optional().default(false).describe("Set to true to skip automatic verification")
  },
  async ({ unitId, targetId, correlationId, skipVerification }) => {
    if (correlationId === undefined) {
      correlationId = ++lastCorrelationId;
    }
    
    let command = `SetAirUnitLanding(${unitId}, ${targetId}) -- ${correlationId}`;
    const success = writeCommandToGame(command);
    
    if (!success) {
      return {
        content: [{ type: "text", text: "Failed to set air unit landing target" }],
        correlationId: correlationId
      };
    }
    
    if (skipVerification) {
      return {
        content: [{ 
          type: "text", 
          text: `Air unit ${unitId} landing target setting attempted with correlation ID: ${correlationId}\n\nVerification skipped.`
        }],
        correlationId: correlationId
      };
    }
    
    await new Promise(resolve => setTimeout(resolve, SUCCESS_READ_TIMEOUT));
    const result = await getCommandResult(correlationId);
    
    return {
      content: [{ 
        type: "text", 
        text: `Air unit ${unitId} landing target ${result.found ? (result.success ? "successfully set" : "failed to set") : "attempted to set"} with correlation ID: ${correlationId}${result.found ? "\n\nResult: " + result.result : "\n\nUse get-command-results to verify success."}`
      }],
      correlationId: correlationId,
      success: result.found ? result.success : undefined
    };
  }
);

// Set submarine state tool
server.tool(
  "set-submarine-state", "Sets the state of a submarine (passive sonar, active sonar, or nuke)",
  { 
    unitId: z.string().describe("ID of the submarine"),
    state: z.number().min(0).max(2).describe("State to set (0: passive sonar, 1: active sonar, 2: nuke)"),
    correlationId: z.number().optional().describe("Optional ID to correlate this command with its result"),
    skipVerification: z.boolean().optional().default(false).describe("Set to true to skip automatic verification")
  },
  async ({ unitId, state, correlationId, skipVerification }) => {
    if (correlationId === undefined) {
      correlationId = ++lastCorrelationId;
    }
    
    let command = `SetSubmarineState(${unitId}, ${state}) -- ${correlationId}`;
    const success = writeCommandToGame(command);
    
    if (!success) {
      return {
        content: [{ type: "text", text: "Failed to set submarine state" }],
        correlationId: correlationId
      };
    }
    
    if (skipVerification) {
      return {
        content: [{ 
          type: "text", 
          text: `Submarine ${unitId} state setting attempted with correlation ID: ${correlationId}\n\nVerification skipped.`
        }],
        correlationId: correlationId
      };
    }
    
    await new Promise(resolve => setTimeout(resolve, SUCCESS_READ_TIMEOUT));
    const result = await getCommandResult(correlationId);
    
    return {
      content: [{ 
        type: "text", 
        text: `Submarine ${unitId} state ${result.found ? (result.success ? "successfully set" : "failed to set") : "attempted to set"} with correlation ID: ${correlationId}${result.found ? "\n\nResult: " + result.result : "\n\nUse get-command-results to verify success."}`
      }],
      correlationId: correlationId,
      success: result.found ? result.success : undefined
    };
  }
);

// Helper function to check command results
async function getCommandResult(correlationId) {
  try {
    const gameState = await fs.promises.readFile(OUTPUTFILE, 'utf8');
    const regex = new RegExp(`Command result:.*\\[ID:${correlationId}\\]`, 'g');
    const matches = gameState.match(regex);
    
    if (matches) {
      // Check if the command was successful
      const successMatch = matches[0].match(/SUCCESS/);
      return {
        found: true,
        success: !!successMatch,
        result: matches[0]
      };
    } else {
      return {
        found: false,
        success: false,
        result: "No result found"
      };
    }
  } catch (error) {
    console.error("Error checking command result:", error);
    return {
      found: false,
      success: false,
      result: "Error checking command result"
    };
  }
}

// Get command results by correlation IDs tool
server.tool(
  "get-command-results", "Retrieves the results of previously executed commands by their correlation IDs. Use this to check results when skipVerification=true was used.",
  { correlationIds: z.array(z.number()).describe("List of correlation IDs to retrieve results for") },
  async ({ correlationIds }) => {
    try {
      const gameState = await fs.promises.readFile(OUTPUTFILE, 'utf8');
      const results = {};
      
      // Search for each correlation ID in the output
      for (const id of correlationIds) {
        const regex = new RegExp(`Command result:.*\\[ID:${id}\\]`, 'g');
        const matches = gameState.match(regex);
        if (matches) {
          results[id] = matches;
        } else {
          results[id] = ["No result found"];
        }
      }
      
      return {
        content: [{ 
          type: "text", 
          text: JSON.stringify(results, null, 2)
        }]
      };
    } catch (error) {
      return {
        content: [{ 
          type: "text", 
          text: `Error retrieving command results: ${error.message}` 
        }],
        isError: true
      };
    }
  }
);

// Get latest game state tool
server.tool(
  "get-game-state", "Retrieves the latest game state information",
  {}, // No parameters needed
  async () => {
    try {
      const gameState = await readGameState();
      
      return {
        content: [{ 
          type: "text", 
          text: gameState
        }]
      };
    } catch (error) {
      return {
        content: [{ 
          type: "text", 
          text: `Error retrieving game state: ${error.message}` 
        }],
        isError: true
      };
    }
  }
);

// Generate AI response tool
server.tool(
  "generate-ai-response", "Generates a prompt for an AI model based on the current game state",
  {}, // No parameters needed
  async () => {
    try {
      // Read the current game state
      const gameState = await readGameState();
      
      // Create the prompt for the LLM
      const prompt = [
        ...PROMPTSETTING.initial,
        {"role": "user", "content": gameState},
        ...PROMPTSETTING.prompt
      ];
      
      return {
        content: [{ 
          type: "text", 
          text: JSON.stringify(prompt, null, 2)
        }]
      };
    } catch (error) {
      return {
        content: [{ 
          type: "text", 
          text: `Error generating AI response: ${error.message}` 
        }],
        isError: true
      };
    }
  }
);

// Get events tool
server.tool(
  "get-events", "Retrieves all game events after the specified event ID",
  { 
    fromId: z.number().describe("Event ID to start from (returns all events with ID > fromId)")
  },
  async ({ fromId }) => {
    try {
      const gameState = await fs.promises.readFile(OUTPUTFILE, 'utf8');
      const eventRegex = /Event: (\d+), ([^,]+), Source: ([^(]+) \(([^)]+)\), Target: ([^(]+) \(([^)]+)\), Location: ([^,]+), ([^,\n]+)/g;
      
      const events = [];
      let match;
      while ((match = eventRegex.exec(gameState)) !== null) {
        const eventId = parseInt(match[1]);
        if (eventId > fromId) {
          events.push({
            id: eventId,
            type: match[2],
            source: match[3].trim(),
            sourceType: match[4],
            target: match[5].trim(),
            targetType: match[6],
            longitude: match[7],
            latitude: match[8]
          });
        }
      }
      
      return {
        content: [{ 
          type: "text", 
          text: events.length > 0 ? 
            `Events after ID ${fromId}:\n\n${events.map(e => 
              `Event ${e.id}: ${e.type}, Source: ${e.source} (${e.sourceType}), Target: ${e.target} (${e.targetType}), Location: ${e.longitude}, ${e.latitude}`
            ).join('\n')}` : 
            `No events found after ID ${fromId}`
        }]
      };
    } catch (error) {
      return {
        content: [{ 
          type: "text", 
          text: `Error retrieving events: ${error.message}` 
        }],
        isError: true
      };
    }
  }
);

// Diplomatic tools
server.tool(
  "request-cease-fire", "Requests a cease fire with another team",
  { 
    teamId: z.string().describe("ID of the team to request cease fire with"),
    correlationId: z.number().optional().describe("Optional ID to correlate this command with its result")
  },
  async ({ teamId, correlationId }) => {
    if (correlationId === undefined) {
      correlationId = ++lastCorrelationId;
    }
    
    let command = `RequestCeaseFire(${teamId}) -- ${correlationId}`;
    const success = writeCommandToGame(command);
    
    return {
      content: [{ 
        type: "text", 
        text: success ? `Cease fire request sent to team ${teamId} with correlation ID: ${correlationId}` : "Failed to send cease fire request" 
      }],
      correlationId: correlationId
    };
  }
);

server.tool(
  "request-share-radar", "Requests radar sharing with another team",
  { 
    teamId: z.string().describe("ID of the team to request radar sharing with"),
    correlationId: z.number().optional().describe("Optional ID to correlate this command with its result")
  },
  async ({ teamId, correlationId }) => {
    if (correlationId === undefined) {
      correlationId = ++lastCorrelationId;
    }
    
    let command = `RequestShareRadar(${teamId}) -- ${correlationId}`;
    const success = writeCommandToGame(command);
    
    return {
      content: [{ 
        type: "text", 
        text: success ? `Radar sharing request sent to team ${teamId} with correlation ID: ${correlationId}` : "Failed to send radar sharing request" 
      }],
      correlationId: correlationId
    };
  }
);

server.tool(
  "request-alliance", "Requests to join an alliance",
  { 
    allianceId: z.string().describe("ID of the alliance to request joining"),
    correlationId: z.number().optional().describe("Optional ID to correlate this command with its result")
  },
  async ({ allianceId, correlationId }) => {
    if (correlationId === undefined) {
      correlationId = ++lastCorrelationId;
    }
    
    let command = `RequestAlliance(${allianceId}) -- ${correlationId}`;
    const success = writeCommandToGame(command);
    
    return {
      content: [{ 
        type: "text", 
        text: success ? `Alliance request sent to alliance ${allianceId} with correlation ID: ${correlationId}` : "Failed to send alliance request" 
      }],
      correlationId: correlationId
    };
  }
);

server.tool(
  "send-vote", "Sends a vote for a game event",
  { 
    eventId: z.string().describe("ID of the event to vote on"),
    inFavor: z.boolean().describe("Whether to vote in favor (true) or against (false)"),
    correlationId: z.number().optional().describe("Optional ID to correlate this command with its result")
  },
  async ({ eventId, inFavor, correlationId }) => {
    if (correlationId === undefined) {
      correlationId = ++lastCorrelationId;
    }
    
    let command = `SendVote(${eventId}, ${inFavor ? "true" : "false"}) -- ${correlationId}`;
    const success = writeCommandToGame(command);
    
    return {
      content: [{ 
        type: "text", 
        text: success ? `Vote sent for event ${eventId} (${inFavor ? "in favor" : "against"}) with correlation ID: ${correlationId}` : "Failed to send vote" 
      }],
      correlationId: correlationId
    };
  }
);

// MCP Prompts

// Game state analysis prompt
server.prompt(
  "analyze-game-state",
  {},
  () => {
    return {
      messages: [
        {
          role: "system",
          content: {
            type: "text",
            text: "You are a strategic AI assistant for the game DEFCON. Analyze the current game state and suggest optimal moves."
          }
        },
        {
          role: "user",
          content: {
            type: "text",
            text: `Please analyze the current game state and suggest strategic moves:\n\n${lastGameState}`
          }
        }
      ]
    };
  }
);

// Structure placement prompt
server.prompt(
  "suggest-structure-placement",
  { structureType: z.enum(["RadarStation", "Silo", "AirBase"]) },
  ({ structureType }) => {
    return {
      messages: [
        {
          role: "system",
          content: {
            type: "text",
            text: "You are a strategic AI assistant for the game DEFCON. Suggest optimal structure placements."
          }
        },
        {
          role: "user",
          content: {
            type: "text",
            text: `Based on the current game state, suggest optimal locations to place ${structureType} structures:\n\n${lastGameState}`
          }
        }
      ]
    };
  }
);

// Nuke targeting prompt
server.prompt(
  "suggest-nuke-targets",
  {},
  () => {
    return {
      messages: [
        {
          role: "system",
          content: {
            type: "text",
            text: "You are a strategic AI assistant for the game DEFCON. Suggest optimal nuclear strike targets."
          }
        },
        {
          role: "user",
          content: {
            type: "text",
            text: `Based on the current game state, suggest optimal targets for nuclear strikes:\n\n${lastGameState}`
          }
        }
      ]
    };
  }
);

// Start the MCP server with the appropriate transport
async function startServer() {
  // Initialize game files
  initializeGameFiles();
  
  // Determine which transport to use based on command line arguments
  const args = process.argv.slice(2);
  
  if (args.includes('--http')) {
    // Start with HTTP transport
    const app = express();
    app.use(express.json());
    
    // Map to store transports by session ID
    const transports = {};
    
    // Handle POST requests for client-to-server communication
    app.post('/mcp', async (req, res) => {
      // Check for existing session ID
      const sessionId = req.headers['mcp-session-id'];
      let transport;
      
      if (sessionId && transports[sessionId]) {
        // Reuse existing transport
        transport = transports[sessionId];
      } else if (!sessionId) {
        // New initialization request
        transport = new StreamableHTTPServerTransport({
          sessionIdGenerator: () => randomUUID(),
          onsessioninitialized: (sessionId) => {
            // Store the transport by session ID
            transports[sessionId] = transport;
          }
        });
        
        // Clean up transport when closed
        transport.onclose = () => {
          if (transport.sessionId) {
            delete transports[transport.sessionId];
          }
        };
        
        // Connect to the MCP server
        await server.connect(transport);
      } else {
        // Invalid request
        res.status(400).json({
          jsonrpc: '2.0',
          error: {
            code: -32000,
            message: 'Bad Request: No valid session ID provided',
          },
          id: null,
        });
        return;
      }
      
      // Handle the request
      await transport.handleRequest(req, res, req.body);
    });
    
    // Reusable handler for GET and DELETE requests
    const handleSessionRequest = async (req, res) => {
      const sessionId = req.headers['mcp-session-id'];
      if (!sessionId || !transports[sessionId]) {
        res.status(400).send('Invalid or missing session ID');
        return;
      }
      
      const transport = transports[sessionId];
      await transport.handleRequest(req, res);
    };
    
    // Handle GET requests for server-to-client notifications via SSE
    app.get('/mcp', handleSessionRequest);
    
    // Handle DELETE requests for session termination
    app.delete('/mcp', handleSessionRequest);
    
    const PORT = process.env.PORT || 3000;
    app.listen(PORT, () => {
      log(`DEFCON MCP Server listening on port ${PORT}`);
    });
  } else {
    // Default to stdio transport
    const transport = new StdioServerTransport();
    await server.connect(transport);
    log("DEFCON MCP Server started with stdio transport");
  }
}

startServer().catch(error => {
  console.error("Failed to start MCP server:", error);
  process.exit(1);
});

function log(message) {
  // log to standard ERROR
  if (typeof message !== 'string') {
    message = JSON.stringify(message);
  }
  //console.error(`[Defcon MCP Server] ${message}`);
}