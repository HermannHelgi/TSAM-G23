#include <stdio.h>
#include <cstdio>
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
#include <time.h>
#include <poll.h>
#include <errno.h>
#include <algorithm>
#include <map>
#include <vector>
#include <list>
#include <queue>
#include <iostream>
#include <sstream>
#include <thread>
#include <fcntl.h>

#ifndef SOCK_NONBLOCK
#include <fcntl.h>
#define SOCK_NONBLOCK O_NONBLOCK
#endif

using namespace std;

// MESSAGE TAGS FOR LOGS:
// SYSTEM - CONNECT, MESSAGE, CLIENT, DISCONNECT, COMMAND, UNKNOWN

// SYSTEM: For if some library function that the server relies on fails, Example binding a socket 
// CONNECT: Logs Relating to attempted connection
// MESSAGE: Something has been recived and needs to be parsed
// CLIENT: Logs specifically related to the client
// DISCONNECT: If a disconnect occurs
// COMMAND; Relating to commands (fails or successful executions)
// UNKNOWN: Essentaily misc, if something unknown or unexpected happens
// SENDING: Whenever a message is sent.

class Server
{
public:
    Server(int portnumber);
    ~Server();

    // Main Server functions
    void InitializeServer();
    int CheckMessages();
    void CheckTimeouts();
    void CheckKeepalive();
    void CheckForMoreConnections();

    // Helper Functions
    void CloseConnection(int fd, int i);
    bool CheckServerConnection(string name, string IP, int port);
    bool ConnectToServer(string ip, int port);
    void ClearBuffer();
    void Log(string message);
    void LogError(string message);
    int OpenSocket(int portno);
    
    // Botnet commands. 
    // Respond means that the server is usually taking in some data and sending something back.
    // Send means the server just sends that command.
    int ReceiveServerCommand(int message_length, int fd);
    void StripServerMessage(int message_length, vector<string> &commands, vector<vector<string>> &variables);
    int RespondHELO(int fd, vector<string> variables);
    int RespondKEEPALIVE(int fd, vector<string> variables);
    int RespondGETMSGS(int fd, vector<string> variables);
    int RespondSERVERS(vector<string> variables);
    int RespondSTATUSREQ(int fd);
    int RespondSTATUSRESP(int fd, vector<string> variables);

    int SendHELO(int fd);
    int SendSERVERS(int fd);
    int SendKEEPALIVE(int fd);
    int SendGETMSGS(int fd);
    int SendSENDMSG(int fd, string to_group_name, string from_group_name, string data);
    int SendSTATUSREQ(int fd);
    
    // Client commands.
    int ReceiveClientCommand(int message_length);
    void StripClientMessage(int message_length, string &command, vector<string> &variables);
    int CheckClientPassword(string password, int &clientSock, int socketNum);
    
    int RespondLISTSERVERS();
    int RespondCONNECTSERVER(vector<string> variables);
    int RespondGetMSG(string group_id);             // For the Client only. It reads from our_message_buffer;
    int RespondMESSAGEBUFFER();
    int RespondDOCSERVERS();
    
    // Variables.
    int connected_servers = 0;                      // How many bots are connected.
    int max_server_capacity = 8;                    // How many bots the server should be connected to.
    int min_server_capacity = 4;                    // The minimum amount of bots the server should keep in contact with. 
    int max_variables = 10000;                      // Ceiling on amount of variables another bot can send.
    int keepalive_packets = 0;

    double keepalive_frequency = 60;                // How often the server sends a KEEPALIVE message.
    time_t last_keepalive;                          // Last time the server sent a keepalive.
    int listenSock;                                 // Socket for connections to server
    int portnum;                                    // The servers port number.
    int timeout = 100;                              // Timeout for Poll()

    int new_socket;                                 // Temp variables for accepting new connections.
    struct sockaddr_in address;     
    int addrlen = sizeof(address);

    int clientSock = INT32_MAX;                     // Socket of connecting client, if its set to INT32_MAX that means no client is present.
    struct sockaddr_in client;
    socklen_t clientLen;

    char buffer[5120];                              // buffer for reading from clients
    int buffer_size = 5120;                         // Size of buffer;
    vector<pollfd> file_descriptors;                // Vector of all file descriptors.
    vector<string> connection_names;                // Vector of names of all connected bots.

    // Map of all connections, including the name of the group, their IP and PORT.
    map<string, pair<string, int>> list_of_connections; // Key : Name of group - Value: Pair(String of IP, Int port number)

    // Map of all documented connections gained from SERVERS command, including the name of the group, their IP and PORT. Used to connect to new servers.
    map<string, pair<string, int>> documented_servers; // Key : Name of group - Value: Pair(String of IP, Int port number)
    map<string, vector<pair<string, string>>> other_groups_message_buffer; // Stores messages for other groups. Key: Name of group - Value: list of pairs(From group name, message)
    map<string, queue<string>> our_message_buffer;  // Stores messages for ourselves.
    vector<string> pending_connections;             // Used to prevent the server from trying to connect to the same bot twice.

    map<int, time_t> socket_timers;                 // Timers for all bots, updates whenever a specific bot sends a message. 
    double expiration_of_servers = 300;             // Timeout on bots, if they have been silent for longer than this time (in seconds) they are disconnected.
    map<int, int> helo_received;                    // Map of whether a new bot has sent a HELO message or not. -1 == no, 1 == yes.
    map<int, string> fd_to_group_name;              // Map to translate File descriptor to the group name
    map<string, int> group_name_to_fd;              // Map to translate group name to File descriptor
    map<int, int> strike_counter;                   // Map of strike counts. From FD to amount of strikes.

    // Name variables and presets.
    string ip_address = "130.208.246.249";          // Server IP address. Must be manuallly changed on relocation.
    string localhost = "127.0.0.1";
    string group_name = "A5_23";
    string client_name = "CLIENT";
    string client_password = "Admin123";            // Password for Client/Server connection.
    string acceptMessage = "Welcome [CLIENT], how can I help you today?";
    string failureMessageClient = "Unrecognized command.";
    string errorMessage = "\x01" "ERROR,UNKNOWN_COMMAND";

    vector<string> blacklist = {"A5_300"};          // Manual Blacklist added.
private:
    int BACKLOG = 8;
};