/*
 *  * DedCon RCON Client: Remote Access for Dedcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  * Copyright (C) 2025 Keiron Mcphee
 *  *
 *  */

#include "RCONClient.h"
#include "ClientEncryption.h"
#include <memory>
#include <vector>
#include <signal.h>

#define RECV_TIMEOUT 5000 // default receive timeout: 5 seconds
#define LOG_RECV_TIMEOUT 500  // shorter timeout for log receiver

bool streamGameEvents = false;
bool streamServerLog = false;

std::atomic<bool> running(true);
std::mutex outputMutex;

static std::unique_ptr<ClientEncryption> g_clientEncryption;
static bool g_encryptionEnabled = false;
static bool g_debugEncryption = false; // set to false initially unless invoked by a command line flag
static SOCKET g_socket = INVALID_SOCKET;
static struct sockaddr_in g_serverAddr;
static bool g_authenticated = false;
static std::thread* g_receiverThread = nullptr;

void signalHandler(int signum) {
    if (g_socket != INVALID_SOCKET && g_authenticated) {
        std::cout << "\nReceived interrupt signal. Disconnecting..." << std::endl;
        
        // Send disconnect message
        std::string disconnectCmd = "DISCONNECT";
        if (sendCommand(g_socket, g_serverAddr, disconnectCmd)) {
            std::string response;
            receiveResponse(g_socket, response);
        }
    }
    
    // stop all threads
    running.store(false);
    
    // join the receiver thread if it exists
    if (g_receiverThread && g_receiverThread->joinable()) {
        g_receiverThread->join();
    }
    
    exit(signum);
}

void cleanup()
{
#ifdef _WIN32
    WSACleanup();
#endif
    // clean up encryption
    g_clientEncryption.reset();
    
    // make sure threads are properly stopped
    running.store(false);
    
    if (g_receiverThread && g_receiverThread->joinable()) {
        g_receiverThread->join();
        delete g_receiverThread;
        g_receiverThread = nullptr;
    }
}

// function to check Linux network settings
#ifndef _WIN32
bool checkLinuxNetworkSettings() 
{
    // for Linux, check if we have the right permissions
    bool isRoot = (geteuid() == 0);
    
    if (!isRoot) {
        std::cout << "Note: Not running as root, some network optimizations may not be available" << std::endl;
        std::cout << "If you continue to have connection issues, consider running with sudo" << std::endl;
    }
    
    return true;
}
#endif

bool initialize()
{
#ifdef _WIN32
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0)
    {
        std::cerr << "WSAStartup failed: " << result << std::endl;
        return false;
    }
#else
    checkLinuxNetworkSettings();
#endif

    // initialize encryption system
    g_clientEncryption = std::make_unique<ClientEncryption>();

    return true;
}

// socket timeout for windows and maxos/linux
bool setSocketTimeout(SOCKET sock, int timeoutMs)
{
#ifdef _WIN32
    DWORD timeout = timeoutMs;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout)) == SOCKET_ERROR)
    {
        std::cerr << "Failed to set socket timeout: " << SOCKET_ERROR_CODE << std::endl;
        return false;
    }
#else
    struct timeval tv;
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv)) == SOCKET_ERROR)
    {
        std::cerr << "Failed to set socket timeout: " << SOCKET_ERROR_CODE << std::endl;
        return false;
    }
#endif
    return true;
}

// resolve hostname to IP address, this will make it easier for server hosters who also have a website
bool resolveHostname(const char *hostname, struct in_addr *addr)
{
    struct addrinfo hints, *result;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;      // IPv4
    hints.ai_socktype = SOCK_DGRAM; // UDP

    int status = getaddrinfo(hostname, NULL, &hints, &result);
    if (status != 0)
    {
#ifdef _WIN32
        std::cerr << "Failed to resolve hostname: " << WSAGetLastError() << std::endl;
#else
        std::cerr << "Failed to resolve hostname: " << gai_strerror(status) << std::endl;
#endif
        return false;
    }

    struct sockaddr_in *ipv4 = (struct sockaddr_in *)result->ai_addr;
    *addr = ipv4->sin_addr;

    freeaddrinfo(result);
    return true;
}

bool authenticate(SOCKET sock, const struct sockaddr_in &serverAddr, const std::string &password, bool disableEncryption)
{
    // empty passwords have been removed from DedconRCON
    if (password.empty()) {
        std::cerr << "ERROR: Empty passwords are not allowed for security reasons." << std::endl;
        std::cerr << "Please provide a password when connecting to the RCON server." << std::endl;
        return false;
    }

    // if encryption is disabled by the user dont initialize it
    if (!disableEncryption) {
        if (!g_clientEncryption->initialize(password))
        {
            std::cerr << "Failed to initialize encryption" << std::endl;
            return false;
        }

        // enable encryption globally
        g_encryptionEnabled = true;
    }
    else {
        g_encryptionEnabled = false;
        std::cout << "Encryption disabled by user request" << std::endl;
    }

    std::string authCommand = "AUTH " + password;
    std::vector<uint8_t> encryptedData;

    bool encryptionSuccess = false;
    // try to encrypt only if encryption is enabled
    if (g_encryptionEnabled && g_clientEncryption->encrypt(
            reinterpret_cast<const uint8_t *>(authCommand.c_str()),
            authCommand.length(),
            encryptedData))
    {
        encryptionSuccess = true;
        // send encrypted data
        if (sendto(sock, reinterpret_cast<const char *>(encryptedData.data()),
                   encryptedData.size(), 0, (struct sockaddr *)&serverAddr,
                   sizeof(serverAddr)) == SOCKET_ERROR)
        {
            std::cerr << "Failed to send encrypted auth command: " << SOCKET_ERROR_CODE << std::endl;
            encryptionSuccess = false;
        }
        else
        {
#ifdef DEBUG
            std::cout << "Sent encrypted auth command successfully" << std::endl;
#endif
        }
    }

    // send unencrypted if encryption is disabled or failed
    if (!encryptionSuccess)
    {
        g_encryptionEnabled = false;
        if (!disableEncryption) {
            std::cout << "Encryption disabled, falling back to unencrypted mode" << std::endl;
        }

        if (sendto(sock, authCommand.c_str(), authCommand.length(), 0,
                   (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
        {
            std::cerr << "Failed to send auth command: " << SOCKET_ERROR_CODE << std::endl;
            return false;
        }
#ifdef DEBUG
        else
        {
            std::cout << "Sent unencrypted auth command successfully" << std::endl;
        }
#endif
    }

    // recieve the response
    char buffer[4096];
    struct sockaddr_in fromAddr;
    socklen_t fromLen = sizeof(fromAddr);

    int received = recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
                            (struct sockaddr *)&fromAddr, &fromLen);
    
#ifdef _WIN32
    if (received == SOCKET_ERROR && WSAGetLastError() == WSAETIMEDOUT)
#else
    if (received == SOCKET_ERROR && (errno == EAGAIN || errno == EWOULDBLOCK))
#endif
    {
        if (g_encryptionEnabled && !disableEncryption) {
            std::cout << "Timeout on first attempt, trying again with unencrypted auth..." << std::endl;
            
            g_encryptionEnabled = false;
            
            if (sendto(sock, authCommand.c_str(), authCommand.length(), 0,
                      (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
            {
                std::cerr << "Failed to send fallback auth command: " << SOCKET_ERROR_CODE << std::endl;
                return false;
            }
            
            received = recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
                               (struct sockaddr *)&fromAddr, &fromLen);
        }
    }

    if (received == SOCKET_ERROR)
    {
#ifdef _WIN32
        std::cerr << "Failed to receive auth response: " << WSAGetLastError() << std::endl;
#else
        std::cerr << "Failed to receive auth response: " << strerror(errno) << " (" << errno << ")" << std::endl;
#endif
        return false;
    }

    // check if response is encrypted
    if (g_encryptionEnabled && ClientEncryption::isEncryptedPacket(reinterpret_cast<const uint8_t *>(buffer), received))
    {
        std::vector<uint8_t> decryptedData;
        if (g_clientEncryption->decrypt(
                reinterpret_cast<const uint8_t *>(buffer),
                received,
                decryptedData))
        {
            // process decrypted response
            std::string response(decryptedData.begin(), decryptedData.end());
            bool isSuccess = (response.find("STATUS 200") != std::string::npos);
            std::cout << (isSuccess ? "Authentication successful (encrypted)!" : "Authentication failed!") << std::endl;
            
            // update the global authentication state
            g_authenticated = isSuccess;
            
            return isSuccess;
        }
        else
        {
            std::cerr << "Failed to decrypt response, falling back to unencrypted" << std::endl;
            g_encryptionEnabled = false;
        }
    }

    // process as plain text
    buffer[received] = '\0';
    bool isSuccess = (strstr(buffer, "STATUS 200") != nullptr);
    std::cout << (isSuccess ? "Authentication successful!" : "Authentication failed!") << std::endl;
    
    // then update the global authentication state
    g_authenticated = isSuccess;
    
#ifdef DEBUG
    std::cout << "Response: " << buffer << std::endl;
#endif
    
    return isSuccess;
}

bool sendCommand(SOCKET sock, const struct sockaddr_in &serverAddr, const std::string &command)
{
    if (g_debugEncryption)
    {
        std::cout << "[DEBUG] Sending command: " << command << std::endl;
    }

    if (g_encryptionEnabled && g_clientEncryption)
    {
        std::vector<uint8_t> encryptedData;
        if (g_clientEncryption->encrypt(
                reinterpret_cast<const uint8_t *>(command.c_str()),
                command.length(),
                encryptedData))
        {

            if (g_debugEncryption)
            {
                std::cout << "[DEBUG] Encrypted size: " << encryptedData.size() << " bytes" << std::endl;
                std::cout << "[DEBUG] First 16 bytes (hex): ";
                size_t limit = encryptedData.size() < 16 ? encryptedData.size() : 16;
                for (size_t i = 0; i < limit; i++)
                {
                    printf("%02X ", encryptedData[i]);
                }
                std::cout << std::endl;
            }

            return sendto(sock, reinterpret_cast<const char *>(encryptedData.data()),
                          encryptedData.size(), 0, (struct sockaddr *)&serverAddr,
                          sizeof(serverAddr)) != SOCKET_ERROR;
        }
    }

    if (g_debugEncryption)
    {
        std::cout << "[DEBUG] Sending unencrypted" << std::endl;
    }

    return sendto(sock, command.c_str(), command.length(), 0,
                  (struct sockaddr *)&serverAddr, sizeof(serverAddr)) != SOCKET_ERROR;
}

bool receiveResponse(SOCKET sock, std::string &response)
{
    char buffer[4096];
    struct sockaddr_in fromAddr;
    socklen_t fromLen = sizeof(fromAddr);

    int received = recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
                            (struct sockaddr *)&fromAddr, &fromLen);

    if (received == SOCKET_ERROR)
    {
        return false;
    }

    if (g_encryptionEnabled && ClientEncryption::isEncryptedPacket(
                                   reinterpret_cast<const uint8_t *>(buffer), received))
    {

        std::vector<uint8_t> decryptedData;
        if (g_clientEncryption && g_clientEncryption->decrypt(
                                      reinterpret_cast<const uint8_t *>(buffer),
                                      received,
                                      decryptedData))
        {

            response = std::string(decryptedData.begin(), decryptedData.end());
            return true;
        }
    }

    buffer[received] = '\0';
    response = buffer;
    return true;
}

// check if a response contains binary/garbled text, this is a byproduct of using encryption.
// or maybe i am just a bad programmer
bool isValidTextResponse(const std::string &text)
{
    // empty strings are valid
    if (text.empty())
        return true;

    // if it starts with STATUS or MESSAGE its a valid response
    if (text.length() > 7 && text.substr(0, 7) == "STATUS ")
    {
        return true;
    }

    if (text.length() > 8 && text.substr(0, 8) == "MESSAGE ")
    {
        return true;
    }

    // count valid and invalid characters
    int validCount = 0;
    int invalidCount = 0;

    for (size_t i = 0; i < text.length(); i++)
    {
        unsigned char c = text[i];
        if ((c < 32 && c != '\n' && c != '\r' && c != '\t') || c > 127)
        {
            invalidCount++;
        }
        else
        {
            validCount++;
        }
    }

    // if more than 10 percent of the characaters are invalid, assume its garbled text
    if (text.length() > 10 && (float)invalidCount / (float)text.length() > 0.1f)
    {
        return false;
    }

    return true;
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
              << "  --logstream-serverlog        - Stream server logs (command line option)\n"
              << "  --no-encryption              - Disable encryption (use only if server has encryption disabled)\n\n"
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
              << "  exit                - Exit the RCON client (sends disconnect to server)\n"
              << "\nServer commands:\n"
              << "  /set <option> <value> - Set a server option\n"
              << "  <option> <value>      - Direct server command, can work but we recommend you use /set\n"
              << "\nCommon server commands:\n"
              << "  ServerName \"New Name\"   - Change server name\n"
              << "  MaxTeams <number>       - Set maximum number of player slots for the server\n"
              << "  MinTeams <number>       - Set minimum number of players to start the game\n"
              << "  ServerPassword <value>  - Set server password\n"
              << "  quit                    - Restart/shutdown the Dedcon server (not the client)\n"
              << "\nAuthentication:\n"
              << "  If your session times out, you'll see 'Authentication required'\n"
              << "  Type 'auth <password>' to re-authenticate\n"
              << "\nSecurity:\n"
              << "  Empty passwords are NOT allowed. You must provide a password.\n"
              << "  Set RCONPassword in your server's config file or use /set RCONPassword <value>\n" 
              << "  After 5 failed login attempts, your IP will be locked out\n"
              << "  Once locked out, the server must be restarted to regain access\n"
              << "  Encryption is enabled by default. Use --no-encryption only if the server has it disabled.\n"
              << "  You can configure the server's encryption with /set RCONEncryption <0 or 1>\n"
              << "\nDisconnection:\n"
              << "  The client will send a proper disconnect notification to the server when exiting.\n"
              << "  If you use Ctrl+C or close the window, it will also attempt to disconnect cleanly.\n"
              << "  Notice how i said ATTEMPT?, this means that its still recommended to use the exit command.\n"
              << "\nPort Configuration:\n"
              << "  Each server instance should use a unique port number (default: 8800)\n"
              << "  Example port assignments:\n"
              << "    - 8800: 1v1 Random\n"
              << "    - 8801: 1v1 Default\n"
              << "    - 8802: 2v2 Random\n"
              << std::endl;
}

// function to display the logstream inside the console
void logReceiver(SOCKET sock, const struct sockaddr_in &serverAddr)
{
    char buffer[4096];
    struct sockaddr_in fromAddr;
    socklen_t fromLen;
    bool firstOutput = true;

    setSocketTimeout(sock, LOG_RECV_TIMEOUT);

    while (running.load())
    {
        fromLen = sizeof(fromAddr);
        int received = recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
                                (struct sockaddr *)&fromAddr, &fromLen);

        if (received > 0)
        {
            // check if packet is encrypted
            if (g_encryptionEnabled && ClientEncryption::isEncryptedPacket(
                                       reinterpret_cast<const uint8_t *>(buffer), received))
            {
                std::vector<uint8_t> decryptedData;
                if (g_clientEncryption && g_clientEncryption->decrypt(
                                          reinterpret_cast<const uint8_t *>(buffer),
                                          received,
                                          decryptedData))
                {
                    std::string decryptedStr(decryptedData.begin(), decryptedData.end());
                    
                    std::lock_guard<std::mutex> lock(outputMutex);
                    
                    // add a new line for the first output
                    if (firstOutput)
                    {
                        std::cout << std::endl;
                        firstOutput = false;
                    }
                    
                    if (decryptedStr.substr(0, 4) == "LOG ")
                    {
                        std::string logData = decryptedStr.substr(4);
                        
                        if (logData.substr(0, 10) == "GAMEEVENT ")
                        {
                            std::cout << "[GAME EVENT] " << logData.substr(10) << std::flush;
                        }
                        else if (logData.substr(0, 10) == "SERVERLOG ")
                        {
                            std::cout << "[SERVER LOG] " << logData.substr(10) << std::flush;
                        }
                    }
                }
                else
                {
#ifdef DEBUG
                    std::cerr << "[DEBUG] Failed to decrypt log message" << std::endl;
#endif
                }
            }
            else
            {
                // process unencrypted message
                buffer[received] = '\0';

#ifdef DEBUG
                std::cout << "[DEBUG] Received in logReceiver: " << buffer << std::endl;
#endif

                if (strncmp(buffer, "LOG ", 4) == 0)
                {
                    char *logData = buffer + 4;

                    std::lock_guard<std::mutex> lock(outputMutex);

                    // add a new line for the first output
                    if (firstOutput)
                    {
                        std::cout << std::endl;
                        firstOutput = false;
                    }

                    // print the rest without newlines
                    if (strncmp(logData, "GAMEEVENT ", 10) == 0)
                    {
                        std::cout << "[GAME EVENT] " << (logData + 10) << std::flush;
                    }
                    else if (strncmp(logData, "SERVERLOG ", 10) == 0)
                    {
                        std::cout << "[SERVER LOG] " << (logData + 10) << std::flush;
                    }
                }
            }
        }
        
        // added short sleep to allow quicker response to shutdown signal
        // this should help the thread exit faster when running is set to false
#ifdef _WIN32
        Sleep(10);
#else
        usleep(10000);
#endif
    }
}

int main(int argc, char *argv[])
{
    // check the arguments
    if (argc < 3)
    {
        std::cerr << "Usage: " << argv[0] << " <server_ip_or_hostname> <server_port> [password] [options]" << std::endl;
        std::cerr << "Example: " << argv[0] << " 192.168.1.100 8800 mypassword" << std::endl;
        std::cerr << "Example: " << argv[0] << " google.com 8800 mypassword --logstream-gameevents" << std::endl;
        std::cerr << "If password is not provided, you will be prompted for it." << std::endl;
        std::cerr << "Type 'help' after connecting for a list of commands." << std::endl;
        return 1;
    }

    // signal handlers for clean disconnects
    signal(SIGINT, signalHandler);  // Ctrl+C
    signal(SIGTERM, signalHandler); // Termination request
#ifdef _WIN32
    signal(SIGBREAK, signalHandler); // Ctrl+Break
#endif

    const char *serverAddress = nullptr;
    int port = 0;
    std::string password;
    int positionalArgs = 0;
    std::thread *receiverThread = nullptr;
    bool disableEncryption = false;

    // parse the logstream commands
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--logstream-gameevents") == 0)
        {
            streamGameEvents = true;
        }
        else if (strcmp(argv[i], "--logstream-serverlog") == 0)
        {
            streamServerLog = true;
        }
        // flag for enabling encryption testing.
        else if (strcmp(argv[i], "--debug-encryption") == 0)
        {
            g_debugEncryption = true;
        }
        else if (strcmp(argv[i], "--no-encryption") == 0)
        {
            disableEncryption = true;
        }
        else
        {
            // the correct method to start the rcon client
            // ./client IPADDRESS PORT PASSWORD
            positionalArgs++;
            if (positionalArgs == 1)
            {
                serverAddress = argv[i];
            }
            else if (positionalArgs == 2)
            {
                port = atoi(argv[i]);
            }
            else if (positionalArgs == 3)
            {
                password = argv[i];
            }
        }
    }

    // check if the user has inputed to few command line arguments
    if (positionalArgs < 2)
    {
        std::cerr << "Error: Missing required arguments (server address and port)" << std::endl;
        std::cerr << "Usage: " << argv[0] << " <server_ip_or_hostname> <server_port> [password] [options]" << std::endl;
        return 1;
    }

    // is the password empty?
    if (password.empty())
    {
        std::cout << "Enter RCON password: ";
        std::getline(std::cin, password);
        
        // if yes give them a lecture
        if (password.empty())
        {
            std::cerr << "ERROR: Empty passwords are not allowed for security reasons." << std::endl;
            std::cerr << "Please provide a valid password for RCON authentication." << std::endl;
            return 1;
        }
    }

    // make sure we can connect to the internet
    if (!initialize())
    {
        std::cerr << "Failed to initialize networking" << std::endl;
        return 1;
    }

    atexit(cleanup);

    // i have never had this error thrown
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET)
    {
        std::cerr << "Socket creation failed: " << SOCKET_ERROR_CODE << std::endl;
        return 1;
    }

    // store socket in global variable for signal handler
    g_socket = sock;

    // socket timeout
    if (!setSocketTimeout(sock, RECV_TIMEOUT))
    {
        closesocket(sock);
        return 1;
    }

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    // check if serverAddress is an IP address or hostname
    struct in_addr addr;
    addr.s_addr = inet_addr(serverAddress);

    if (addr.s_addr == INADDR_NONE)
    {
        // not an ip address, attempt to resolve hostname
        std::cout << "Resolving hostname: " << serverAddress << std::endl;
        if (!resolveHostname(serverAddress, &addr))
        {
            std::cerr << "Failed to resolve hostname. Please use an IP address instead." << std::endl;
            closesocket(sock);
            return 1;
        }
        std::cout << "Resolved to IP: " << inet_ntoa(addr) << std::endl;
    }

    serverAddr.sin_addr = addr;

    // store server address in global for signal handler
    g_serverAddr = serverAddr;

    if (!authenticate(sock, serverAddr, password, disableEncryption))
    {
        std::cerr << "Authentication failed!" << std::endl;
        closesocket(sock);
        return 1;
    }

    // mark as authenticated for signal handler
    g_authenticated = true;

    // start the logstreams if requested
    // gameevents logstream
    if (streamGameEvents)
    {
        std::string command = "STARTGAMELOG";
#ifdef DEBUG
        std::cout << "[DEBUG] Starting game log: " << command << std::endl;
#endif
        if (!sendCommand(sock, serverAddr, command))
        {
            std::cerr << "Failed to send game event log start command: " << SOCKET_ERROR_CODE << std::endl;
        }
        else
        {
            std::string response;
            if (receiveResponse(sock, response))
            {
                std::cout << response << std::endl;
            }
        }
    }

    // serverlog/console logstream
    if (streamServerLog)
    {
        std::string command = "STARTSERVERLOG";
#ifdef DEBUG
        std::cout << "[DEBUG] Starting server log: " << command << std::endl;
#endif
        if (!sendCommand(sock, serverAddr, command))
        {
            std::cerr << "Failed to send server log start command: " << SOCKET_ERROR_CODE << std::endl;
        }
        else
        {
            std::string response;
            if (receiveResponse(sock, response))
            {
                std::cout << response << std::endl;
            }
        }
    }

    if (streamGameEvents || streamServerLog)
    {
        receiverThread = new std::thread(logReceiver, sock, serverAddr);
        g_receiverThread = receiverThread; // store in global for signal handler
    }

    std::cout << "Enter commands (type 'help' for available commands):" << std::endl;

    std::string command;
    bool authenticated = true;

    while (true)
    {
        {
            std::lock_guard<std::mutex> lock(outputMutex);
            std::cout << "> ";
        }
        std::getline(std::cin, command);

#ifdef DEBUG
        std::cout << "[DEBUG] Command entered: " << command << std::endl;
#endif

        // check for program specific commands
        if (command == "exit")
        {
            // send a proper disconnect notification to the server
            std::string disconnectCmd = "DISCONNECT";
            if (sendCommand(sock, serverAddr, disconnectCmd))
            {
                std::string response;
                receiveResponse(sock, response);
                std::cout << "Disconnected from server." << std::endl;
            }
            
            // stop all threads
            running.store(false);
            
            // reset the global variables
            g_authenticated = false;
            g_socket = INVALID_SOCKET;
            
            // clean up the receiver thread if it exists
            if (receiverThread)
            {
                receiverThread->join();
                delete receiverThread;
                receiverThread = nullptr;
                g_receiverThread = nullptr; 
            }
            
            // close socket and exit
            closesocket(sock);
            exit(0);
        }
        else if (command == "help")
        {
            printHelp();
            continue;
        }
        // if the user times out they can always reauthenticate by using the already existing AUTH net command
        else if (command.substr(0, 5) == "auth ")
        {
            if (command.length() > 5)
            {
                password = command.substr(5);
                
                // check for empty password
                if (password.empty()) {
                    std::cout << "ERROR: Empty passwords are not allowed for security reasons." << std::endl;
                    std::cout << "Please provide a valid password." << std::endl;
                    continue;
                }
                
                if (authenticate(sock, serverAddr, password, disableEncryption))
                {
                    authenticated = true;
                    std::cout << "Re-authentication successful." << std::endl;
                }
            }
            else
            {
                std::cout << "Usage: auth <password>" << std::endl;
                std::cout << "Note: Empty passwords are not allowed." << std::endl;
            }
            continue;
        }
        // runtime command to test if encryption works, development use i dont intend for the public to use it
        else if (command == "test-encryption")
        {
            std::cout << "Testing encryption..." << std::endl;

            // test message
            std::string testMsg = "Hello, this is a test message!";
            std::vector<uint8_t> encrypted;

            if (g_clientEncryption && g_clientEncryption->encrypt(
                                          reinterpret_cast<const uint8_t *>(testMsg.c_str()),
                                          testMsg.length(),
                                          encrypted))
            {

                std::cout << "Original message: " << testMsg << std::endl;
                std::cout << "Encrypted size: " << encrypted.size() << " bytes" << std::endl;
                std::cout << "First 16 bytes (hex): ";
                size_t limit = encrypted.size() < 16 ? encrypted.size() : 16;
                for (size_t i = 0; i < limit; i++)
                {
                    printf("%02X ", encrypted[i]);
                }
                std::cout << std::endl;

                // decrypt it back
                std::vector<uint8_t> decrypted;
                if (g_clientEncryption->decrypt(encrypted.data(), encrypted.size(), decrypted))
                {
                    std::string decryptedMsg(decrypted.begin(), decrypted.end());
                    std::cout << "Decrypted message: " << decryptedMsg << std::endl;
                    std::cout << "Encryption working: " << (testMsg == decryptedMsg ? "YES" : "NO") << std::endl;
                }
                else
                {
                    std::cout << "Decryption failed!" << std::endl;
                }
            }
            else
            {
                std::cout << "Encryption not available" << std::endl;
            }
            continue;
        }
        else if (command == "logstream-gameevents")
        {
            if (!streamGameEvents)
            {
                std::string startCmd = "STARTGAMELOG";
                if (!sendCommand(sock, serverAddr, startCmd))
                {
                    std::cerr << "Failed to send game event log start command: " << SOCKET_ERROR_CODE << std::endl;
                }
                else
                {
                    std::string response;
                    if (receiveResponse(sock, response))
                    {
                        std::cout << "Game event logging started" << std::endl;
                    }
                }

                streamGameEvents = true;

                if (!streamServerLog && !receiverThread)
                {
                    running.store(true);
                    receiverThread = new std::thread(logReceiver, sock, serverAddr);
                }
            }
            else
            {
                std::cout << "Game event logging already active" << std::endl;
            }
            continue;
        }
        else if (command == "logstream-serverlog")
        {
            if (!streamServerLog)
            {
                std::string startCmd = "STARTSERVERLOG";
                if (!sendCommand(sock, serverAddr, startCmd))
                {
                    std::cerr << "Failed to send server log start command: " << SOCKET_ERROR_CODE << std::endl;
                }
                else
                {
                    std::string response;
                    if (receiveResponse(sock, response))
                    {
                        std::cout << "Server logging started" << std::endl;
                    }
                }

                streamServerLog = true;

                if (!streamGameEvents && !receiverThread)
                {
                    running.store(true);
                    receiverThread = new std::thread(logReceiver, sock, serverAddr);
                }
            }
            else
            {
                std::cout << "Server logging already active" << std::endl;
            }
            continue;
        }
        else if (command == "!logstream-gameevents")
        {
            if (streamGameEvents)
            {
                std::string stopCmd = "STOPGAMELOG";
                if (!sendCommand(sock, serverAddr, stopCmd))
                {
                    std::cerr << "Failed to send game event log stop command: " << SOCKET_ERROR_CODE << std::endl;
                }
                else
                {
                    std::string response;
                    if (receiveResponse(sock, response))
                    {
                        std::cout << "Game event logging stopped" << std::endl;
                    }
                }

                streamGameEvents = false;

                if (!streamServerLog && receiverThread)
                {
                    running.store(false);
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
        else if (command == "!logstream-serverlog")
        {
            if (streamServerLog)
            {
                std::string stopCmd = "STOPSERVERLOG";
                if (!sendCommand(sock, serverAddr, stopCmd))
                {
                    std::cerr << "Failed to send server log stop command: " << SOCKET_ERROR_CODE << std::endl;
                }
                else
                {
                    std::string response;
                    if (receiveResponse(sock, response))
                    {
                        std::cout << "Server logging stopped" << std::endl;
                    }
                }

                streamServerLog = false;

                if (!streamGameEvents && receiverThread)
                {
                    running.store(false);
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
        else if (command == "logstream-all")
        {
            bool startedAny = false;

            if (!streamGameEvents)
            {
                std::string startCmd = "STARTGAMELOG";
                if (!sendCommand(sock, serverAddr, startCmd))
                {
                    std::cerr << "Failed to send game event log start command: " << SOCKET_ERROR_CODE << std::endl;
                }
                else
                {
                    std::string response;
                    if (receiveResponse(sock, response))
                    {
                        std::cout << "Game event logging started" << std::endl;
                        startedAny = true;
                    }
                }

                streamGameEvents = true;
            }

            if (!streamServerLog)
            {
                std::string startCmd = "STARTSERVERLOG";
                if (!sendCommand(sock, serverAddr, startCmd))
                {
                    std::cerr << "Failed to send server log start command: " << SOCKET_ERROR_CODE << std::endl;
                }
                else
                {
                    std::string response;
                    if (receiveResponse(sock, response))
                    {
                        std::cout << "Server logging started" << std::endl;
                        startedAny = true;
                    }
                }

                streamServerLog = true;
            }

            // start the receiver thread if its not running and if we started at least one stream
            if (startedAny && !receiverThread)
            {
                running.store(true);
                receiverThread = new std::thread(logReceiver, sock, serverAddr);
            }

            if (!startedAny)
            {
                std::cout << "All log streams already active" << std::endl;
            }

            continue;
        }
        else if (command == "!logstream-all")
        {
            bool wasActive = streamGameEvents || streamServerLog;

            // stop game events stream if active
            if (streamGameEvents)
            {
                std::string stopCmd = "STOPGAMELOG";
                if (!sendCommand(sock, serverAddr, stopCmd))
                {
                    std::cerr << "Failed to send game event log stop command: " << SOCKET_ERROR_CODE << std::endl;
                }
                else
                {
                    std::string response;
                    receiveResponse(sock, response);
                }

                // even if we dont get a response, we know we tried to stop it
                streamGameEvents = false;
            }

            // stop server log stream if active
            if (streamServerLog)
            {
                std::string stopCmd = "STOPSERVERLOG";
                if (!sendCommand(sock, serverAddr, stopCmd))
                {
                    std::cerr << "Failed to send server log stop command: " << SOCKET_ERROR_CODE << std::endl;
                }
                else
                {
                    std::string response;
                    receiveResponse(sock, response);
                }

                streamServerLog = false;
            }

            if (!streamGameEvents && !streamServerLog && receiverThread)
            {
                running.store(false);
                receiverThread->join();
                delete receiverThread;
                receiverThread = nullptr;
            }

            if (wasActive)
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
        else if (command == "--logstream-gameevents")
        {
            // parse the command to the runtime equivilant
            command = "logstream-gameevents";
            continue;
        }
        else if (command == "--logstream-serverlog")
        {
            command = "logstream-serverlog";
            continue;
        }

        std::string fullCommand = "EXEC " + command;
#ifdef DEBUG
        std::cout << "[DEBUG] Sending command: " << fullCommand << std::endl;
#endif

        if (!sendCommand(sock, serverAddr, fullCommand))
        {
            std::cerr << "Failed to send command: " << SOCKET_ERROR_CODE << std::endl;
            continue;
        }

        if (command == "quit" || command.substr(0, 4) == "/set")
        {
            std::cout << "Command sent." << std::endl;
        }

        std::string response;
        if (!receiveResponse(sock, response))
        {
            int error = SOCKET_ERROR_CODE;
#ifdef _WIN32
            if (error == WSAETIMEDOUT)
            {
#else
            if (error == ETIMEDOUT)
            {
#endif
                continue;
            }
            else
            {
                std::cerr << "Failed to receive response: " << error << std::endl;
                continue;
            }
        }

#ifdef DEBUG
        std::cout << "[DEBUG] Response received: " << response << std::endl;
#endif

        // only print valid text responses
        if (isValidTextResponse(response))
        {
            std::cout << response << std::endl;
        }
#ifdef DEBUG
        else
        {
            std::cout << "[DEBUG] Filtered out garbled response text" << std::endl;
        }
#endif

        // check if we need to reauthenticate
        if (strstr(response.c_str(), "STATUS 401") != nullptr &&
            strstr(response.c_str(), "Authentication required") != nullptr)
        {
            authenticated = false;
            // tell the user to reauthenticate
            std::cout << "MESSAGE Re-enter your password to continue" << std::endl;
            std::cout << "Type 'auth <password>' to re-authenticate" << std::endl;
        }
        // check if the ip address is blocked
        else if (strstr(response.c_str(), "STATUS 403") != nullptr)
        {
            std::cout << "Your IP has been locked out due to too many failed authentication attempts." << std::endl;
            std::cout << "The server will need to be restarted before you can connect again." << std::endl;
            std::cout << "Press Enter to exit..." << std::endl;
            std::getline(std::cin, command);
            break;
        }
    }

    running.store(false);

    if (receiverThread)
    {
        receiverThread->join();
        delete receiverThread;
        g_receiverThread = nullptr;
    }

    // send a disconnect notification to the server
    if (g_authenticated)
    {
        std::string disconnectCmd = "DISCONNECT";
        if (sendCommand(sock, serverAddr, disconnectCmd))
        {
            std::string response;
            receiveResponse(sock, response);
        }
    }

    // reset globals
    g_authenticated = false;
    g_socket = INVALID_SOCKET;
    
    closesocket(sock);
    return 0;
}