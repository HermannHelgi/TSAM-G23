//
// Simple chat server for TSAM-409
//
// Command line: ./chat_server 4000 
//
// Author: Jacky Mallett (jacky@ru.is)
//
#include <stdio.h>
#include <cstdio>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <algorithm>
#include <map>
#include <vector>
#include <list>
#include <poll.h>

#include <iostream>
#include <sstream>
#include <thread>
#include <map>

#include <unistd.h>

// fix SOCK_NONBLOCK for OSX
#ifndef SOCK_NONBLOCK
#include <fcntl.h>
#define SOCK_NONBLOCK O_NONBLOCK
#endif

#define BACKLOG  5          // Allowed length of queue of waiting connections

using namespace std;

// Open socket for specified port.
// Returns -1 if unable to create the socket for any reason.

int open_socket(int portno)
{
   struct sockaddr_in sk_addr;   // address settings for bind()
   int sock;                     // socket opened for this port
   int set = 1;                  // for setsockopt

   // Create socket for connection. Set to be non-blocking, so recv will
   // return immediately if there isn't anything waiting to be read.
#ifdef __APPLE__     
   if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
   {
    LogError(string("// SYSTEM // Failed to open socket."));
      return(-1);
   }
#else
   if((sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0)
   {
    LogError(string("// SYSTEM // Failed to open socket."));
    return(-1);
   }
#endif

   // Turn on SO_REUSEADDR to allow socket to be quickly reused after 
   // program exit.

   if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set)) < 0)
   {
    LogError(string("// SYSTEM // Failed to set SO_REUSEADDR."));
   }
   set = 1;
#ifdef __APPLE__     
   if(setsockopt(sock, SOL_SOCKET, SOCK_NONBLOCK, &set, sizeof(set)) < 0)
   {
    LogError(string("// SYSTEM // Failed to set SOCK_NOBBLOCK."));
   }
#endif
   memset(&sk_addr, 0, sizeof(sk_addr));

   sk_addr.sin_family      = AF_INET;
   sk_addr.sin_addr.s_addr = INADDR_ANY;
   sk_addr.sin_port        = htons(portno);

   // Bind to socket to listen for connections from clients

   if(bind(sock, (struct sockaddr *)&sk_addr, sizeof(sk_addr)) < 0)
   {
    LogError(string("// SYSTEM // Failed to bind socket."));
      return(-1);
   }
   else
   {
      return(sock);
   }
}

// Process command from client on the server
void ClientCommand(char buffer[])
{
    string message = buffer;

    if (message.substr(0, 6) == "GETMSG")
    {
        // LOG
        // TODO

    }
    else if (message.substr(0, 7) == "SENDMSG")
    {
        // LOG
        // TODO

    }
    else if (message.substr(0, 10) == "LISTSERVERS")
    {
        // LOG
        // TODO

    }
    else // Unknown
    {
        // LOG
        LogError(string("// CLIENT // Unknown command from client."));
    }
}

int CheckClientPassword(char buffer[], string password, int &clientSock, int socketNum)
{
    string passwordCheck = buffer;
    if (password == passwordCheck)
    {
        clientSock = socketNum;
        return 1;
    }
    return -1;
}

int ServerCommand(char buffer[])
{
    // TODO
    string message = buffer;



    // If message is not recognized:

    // DO NOT LOG UNKOWN MESSAGES HERE! OTHERWISE IT MIGHT LEAK THE PASSWORD TO THE LOG FILE WHICH ANYONE CAN READ!!!  
    return -1;
}

void LogError(string message)
{
    // Making time stamp
    char timebuffer[64];
    time_t timestamp = time(NULL);
    struct tm* localTime = localtime(&timestamp);
    strftime(timebuffer, sizeof(timebuffer), "[%Y-%m-%d %H:%M:%S] ", localTime);

    cerr << timebuffer;
    cerr << message << endl;
}

void Log(string message)
{
    // Making time stamp
    char timebuffer[64];
    time_t timestamp = time(NULL);
    struct tm* localTime = localtime(&timestamp);
    strftime(timebuffer, sizeof(timebuffer), "[%Y-%m-%d %H:%M:%S] ", localTime);

    cout << timebuffer;
    cout << message << endl;
}

int main(int argc, char* argv[])
{
    // MESSAGE TAGS:
    // SYSTEM - CONNECT, MESSAGE, CLIENT, DISCONNECT, COMMAND, UNKNOWN

    bool finished;
    int listenSock;                 // Socket for connections to server
    int timeout = 50;               // Timeout for Poll()

    int new_socket;                 // Temp variables for accepting new connections.
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    int clientSock = INT32_MAX;                 // Socket of connecting client
    struct sockaddr_in client;
    socklen_t clientLen;
    
    char buffer[1025];              // buffer for reading from clients
    vector<pollfd> file_descriptors;
    vector<string> connection_names;

    // Sets logging pattern to send all clog and cerr into log.txt
    freopen("ErrorLog.txt", "a", stderr);
    freopen("Log.txt", "a", stdout);

    string password;                // Responses and basic strings.
    string acceptMessage = "Welcome [CLIENT], how can I help you today?";
    string errorMessage = "ERROR,UNKOWN_COMMAND";

    if(argc != 3)
    {
        printf("Usage: chat_server <ip port> <password for client connection>\n");
        exit(0);
    }

    password = argv[2];

    // Setup socket for server to listen to
    listenSock = open_socket(atoi(argv[1])); 
    Log(string("// SYSTEM // Starting listen on port " + to_string(atoi(argv[1]))));

    if(listen(listenSock, BACKLOG) < 0)
    {
        LogError(string("// SYSTEM // Listen failed on port " + to_string(atoi(argv[1]))));
        exit(0);
    }

    struct pollfd server_pollfd = {listenSock, POLLIN, 0};
    file_descriptors.emplace_back(server_pollfd);

    finished = false;
    while(!finished)
    {
        // Get modifiable copy of readSockets
        memset(buffer, 0, sizeof(buffer));

        // Look at sockets and see which ones have something to be read()
        int n = poll(file_descriptors.data(), file_descriptors.size(), timeout);

        if (n < 0)
        {
            LogError(string("// SYSTEM // Poll ran into a problem, considered critical, shutting down."));
            finished = true;
        }
        else
        {
            for (int i = 0; i < file_descriptors.size(); i++)
            {
                // There is something to be read!
                if (file_descriptors[i].revents & POLLIN)
                {
                    // The server has something to read, meaning there's a connection to be made
                    if (file_descriptors[i].fd == listenSock)
                    {
                        Log(string("// CONNECT // New connection trying to be made."));
                        if ((new_socket = accept(listenSock, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
                            LogError(string("// CONNECT // Failed to make new connection."));
                            continue;
                        }

                        // New connection made
                        struct pollfd new_pollfd = {new_socket, POLLIN, 0};
                        file_descriptors.push_back(new_pollfd);
                        Log(string("// CONNECT // New connection made: " + to_string(new_pollfd.fd)));
                    }
                    else
                    {
                        Log(string("// MESSAGE // New message received from: " + to_string(file_descriptors[i].fd)));
                        int valread = recv(file_descriptors[i].fd, buffer, 1024, MSG_DONTWAIT);

                        if (valread <= 0) 
                        {
                            if (file_descriptors[i].fd == clientSock)
                            {
                                Log(string("// CLIENT // Client Disconnected: " + to_string(file_descriptors[i].fd)));
                                close(file_descriptors[i].fd);
                                file_descriptors.erase(file_descriptors.begin() + i);
                                i--;
                                clientSock = INT32_MAX;
                            }
                            else
                            {
                                Log(string("// DISCONNECT // Server Disconnected: " + to_string(file_descriptors[i].fd)));
                                close(file_descriptors[i].fd);
                                file_descriptors.erase(file_descriptors.begin() + i);
                                i--;
                            }
                        }
                        else
                        {
                            if (file_descriptors[i].fd == clientSock)
                            {
                                Log(string("// CLIENT // New command from Client: " + to_string(file_descriptors[i].fd)));
                                ClientCommand(buffer);
                            }
                            else
                            {
                                Log(string("// COMMAND // New command from Server: " + to_string(file_descriptors[i].fd)));
                                int val = ServerCommand(buffer);

                                if (val == -1 && clientSock == INT32_MAX) // Might be client trying to connect.
                                {
                                    Log(string("// UNKNOWN // Unknown command from Server, checking for client password: " + to_string(file_descriptors[i].fd)));
                                    val = CheckClientPassword(buffer, password, clientSock, file_descriptors[i].fd);

                                    // Just an Unkown message
                                    if (val == -1)
                                    {
                                        LogError(string("// UNKNOWN // Unknown command from server: " + to_string(file_descriptors[i].fd)));
                                        LogError(string(buffer));
                                        send(file_descriptors[i].fd, errorMessage.data(), errorMessage.size(), 0);
                                    }
                                    // This is client
                                    else
                                    {
                                        Log(string("// CLIENT // New client recognized: " + to_string(file_descriptors[i].fd)));
                                        send(file_descriptors[i].fd, acceptMessage.data(), acceptMessage.size(), 0);
                                        continue; 
                                    }
                                }
                                else
                                {
                                    LogError(string("// UNKNOWN // Unknown command from server: " + to_string(file_descriptors[i].fd)));
                                    LogError(string(buffer));
                                    send(file_descriptors[i].fd, errorMessage.data(), errorMessage.size(), 0);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}