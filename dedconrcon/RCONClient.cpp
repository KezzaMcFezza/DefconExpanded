/*
 *  * DedCon RCON Client: Remote Access for Dedcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  * Copyright (C) 2025 Keiron Mcphee
 *  *
 *  */

 #include "RCONClient.h"

 bool streamGameEvents = false;
 bool streamServerLog = false;
 std::atomic<bool> running( true );
 std::mutex outputMutex;
 
 void cleanup()
 {
 #ifdef _WIN32
     WSACleanup();
 #endif
 }
 
 bool initialize()
 {
 #ifdef _WIN32
     WSADATA wsaData;
     int result = WSAStartup( MAKEWORD( 2, 2 ), &wsaData );
     if ( result != 0 )
     {
         std::cerr << "WSAStartup failed: " << result << std::endl;
         return false;
     }
 #endif
     return true;
 }
 
 // socket timeout for windows and maxos/linux
 bool setSocketTimeout( SOCKET sock, int timeoutMs )
 {
 #ifdef _WIN32
     DWORD timeout = timeoutMs;
     if ( setsockopt( sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof( timeout ) ) == SOCKET_ERROR )
     {
         std::cerr << "Failed to set socket timeout: " << SOCKET_ERROR_CODE << std::endl;
         return false;
     }
 #else
     struct timeval tv;
     tv.tv_sec = timeoutMs / 1000;
     tv.tv_usec = ( timeoutMs % 1000 ) * 1000;
     if ( setsockopt( sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof( tv ) ) == SOCKET_ERROR )
     {
         std::cerr << "Failed to set socket timeout: " << SOCKET_ERROR_CODE << std::endl;
         return false;
     }
 #endif
     return true;
 }
 
 // resolve hostname to IP address, this will make it easier for server hosters who also have a website
 bool resolveHostname( const char * hostname, struct in_addr * addr )
 {
     struct addrinfo hints, *result;
     memset( &hints, 0, sizeof( hints ) );
     hints.ai_family = AF_INET; // IPv4
     hints.ai_socktype = SOCK_DGRAM; // UDP
 
     int status = getaddrinfo( hostname, NULL, &hints, &result );
     if ( status != 0 )
     {
 #ifdef _WIN32
         std::cerr << "Failed to resolve hostname: " << WSAGetLastError() << std::endl;
 #else
         std::cerr << "Failed to resolve hostname: " << gai_strerror( status ) << std::endl;
 #endif
         return false;
     }
 
     struct sockaddr_in * ipv4 = (struct sockaddr_in *)result->ai_addr;
     *addr = ipv4->sin_addr;
 
     freeaddrinfo( result );
     return true;
 }
 
 bool authenticate( SOCKET sock, const struct sockaddr_in & serverAddr, const std::string & password )
 {
     // send authentication command
     std::string authCommand = "AUTH " + password;
 #ifdef DEBUG
     std::cout << "[DEBUG] Sending auth command: " << authCommand << std::endl;
 #endif
     if ( sendto( sock, authCommand.c_str(), authCommand.length(), 0, (struct sockaddr *)&serverAddr, sizeof( serverAddr ) ) == SOCKET_ERROR )
     {
         std::cerr << "Failed to send auth command: " << SOCKET_ERROR_CODE << std::endl;
         return false;
     }
 
     char buffer[4096];
     struct sockaddr_in fromAddr;
     socklen_t fromLen = sizeof( fromAddr );
 
     int received = recvfrom( sock, buffer, sizeof( buffer ) - 1, 0, (struct sockaddr *)&fromAddr, &fromLen );
     if ( received == SOCKET_ERROR )
     {
         std::cerr << "Failed to receive auth response: " << SOCKET_ERROR_CODE << std::endl;
         return false;
     }
 
     // null-terminate and check authentication status
     buffer[received] = '\0';
     
 #ifdef DEBUG
     std::cout << "[DEBUG] Auth response: " << buffer << std::endl;
 #endif
 
     // Don't print raw response, just check if successful
     bool isSuccess = ( strstr( buffer, "STATUS 200" ) != nullptr );
 
     if ( isSuccess )
     {
         std::cout << "Authentication successful!" << std::endl;
     }
 
     return isSuccess;
 }
 
 void printHelp()
 {
     std::cout << "\nRCON Client Help:\n"
               << "=================\n\n"
               << "Usage:\n"
               << "  rcon_client <server_ip_or_hostname> <server_port> [password] [options]\n\n"
               << "  Example: rcon_client 192.168.1.100 8800 mypassword\n"
               << "  Example: rcon_client google.com 8800 mypassword --logstream-gameevents\n\n"
               << "Options:\n"
               << "  --logstream-gameevents       - Stream game event logs (command line option)\n"
               << "  --logstream-serverlog        - Stream server logs (command line option)\n\n"
               << "Runtime log commands:\n"
               << "  logstream-gameevents         - Start streaming game events\n"
               << "  logstream-serverlog          - Start streaming server logs\n"
               << "  logstream-all                - Start streaming both game events and server logs\n"
               << "  !logstream-gameevents        - Stop streaming game events\n"
               << "  !logstream-serverlog         - Stop streaming server logs\n"
               << "  !logstream-all               - Stop streaming both game events and server logs\n\n"
               << "Available commands:\n"
               << "  help                - Display this help message\n"
               << "  auth <password>     - Re-authenticate with the server\n"
               << "  exit                - Exit the RCON client\n"
               << "\nServer commands:\n"
               << "  /set <option> <value> - Set a server option\n"
               << "  <option> <value>      - Direct server command, can work but we recommend you use /set\n"
               << "\nCommon server commands:\n"
               << "  ServerName \"New Name\"   - Change server name\n"
               << "  MaxTeams <number>       - Set maximum number of player slots for the server\n"
               << "  MinTeams <number>       - Set minimum number of players to start the game\n"
               << "  ServerPassword <value>  - Set server password\n"
               << "  quit                    - Restart the server\n"
               << "\nAuthentication:\n"
               << "  If your session times out, you'll see 'Authentication required'\n"
               << "  Type 'auth <password>' to re-authenticate\n"
               << "\nSecurity:\n"
               << "  After 5 failed login attempts, your IP will be locked out\n"
               << "  Once locked out, the server must be restarted to regain access\n"
               << "\nPort Configuration:\n"
               << "  Each server instance should use a unique port number (default: 8800)\n"
               << "  Example port assignments:\n"
               << "    - 8800: 1v1 Random\n"
               << "    - 8801: 1v1 Default\n"
               << "    - 8802: 2v2 Random\n"
               << std::endl;
 }
 
 // function to display the logstream inside the console
 void logReceiver( SOCKET sock, const struct sockaddr_in & serverAddr )
 {
     char buffer[4096];
     struct sockaddr_in fromAddr;
     socklen_t fromLen;
     bool firstOutput = true;
     
     setSocketTimeout(sock, LOG_RECV_TIMEOUT);
 
     while ( running.load() )
     {
         fromLen = sizeof( fromAddr );
         int received = recvfrom( sock, buffer, sizeof( buffer ) - 1, 0,
                                  (struct sockaddr *)&fromAddr, &fromLen );
 
         if ( received > 0 )
         {
             buffer[received] = '\0';
 
 #ifdef DEBUG
             std::cout << "[DEBUG] Received in logReceiver: " << buffer << std::endl;
 #endif
 
             if ( strncmp( buffer, "LOG ", 4 ) == 0 )
             {
                 char * logData = buffer + 4;
 
                 std::lock_guard<std::mutex> lock( outputMutex );
 
                 // add a new line for the first output
                 if (firstOutput) {
                     std::cout << std::endl;
                     firstOutput = false;
                 }
 
                 // print the rest without newlines
                 if ( strncmp( logData, "GAMEEVENT ", 10 ) == 0 )
                 {
                     std::cout << "[GAME EVENT] " << ( logData + 10 ) << std::flush;
                 }
                 else if ( strncmp( logData, "SERVERLOG ", 10 ) == 0 )
                 {
                     std::cout << "[SERVER LOG] " << ( logData + 10 ) << std::flush;
                 }
             }
         }
     }
 }
 
 int main( int argc, char * argv[] )
 {
     // check the arguments
     if ( argc < 3 )
     {
         std::cerr << "Usage: " << argv[0] << " <server_ip_or_hostname> <server_port> [password] [options]" << std::endl;
         std::cerr << "Example: " << argv[0] << " 192.168.1.100 8800 mypassword" << std::endl;
         std::cerr << "Example: " << argv[0] << " google.com 8800 mypassword --logstream-gameevents" << std::endl;
         std::cerr << "If password is not provided, you will be prompted for it." << std::endl;
         std::cerr << "Type 'help' after connecting for a list of commands." << std::endl;
         return 1;
     }
 
     const char * serverAddress = nullptr;
     int port = 0;
     std::string password;
     int positionalArgs = 0;
     std::thread * receiverThread = nullptr;
 
     // parse the logstream commands
     for ( int i = 1; i < argc; i++ )
     {
         if ( strcmp( argv[i], "--logstream-gameevents" ) == 0 )
         {
             streamGameEvents = true;
         }
         else if ( strcmp( argv[i], "--logstream-serverlog" ) == 0 )
         {
             streamServerLog = true;
         }
         else
         {
             // Handle positional arguments
             positionalArgs++;
             if ( positionalArgs == 1 )
             {
                 serverAddress = argv[i];
             }
             else if ( positionalArgs == 2 )
             {
                 port = atoi( argv[i] );
             }
             else if ( positionalArgs == 3 )
             {
                 password = argv[i];
             }
         }
     }
 
     if ( positionalArgs < 2 )
     {
         std::cerr << "Error: Missing required arguments (server address and port)" << std::endl;
         std::cerr << "Usage: " << argv[0] << " <server_ip_or_hostname> <server_port> [password] [options]" << std::endl;
         return 1;
     }
 
     if ( password.empty() )
     {
         std::cout << "Enter RCON password: ";
         std::getline( std::cin, password );
     }
 
     // make sure we can connect to the internet
     if ( !initialize() )
     {
         std::cerr << "Failed to initialize networking" << std::endl;
         return 1;
     }
 
     atexit( cleanup );
 
     SOCKET sock = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
     if ( sock == INVALID_SOCKET )
     {
         std::cerr << "Socket creation failed: " << SOCKET_ERROR_CODE << std::endl;
         return 1;
     }
 
     // socket timeout
     if ( !setSocketTimeout( sock, RECV_TIMEOUT ) )
     {
         closesocket( sock );
         return 1;
     }
 
     struct sockaddr_in serverAddr;
     memset( &serverAddr, 0, sizeof( serverAddr ) );
     serverAddr.sin_family = AF_INET;
     serverAddr.sin_port = htons( port );
 
     // check if serverAddress is an IP address or hostname
     struct in_addr addr;
     addr.s_addr = inet_addr( serverAddress );
 
     if ( addr.s_addr == INADDR_NONE )
     {
         // not an ip address, attempt to resolve hostname
         std::cout << "Resolving hostname: " << serverAddress << std::endl;
         if ( !resolveHostname( serverAddress, &addr ) )
         {
             std::cerr << "Failed to resolve hostname. Please use an IP address instead." << std::endl;
             closesocket( sock );
             return 1;
         }
         std::cout << "Resolved to IP: " << inet_ntoa( addr ) << std::endl;
     }
 
     serverAddr.sin_addr = addr;
 
     if ( !authenticate( sock, serverAddr, password ) )
     {
         std::cerr << "Authentication failed!" << std::endl;
         closesocket( sock );
         return 1;
     }
 
     // start the logstreams if requested
 
     // gameevents logstream
     if ( streamGameEvents )
     {
         std::string command = "STARTGAMELOG";
 #ifdef DEBUG
         std::cout << "[DEBUG] Starting game log: " << command << std::endl;
 #endif
         sendto( sock, command.c_str(), command.length(), 0,
                 (struct sockaddr *)&serverAddr, sizeof( serverAddr ) );
 
         char buffer[4096];
         struct sockaddr_in fromAddr;
         socklen_t fromLen = sizeof( fromAddr );
         int received = recvfrom( sock, buffer, sizeof( buffer ) - 1, 0,
                                  (struct sockaddr *)&fromAddr, &fromLen );
         if ( received > 0 )
         {
             buffer[received] = '\0';
             std::cout << buffer << std::endl;
         }
     }
 
     // serverlog/console logstream
     if ( streamServerLog )
     {
         std::string command = "STARTSERVERLOG";
 #ifdef DEBUG
         std::cout << "[DEBUG] Starting server log: " << command << std::endl;
 #endif
         sendto( sock, command.c_str(), command.length(), 0,
                 (struct sockaddr *)&serverAddr, sizeof( serverAddr ) );
 
         char buffer[4096];
         struct sockaddr_in fromAddr;
         socklen_t fromLen = sizeof( fromAddr );
         int received = recvfrom( sock, buffer, sizeof( buffer ) - 1, 0,
                                  (struct sockaddr *)&fromAddr, &fromLen );
         if ( received > 0 )
         {
             buffer[received] = '\0';
             std::cout << buffer << std::endl;
         }
     }
 
     if ( streamGameEvents || streamServerLog )
     {
         receiverThread = new std::thread( logReceiver, sock, serverAddr );
     }
 
     std::cout << "Enter commands (type 'help' for available commands):" << std::endl;
 
     std::string command;
     bool authenticated = true;
 
     while ( true )
     {
         {
             std::lock_guard<std::mutex> lock( outputMutex );
             std::cout << "> ";
         }
         std::getline( std::cin, command );
 
 #ifdef DEBUG
         std::cout << "[DEBUG] Command entered: " << command << std::endl;
 #endif
 
         // check for program specific commands
         if ( command == "quit" || command == "exit" )
         {
             break;
         }
         else if ( command == "help" )
         {
             printHelp();
             continue;
         }
         else if ( command.substr( 0, 5 ) == "auth " )
         {
             if ( command.length() > 5 )
             {
                 password = command.substr( 5 );
                 if ( authenticate( sock, serverAddr, password ) )
                 {
                     authenticated = true;
                     std::cout << "Re-authentication successful." << std::endl;
                 }
             }
             else
             {
                 std::cout << "Usage: auth <password>" << std::endl;
             }
             continue;
         }
         else if ( command == "logstream-gameevents" )
         {
             if ( !streamGameEvents )
             {
                 std::string startCmd = "STARTGAMELOG";
                 sendto( sock, startCmd.c_str(), startCmd.length(), 0,
                         (struct sockaddr *)&serverAddr, sizeof( serverAddr ) );
                 
                 char buffer[4096];
                 struct sockaddr_in fromAddr;
                 socklen_t fromLen = sizeof( fromAddr );
                 int received = recvfrom( sock, buffer, sizeof( buffer ) - 1, 0,
                                         (struct sockaddr *)&fromAddr, &fromLen );
                 
                 if ( received > 0 )
                 {
                     buffer[received] = '\0';
                     std::cout << "Game event logging started" << std::endl;
                 }
                 
                 streamGameEvents = true;
                 
                 if ( !streamServerLog && !receiverThread )
                 {
                     running.store( true );
                     receiverThread = new std::thread( logReceiver, sock, serverAddr );
                 }
             }
             else
             {
                 std::cout << "Game event logging already active" << std::endl;
             }
             continue;
         }
         else if ( command == "logstream-serverlog" )
         {
             if ( !streamServerLog )
             {
                 std::string startCmd = "STARTSERVERLOG";
                 sendto( sock, startCmd.c_str(), startCmd.length(), 0,
                         (struct sockaddr *)&serverAddr, sizeof( serverAddr ) );
                 
                 char buffer[4096];
                 struct sockaddr_in fromAddr;
                 socklen_t fromLen = sizeof( fromAddr );
                 int received = recvfrom( sock, buffer, sizeof( buffer ) - 1, 0,
                                         (struct sockaddr *)&fromAddr, &fromLen );
                 
                 if ( received > 0 )
                 {
                     buffer[received] = '\0';
                     std::cout << "Server logging started" << std::endl;
                 }
                 
                 streamServerLog = true;
                 
                 if ( !streamGameEvents && !receiverThread )
                 {
                     running.store( true );
                     receiverThread = new std::thread( logReceiver, sock, serverAddr );
                 }
             }
             else
             {
                 std::cout << "Server logging already active" << std::endl;
             }
             continue;
         }
         else if ( command == "!logstream-gameevents" )
         {
             if ( streamGameEvents )
             {
                 std::string stopCmd = "STOPGAMELOG";
                 sendto( sock, stopCmd.c_str(), stopCmd.length(), 0,
                         (struct sockaddr *)&serverAddr, sizeof( serverAddr ) );
                 
                 char buffer[4096];
                 struct sockaddr_in fromAddr;
                 socklen_t fromLen = sizeof( fromAddr );
                 int received = recvfrom( sock, buffer, sizeof( buffer ) - 1, 0,
                                         (struct sockaddr *)&fromAddr, &fromLen );
                 
                 if ( received > 0 )
                 {
                     buffer[received] = '\0';
                     std::cout << "Game event logging stopped" << std::endl;
                 }
                 
                 streamGameEvents = false;
                 
                 if ( !streamServerLog && receiverThread )
                 {
                     running.store( false );
                     receiverThread->join();
                     delete receiverThread;
                     receiverThread = nullptr;
                 }
             }
             else
             {
                 std::cout << "Game event logging not active" << std::endl;
             }
             continue;
         }
         else if ( command == "!logstream-serverlog" )
         {
             if ( streamServerLog )
             {
                 std::string stopCmd = "STOPSERVERLOG";
                 sendto( sock, stopCmd.c_str(), stopCmd.length(), 0,
                         (struct sockaddr *)&serverAddr, sizeof( serverAddr ) );
                 
                 char buffer[4096];
                 struct sockaddr_in fromAddr;
                 socklen_t fromLen = sizeof( fromAddr );
                 int received = recvfrom( sock, buffer, sizeof( buffer ) - 1, 0,
                                         (struct sockaddr *)&fromAddr, &fromLen );
                 
                 if ( received > 0 )
                 {
                     buffer[received] = '\0';
                     std::cout << "Server logging stopped" << std::endl;
                 }
                 
                 streamServerLog = false;
                 
                 if ( !streamGameEvents && receiverThread )
                 {
                     running.store( false );
                     receiverThread->join();
                     delete receiverThread;
                     receiverThread = nullptr;
                 }
             }
             else
             {
                 std::cout << "Server logging not active" << std::endl;
             }
             continue;
         }
         // command to show all logstreams during runtime
         else if ( command == "logstream-all" )
         {
             bool startedAny = false;
             
             if ( !streamGameEvents )
             {
                 std::string startCmd = "STARTGAMELOG";
                 sendto( sock, startCmd.c_str(), startCmd.length(), 0,
                         (struct sockaddr *)&serverAddr, sizeof( serverAddr ) );
                 
                 char buffer[4096];
                 struct sockaddr_in fromAddr;
                 socklen_t fromLen = sizeof( fromAddr );
                 int received = recvfrom( sock, buffer, sizeof( buffer ) - 1, 0,
                                         (struct sockaddr *)&fromAddr, &fromLen );
                 
                 if ( received > 0 )
                 {
                     buffer[received] = '\0';
                     std::cout << "Game event logging started" << std::endl;
                     startedAny = true;
                 }
                 
                 streamGameEvents = true;
             }
             
             if ( !streamServerLog )
             {
                 std::string startCmd = "STARTSERVERLOG";
                 sendto( sock, startCmd.c_str(), startCmd.length(), 0,
                         (struct sockaddr *)&serverAddr, sizeof( serverAddr ) );
                 
                 char buffer[4096];
                 struct sockaddr_in fromAddr;
                 socklen_t fromLen = sizeof( fromAddr );
                 int received = recvfrom( sock, buffer, sizeof( buffer ) - 1, 0,
                                         (struct sockaddr *)&fromAddr, &fromLen );
                 
                 if ( received > 0 )
                 {
                     buffer[received] = '\0';
                     std::cout << "Server logging started" << std::endl;
                     startedAny = true;
                 }
                 
                 streamServerLog = true;
             }
             
             // start the receiver thread if its not running and we started at least one stream
             if ( startedAny && !receiverThread )
             {
                 running.store( true );
                 receiverThread = new std::thread( logReceiver, sock, serverAddr );
             }
             
             if ( !startedAny )
             {
                 std::cout << "All log streams already active" << std::endl;
             }
             
             continue;
         }
         else if ( command == "!logstream-all" )
         {
             bool wasActive = streamGameEvents || streamServerLog;
             
             // stop game events stream if active
             if ( streamGameEvents )
             {
                 std::string stopCmd = "STOPGAMELOG";
                 sendto( sock, stopCmd.c_str(), stopCmd.length(), 0,
                         (struct sockaddr *)&serverAddr, sizeof( serverAddr ) );
                 
                 char buffer[4096];
                 struct sockaddr_in fromAddr;
                 socklen_t fromLen = sizeof( fromAddr );
                 int received = recvfrom( sock, buffer, sizeof( buffer ) - 1, 0,
                                         (struct sockaddr *)&fromAddr, &fromLen );
                 
                 //even if we dont get a response, we know we tried to stop it
                 streamGameEvents = false;
             }
             
             // stop server log stream if active
             if ( streamServerLog )
             {
                 std::string stopCmd = "STOPSERVERLOG";
                 sendto( sock, stopCmd.c_str(), stopCmd.length(), 0,
                         (struct sockaddr *)&serverAddr, sizeof( serverAddr ) );
                 
                 char buffer[4096];
                 struct sockaddr_in fromAddr;
                 socklen_t fromLen = sizeof( fromAddr );
                 int received = recvfrom( sock, buffer, sizeof( buffer ) - 1, 0,
                                         (struct sockaddr *)&fromAddr, &fromLen );
                 
                 // even if we dont get a response, we know we tried to stop it
                 streamServerLog = false;
             }
             
             if ( !streamGameEvents && !streamServerLog && receiverThread )
             {
                 running.store( false );
                 receiverThread->join();
                 delete receiverThread;
                 receiverThread = nullptr;
             }
             
             if ( wasActive )
             {
                 std::cout << "All log streams stopped" << std::endl;
             }
             else
             {
                 std::cout << "No log streams were active" << std::endl;
             }
             
             continue;
         }
 
         // execution flags, when starting the program we can invoke the program to display logstreams
         else if ( command == "--logstream-gameevents" )
         {
             // parse the command to the runtime equivilant
             command = "logstream-gameevents";
             continue;
         }
         else if ( command == "--logstream-serverlog" )
         {
             command = "logstream-serverlog";
             continue;
         }
 
         std::string fullCommand = "EXEC " + command;
 #ifdef DEBUG
         std::cout << "[DEBUG] Sending command: " << fullCommand << std::endl;
 #endif
         if ( sendto( sock, fullCommand.c_str(), fullCommand.length(), 0, (struct sockaddr *)&serverAddr, sizeof( serverAddr ) ) == SOCKET_ERROR )
         {
             std::cerr << "Failed to send command: " << SOCKET_ERROR_CODE << std::endl;
             continue;
         }
 
         char buffer[4096];
         struct sockaddr_in fromAddr;
         socklen_t fromLen = sizeof( fromAddr );
 
         int received = recvfrom( sock, buffer, sizeof( buffer ) - 1, 0, (struct sockaddr *)&fromAddr, &fromLen );
         if ( received == SOCKET_ERROR )
         {
             int error = SOCKET_ERROR_CODE;
 
 #ifdef _WIN32
             if ( error == WSAETIMEDOUT )
             {
 #else
             if ( error == ETIMEDOUT )
             {
 #endif
                 std::cerr << "Server response timeout - server may be restarting" << std::endl;
 
                 // if the command was quit, the server is likely restarting
                 if ( command == "quit" || command == "/set quit" )
                 {
                     std::cout << "Server is restarting. You may need to reconnect in a few moments." << std::endl;
                     std::cout << "Would you like to attempt reconnection? (y/n): ";
                     std::string response;
                     std::getline( std::cin, response );
 
                     if ( response == "y" || response == "Y" )
                     {
                         std::cout << "Waiting for server to restart... (10 seconds)" << std::endl;
 
 // delay to wait for the server to restart
 #ifdef _WIN32
                         Sleep( 10000 );
 #else
                         sleep( 10 );
 #endif
 
                         // try to authenticate again
                         if ( authenticate( sock, serverAddr, password ) )
                         {
                             authenticated = true;
                             std::cout << "Reconnection successful." << std::endl;
                         }
                         else
                         {
                             std::cout << "Failed to reconnect. Please restart the client." << std::endl;
                         }
                     }
                 }
             }
             else
             {
                 std::cerr << "Failed to receive response: " << error << std::endl;
             }
             continue;
         }
 
         // null-terminate and print
         buffer[received] = '\0';
 
 #ifdef DEBUG
         std::cout << "[DEBUG] Response received: " << buffer << std::endl;
 #endif
 
         if ( strncmp( buffer, "LOG ", 4 ) != 0 )
         {
             std::cout << buffer << std::endl;
         }
 
         // check if we need to reauthenticate
         if ( strstr( buffer, "STATUS 401" ) != nullptr &&
              strstr( buffer, "Authentication required" ) != nullptr )
         {
             authenticated = false;
             // tell the user to reauthenticate
             std::cout << "MESSAGE Re-enter your password to continue" << std::endl;
             std::cout << "Type 'auth <password>' to re-authenticate" << std::endl;
         }
         // check if the ip address is blocked
         else if ( strstr( buffer, "STATUS 403" ) != nullptr )
         {
             std::cout << "Your IP has been locked out due to too many failed authentication attempts." << std::endl;
             std::cout << "The server will need to be restarted before you can connect again." << std::endl;
             std::cout << "Press Enter to exit..." << std::endl;
             std::getline( std::cin, command );
             break;
         }
     }
 
     running.store( false );
 
     if ( receiverThread )
     {
         receiverThread->join();
         delete receiverThread;
     }
 
     closesocket( sock );
     return 0;
 }