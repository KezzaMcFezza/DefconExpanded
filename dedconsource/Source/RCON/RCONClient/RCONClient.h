/*
 *  * DedCon RCON Client: Remote Access for Dedcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  * Copyright (C) 2025 Keiron Mcphee
 *  *
 *  */

#ifndef RCONCLIENT_H
#define RCONCLIENT_H

#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <thread>
#include <atomic>
#include <mutex>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#define SOCKET_ERROR_CODE WSAGetLastError()
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#define SOCKET_ERROR_CODE errno
#endif

extern bool streamGameEvents;
extern bool streamServerLog;
extern std::atomic<bool> running;
extern std::mutex outputMutex;

void signalHandler                                                              (int signum);
void cleanup                                                                    ();
bool initialize                                                                 ();
bool setSocketTimeout                                                           (SOCKET sock, int timeoutMs);
bool resolveHostname                                                            (const char *hostname, struct in_addr *addr);
bool authenticate                                                               (SOCKET sock, const struct sockaddr_in &serverAddr, const std::string &password, bool disableEncryption);
bool sendCommand                                                                (SOCKET sock, const struct sockaddr_in &serverAddr, const std::string &command);
bool receiveResponse                                                            (SOCKET sock, std::string &response);
bool isValidTextResponse                                                        (const std::string &text);
void printHelp                                                                  ();
void logReceiver                                                                (SOCKET sock, const struct sockaddr_in &serverAddr);

#endif // RCONCLIENT_H