/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#ifndef DEDCON_RCONServer_INCLUDED
#define DEDCON_RCONServer_INCLUDED

#include <string>
#include <map>
#include <streambuf>
#include <ostream>
#include <vector>
#include "Network\Socket.h"
#include "GameSettings\Settings.h"
#include "Time.h"

class SettingsReader;

class RCONClient
{
public:
    RCONClient                                                      ( struct sockaddr_in addr );
    ~RCONClient                                                     ();
    bool IsAuthenticated                                            () const { return authenticated; }
    void SetAuthenticated                                           ( bool auth ) { authenticated = auth; }

    Time lastActivity;
    struct sockaddr_in clientAddr;
    std::string outputBuffer;

    bool wantsGameEvents;
    bool wantsServerLog;

private:
    bool authenticated;
};

class RCONServer
{
public:
    static RCONServer & GetInstance                                 ();
    bool Initialize                                                 ();
    void ProcessCommands                                            ();
    void Shutdown                                                   ();
    void BroadcastGameEvent                                         ( const std::string & message );
    void BroadcastServerLog                                         ( const std::string & message );

private:
    RCONServer                                                      ();
    ~RCONServer                                                     ();
    RCONServer                                                      ( const RCONServer & );
    RCONServer & operator=                                          ( const RCONServer & );
    void ProcessCommand                                             ( const std::string & command, const struct sockaddr_in & clientAddr );
    std::string ExecuteCommand                                      ( const std::string & command );
    void SendResponse                                               ( RCONClient * client, int statusCode, const std::string & message );
    void SendLogMessage                                             ( RCONClient * client, const std::string & logType, const std::string & message );
    RCONClient * FindClient                                         ( const struct sockaddr_in & addr );
    RCONClient * CreateClient                                       ( const struct sockaddr_in & addr );
    bool IsIPLockedOut                                              ( const struct sockaddr_in & addr );
    void RecordFailedLogin                                          ( const struct sockaddr_in & addr );
    std::string IPToString                                          ( const struct sockaddr_in & addr );
    void CleanupIdleClients                                         ();

    bool initialized;
    Socket rconSocket;
    std::map<std::string, RCONClient *> clients;
    std::map<std::string, int> failedLoginAttempts;
    static const int MAX_FAILED_ATTEMPTS = 5;
    SettingsReader * settingsReader;
};

// buffer stream to output to RCON
class RCONLogStreambuf: public std::streambuf
{
public:
    RCONLogStreambuf                                                ( RCONServer * server, const std::string & logType );

protected:
    virtual int overflow                                            ( int c ) override;
    virtual std::streamsize xsputn                                  ( const char * s, std::streamsize n ) override;

private:
    RCONServer * server;
    std::string logType;
    std::string buffer;
};

// configuration settings
extern IntegerSetting rconEnabled;   // enable/disable RCON functionality (0=disabled, 1=enabled)
extern IntegerSetting rconPort;      // static port assigned by the user in the config
extern IntegerSetting rconStartPort; // start of the port range
extern IntegerSetting rconEndPort;   // end of the port range
extern StringSetting rconPassword;   // password

#endif // DEDCON_RCONServer_INCLUDED