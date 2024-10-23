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
#include <queue>
#include <time.h>

#include <iostream>
#include <sstream>
#include <thread>
#include <map>

#include <fcntl.h>
#include <poll.h>
#include <errno.h>

#include <unistd.h>

#ifndef SOCK_NONBLOCK
#include <fcntl.h>
#define SOCK_NONBLOCK O_NONBLOCK
#endif

using namespace std;

// MESSAGE TAGS FOR LOGS:
// SYSTEM - CONNECT, MESSAGE, CLIENT, DISCONNECT, COMMAND, UNKNOWN

//SYSTEM: For if some function that the server relies on fails, Example binding a socket 
//CONNECT: Logs Relating to attempting connection
//MESSAGE: Something has been recived and needs to be parsed
//CLIENT: Logs specificly related to the client
//DISCONNECT: If a disconnect occurs
//COMMAND; Relating to commands (fails or succesful executions)
//UNKNOWN: essentaily misc, if something unknown or unexpected happens
//SENDING: Whenever a message is sent, it should have the sending tag.



class Server
{
public:
    Server(int portnumber, string password);
    ~Server();

    void InitializeServer();
    void CheckForMoreConnections();
    bool ConnectToServer(string ip, int port);
    void CheckTimeouts();
    void CheckKeepalive();

    void ClearBuffer();
    int CheckMessages();
    void Log(string message);
    void LogError(string message);
    int open_socket(int portno);
    void StripServerMessage(int message_length, string &command, vector<string> &variables);
    void StripClientMessage(int message_length, string &command, vector<string> &variables);
    int CheckClientPassword(string password, int &clientSock, int socketNum);

    // May wanna change this to simply be each individual command/message
    int ReceiveClientCommand(int message_length);
    int ReceiveServerCommand(int message_length, int fd);
    
    int RespondHELO(int fd, vector<string> variables);
    int RespondKEEPALIVE(int fd, vector<string> variables);
    int RespondGETMSGS(int fd, vector<string> variables);
    int RespondLISTSERVERS();
    int RespondSERVERS(vector<string> variables);
    int RespondSTATUSREQ(int fd);
    int RespondSTATUSRESP(int fd, vector<string> variables);

    int RespondCONNECTSERVER(vector<string> variables);
    int RespondGetMSG(string group_id); // For the Client only. It reads from our_message_buffer;

    int SendHELO(int fd);
    int SendSERVERS(int fd);
    int SendKEEPALIVE();
    int SendGETMSGS(int fd);
    int SendSENDMSG(int fd, string to_group_name, string from_group_name, string data);
    int SendSTATUSREQ(int fd);

    int connected_servers = 0;
    int max_server_capacity = 8;
    int min_server_capacity = 3;
    double expiration_of_servers = 300;
    double keepalive_frequency = 120;
    time_t last_keepalive;
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
    
    char buffer[5120];              // buffer for reading from clients
    int buffer_size = 5120;
    vector<pollfd> file_descriptors;
    vector<string> connection_names;
    map<string, pair<string, int>> list_of_connections; // Key : Name of group - Value: Pair(String of IP, Int port number)
    map<string, pair<string, int>> documented_servers; // Key : Name of group - Value: Pair(String of IP, Int port number)
    map<string, vector<pair<string, string>>> other_groups_message_buffer; // Stores messages for other groups. Key: Name of group - Value: list of pairs(From group name, message)
    map<string, queue<string>> our_message_buffer; // Stores messages for ourselves.°
    map<int, time_t> socket_timers;
    map<int, int> helo_received;
    map<int, string> fd_to_group_name; 
    map<string, int> group_name_to_fd; 

    string group_name = "A5_23";
    string client_name = "CLIENT";
    string client_password;                // Responses and basic strings.
    string acceptMessage = "Welcome [CLIENT], how can I help you today?";
    string errorMessage = "\x01 ERROR,UNKOWN_COMMAND\x04";

private:
    int BACKLOG = 8;
};