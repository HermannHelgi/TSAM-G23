#include "server.h"

Server::Server(int portnumber, string password) 
{
    portnum = portnumber;
    client_password = password;
}

Server::~Server() {}

int Server::InitializeServer()
{
    listenSock = open_socket(portnum); 
    Log(string("// SYSTEM // Starting listen on port " + to_string(portnum)));

    if(listen(listenSock, BACKLOG) < 0)
    {
        LogError(string("// SYSTEM // Listen failed on port " + to_string(portnum)));
        exit(0);
    }

    server_pollfd = {listenSock, POLLIN, 0};
    file_descriptors.emplace_back(server_pollfd);
}

int Server::CheckMessages()
{


    // Get modifiable copy of readSockets
    // Look at sockets and see which ones have something to be read()
    int n = poll(file_descriptors.data(), file_descriptors.size(), timeout);

    if (n < 0)
    {
        LogError(string("// SYSTEM // Poll ran into a problem, considered critical, shutting down."));
        return -1;
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
                    int valread = recv(file_descriptors[i].fd, buffer, buffer_size, MSG_DONTWAIT);

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
                            ClientCommand();
                        }
                        else
                        {
                            Log(string("// COMMAND // New command from Server: " + to_string(file_descriptors[i].fd)));
                            int val = ServerCommand(valread, list_of_connections);

                            if (val == -1 && clientSock == INT32_MAX) // Might be client trying to connect.
                            {
                                Log(string("// UNKNOWN // Unknown command from Server, checking for client password: " + to_string(file_descriptors[i].fd)));
                                val = CheckClientPassword(client_password, clientSock, file_descriptors[i].fd);

                                // Just an Unknown message
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
    return 0;
}

void Server::ClearBuffer()
{
    memset(buffer, 0, sizeof(buffer));
}

void Server::LogError(string message)
{
    // Making time stamp
    char timebuffer[64];
    time_t timestamp = time(NULL);
    struct tm* localTime = localtime(&timestamp);
    strftime(timebuffer, sizeof(timebuffer), "[%Y-%m-%d %H:%M:%S] ", localTime);

    cerr << timebuffer;
    cerr << message << endl;
}

void Server::Log(string message)
{
    // Making time stamp
    char timebuffer[64];
    time_t timestamp = time(NULL);
    struct tm* localTime = localtime(&timestamp);
    strftime(timebuffer, sizeof(timebuffer), "[%Y-%m-%d %H:%M:%S] ", localTime);

    cout << timebuffer;
    cout << message << endl;
}

int Server::open_socket(int portno)
{
    // Open socket for specified port.
    // Returns -1 if unable to create the socket for any reason.
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

void Server::StripServerMessage(int message_length, string &command, vector<string> &variables)
{
    string main = buffer + '\0';
    string buffered_string = buffer + '\0';
    cout << message_length << endl;
    if (buffer[0] == '\x01')
    {
        main = buffered_string.substr(1);
        size_t find_EOT = main.find('\x04');
        if (find_EOT != string::npos)
        {
            main = main.substr(0, find_EOT);
        }
        else
        {
            LogError("// MESSAGE // Missing EOT Symbol.");
        }
    }
    else
    {
        LogError("// MESSAGE // Missing SOH Symbol.");
        size_t find_EOT = main.find('\x04');
        if (find_EOT != string::npos)
        {
            main = buffered_string.substr(0, find_EOT);
        }
        else
        {
            LogError("// MESSAGE // Missing EOT Symbol.");
        }
    }

    size_t old_comma_index = 0;
    size_t comma_index = main.find(',');
    if (comma_index != string::npos)
    {
        command = main.substr(0, comma_index);
        while (comma_index != string::npos)
        {
            old_comma_index = comma_index;
            comma_index = main.find(',', (old_comma_index+1));
            variables.emplace_back(main.substr(old_comma_index+1, comma_index));
            if (comma_index != std::string::npos) 
            {
                variables.emplace_back(main.substr(old_comma_index + 1, comma_index - old_comma_index - 1));
            } 
            else 
            {
                variables.emplace_back(main.substr(old_comma_index + 1));
            }
        }
    }
    else
    {
        command = main;
    }
}

int Server::CheckClientPassword(string password, int &clientSock, int socketNum)
{
    string passwordCheck = buffer;
    if (password == passwordCheck)
    {
        clientSock = socketNum;
        return 1;
    }
    return -1;
}

int Server::SendServerCommand()
{
    // TODO
    // Should send messages to specified servers.
}

int Server::ServerCommand(int message_length, map<string, pair<string, int>> &list_of_connections)
{
    string command;
    vector<string> variables;
    StripServerMessage(message_length, command, variables);
    
    if (command == "HELO")
    {
        Log(string("// COMMAND // HELO detected. Taking in data."));
        if (variables.size() == 3)
        {
            list_of_connections[variables[0]] = {variables[1], stoi(variables[2])};
            // TODO: Should respond with SERVERS command.
        }
        else
        {
            LogError(string("// COMMAND // Not correct amount of variables in HELO command. Aborting."));
            return -1;
        }
    }
    else if (command == "SERVERS")
    {
        Log(string("// COMMAND // SERVERS detected. Taking in data."));
        // TODO
        // Don't really know what we should do with this stuff? Maybe check if we're not ocnnected to enough servers then send out some connections?
    }
    else if (command == "KEEPALIVE")
    {
        // TODO
        // LOG
    }
    else if (command == "GETMSGS")
    {
        // TODO
        // LOG
    }
    else if (command == "SENDMSG")
    {
        // TODO
        // LOG
    }
    else if (command == "STATUSREQ")
    {
        // TODO
        // Should respond with a STATUSRESP message.
        // LOG
    }
    else if (command == "STATUSRESP")
    {
        // TODO
        // LOG
    }
    else
    {
        // DO NOT LOG UNKOWN MESSAGES HERE! OTHERWISE IT MIGHT LEAK THE PASSWORD TO THE LOG FILE WHICH ANYONE CAN READ!!!  
        return -1;
    }
}

// Process command from client on the server
void Server::ClientCommand()
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