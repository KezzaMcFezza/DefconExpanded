# DedconRCON Framework

A simple framework for integrating RCON communication with Dedcon servers into web applications.

This package includes a basic implementation for connecting to Dedcon servers through a web application and is meant to be a starting point.
You have permission to use all of these resources i have provided and modify them to your liking, i decided to use node.js for the backend
implementation however this is due to my preference. If you are a developer you will not find it hard to integrate this with your own website
using PHP or Django.

## Installation

1. Make sure you have Node.js installed (v14+ recommended)
2. Clone or download this repository
3. Install dependencies:

```bash
npm install
```

## Running the Server

Start the server with:

```bash
npm start
```

Or

```bash
node server.js
```

The server will run on http://localhost:3000 by default unless you create an Apache or Nginx configuration.

## Server Configuration

Before using this RCON console, make sure your Dedcon server has RCON enabled:

Add these lines to your server config file:

- `RCONEnabled 1`
- `RCONPort 8800` The RCON port is configurable between 8800 and 8850
- `RCONPassword your-secure-password`

## Common RCON Commands

- `ServerName "New Name"` - Change server name
- `MaxTeams <number>` - Set maximum player slots
- `MinTeams <number>` - Set minimum players to start
- `ServerPassword <password>` - Set server password
- `quit` - Restart the server (this will also disconnect your session to be sure to reconnect)

## Security Considerations

- This framework is designed for local use, if you unleash this onto the open internet there could be some issues without some real authentication
- HTTPS is necessary since you dont want hackers to sniff out the password in string format
- And of course the rcon password must be good, if someone gains access to your server they can unleash hell on it

## Project Structure

- `Server.js` - Node.js server implementation
- `public/index.html` - Web interface
- `public/main.css` - CSS styles
- `public/index.js` - Frontend JavaScript

## Contact

Any problems or issues you encounter with this implementation or just DedconRCON itself dont hesitate to contact me at

keiron.mcphee1@gmail.com