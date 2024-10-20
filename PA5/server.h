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

#ifndef SOCK_NONBLOCK
#include <fcntl.h>
#define SOCK_NONBLOCK O_NONBLOCK
#endif

using namespace std;

// MESSAGE TAGS:
// SYSTEM - CONNECT, MESSAGE, CLIENT, DISCONNECT, COMMAND, UNKNOWN

class Server
{
public:
    Server(int portnumber, string password);
    ~Server();

    int InitializeServer();

    void ClearBuffer();
    int CheckMessages();
    void Log(string message);
    void LogError(string message);
    int open_socket(int portno);
    void StripServerMessage(int message_length, string &command, vector<string> &variables);
    int CheckClientPassword(string password, int &clientSock, int socketNum);

    // May wanna change this to simply be each individual command/message
    void ReceiveClientCommand();
    int ReceiveServerCommand(int message_length);
    
    int SendHELO();
    int SendSERVERS();
    int SendKEEPALIVE();
    int SendGETMSGS();
    int SendSENDMSG();
    int SendSTATUSREQ();
    int SendSTATUSRESP();


    int listenSock;                 // Socket for connections to server
    int portnum;
    int timeout = 50;               // Timeout for Poll()

    int new_socket;                 // Temp variables for accepting new connections.
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    int clientSock = INT32_MAX;                 // Socket of connecting client
    struct sockaddr_in client;
    socklen_t clientLen;

    struct pollfd server_pollfd;
    
    char buffer[5121];              // buffer for reading from clients
    int buffer_size = 5120;
    vector<pollfd> file_descriptors;
    vector<string> connection_names;
    map<string, pair<string, int>> list_of_connections;
    map<string, vector<string>> message_buffer; // Stores messages for groups.

    string client_password;                // Responses and basic strings.
    string acceptMessage = "Welcome [CLIENT], how can I help you today?";
    string errorMessage = "\x01 ERROR,UNKOWN_COMMAND\x04";

private:
    int BACKLOG = 5;
};