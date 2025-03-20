/**
 * Defcon Expanded Server - Main Entry Point
 * 
 * This file serves as the entry point for the Defcon Expanded server application.
 * It initializes middleware, database connections, and sets up API routes.
 */

const express = require('express');
const http = require('http');
const io = require('socket.io');
const fs = require('fs');
const path = require('path');

// Import utilities
const utils = require('./Utils/shared-utils');
const { setupRoutes } = require('./Utils/route-setup');
const { errorMiddleware } = require('./Utils/error-handler');

// Import middleware setup
const middlewareSetup = require('./Middleware/middleware');

// Import core functionality
const db = require('./Functions/database-connection');
const discordFunctions = require('./Functions/discord-functions');

// Application constants
const startTime = Date.now();
const PORT = process.env.PORT || 3000;

// Initialize Express app
const app = express();
const server = http.createServer(app);
const socketIO = io(server);

// Create log stream - adjusted path to go up one directory level
const logStream = fs.createWriteStream(path.join(__dirname, '..', 'server.log'), { flags: 'a' });

// Setup middleware
middlewareSetup.setupMiddleware(app);

// Setup routes
setupRoutes(app);

// Socket.IO connection handler
socketIO.on('connection', (socket) => {
  console.log('A user connected to console output');
});

// Override console.log to broadcast to Socket.IO
const originalLog = console.log;
console.log = function () {
  const timestamp = utils.formatTimestamp(new Date());
  const args = Array.from(arguments);
  const message = `[${timestamp}] ${args.join(' ')}`;

  logStream.write(message + '\n');

  socketIO.emit('console_output', message);

  originalLog.apply(console, [timestamp, ...args]);
};

// Graceful shutdown handler
process.on('SIGINT', async () => {
  console.log('Shutting down server...');
  discordFunctions.shutdownDiscordBot();

  logStream.end(() => {
    console.log('Log file stream closed.');
    process.exit(0);
  });
});

// Initialize core components and start server
async function startServer() {
  try {
    // Check database connection
    const dbConnected = await db.checkConnection();
    if (!dbConnected) {
      console.error('Failed to connect to database. Server starting with limited functionality.');
    }

    // Initialize Discord bot
    discordFunctions.initializeDiscordBot();

    // Start the server
    server.listen(PORT, async () => {
      console.log(`Defcon Expanded Demo and File Server listening at http://localhost:${PORT}`);
      console.log(`Server started at: ${new Date().toISOString()}`);
      console.log(`Environment: ${process.env.NODE_ENV || 'development'}`);
      console.log(`Node.js version: ${process.version}`);
      console.log(`Platform: ${process.platform}`);

      try {
        // Setup file watchers and process pending demos
        discordFunctions.setupFileWatchers();
        await discordFunctions.initializeServer();
        console.log("Server initialization completed successfully.");
      } catch (error) {
        console.error("Error during server initialization:", error);
      }
    });
  } catch (error) {
    console.error('Failed to start server:', error);
    process.exit(1);
  }
}

// Start the server
startServer();