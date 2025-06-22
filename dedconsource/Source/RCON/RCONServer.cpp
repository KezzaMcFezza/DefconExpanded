/*
 *  * DedCon RCON Client: Remote Access for Dedcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  * Copyright (C) 2025 Keiron Mcphee
 *  *
 *  */

#include "RCONServer.h"
#include "Main/Log.h"
#include "GameSettings/Settings.h"
#include "Lib/FakeEncryption.h"
#include "RealEncryption.h"
#include <sstream>
#include <iostream>
#include <string.h>
#include <algorithm>

// configuration settings
IntegerSetting rconEnabled(Setting::Admin, "RCONEnabled", 0);
IntegerSetting rconPort(Setting::Admin, "RCONPort", 8800); // static port assignment, can be set manually inside the configfiles

// RCON port range
// 100 port range should serve everyone, i would fall off my chair if someone somehow was able to exceed 100 servers ever
IntegerSetting rconStartPort(Setting::Admin, "RCONStartPort", 8800); // beginning
IntegerSetting rconEndPort(Setting::Admin, "RCONEndPort", 8900); // end
SecretStringSetting rconPassword(Setting::Admin, "RCONPassword", "");
IntegerSetting rconTimeoutWindow(Setting::Admin, "RCONTimeoutWindow", 300); // timeout in seconds, default 5 minutes
IntegerSetting rconEncryption(Setting::Admin, "RCONEncryption", 1); // encryption enabled by default, only reason to turn it off is for testing or the user is having issues with encryption

// turns out that GCC and Clang do not initialize static const members in the class definition
// so we need to do it outside of the class definition
const int RCONServer::MAX_FAILED_ATTEMPTS;

// client implementation
RCONClient::RCONClient(struct sockaddr_in addr)
    : authenticated(false), clientAddr(addr), wantsGameEvents(false), wantsServerLog(false)
{
    lastActivity.GetTime(); // get current time
}

RCONClient::~RCONClient()
{
}

// RCONServer implementation
RCONServer::RCONServer()
    : initialized(false), settingsReader(NULL)
{
}

RCONServer::~RCONServer()
{
    Shutdown(); // describes itself
}

RCONServer &RCONServer::GetInstance()
{
    static RCONServer instance;
    return instance;
}

bool RCONServer::Initialize()
{
    if (initialized)
    {
        return true;
    }

    // are we enabled?
    if (rconEnabled.Get() <= 0)
    {
        return false;
    }

    // Check if password is set
    if (rconPassword.Get().empty())
    {
        Log::Err() << "ERROR: RCON password is not set! RCON functionality will be DISABLED.\n";
        Log::Err() << "       Set RCONPassword in your config file to enable RCON.\n";
        return false; // refuse to initilise with empty password
    }
    else
    {
        // only initialize encryption if its enabled
        if (rconEncryption.Get() > 0)
        {
            if (!initServerEncryption(rconPassword.Get()))
            {
                Log::Err() << "Failed to initialize encryption with the provided password\n";
            }
            else
            {
                Log::Out() << "RCON encryption enabled\n";
            }
        }
        else
        {
            Log::Out() << "RCON encryption disabled\n";
        }
    }

    // try the specific port if set
    int specificPort = rconPort.Get();
    if (specificPort > 0)
    {
        rconSocket.Open();
        rconSocket.SetPort(specificPort);

        if (rconSocket.Bind())
        {
            initialized = true;
            int timeoutSecs = rconTimeoutWindow.Get();
            if (timeoutSecs <= 0)
                Log::Out() << "RCON server listening on port " << specificPort << " (timeouts disabled)\n";
            else
                Log::Out() << "RCON server listening on port " << specificPort << " (timeout: " << timeoutSecs << " seconds)\n";

            // set up log streaming to fix blocking the serverlog stream, so creating a third stream might not be an elegant fix but it works :)
            static RCONLogStreambuf gameEventBuf(this, "GAMEEVENT");
            static RCONLogStreambuf serverLogBuf(this, "SERVERLOG");
            static std::ostream gameEventStream(&gameEventBuf);
            static std::ostream serverLogStream(&serverLogBuf);

            // use third stream instead of replacing the secondary stream
            Log::Event().GetWrapper().SetRemoteStream(&gameEventStream);
            Log::Out().GetWrapper().SetRemoteStream(&serverLogStream);
            Log::Err().GetWrapper().SetRemoteStream(&serverLogStream);

            return true;
        }

        rconSocket.Close();
        Log::Err() << "Failed to bind RCON server to specified port " << specificPort << "\n";
    }

    // if no port is set inside the configuration file, lets dynamically set a port
    for (int port = rconStartPort.Get(); port <= rconEndPort.Get(); port++)
    {
        rconSocket.Open();
        rconSocket.SetPort(port);

        if (rconSocket.Bind())
        {
            initialized = true;
            int timeoutSecs = rconTimeoutWindow.Get();
            if (timeoutSecs <= 0)
                Log::Out() << "RCON server listening on port " << port << " (from range, timeouts disabled)\n";
            else
                Log::Out() << "RCON server listening on port " << port << " (from range, timeout: " << timeoutSecs << " seconds)\n";

            static RCONLogStreambuf gameEventBuf(this, "GAMEEVENT");
            static RCONLogStreambuf serverLogBuf(this, "SERVERLOG");
            static std::ostream gameEventStream(&gameEventBuf);
            static std::ostream serverLogStream(&serverLogBuf);

            Log::Event().GetWrapper().SetRemoteStream(&gameEventStream);
            Log::Out().GetWrapper().SetRemoteStream(&serverLogStream);
            Log::Err().GetWrapper().SetRemoteStream(&serverLogStream);

            return true;
        }

        // close socket and try next port
        rconSocket.Close();
    }

    // if the user has already a port in use by another program, or by another server...
    Log::Err() << "Failed to initialize RCON server: couldn't bind to any port\n";
    return false;
}

void RCONServer::Shutdown()
{
    if (initialized)
    {
        rconSocket.Close();

        for (std::map<std::string, RCONClient *>::iterator it = clients.begin();
             it != clients.end(); ++it)
        {
            delete it->second;
        }
        clients.clear();

        failedLoginAttempts.clear();

        initialized = false;
    }
}

void RCONServer::ProcessCommands()
{
    if (!initialized)
        return;

    char buffer[4096];
    struct sockaddr_in fromAddr;
    int bytesRead = rconSocket.RecvFrom(fromAddr, buffer, sizeof(buffer) - 1);

    if (bytesRead > 0)
    {
        std::string command;
        bool decryptionSuccess = false;

        // check if encryption is enabled
        if (rconEncryption.Get() > 0)
        {
            // Check if the packet is encrypted
            if (RealEncryption::isEncryptedPacket(reinterpret_cast<const uint8_t *>(buffer), bytesRead))
            {
                if (g_serverEncryption)
                {
                    std::vector<uint8_t> decrypted;
                    if (g_serverEncryption->decrypt(
                            reinterpret_cast<const uint8_t *>(buffer),
                            bytesRead,
                            decrypted))
                    {
                        command = std::string(decrypted.begin(), decrypted.end());
                        decryptionSuccess = true;
                    }
                    else
                    {
                        Log::Err() << "Failed to decrypt packet from " << IPToString(fromAddr) << "\n";
                        return; // encryption is required, so reject unencrypted packets
                    }
                }
            }
            else
            {
                // if encryption is required but packet is not encrypted then reject it
                Log::Err() << "Rejected unencrypted packet from " << IPToString(fromAddr) << " (encryption required)\n";
                return;
            }
        }
        else
        {
            // encryption is disabled so use plain text
            buffer[bytesRead] = '\0';
            command = buffer;
            decryptionSuccess = true;
        }
        
        if (decryptionSuccess) {
            ProcessCommand(command, fromAddr);
        }
    }

    CleanupIdleClients();
}

// cleanup clients that do not disconnect correctly
void RCONServer::CleanupIdleClients()
{
    // use configurable timeout window
    int timeoutSeconds = rconTimeoutWindow.Get();
    
    // if timeout is set to 0 or negative, disable client timeout
    if (timeoutSeconds <= 0)
    {
        return;
    }
    
    Time currentTime;
    currentTime.GetTime();

    std::map<std::string, RCONClient *>::iterator it = clients.begin();
    while (it != clients.end())
    {
        RCONClient *client = it->second;

        // check if client has been idle for too long
        Time idleTime = currentTime - client->lastActivity;
        if (idleTime.Seconds() > timeoutSeconds)
        {
            Log::Out() << "RCON client timed out: " << it->first << "\n";
            delete client;
            it = clients.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

std::string RCONServer::IPToString(const struct sockaddr_in &addr)
{
    char ipStr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(addr.sin_addr), ipStr, INET_ADDRSTRLEN);

    std::stringstream ss;
    ss << ipStr << ":" << ntohs(addr.sin_port);
    return ss.str();
}

bool RCONServer::IsIPLockedOut(const struct sockaddr_in &addr)
{
    std::string ipStr = IPToString(addr);

    // check if the IP has too many failed attempts
    std::map<std::string, int>::iterator it = failedLoginAttempts.find(ipStr);
    if (it != failedLoginAttempts.end() && it->second >= MAX_FAILED_ATTEMPTS)
    {
        return true;
    }

    return false;
}

void RCONServer::RecordFailedLogin(const struct sockaddr_in &addr)
{
    std::string ipStr = IPToString(addr);

    // increment the counter
    failedLoginAttempts[ipStr]++;

    int attempts = failedLoginAttempts[ipStr];
    Log::Err() << "Failed RCON authentication attempt from " << ipStr
               << " (" << attempts << "/" << MAX_FAILED_ATTEMPTS << ")\n";

    // lockout the user from signing in again until the dedcon client restarts
    if (attempts >= MAX_FAILED_ATTEMPTS)
    {
        Log::Err() << "RCON access from " << ipStr << " locked until server restart due to too many failed attempts\n";
    }
}

RCONClient *RCONServer::FindClient(const struct sockaddr_in &addr)
{
    std::string ipStr = IPToString(addr);

    std::map<std::string, RCONClient *>::iterator it = clients.find(ipStr);
    if (it != clients.end())
    {
        // still update the activity time to track when the client was last active
        it->second->lastActivity.GetTime();
        return it->second;
    }

    return NULL;
}

RCONClient *RCONServer::CreateClient(const struct sockaddr_in &addr)
{
    std::string ipStr = IPToString(addr);

    RCONClient *client = new RCONClient(addr);
    clients[ipStr] = client;

    Log::Out() << "New RCON connection from " << ipStr << "\n";

    return client;
}

void RCONServer::ProcessCommand(const std::string &command, const struct sockaddr_in &clientAddr)
{
    std::string ipStr = IPToString(clientAddr);

    // check if the user is locked out
    if (IsIPLockedOut(clientAddr))
    {
        // create a temporary client just for the response
        RCONClient tempClient(clientAddr);
        SendResponse(&tempClient, 403, "Too many failed authentication attempts. Access locked until server restart.");
        return;
    }

    RCONClient *client = FindClient(clientAddr);
    if (!client)
    {
        client = CreateClient(clientAddr);
    }

    if (command.substr(0, 5) == "AUTH ")
    {
        std::string password = command.substr(5);
        
        // handle empty passwords - always reject them for security
        if (password.empty() || password == "<empty>")
        {
            SendResponse(client, 401, "Empty passwords are not allowed");
            Log::Err() << "Rejected empty password authentication attempt from " << ipStr << "\n";
            RecordFailedLogin(clientAddr);
            return;
        }

        // make sure the password is correct
        if (password == rconPassword.Get())
        {
            client->SetAuthenticated(true);
            SendResponse(client, 200, "Authentication successful");
            Log::Out() << "RCON authentication successful from " << ipStr << "\n";

            // clear the failed attempts
            failedLoginAttempts[ipStr] = 0;
        }
        else
        {
            SendResponse(client, 401, "Authentication failed: Invalid password");

            // log the failed attempt
            RecordFailedLogin(clientAddr);
        }
    }
    else if (command == "STARTGAMELOG")
    {
        if (!client->IsAuthenticated())
        {
            SendResponse(client, 401, "Authentication required");
            return;
        }

        client->wantsGameEvents = true;
        SendResponse(client, 200, "Game event logging started");
        Log::Out() << "RCON game event logging started for " << ipStr << "\n";
    }
    else if (command == "STOPGAMELOG")
    {
        client->wantsGameEvents = false;
        SendResponse(client, 200, "Game event logging stopped");
    }
    else if (command == "STARTSERVERLOG")
    {
        if (!client->IsAuthenticated())
        {
            SendResponse(client, 401, "Authentication required");
            return;
        }

        client->wantsServerLog = true;
        SendResponse(client, 200, "Server logging started");
        Log::Out() << "RCON server logging started for " << ipStr << "\n";
    }
    else if (command == "STOPSERVERLOG")
    {
        client->wantsServerLog = false;
        SendResponse(client, 200, "Server logging stopped");
    }
    // process the disconnect command for clean disconnects
    else if (command == "DISCONNECT")
    {
        if (client->IsAuthenticated())
        {
            SendResponse(client, 200, "Disconnected successfully");
            Log::Out() << "RCON client " << ipStr << " disconnected\n";
            
            std::string clientKey = IPToString(client->clientAddr);
            delete client;
            clients.erase(clientKey);
        }
        else
        {
            SendResponse(client, 401, "Authentication required");
        }
    }
    else if (command.substr(0, 5) == "EXEC ")
    {
        // execute a command, such as /set hostname or /set maxteams
        if (!client->IsAuthenticated())
        {
            SendResponse(client, 401, "Authentication required");
            return;
        }

        // parse the command
        std::string serverCommand = command.substr(5);
        // log it
        Log::Out() << "RCON command from " << ipStr << ": " << serverCommand << "\n";
        // execute
        std::string result = ExecuteCommand(serverCommand);

        SendResponse(client, 200, result);
    }
    else
    {
        SendResponse(client, 400, "Unknown command");
    }
}

std::string RCONServer::ExecuteCommand(const std::string &command)
{
    SettingsReader &settingsReader = SettingsReader::GetSettingsReader();

    std::stringstream output;

    if (command == "quit")
    {
        // handle the quit command
        output << "Server restarting...";
        settingsReader.AddLine("RCON", "QUIT");
    }
    else if (command.substr(0, 4) == "/set")
    {
        // extract the GameSetting from the command
        if (command.length() > 5)
        {
            std::string settingCommand = command.substr(5); // skip /set

            settingsReader.AddLine("RCON", settingCommand);
            output << "Setting updated: " << settingCommand;
        }
        else
        {
            output << "Invalid /set command format. Use /set <name> <value>";
        }
    }
    else
    {
        settingsReader.AddLine("RCON", command);
        output << "Command executed: " << command;
    }

    return output.str();
}

void RCONServer::SendResponse(RCONClient *client, int statusCode, const std::string &message)
{
    std::stringstream response;
    response << "STATUS " << statusCode << "\nMESSAGE " << message << "\n";
    std::string responseStr = response.str();

    if (rconEncryption.Get() > 0)
    {
        if (g_serverEncryption)
        {
#ifdef DEBUG
            Log::Out() << "Sending encrypted response to " << IPToString(client->clientAddr) << "\n";
#endif
            std::vector<uint8_t> encrypted;
            if (g_serverEncryption->encrypt(
                    reinterpret_cast<const uint8_t *>(responseStr.c_str()),
                    responseStr.length(),
                    encrypted))
            {
                // Send the encrypted response
                if (rconSocket.SendTo(client->clientAddr,
                                  reinterpret_cast<const char *>(encrypted.data()),
                                  encrypted.size(),
                                  NO_ENCRYPTION) < 0)
                {
                    Log::Err() << "Failed to send encrypted response to " << IPToString(client->clientAddr) << "\n";
                }
#ifdef DEBUG
                else
                {
                    Log::Out() << "Encrypted response sent successfully to " << IPToString(client->clientAddr) << "\n";
                }
#endif
            }
            else
            {
                Log::Err() << "Failed to encrypt response for " << IPToString(client->clientAddr) << "\n";
            }
        }
        else
        {
            Log::Err() << "Encryption is enabled but not initialized properly\n";
        }
    }
    else
    {
        // send unencrypted response
#ifdef DEBUG
        Log::Out() << "Sending unencrypted response to " << IPToString(client->clientAddr) << "\n";
#endif
        if (rconSocket.SendTo(client->clientAddr, responseStr.c_str(), responseStr.length(), NO_ENCRYPTION) < 0)
        {
            Log::Err() << "Failed to send unencrypted response to " << IPToString(client->clientAddr) << "\n";
        }
#ifdef DEBUG
        else
        {
            Log::Out() << "Unencrypted response sent successfully to " << IPToString(client->clientAddr) << "\n";
        }
#endif
    }
}

void RCONServer::SendLogMessage(RCONClient *client, const std::string &logType, const std::string &message)
{
    std::stringstream response;
    response << "LOG " << logType << " " << message;

    std::string responseStr = response.str();

    if (rconEncryption.Get() > 0)
    {
        if (g_serverEncryption)
        {
            std::vector<uint8_t> encrypted;
            if (g_serverEncryption->encrypt(
                    reinterpret_cast<const uint8_t *>(responseStr.c_str()),
                    responseStr.length(),
                    encrypted))
            {
                rconSocket.SendTo(client->clientAddr,
                              reinterpret_cast<const char *>(encrypted.data()),
                              encrypted.size(),
                              NO_ENCRYPTION);
            }
        }
    }
    else
    {
        rconSocket.SendTo(client->clientAddr, responseStr.c_str(), responseStr.length(), NO_ENCRYPTION);
    }
}

void RCONServer::BroadcastGameEvent(const std::string &message)
{
    for (std::map<std::string, RCONClient *>::iterator it = clients.begin();
         it != clients.end(); ++it)
    {
        RCONClient *client = it->second;
        if (client->IsAuthenticated() && client->wantsGameEvents)
        {
            SendLogMessage(client, "GAMEEVENT", message);
        }
    }
}

void RCONServer::BroadcastServerLog(const std::string &message)
{
    for (std::map<std::string, RCONClient *>::iterator it = clients.begin();
         it != clients.end(); ++it)
    {
        RCONClient *client = it->second;
        if (client->IsAuthenticated() && client->wantsServerLog)
        {
            SendLogMessage(client, "SERVERLOG", message);
        }
    }
}

RCONServer::RCONServer(const RCONServer &)
{
}

RCONServer &RCONServer::operator=(const RCONServer &)
{
    return *this;
}

RCONLogStreambuf::RCONLogStreambuf(RCONServer *server, const std::string &logType)
    : server(server), logType(logType)
{
}

int RCONLogStreambuf::overflow(int c)
{
    if (c != EOF)
    {
        buffer += static_cast<char>(c);
        if (c == '\n')
        {
            if (logType == "GAMEEVENT")
            {
                server->BroadcastGameEvent(buffer);
            }
            else if (logType == "SERVERLOG")
            {
                server->BroadcastServerLog(buffer);
            }
            buffer.clear();
        }
    }
    return c;
}

std::streamsize RCONLogStreambuf::xsputn(const char *s, std::streamsize n)
{
    for (std::streamsize i = 0; i < n; ++i)
    {
        overflow(s[i]);
    }
    return n;
}