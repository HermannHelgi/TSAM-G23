#include "server.h"

Server::Server(int portnumber, string password) 
{
    portnum = portnumber;
    client_password = password;
}

Server::~Server() {}

void Server::CheckKeepalive()
{
    time_t now = time(NULL);

    if (difftime(now, last_keepalive) > keepalive_frequency)
    {
        Log(string("// MESSAGE // Sending status packets."));
        for (int i = 0; i < file_descriptors.size(); i++)
        {
            if (file_descriptors[i].fd != listenSock && file_descriptors[i].fd != clientSock)
            {
                SendSTATUSREQ(file_descriptors[i].fd);
            }
        }
        last_keepalive = time(NULL);
    }
}

int Server::SendSTATUSREQ(int fd)
{
    string statusreq = "STATUSREQ";
    if (send(fd, statusreq.data(), statusreq.length(), 0) < 0)
    {
        LogError(string("// COMMAND // Failed to send STATUSREQ."));
        return -1;
    }
    return 1;
}

void Server::CheckTimeouts()
{
    map<int, time_t>::iterator it = socket_timers.begin(); 
    time_t now = time(NULL);

    while (it != socket_timers.end())
    {
        if (difftime(now, it->second) > expiration_of_servers)
        {
            Log(string("// DISCONNECT // Found a server who has been silent for too long: " + it->first));

            if (helo_received[it->first] == 1)
            {
                int socket = it->first;
                helo_received.erase(socket);
                list_of_connections.erase(fd_to_group_name[socket]);
                connection_names.erase(find(connection_names.begin(), connection_names.end(), fd_to_group_name[socket]));
                group_name_to_fd.erase(fd_to_group_name[socket]);
                fd_to_group_name.erase(socket);
                it = socket_timers.erase(it);

                close(socket);
                file_descriptors.erase(std::find_if(file_descriptors.begin(), file_descriptors.end(), [&](const pollfd& pfd) {
                    return pfd.fd == socket;  // Compare the fd field of pollfd with socket
                }));
                connected_servers--;
            }
            else
            {
                int socket = it->first;
                helo_received.erase(socket);
                it = socket_timers.erase(it);
                close(socket);
                file_descriptors.erase(std::find_if(file_descriptors.begin(), file_descriptors.end(), [&](const pollfd& pfd) {
                    return pfd.fd == socket;  // Compare the fd field of pollfd with socket
                }));
                connected_servers--;
            }
        }
        else
        {
            it++;
            continue;
        }
    }
}

void Server::CheckForMoreConnections()
{
    if (connected_servers < min_server_capacity && documented_servers.size() != connected_servers)
    {
        map<string, pair<string, int>>::iterator it = documented_servers.begin();

        while (it != documented_servers.end())
        {
            if (list_of_connections.count(it->first))
            {
                // ALREADY CONNECTED
                it++;
                continue;
            }
            else if (it->first != group_name)
            {
                // CAN CONNECT
                string server_ip = it->second.first;
                int server_port = it->second.second;

                Log(string("// CONNECT // Attempting new connection with documented server."));
                if (ConnectToServer(server_ip, server_port))
                {
                    Log(string("// CONNECT // New connection established."));
                    it++;

                    if (connected_servers >= 3)
                    {
                        break;
                    }
                }
                else
                {
                    LogError(string("// CONNECT // Failed to set up connection with documented server: " + it->first));
                    it = documented_servers.erase(it);
                }
            }
        }
    }
    return;
}

#include <fcntl.h>
#include <poll.h>
#include <errno.h>

bool Server::ConnectToServer(string ip, int port)
{
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &server_address.sin_addr) <= 0)
    {
        LogError(string("// CONNECT // Invalid IP address."));
        return false;
    }

    int result = connect(listenSock, (struct sockaddr*)&server_address, sizeof(server_address));

    if (result < 0 && errno != EINPROGRESS)
    {
        LogError(string("// CONNECT // Connection to " + ip + ":" + to_string(port) + " failed."));
        return false;
    }

    if (result == 0) {
        connected_servers++;
        struct pollfd new_pollfd = {listenSock, POLLIN, 0};
        file_descriptors.push_back(new_pollfd);
        helo_received[new_pollfd.fd] = -1;
        SendHELO(new_pollfd.fd);
        return true;
    }

    struct pollfd poll_fd = {listenSock, POLLOUT, 0};
    int poll_result = poll(&poll_fd, 1, timeout); 

    if (poll_result > 0 && (poll_fd.revents & POLLOUT))
    {
        connected_servers++;
        struct pollfd new_pollfd = {listenSock, POLLIN, 0};
        file_descriptors.push_back(new_pollfd);
        helo_received[new_pollfd.fd] = -1;
        SendHELO(new_pollfd.fd);
        return true;
    }
    else
    {
        LogError(string("// CONNECT // Connection to " + ip + ":" + to_string(port) + " timed out or failed."));
        return false;
    }
}

void Server::InitializeServer()
{
    listenSock = open_socket(portnum); 
    Log(string("// SYSTEM // Starting listen on port " + to_string(portnum)));

    if(listen(listenSock, BACKLOG) < 0)
    {
        LogError(string("// SYSTEM // Listen failed on port " + to_string(portnum)));
        exit(0);
    }

    last_keepalive = time(NULL);
    server_pollfd = {listenSock, POLLIN, 0};
    file_descriptors.emplace_back(server_pollfd);
    
    int flags = fcntl(listenSock, F_GETFL, 0);
    if (flags < 0)
    {
        LogError("// SYSTEM // Failed to get socket flags.");
    }

    if (fcntl(listenSock, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        LogError("// SYSTEM // Failed to set non-blocking mode.");
    }
}

int Server::CheckMessages()
{
    ClearBuffer();
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
                    if (connected_servers < max_server_capacity) 
                    {
                        if ((new_socket = accept(listenSock, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
                            LogError(string("// CONNECT // Failed to make new connection."));
                            continue;
                        }

                        // New connection made
                        socket_timers[file_descriptors[i].fd] = time(NULL);
                        connected_servers++;
                        struct pollfd new_pollfd = {new_socket, POLLIN, 0};
                        file_descriptors.push_back(new_pollfd);
                        Log(string("// CONNECT // New connection made: " + to_string(new_pollfd.fd)));
                        helo_received[new_pollfd.fd] = -1;
                        SendHELO(new_pollfd.fd);
                    }
                    else
                    {
                        Log(string("// CONNECT // Reached connection limit, rejecting connection."));
                        
                        if ((new_socket = accept(listenSock, (struct sockaddr *)&address, (socklen_t *)&addrlen)) >= 0) {
                            close(new_socket);
                        }
                    }
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

                            helo_received.erase(file_descriptors[i].fd);
                            list_of_connections.erase(fd_to_group_name[file_descriptors[i].fd]);
                            connection_names.erase(find(connection_names.begin(), connection_names.end(), fd_to_group_name[file_descriptors[i].fd]));
                            group_name_to_fd.erase(fd_to_group_name[file_descriptors[i].fd]);
                            fd_to_group_name.erase(file_descriptors[i].fd);
                            socket_timers.erase(file_descriptors[i].fd);
                            close(file_descriptors[i].fd);
                            file_descriptors.erase(file_descriptors.begin() + i);
                            i--;
                            clientSock = INT32_MAX;
                        }
                        else
                        {
                            Log(string("// DISCONNECT // Server Disconnected: " + to_string(file_descriptors[i].fd)));
                            
                            // In case someone disconnects without saying HELO
                            if (helo_received[file_descriptors[i].fd] == 1)
                            {
                                helo_received.erase(file_descriptors[i].fd);
                                list_of_connections.erase(fd_to_group_name[file_descriptors[i].fd]);
                                connection_names.erase(find(connection_names.begin(), connection_names.end(), fd_to_group_name[file_descriptors[i].fd]));
                                group_name_to_fd.erase(fd_to_group_name[file_descriptors[i].fd]);
                                fd_to_group_name.erase(file_descriptors[i].fd);
                                socket_timers.erase(file_descriptors[i].fd);

                                close(file_descriptors[i].fd);
                                file_descriptors.erase(file_descriptors.begin() + i);
                                i--;
                                connected_servers--;
                            }
                            else
                            {
                                helo_received.erase(file_descriptors[i].fd);
                                socket_timers.erase(file_descriptors[i].fd);
                                close(file_descriptors[i].fd);
                                file_descriptors.erase(file_descriptors.begin() + i);
                                i--;
                                connected_servers--;
                            }
                        }
                    }
                    else
                    {
                        if (file_descriptors[i].fd == clientSock)
                        {
                            Log(string("// CLIENT // New command from Client: " + to_string(file_descriptors[i].fd)));
                            int val = ReceiveClientCommand(valread);
                            if (val == -1)
                            {
                                send(clientSock, errorMessage.data(), errorMessage.size(), 0);
                            }
                        }
                        else
                        {
                            Log(string("// COMMAND // New command from Server: " + to_string(file_descriptors[i].fd)));
                            int val = ReceiveServerCommand(valread, file_descriptors[i].fd);
                            socket_timers[file_descriptors[i].fd] = time(NULL);

                            if (val == -1 && clientSock == INT32_MAX) // Might be client trying to connect.
                            {
                                Log(string("// UNKNOWN // Failed to process command from Server, checking for client password: " + to_string(file_descriptors[i].fd)));
                                val = CheckClientPassword(client_password, clientSock, file_descriptors[i].fd);

                                // Just an Unknown message
                                if (val == -1)
                                {
                                    LogError(string("// UNKNOWN // Failed to process command from server: " + to_string(file_descriptors[i].fd)));
                                    LogError(string(buffer));
                                    send(file_descriptors[i].fd, errorMessage.data(), errorMessage.size(), 0);
                                }
                                // This is client
                                else
                                {
                                    connected_servers--;
                                    Log(string("// CLIENT // New client recognized: " + to_string(file_descriptors[i].fd)));
                                    send(file_descriptors[i].fd, acceptMessage.data(), acceptMessage.size(), 0);
                                    continue; 
                                }
                            }
                            else if (val == -1)
                            {
                                LogError(string("// UNKNOWN // Failed to process command from server: " + to_string(file_descriptors[i].fd)));
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
    // TODO: Strip server message will not work with stripping SERVERS command due to ;
    string main = buffer + '\0';
    string buffered_string = buffer + '\0';
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
    size_t semicomma_index = main.find(';');
    if (comma_index != string::npos || semicomma_index != string::npos)
    {
        command = main.substr(0, min(comma_index, semicomma_index));
        while (comma_index != string::npos || semicomma_index != string::npos)
        {
            old_comma_index = min(comma_index, semicomma_index);
            comma_index = main.find(',', (old_comma_index+1));
            semicomma_index = main.find(',', (old_comma_index+1));
            
            if (comma_index != std::string::npos || semicomma_index != string::npos) 
            {
                variables.emplace_back(main.substr(old_comma_index + 1, min(comma_index, semicomma_index) - old_comma_index - 1));
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
        connection_names.emplace_back(client_name);
        struct sockaddr_in sin;
        socklen_t len = sizeof(sin);
        if (getpeername(clientSock, (struct sockaddr*)&sin, &len) < 0)
        {
            LogError("// SYSTEM // GetPeerName Function failed.");
            return -1;
        }
        else
        {
            helo_received[clientSock] = 1;
            fd_to_group_name[clientSock] = client_name;
            group_name_to_fd[client_name] = clientSock;
            string ip_address = inet_ntoa(sin.sin_addr);
            list_of_connections[client_name] = {ip_address, ntohs(sin.sin_port)};
            Log(string("// COMMAND // CLIENT has been tied to: " + ip_address));
        }
        return 1;
    }
    return -1;
}

int Server::SendHELO(int fd)
{
    string helo = "HELO," + group_name;
    send(fd, helo.data(), helo.size(), 0);
    return 1;
}

int Server::ReceiveServerCommand(int message_length, int fd)
{
    string command;
    vector<string> variables;
    StripServerMessage(message_length, command, variables);
    
    if (command == "HELO" && helo_received[fd] == -1)
    {
        Log(string("// COMMAND // New command: " + string(buffer)));
        Log(string("// COMMAND // HELO detected. Taking in data."));
        return RespondHELO(fd, variables);
    }
    else if (command == "HELO" && helo_received[fd] == 1)
    {
        Log(string("// COMMAND // New command: " + string(buffer)));
        Log(string("// COMMAND // HELO detected. Server already has said HELO. Sending SERVERS."));
        return SendSERVERS(fd);
    }
    else if (command == "SERVERS" && helo_received[fd] == 1)
    {
        Log(string("// COMMAND // New command: " + string(buffer)));
        Log(string("// COMMAND // SERVERS detected. Taking in data."));
        return RespondSERVERS(variables);
    }
    else if (command == "KEEPALIVE" && helo_received[fd] == 1)
    {
        Log(string("// COMMAND // New command: " + string(buffer)));
        Log(string("// COMMAND // KEEPALIVE detected. Taking in data."));
        return RespondKEEPALIVE(fd, variables);
    }
    else if (command == "GETMSGS" && helo_received[fd] == 1)
    {
        Log(string("// COMMAND // New command: " + string(buffer)));
        Log(string("// COMMAND // GETMSGS detected. Sending data."));
        return RespondGETMSGS(fd, variables);
    }
    else if (command == "SENDMSG" && helo_received[fd] == 1)
    {
        Log(string("// COMMAND // New command: " + string(buffer)));
        Log(string("// COMMAND // SENDMSG detected. Sending data"));
        if(variables.size() == 3)
        {
            Log(string("// COMMAND // SENDMSG has correct amount of variables, attempting to send msg."));

            return SendSENDMSG(0,variables[0],variables[1],variables[2]);
        }
        else
        {
            LogError("// COMMAND// SENDMSG has inncorrect ammount of variables");
            return -1;
        }
    }
    else if (command == "STATUSREQ" && helo_received[fd] == 1)
    {
        Log(string("// COMMAND // New command: " + string(buffer)));
        Log(string("// COMMAND // STATUSREQ detected. Sending STATUSRESP."));
        return RespondSTATUSREQ(fd);
    }
    else if (command == "STATUSRESP" && helo_received[fd] == 1)
    {
        Log(string("// COMMAND // New command: " + string(buffer)));
        Log(string("// COMMAND // STATUSRESP detected. Taking in data."));
        return RespondSTATUSRESP(fd, variables);
    }
    else
    {
        // DO NOT LOG UNKOWN MESSAGES HERE! OTHERWISE IT MIGHT LEAK THE PASSWORD TO THE LOG FILE WHICH ANYONE CAN READ!!!  
        return -1;
    }
}

int Server::RespondSTATUSRESP(int fd, vector<string> variables)
{
    for (int i = 0; i < variables.size(); i++)
    {
        if (variables[i] == group_name)
        {
            Log(string("// COMMAND // STATUSRESP has our messages. Calling GETMSGS."));
            return SendGETMSGS(fd);
        }
    }
    return 1;
}

int Server::RespondSTATUSREQ(int fd)
{
    string full_msg = "STATUSRESP";
    map<string, vector<pair<string, string>>>::iterator it;

    for (it = other_groups_message_buffer.begin(); it != other_groups_message_buffer.end(); it++)
    {
        full_msg += "," + it->first + "," + to_string(it->second.size());
    }

    if(send(fd, full_msg.data(), full_msg.length(), 0) < 0)
    {
        LogError(string("// COMMAND // Failed to send STATUSRESP to server: " + to_string(fd)));
        return -1;
    }
    else
    {
        Log(string("// COMMAND // Successfully sent STATUSRESP to server: " + to_string(fd)));
        return 1;
    }
}

int Server::SendSENDMSG(int fd, string to_group_name, string from_group_name, string data)
{
    char send_buffer[5121];
    memset(send_buffer,0,sizeof(send_buffer));
    //Begin by storing the message.
    if(to_group_name == group_name) // The msg is addressed to us
    {
        our_message_buffer[from_group_name].push(data); //Store in private buffer
        Log(string("// COMMAND // Message is addressed to us. Storing message."));
        return 1;
    }
    else
    {
        if (fd != 0)
        {
            Log(string("// COMMAND // Demanded by group: " + to_string(fd) + " to send messages of: " + to_group_name));
            strcat(send_buffer,("SENDMSG," + to_group_name + "," + from_group_name + "," + data).c_str());
            if(send(fd,send_buffer,sizeof(buffer),0) < 0)
            {
                LogError(string("// COMMAND // Failed sending message to group: " + to_group_name));
                Log(string("// COMMAND // Storing message"));
                other_groups_message_buffer[to_group_name].push_back({from_group_name,data});
                return -1;
            }
            else
            {
                Log(string("// COMMAND // Succeeded sending message to group: " + fd));
                return 1;
            }
        }
        else
        {
            //check if to_group_name is connected
            if(find(connection_names.begin(),connection_names.end(),to_group_name) != connection_names.end())
            {
                Log(string("// COMMAND // Conected to group: " + to_group_name + " attempting to send message"));
                strcat(send_buffer,("SENDMSG," + to_group_name + "," + from_group_name + "," + data).c_str());
                if(send(group_name_to_fd[to_group_name],send_buffer,sizeof(buffer),0) < 0)
                {
                    LogError(string("// COMMAND // Failed sending message to group: " + to_group_name));
                    Log(string("// COMMAND // Storing message"));
                    other_groups_message_buffer[to_group_name].push_back({from_group_name,data});
                    return -1;
                }
                else
                {
                    Log(string("// COMMAND // Succeeded sending message to group: " + to_group_name));
                    return 1;
                }
            }
            else
            {
                LogError(string("// COMMAND // Not connected to group: " + to_group_name));
                Log(string("// COMMAND // Storing message"));
                other_groups_message_buffer[to_group_name].push_back({from_group_name,data});
                return -1;
            }
        }
    }
}

int Server::RespondSERVERS(vector<string> variables)
{
    if (variables.size() == 0)
    {
        Log(string("// COMMAND // No new servers to document."));
        return 1;
    }
    else
    {
        for (int i = 0; i < variables.size(); i += 3)
        {
            string new_group_name = variables[i];
            string new_group_ip = variables[i+1];
            string new_group_port = variables[i+2];

            struct sockaddr_in new_addr_test;

            if (isdigit(new_group_port.c_str()[0]))
            {
                if (inet_pton(AF_INET, new_group_ip.data(), &new_addr_test.sin_addr) > 0)
                {
                    Log(string("// COMMAND // New server: " + new_group_name + " IP: " + new_group_ip + " Port: " + new_group_port + " documented."));
                    documented_servers[new_group_name] = {new_group_ip, stoi(new_group_port)};
                }
                else
                {
                    LogError(string("// COMMAND // IP address not valid: " + new_group_ip  + " continuing."));
                    continue;
                }
            }
            else
            {
                LogError(string("// COMMAND // Portnum not a number: " + new_group_port + " continuing."));
                continue;
            }
        }
    }
    return 1;
}

int Server::RespondGETMSGS(int fd, vector<string> variables)
{
    if (variables.size() >= 1)
    {
        Log(string("// COMMAND // Too few variables in GETMSG command. Aborting."));
        for (int i = 0; i < variables.size(); i++)
        {
            if (other_groups_message_buffer.find(variables[i]) != other_groups_message_buffer.end())
            {
                for (int j = 0; j < other_groups_message_buffer[variables[i]].size(); j++)
                {
                    //              TO_GROUP      FROM_GROUP                                          DATA 
                    SendSENDMSG(fd, variables[i], other_groups_message_buffer[variables[i]][j].first, other_groups_message_buffer[variables[i]][j].second);
                    // Doesn't matter if it fails methinks
                }
            }
            vector<pair<string, string>> new_vec;
            other_groups_message_buffer[variables[i]] = new_vec;
        }
    }
    else
    {
        LogError(string("// COMMAND // Too few variables in GETMSG command. Aborting."));
        return -1;
    }
    return 1;
}

int Server::RespondHELO(int fd, vector<string> variables)
{
    if (variables.size() == 1)
    {
        connection_names.emplace_back(variables[0]);
        struct sockaddr_in sin;
        socklen_t len = sizeof(sin);
        if (getpeername(fd, (struct sockaddr*)&sin, &len) < 0)
        {
            LogError("// SYSTEM // GetPeerName Function failed.");
            return -1;
        }
        else
        {
            helo_received[fd] = 1;
            fd_to_group_name[fd] = variables[0];
            group_name_to_fd[variables[0]] = fd;
            string ip_address = inet_ntoa(sin.sin_addr);
            list_of_connections[variables[0]] = {ip_address, ntohs(sin.sin_port)};
            documented_servers[variables[0]] = {ip_address, ntohs(sin.sin_port)};
            Log(string("// COMMAND // Group " + variables[0] + " has been tied to: " + ip_address));
        }

        return SendSERVERS(fd);
    }
    else
    {
        LogError(string("// COMMAND // Not correct amount of variables in HELO command. Aborting."));
        return -1;
    }
}

int Server::RespondKEEPALIVE(int fd, vector<string> variables)
{
    if (variables.size() == 1)
    {
        if (stoi(variables[0]) > 0)
        {
            Log("// COMMAND // KEEPALIVE from " + to_string(fd) + " has messages. Collecting. ");
            return SendGETMSGS(fd);
        }
        else
        {
            Log("// COMMAND // KEEPALIVE from " + to_string(fd) + " is empty. ");
            return 0;
        }
    }
    else
    {
        LogError(string("// COMMAND // Not correct amount of variables in KEEPALIVE command. Aborting."));
        return -1;
    }
}

int Server::SendGETMSGS(int fd)
{
    string statusreq = "GETMSGS," + group_name;
    if (send(fd, statusreq.data(), statusreq.length(), 0) < 0)
    {
        LogError(string("// COMMAND // Failed to send GETMSGS"));
        return -1;
    }
    return 1;
}

//Sends a list servers to the given file_descriptor
int Server::SendSERVERS(int fd)
{
    char send_buffer[5121];
    memset(send_buffer, 0, sizeof(send_buffer));
    size_t pos = 0;
    string group_info;
    send_buffer[0] = 'S';
    send_buffer[1] = 'E';
    send_buffer[2] = 'R';
    send_buffer[3] = 'V';
    send_buffer[4] = 'E';
    send_buffer[5] = 'R';
    send_buffer[6] = 'S';
    send_buffer[7] = ',';

    for (const auto& group_server : Server::list_of_connections) 
    {
        if (group_server.first == client_name)
        {
            continue;
        }
        group_info = group_server.first +","+ group_server.second.first +","+ to_string(group_server.second.second)+";";
        strcat(send_buffer,group_info.c_str());
    }
    if(send(fd,send_buffer,sizeof(send_buffer),0) < 0)
    {
        LogError(string("// COMMAND // Failed to send list of servers"));
        return -1;
    }
    else
    {
        Log(string("// COMMAND // Succeeded in sending list of servers"));
        return 1;
    }
}

int Server::RespondGetMSG(string group_id)
{

    char send_buffer[5121];
    memset(send_buffer,0,sizeof(send_buffer));

    //Check if the group has ever sent anything
    if(our_message_buffer.find(group_id) != our_message_buffer.end()) 
    {
        //There is a stored messages
        if(our_message_buffer[group_id].size() > 0)
        {
            LogError(our_message_buffer[group_id].front());
            strcat(send_buffer,our_message_buffer[group_id].front().c_str());
            our_message_buffer[group_id].pop();
            Log(string("// CLIENT // Group: " + group_id + " Has a messages for client. Responding to client"));
            //Attempting to send
            if(send(clientSock,send_buffer,sizeof(send_buffer),0) < 0)
            {
                LogError(string("// CLIENT // Failed to send message to client from group: " + group_id));
                return -1;
            }
            else
            {
                Log(string("// CLIENT // Group: " + group_id + " has a message for client. Replied to Client Suceeded."));
                return 1;
            }
            LogError(string("// UNKNOWN // Something failed when responding to GETMSG from Client"));
        }
        else
        {
            //No messages stored from group
            Log(string("// CLIENT // Group: " + group_id + " Has no messages for client. Responding to client"));
            strcat(send_buffer,("Currently no messages from group: " + group_id +".").c_str()); //Perhaps might fail?
            //Respond with there being no messages
            if(send(clientSock,send_buffer,sizeof(send_buffer),0) < 0)
            {
                LogError(string("// CLIENT // Failed to send no message from group: " + group_id));
                return -1;
            }
            else
            {
                Log(string("// CLIENT // Group: " + group_id + " has never sent a message. Replied to Client Suceeded."));
                return 1;
            }
            LogError(string("// UNKNOWN // Something failed when responding to GETMSG from Client"));
        }

    }
    else
    {
        //Group has never connected
        Log(string("// CLIENT // Group: " + group_id + " has never sent a message. Responding to client"));
        strcat(send_buffer,("Group: " + group_id +" has never sent a message.").c_str()); //Perhaps might fail? 
        //Responding to client that group has never tried sending
        if(send(clientSock,send_buffer,sizeof(send_buffer),0) < 0)
        {
            LogError(string("// CLIENT // Failed to send never messaged from group: " + group_id)); 
            return -1;
        }
        else
        {
            Log(string("// CLIENT // Group: " + group_id + " has never sent a message. Replied to Client Suceeded."));
            return 1;
        }
            LogError(string("// UNKNOWN // Something failed when responding to GETMSG from Client"));
    }
    LogError(string("// UNKNOWN // Something failed when responding to GETMSG from Client"));
}

// Process command from client on the server
int Server::ReceiveClientCommand(int message_length)
{
    vector<string> variables;
    string message;
    StripServerMessage(message_length, message, variables);

    if (message.substr(0, 6) == "GETMSG")
    {
        if(variables.size() == 1)
        {
            Log(string("// COMMAND // GETMSG is correctly formated. Checking for messages"));
            return RespondGetMSG(variables[0]);
        }
        else
        {
            LogError(string("// COMMAND // GETMSG is incorrectly formated."));
            //Respond to client with error.
            char send_buffer[5121];
            memset(send_buffer,0,sizeof(send_buffer));
            string error_msg = "GETMSG is incorrectly formated";
            strcat(send_buffer,error_msg.c_str());
            if(send(clientSock,send_buffer,sizeof(send_buffer),0) < 0)
            {
                LogError(string("// CLIENT // Failed to send error msg: "+ error_msg));
            }
            else
            {
                Log(string("// CLIENT // Error msg succeeded sending: "+error_msg));
            }
            return -1;
        }
    }
    else if (message.substr(0, 8) == "SENDMSG")
    {
        Log(string("// COMMAND // Attempting to send a message"));
        if(variables.size() == 2)
        {
            return SendSENDMSG(0,variables[0],group_name,variables[1]);
        }
        else
        {
            LogError("// COMMAND // SENDMSG has inncorrect ammount of variables");    

            //Respond to client with error.
            char send_buffer[5121];
            memset(send_buffer,0,sizeof(send_buffer));
            string error_msg = "SENDMSG has inncorrect ammount of variables";
            strcat(send_buffer,error_msg.c_str());
            if(send(clientSock,send_buffer,sizeof(send_buffer),0) < 0)
            {
                LogError(string("// CLIENT // Failed to send error msg: "+ error_msg));
            }
            else
            {
                Log(string("// CLIENT // Error msg succeeded sending: "+error_msg));
            }
            return -1;
        }

    }
    else if (message.substr(0, 11) == "LISTSERVERS")
    {
        Log(string("// CLIENT // Attempting to list of servers to client"));
        //We can just reuse our servers function for this
        return SendSERVERS(clientSock); 
    }
    else if (message.substr(0, 13) == "CONNECTSERVER")
    {
        Log(string("// CLIENT // Attempting to connect to server specified by client."));
        if(variables.size() == 2)
        {
            if (isdigit(variables[1].c_str()[0]))
            {
                struct sockaddr_in new_addr_test;
                if (inet_pton(AF_INET, variables[0].data(), &new_addr_test.sin_addr) > 0)
                {
                    return ConnectToServer(variables[0], stoi(variables[1]));
                }
                else
                {
                    LogError(string("// COMMAND // IP address not valid, continuing."));
                }
            }
            else
            {
                LogError(string("// CLIENT // Port number given is not a number."));
            }
        }
        else
        {
            LogError(string("// CLIENT // Too few variables given to connect."));
        }
    }
    else // Unknown
    {
        // LOG
        LogError(string("// CLIENT // Unknown command from client."));
        LogError(message);
        return -1;
    }
    return 1;
}

int Server::RespondLISTSERVERS()
{

    //We can just reuse our servers function for this
    return SendSERVERS(clientSock);

}