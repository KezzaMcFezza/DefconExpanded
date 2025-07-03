# DEFCON MCP Server

This is a [Model Context Protocol (MCP)](https://modelcontextprotocol.io/introduction) server for Introversion Software's 2007 game [DEFCON](https://store.steampowered.com/app/1520/DEFCON/), which allows your favourite AI tool to play GLOBAL THERMONUCLEAR WAR.

![](./docs/assets/screenshot_1.png)
![](./docs/assets/screenshot_2.png)

(work in progress; not all commands are available yet!)

## Setting up

1. Install DEFCON. Make sure to start it once to load your CD key.
2. Download the [DEFCON AI API](https://defconexpanded.com/api/download-mod/88) and extract v1.57 to your DEFCON installation folder. You may want to rename the AI API .exe to `DEFCON_ai.exe`.
3. Download [LUABOT](https://defconexpanded.com/api/download-mod/93) and extract it to `<defcon installation folder>/AI`; rename the folder so that you have `<defcon installation folder>/AI/luabot/luabot.dll`.
4. Git clone this repository into the `luabot` folder.
5. The MCP server writes to an input text file, which the Lua bot reads. The lua bot then writes to an output text file which is read by the MCP server. You will have to set the file paths yourself; they're hardcoded to write to `R:\input.txt` and `R:\output.txt` by default, but you can supply `inputfile` and/or `outputfile` CLI arguments to Defcon to override tis. A RAMDisk is recommended if you can set one up.
6. Install dependencies with `npm ci` in this repository folder.
7. Start the game via `.\Defcon_ai.exe host nowan nolan nowlan luabot="AI\luabot\main.lua" numplayers=2 territory=0 debug inputfile="D://input.txt" outputfile="D://output.txt"`. Select `AI/luabot/luabot.dll` as the "external bot", and then add an internal AI player as well. The game should start automatically; if it doesn't just ready up to start!
8. Configure your MCP-enabled LLM interface to start the MCP server via `node mcp-server.mjs` (for stdio transport) or `node mcp-server.mjs --http` (for HTTP transport). You can pass `--inputfile="D://input.txt"` and `--outputfile="D://output.txt"` or similar to set the location of input/output text files.
9. Watch the carnage unfold!

## Architecture

The DEFCON MCP Server uses a multi-layered architecture to enable AI-powered gameplay:

1. **Game Layer**: DEFCON game running with the AI API extension
2. **Lua Bridge**: A Lua bot (`main.lua`) that interfaces with the game through the DEFCON AI API
3. **File I/O Layer**: Communication between the Lua bot and MCP server via text files
   - `input.txt`: Commands from MCP server to the game
   - `output.txt`: Game state information from the game to the MCP server
4. **MCP Server Layer**: Node.js server (`mcp-server.mjs`) implementing the Model Context Protocol
   - Provides standardized tools, resources, and prompts for LLM interaction
   - Supports both stdio and HTTP transport methods
5. **LLM Integration Layer**: Your AI tool connects to the MCP server to analyze game state and issue commands

### Data Flow

1. The Lua bot reads the game state and writes it to `output.txt`
2. The MCP server reads `output.txt` to get the current game state
3. The LLM analyzes the game state and decides on actions
4. The MCP server writes commands to `input.txt`
5. The Lua bot reads `input.txt` and executes commands in the game

### Command Correlation

The system uses correlation IDs to track command execution:
- Each command sent to the game includes a unique ID
- Command results are tagged with the same ID
- This allows the MCP server to verify if commands were executed successfully

## Available MCP Tools

The MCP server provides the following tools:

- `debug-log`: Send a debug message to the game log
- `send-chat`: Send a chat message to opponents
- `place-structure`: Place a structure (RadarStation, Silo, or AirBase) at specified coordinates
- `place-fleet`: Place a fleet of ships at specified coordinates
- `move-fleet`: Move a fleet to specified coordinates
- `whiteboard-draw`: Draw a line on the whiteboard
- `whiteboard-clear`: Clear all lines from the whiteboard
- `launch-nuke`: Launch a nuclear missile from a silo to target coordinates
- `set-silo-defensive`: Set a silo to defensive mode
- `get-command-results`: Retrieve results of previously executed commands
- `get-game-state`: Get the latest game state information
- `generate-ai-response`: Generate an AI response based on the current game state

## Available MCP Resources

The MCP server provides the following resources:

- `defcon://game-state`: Get the current game state
- `defcon://level`: Get the current DEFCON level

## Available MCP Prompts

The MCP server provides the following prompts:

- `analyze-game-state`: Analyze the current game state and suggest optimal moves
- `suggest-structure-placement`: Suggest optimal locations for structure placement
- `suggest-nuke-targets`: Suggest optimal targets for nuclear strikes