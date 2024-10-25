#include "server.h"

Server::Server(int portnumber) 
{
    portnum = portnumber;
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
                if (keepalive_packets == 3)
                {
                    Log("// MESSAGE // Sending STATUSREQ to: " + fd_to_group_name[file_descriptors[i].fd] + " : " + to_string(file_descriptors[i].fd));
                    SendSTATUSREQ(file_descriptors[i].fd);
                }
                else
                {
                    Log("// MESSAGE // Sending KEEPALIVE to: " + fd_to_group_name[file_descriptors[i].fd] + " : " + to_string(file_descriptors[i].fd));
                    SendKEEPALIVE(file_descriptors[i].fd);
                }
            }
        }
        last_keepalive = time(NULL);
        if (keepalive_packets == 3)
        {
            keepalive_packets = 0;
        }
        else
        {
            keepalive_packets++;
        }
    }
}

void Server::CheckTimeouts()
{
    map<int, time_t>::iterator it = socket_timers.begin(); 
    time_t now = time(NULL);

    while (it != socket_timers.end())
    {
        if (difftime(now, it->second) > expiration_of_servers && it->first != clientSock && it->first != listenSock)
        {
            Log(string("// DISCONNECT // Found a server who has been silent for too long: " + fd_to_group_name[it->first] + " : " + to_string(it->first)));
            int socket = it->first;
            auto it = std::find_if(file_descriptors.begin(), file_descriptors.end(),
                        [socket](const pollfd& pfd) { return pfd.fd == socket;});
            int index = distance(file_descriptors.begin(), it);
            CloseConnection(socket, index);
        }
        else
        {
            it++;
            continue;
        }
    }
}

bool Server::CheckServerConnection(string name, string IP, int port)
{
    for (const auto& connection : list_of_connections) {
        string existingGroupName = connection.first;
        pair<string, int> ipAndPort = connection.second;

        if (existingGroupName == name || (ipAndPort.first == IP && ipAndPort.second == port)) {
            return true; 
        }
    }
    return false;
}

void Server::CheckForMoreConnections()
{
    if (connected_servers < min_server_capacity && documented_servers.size() != connected_servers)
    {
        map<string, pair<string, int>>::iterator it = documented_servers.begin();

        while (it != documented_servers.end())
        {
            if (CheckServerConnection(it->first, it->second.first, it->second.second) || find(pending_connections.begin(), pending_connections.end(), it->first) != pending_connections.end())
            {
                // ALREADY CONNECTED / PENDING
                it++;
                continue;
            }
            else if (it->first != group_name && (it->second.second != portnum || (it->second.first != ip_address && it->second.first != localhost)) && find(blacklist.begin(), blacklist.end(), it->first) == blacklist.end())
            {
                // CAN CONNECT
                string server_ip = it->second.first;
                int server_port = it->second.second;

                Log(string("// CONNECT // Attempting new connection with documented server: " + it->first + " : " + server_ip + " : " + to_string(server_port)));
                pending_connections.emplace_back(it->first);
                if (ConnectToServer(server_ip, server_port))
                {
                    Log(string("// CONNECT // New connection established with: " + it->first));
                    it++;

                    if (connected_servers >= 3)
                    {
                        break;
                    }
                }
                else
                {
                    LogError(string("// CONNECT // Failed to set up connection with documented server: " + it->first + " : " + server_ip + " : " + to_string(server_port)));
                    it = documented_servers.erase(it);
                }
            }
            else
            {
                string server_ip = it->second.first;
                int server_port = it->second.second;
                LogError("// CONNECT // Documented server: " + it->first + " : " + server_ip + " : " + to_string(server_port) + " failed some parameters and was removed.");
                it = documented_servers.erase(it);
            }
        }
    }
    return;
}

bool Server::ConnectToServer(string ip, int port)
{
    struct sockaddr_in new_server_address;
    new_server_address.sin_family = AF_INET;
    new_server_address.sin_port = htons(port);
    int outboundSocket;

    if ((outboundSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    {
        LogError("// SYSTEM // Failed to open new socket.");
        return false;
    }

    if (inet_pton(AF_INET, ip.c_str(), &new_server_address.sin_addr) <= 0)
    {
        LogError(string("// CONNECT // Invalid IP address."));
        return false;
    }

    int flags = fcntl(outboundSocket, F_GETFL, 0);
    if (flags < 0 || fcntl(outboundSocket, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        LogError("// CONNECT // Failed to set socket to non-blocking mode.");
        return false;
    }

    if (connect(outboundSocket, (struct sockaddr*)&new_server_address, sizeof(new_server_address)) < 0)
    {
        if (errno != EINPROGRESS)
        {
            LogError("// CONNECT // Could not connect to new IP.");
            return false;
        }

        struct pollfd pfd = {outboundSocket, POLLOUT, 0};
        int result = poll(&pfd, 1, timeout);

        if (result == 0)
        {
            LogError("// CONNECT // Connection timed out.");
            return false;
        }
        else if (result < 0)
        {
            LogError("// CONNECT // Poll error.");
            return false;
        }

        int err;
        socklen_t len = sizeof(err);
        if (getsockopt(outboundSocket, SOL_SOCKET, SO_ERROR, &err, &len) < 0 || err != 0)
        {
            LogError("// CONNECT // Connection failed.");
            return false;
        }
    }

    connected_servers++;
    struct pollfd new_pollfd = {outboundSocket, POLLIN, 0};
    file_descriptors.push_back(new_pollfd);
    helo_received[new_pollfd.fd] = -1;
    socket_timers[new_pollfd.fd] = time(NULL);
    strike_counter[new_pollfd.fd] = 0;
    Log(string("// CONNECT // New server connected on: ") + ip + " : " + to_string(new_pollfd.fd));
    SendHELO(new_pollfd.fd);
    return true;
}

void Server::InitializeServer()
{
    listenSock = OpenSocket(portnum); 
    Log(string("// SYSTEM // Starting listen on port " + to_string(portnum)));

    if(listen(listenSock, BACKLOG) < 0)
    {
        LogError(string("// SYSTEM // Listen failed on port " + to_string(portnum)));
        exit(0);
    }

    last_keepalive = time(NULL);
    struct pollfd server_pollfd = {listenSock, POLLIN, 0};
    file_descriptors.emplace_back(server_pollfd);
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
                        connected_servers++;
                        struct pollfd new_pollfd = {new_socket, POLLIN, 0};
                        file_descriptors.push_back(new_pollfd);
                        Log(string("// CONNECT // New connection made: " + to_string(new_pollfd.fd)));
                        helo_received[new_pollfd.fd] = -1;
                        socket_timers[new_pollfd.fd] = time(NULL);
                        strike_counter[new_pollfd.fd] = 0;
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
                    Log(string("// MESSAGE // New message received from: " + fd_to_group_name[file_descriptors[i].fd] +  " : " + to_string(file_descriptors[i].fd)));
                    int valread = recv(file_descriptors[i].fd, buffer, buffer_size, MSG_DONTWAIT);

                    if (valread <= 0) 
                    {
                        if (file_descriptors[i].fd == clientSock)
                        {
                            Log(string("// CLIENT // Client Disconnected: " + to_string(file_descriptors[i].fd)));

                            CloseConnection(file_descriptors[i].fd, i);
                            i--;
                            clientSock = INT32_MAX;
                        }
                        else
                        {
                            Log(string("// DISCONNECT // Server Disconnected: " + fd_to_group_name[file_descriptors[i].fd] + " : " + to_string(file_descriptors[i].fd)));
                            
                            // In case someone disconnects without saying HELO
                            CloseConnection(file_descriptors[i].fd, i);
                            i--;
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
                                Log(string("// SENDING // " + failureMessageClient));
                                send(clientSock, failureMessageClient.data(), failureMessageClient.size(), 0);
                            }
                        }
                        else
                        {
                            Log(string("// COMMAND // New command from Server: " + fd_to_group_name[file_descriptors[i].fd] + " : " + to_string(file_descriptors[i].fd)));
                            int val = ReceiveServerCommand(valread, file_descriptors[i].fd);
                            time_t old_time = socket_timers[file_descriptors[i].fd];
                            socket_timers[file_descriptors[i].fd] = time(NULL);

                            if (val == 1)
                            {
                                strike_counter[file_descriptors[i].fd] = 0;
                            }
                            else if (val == 2 && clientSock == INT32_MAX) // Might be client trying to connect.
                            {
                                val = CheckClientPassword(client_password, clientSock, file_descriptors[i].fd);

                                if (val != -1 && val != -2)
                                {
                                    connected_servers--;
                                    Log(string("// CLIENT // New client recognized: " + to_string(file_descriptors[i].fd)));
                                    Log(string("// SENDING // " + acceptMessage));
                                    send(file_descriptors[i].fd, acceptMessage.data(), acceptMessage.size(), 0);
                                    continue; 
                                }
                            }
                            else if (val == -1)
                            {
                                strike_counter[file_descriptors[i].fd]++;
                                socket_timers[file_descriptors[i].fd] = old_time;
                                LogError(string("// UNKNOWN // Failed to process command from server: " + to_string(file_descriptors[i].fd)));
                                LogError(string(buffer));
                                string sendMSG;
                                if (strike_counter[file_descriptors[i].fd] < 3)
                                {
                                    sendMSG = errorMessage + ",STRIKE:"+to_string(strike_counter[file_descriptors[i].fd]) + "\x04";
                                }
                                else
                                {
                                    sendMSG = errorMessage + ",STRIKE:"+to_string(strike_counter[file_descriptors[i].fd])+",YOU'RE,OUT" + "\x04";
                                }

                                Log(string("// SENDING // " + sendMSG));
                                send(file_descriptors[i].fd, sendMSG.data(), sendMSG.size(), 0);

                                if (strike_counter[file_descriptors[i].fd] >= 3)
                                {
                                    Log("// DISCONNECT // Throwing out bad bot, too many failed messages: " + fd_to_group_name[file_descriptors[i].fd] + " : " + to_string(file_descriptors[i].fd));
                                    CloseConnection(file_descriptors[i].fd, i);
                                    i--;
                                }
                            }
                            else if (val == -3)
                            {
                                Log("// DISCONNECT // Throwing out blacklisted bot: " + fd_to_group_name[file_descriptors[i].fd] + " : " + to_string(file_descriptors[i].fd));
                                CloseConnection(file_descriptors[i].fd, i);
                                i--;
                            }
                        }
                    }
                }
            }
        }
    }
    return 0;
}

void Server::CloseConnection(int fd, int i)
{
    if (helo_received[fd] == 1)
    {
        helo_received.erase(fd);
        strike_counter.erase(fd);
        documented_servers.erase(fd_to_group_name[fd]); // Don't want to reconnect to the same dud over and over
        list_of_connections.erase(fd_to_group_name[fd]);
        connection_names.erase(find(connection_names.begin(), connection_names.end(), fd_to_group_name[fd]));
        group_name_to_fd.erase(fd_to_group_name[fd]);
        fd_to_group_name.erase(fd);
        socket_timers.erase(fd);
        close(fd);
        file_descriptors.erase(file_descriptors.begin() + i);
        connected_servers--;
    }
    else
    {
        helo_received.erase(fd);
        strike_counter.erase(fd); 
        socket_timers.erase(fd);
        close(fd);
        file_descriptors.erase(file_descriptors.begin() + i);
        connected_servers--;
    }
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

int Server::OpenSocket(int portno)
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
 
int Server::ReceiveServerCommand(int message_length, int fd)
{
    vector<string> commands;
    vector<vector<string>> full_variables_vector(max_variables);
    StripServerMessage(message_length, commands, full_variables_vector);
    
    if (commands[0] == client_password)
    {
        Log("// CLIENT // Detected CLIENT password.");
        return 2;
    }

    Log("// COMMAND // " + string(buffer));
    for (int i = 0; i < commands.size(); i++)
    {
        int error_code = 5;
        string command = commands[i];
        vector<string> variables = full_variables_vector[i];

        if (command == "HELO" && helo_received[fd] == -1)
        {
            Log(string("// COMMAND // New command: " + string(command)));
            Log(string("// COMMAND // HELO detected. Taking in data."));
            error_code = RespondHELO(fd, variables);
        }
        else if (command == "HELO" && helo_received[fd] == 1)
        {
            Log(string("// COMMAND // New command: " + string(command)));
            Log(string("// COMMAND // HELO detected. Server already has said HELO. Sending SERVERS."));
            error_code = SendSERVERS(fd);
        }
        else if (command == "SERVERS" && helo_received[fd] == 1)
        {
            Log(string("// COMMAND // New command: " + string(command)));
            Log(string("// COMMAND // SERVERS detected. Taking in data."));
            error_code = RespondSERVERS(variables);
        }
        else if (command == "KEEPALIVE" && helo_received[fd] == 1)
        {
            Log(string("// COMMAND // New command: " + string(command)));
            Log(string("// COMMAND // KEEPALIVE detected. Taking in data."));
            error_code = RespondKEEPALIVE(fd, variables);
        }
        else if (command == "GETMSGS" && helo_received[fd] == 1)
        {
            Log(string("// COMMAND // New command: " + string(command)));
            Log(string("// COMMAND // GETMSGS detected. Sending data."));
            error_code = RespondGETMSGS(fd, variables);
        }
        else if (command == "SENDMSG" && helo_received[fd] == 1)
        {
            Log(string("// COMMAND // New command: " + string(command)));
            Log(string("// COMMAND // SENDMSG detected. Sending data"));
            if(variables.size() == 3)
            {
                Log(string("// COMMAND // SENDMSG has correct amount of variables, attempting to send message."));
                error_code = SendSENDMSG(0,variables[0],variables[1],variables[2]);
            }
            else
            {
                LogError("// COMMAND // SENDMSG has inncorrect ammount of variables");
                return -1;
            }
        }
        else if (command == "STATUSREQ" && helo_received[fd] == 1)
        {
            Log(string("// COMMAND // New command: " + string(command)));
            Log(string("// COMMAND // STATUSREQ detected. Sending STATUSRESP."));
            error_code = RespondSTATUSREQ(fd);
        }
        else if (command == "STATUSRESP" && helo_received[fd] == 1)
        {
            Log(string("// COMMAND // New command: " + string(command)));
            Log(string("// COMMAND // STATUSRESP detected. Taking in data."));
            error_code = RespondSTATUSRESP(fd, variables);
        }
        else
        {
            return -1;
        }

        if (error_code < 0)
        {
            return error_code;
        }
    }
    Log("// COMMAND // Finished commands list.");
    return 1;
}

void Server::StripServerMessage(int message_length, vector<string> &commands, vector<vector<string>> &variables)
{
    vector<string> messages;
    string main = buffer + '\0';
    int SOHlocation = main.find('\x01');
    int EOTlocation = main.find('\x04');

    while (SOHlocation != string::npos && EOTlocation != string::npos)
    {
        messages.emplace_back(main.substr(SOHlocation + 1, EOTlocation - (SOHlocation + 1)));

        SOHlocation = main.find('\x01', SOHlocation + 1);
        EOTlocation = main.find('\x04', EOTlocation + 1);
    }

    if (messages.size() == 0)
    {
        LogError("// MESSAGE // Could not find SOH or EOT.");
        messages.emplace_back(main);
    }

    for (int i = 0; i < messages.size(); i++)
    {
        string command;
        main = messages[i];
        size_t old_comma_index = 0;
        size_t comma_index = main.find(',');
        size_t semicomma_index = main.find(';');
        if (comma_index != string::npos)
        {
            command = main.substr(0, min(comma_index, semicomma_index));
            commands.emplace_back(command);

            if (command == "SENDMSG")
            {
                old_comma_index = comma_index;
                comma_index = main.find(',', (old_comma_index+1));
                variables[i].emplace_back(main.substr(old_comma_index + 1, comma_index - old_comma_index - 1));
                old_comma_index = comma_index;
                comma_index = main.find(',', (old_comma_index+1));
                variables[i].emplace_back(main.substr(old_comma_index + 1, comma_index - old_comma_index - 1));
                old_comma_index = comma_index;
                variables[i].emplace_back(main.substr(old_comma_index + 1));
            }
            else
            {
                while (comma_index != string::npos || semicomma_index != string::npos)
                {
                    old_comma_index = min(comma_index, semicomma_index);
                    comma_index = main.find(',', (old_comma_index+1));
                    semicomma_index = main.find(';', (old_comma_index+1));
                    
                    if (comma_index != std::string::npos || semicomma_index != string::npos) 
                    {
                        variables[i].emplace_back(main.substr(old_comma_index + 1, min(comma_index, semicomma_index) - old_comma_index - 1));
                    } 
                    else 
                    {
                        variables[i].emplace_back(main.substr(old_comma_index + 1));
                    }
                }
            }
        }
        else
        {
            command = main;
            commands.emplace_back(command);
            vector<string> empty;
            variables.emplace_back(empty);
        }
    }
}

int Server::SendKEEPALIVE(int fd)
{
    string keepalive = "\x01KEEPALIVE,";

    keepalive += to_string(other_groups_message_buffer[fd_to_group_name[fd]].size());
    keepalive += '\x04';

    Log(string("// SENDING // " + keepalive));
    if (send(fd, keepalive.data(), keepalive.length(), 0) < 0)
    {
        LogError(string("// COMMAND // Failed to send KEEPALIVE."));
        return -1;
    }
    return 1;
}

int Server::SendSTATUSREQ(int fd)
{
    string statusreq = "\x01STATUSREQ\x04";
    Log(string("// SENDING // " + statusreq));
    if (send(fd, statusreq.data(), statusreq.length(), 0) < 0)
    {
        LogError(string("// COMMAND // Failed to send STATUSREQ."));
        return -1;
    }
    return 1;
}

int Server::SendHELO(int fd)
{
    string helo = "\x01HELO," + group_name + '\x04';
    Log(string("// SENDING // " + helo));
    send(fd, helo.data(), helo.size(), 0);
    return 1;
}

int Server::RespondSTATUSRESP(int fd, vector<string> variables)
{
    for (int i = 0; (i+1) < variables.size(); i++)
    {
        if (!variables[i+1].empty() && all_of(variables[i+1].begin(), variables[i+1].end(), ::isdigit))
        {
            if (variables[i] == group_name && stoi(variables[i+1]) > 0)
            {
                Log(string("// COMMAND // STATUSRESP has our messages. Calling GETMSGS."));
                return SendGETMSGS(fd);
            }
        }
    }
    return 1;
}

int Server::RespondSTATUSREQ(int fd)
{
    string full_msg = "\x01STATUSRESP";
    map<string, vector<pair<string, string>>>::iterator it;

    for (it = other_groups_message_buffer.begin(); it != other_groups_message_buffer.end(); it++)
    {
        if (it->second.size() != 0)
        {
            full_msg += "," + it->first + "," + to_string(it->second.size());
        }
    }

    full_msg += '\x04';
    Log(string("// SENDING // " + full_msg));
    if(send(fd, full_msg.data(), full_msg.length(), 0) < 0)
    {
        LogError(string("// COMMAND // Failed to send STATUSRESP to server: " + fd_to_group_name[fd] + " : " + to_string(fd)));
        return -2;
    }
    else
    {
        Log(string("// COMMAND // Successfully sent STATUSRESP to server: " + fd_to_group_name[fd] + " : " + to_string(fd)));
        return 1;
    }
}

int Server::SendSENDMSG(int fd, string to_group_name, string from_group_name, string data)
{
    string send_buffer;
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
            send_buffer = ("\x01SENDMSG," + to_group_name + "," + from_group_name + "," + data + '\x04');
            Log(string("// SENDING // " + string(send_buffer)));
            if(send(fd, send_buffer.data(), send_buffer.length(), 0) < 0)
            {
                LogError(string("// COMMAND // Failed sending message to group: " + to_group_name));
                Log(string("// COMMAND // Storing message"));
                other_groups_message_buffer[to_group_name].push_back({from_group_name,data});
                return -2;
            }
            else
            {
                Log(string("// COMMAND // Succeeded sending message to group: " + fd_to_group_name[fd] + " : " + to_string(fd)));
                return 1;
            }
        }
        else
        {
            //check if to_group_name is connected
            if(find(connection_names.begin(),connection_names.end(),to_group_name) != connection_names.end())
            {
                Log(string("// COMMAND // Connected to group: " + to_group_name + " attempting to send message"));
                send_buffer = ("\x01SENDMSG," + to_group_name + "," + from_group_name + "," + data + '\x04');
                Log(string("// SENDING // " + string(send_buffer)));
                if(send(group_name_to_fd[to_group_name], send_buffer.data(), send_buffer.length(), 0) < 0)
                {
                    LogError(string("// COMMAND // Failed sending message to group: " + to_group_name));
                    Log(string("// COMMAND // Storing message"));
                    other_groups_message_buffer[to_group_name].push_back({from_group_name,data});
                    return -2;
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
                return 1;
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
    else if (variables.size() > 2)
    {
        for (int i = 0; (i+2) < variables.size(); i += 3)
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
    else
    {
        LogError("// COMMAND // Too few variables in SERVERS command, aborting.");
        return 1;
    }
    return 1;
}

int Server::RespondGETMSGS(int fd, vector<string> variables)
{
    if (variables.size() >= 1)
    {
        Log(string("// COMMAND // Group variables detected, attempting to send messages."));
        for (int i = 0; i < variables.size(); i++)
        {
            Log("// COMMAND // GETMSGS asked for group: " + variables[i]);
            if (other_groups_message_buffer.find(variables[i]) != other_groups_message_buffer.end())
            {
                if (other_groups_message_buffer[variables[i]].size() == 0)
                {
                    Log("// COMMAND // Buffer for specified group: " + variables[i] + " is empty.");
                }
                else
                {
                    for (int j = 0; j < other_groups_message_buffer[variables[i]].size(); j++)
                    {
                        //              TO_GROUP      FROM_GROUP                                          DATA 
                        SendSENDMSG(fd, variables[i], other_groups_message_buffer[variables[i]][j].first, other_groups_message_buffer[variables[i]][j].second);
                        // Doesn't matter if it fails methinks
                    }
                }
            }
            else
            {
                Log("// COMMAND // Have never received message for specified group: " + variables[i]);
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
            LogError("// SYSTEM // GetPeerName Function failed: " + string(strerror(errno)));
            return -2;
        }
        else
        {
            if (find(blacklist.begin(), blacklist.end(), variables[0]) != blacklist.end())
            {
                Log("// DISCONNECT // Found BLACKLIST target. Throwing out.");
                return -3;
            }
            else
            {
                helo_received[fd] = 1;
                fd_to_group_name[fd] = variables[0];
                group_name_to_fd[variables[0]] = fd;
                string ip_address = inet_ntoa(sin.sin_addr);
                list_of_connections[variables[0]] = {ip_address, ntohs(sin.sin_port)};
                documented_servers[variables[0]] = {ip_address, ntohs(sin.sin_port)};
                
                auto it = find(pending_connections.begin(), pending_connections.end(), variables[0]);
                if (it != pending_connections.end())
                {
                    pending_connections.erase(it);
                }
                Log(string("// COMMAND // Group " + variables[0] + " has been tied to: " + ip_address));
            }
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
            Log("// COMMAND // KEEPALIVE from " + fd_to_group_name[fd] + " : " + to_string(fd) + " has messages. Collecting. ");
            return SendGETMSGS(fd);
        }
        else
        {
            Log("// COMMAND // KEEPALIVE from " + fd_to_group_name[fd] + " : " + to_string(fd) + " is empty. ");
            return 1;
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
    string statusreq = "\x01GETMSGS," + group_name + "\x04";
    Log(string("// SENDING // " + statusreq));
    if (send(fd, statusreq.data(), statusreq.length(), 0) < 0)
    {
        LogError(string("// COMMAND // Failed to send GETMSGS"));
        return -2;
    }
    return 1;
}

//Sends a list servers to the given file_descriptor
int Server::SendSERVERS(int fd)
{
    string send_buffer = "\x01SERVERS," + group_name + "," + ip_address + "," + to_string(portnum) + ";";
    size_t pos = 0;
    string group_info;

    for (const auto& group_server : Server::list_of_connections) 
    {
        if (group_server.first == client_name)
        {
            continue;
        }
        group_info = group_server.first +","+ group_server.second.first +","+ to_string(group_server.second.second)+";";
        send_buffer += group_info;
    }

    send_buffer += '\x04';
    Log(string("// SENDING // " + string(send_buffer)));
    if(send(fd, send_buffer.data(), send_buffer.length(), 0) < 0)
    {
        LogError(string("// COMMAND // Failed to send list of servers"));
        return -2;
    }
    else
    {
        Log(string("// COMMAND // Succeeded in sending list of servers"));
        return 1;
    }
}

// Process command from client on the server
int Server::ReceiveClientCommand(int message_length)
{
    vector<string> variables;
    string message;
    StripClientMessage(message_length, message, variables);

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
            string error_msg = "GETMSG is incorrectly formated";
            string send_buffer = error_msg;

            Log(string("// SENDING // " + string(send_buffer)));
            if(send(clientSock, send_buffer.data(), send_buffer.length(),0) < 0)
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
            int error_code = SendSENDMSG(0,variables[0],group_name,variables[1]);
            if (error_code == 1)
            {
                string success_msg = "Message is in the botnet.";

                Log(string("// SENDING // " + string(success_msg)));
                if(send(clientSock, success_msg.data(), success_msg.length(), 0) < 0)
                {
                    LogError(string("// CLIENT // Failed to send status message: "+ success_msg));
                }
            }
            else
            {
                return error_code;
            }
        }
        else
        {
            LogError("// COMMAND // SENDMSG has inncorrect ammount of variables");    

            //Respond to client with error.
            string error_msg = "SENDMSG has incorrect ammount of variables";
            string send_buffer = error_msg;

            Log(string("// SENDING // " + string(send_buffer)));
            if(send(clientSock, send_buffer.data(), send_buffer.length(), 0) < 0)
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
        return RespondLISTSERVERS(); 
    }
    else if (message.substr(0, 13) == "CONNECTSERVER")
    {
        Log(string("// CLIENT // Attempting to connect to server specified by client."));
        return RespondCONNECTSERVER(variables);
    }
    else if (message == "MESSAGEBUFFER")
    {
        Log("// CLIENT // Client asking for entirety of message buffer.");
        return RespondMESSAGEBUFFER();
    }
    else if (message == "DOCSERVERS")
    {
        Log("// CLIENT // Client is asking for entirety of documented servers.");
        return RespondDOCSERVERS();
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

void Server::StripClientMessage(int message_length, string &command, vector<string> &variables)
{
    string main = buffer + '\0';
    size_t old_comma_index = 0;
    size_t comma_index = main.find(',');
    size_t semicomma_index = main.find(';');
    if (comma_index != string::npos || semicomma_index != string::npos)
    {
        command = main.substr(0, min(comma_index, semicomma_index));
        if (command == "SENDMSG")
        {
            old_comma_index = comma_index;
            comma_index = main.find(',', (old_comma_index+1));
            variables.emplace_back(main.substr(old_comma_index + 1, comma_index - old_comma_index - 1));
            old_comma_index = comma_index;
            variables.emplace_back(main.substr(old_comma_index + 1));
        }
        else
        {
            while (comma_index != string::npos || semicomma_index != string::npos)
            {
                old_comma_index = min(comma_index, semicomma_index);
                comma_index = main.find(',', (old_comma_index+1));
                semicomma_index = main.find(';', (old_comma_index+1));
                
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
            return -2;
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

int Server::RespondLISTSERVERS()
{
    string send_buffer = "SERVERS: \n";
    size_t pos = 0;
    string group_info;

    for (const auto& group_server : Server::list_of_connections) 
    {
        if (group_server.first == client_name)
        {
            continue;
        }
        group_info = group_server.first +", "+ group_server.second.first +", "+ to_string(group_server.second.second)+";\n";
        send_buffer += group_info;
    }

    send_buffer += '\x04';
    Log(string("// SENDING // " + string(send_buffer)));
    if(send(clientSock, send_buffer.data(), send_buffer.length(), 0) < 0)
    {
        LogError(string("// COMMAND // Failed to send list of servers"));
        return -2;
    }
    else
    {
        Log(string("// COMMAND // Succeeded in sending list of servers"));
        return 1;
    }
}

int Server::RespondGetMSG(string group_id)
{
    string send_buffer;

    //Check if the group has ever sent anything
    if(our_message_buffer.find(group_id) != our_message_buffer.end()) 
    {
        //There is a stored messages
        if(our_message_buffer[group_id].size() > 0)
        {
            LogError(our_message_buffer[group_id].front());
            send_buffer += our_message_buffer[group_id].front();
            our_message_buffer[group_id].pop();

            Log(string("// CLIENT // Group: " + group_id + " Has a messages for client. Responding to client"));
            Log(string("// SENDING // " + string(send_buffer)));
            if(send(clientSock, send_buffer.data(), send_buffer.length(), 0) < 0)
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
            send_buffer += ("Currently no messages from group: " + group_id +".");
            //Respond with there being no messages
            Log(string("// SENDING // " + string(send_buffer)));
            if(send(clientSock, send_buffer.data(), send_buffer.length(), 0) < 0)
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
        send_buffer += ("Group: " + group_id +" has never sent a message.");

        //Responding to client that group has never tried sending
        Log(string("// SENDING // " + string(send_buffer)));
        if(send(clientSock, send_buffer.data(), send_buffer.length(), 0) < 0)
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
    return -1;
}

int Server::RespondMESSAGEBUFFER()
{
    string full_msg = "MESSAGEBUFFER";
    map<string, queue<string>>::iterator it;

    for (it = our_message_buffer.begin(); it != our_message_buffer.end(); it++)
    {
        if (it->second.size() > 0)
        {
            full_msg += ", " + it->first + ", " + to_string(it->second.size());
        }
    }

    Log(string("// SENDING // " + full_msg));
    if(send(clientSock, full_msg.data(), full_msg.length(), 0) < 0)
    {
        LogError(string("// COMMAND // Failed to send MESSAGEBUFFER to client."));
        return -1;
    }
    else
    {
        Log(string("// COMMAND // Successfully sent MESSAGEBUFFER to client."));
        return 1;
    }
}

int Server::RespondCONNECTSERVER(vector<string> variables)
{
    if(variables.size() == 2)
    {
        if (isdigit(variables[1].c_str()[0]))
        {
            struct sockaddr_in new_addr_test;
            if (inet_pton(AF_INET, variables[0].data(), &new_addr_test.sin_addr) > 0)
            {
                if (ConnectToServer(variables[0], stoi(variables[1])))
                {
                    Log("// CLIENT // Successfully connected to specified server.");
                    string success_msg = "Successfully connected to specified server.";
                    Log("// SENDING // " + success_msg);
                    if (send(clientSock, success_msg.data(), success_msg.length(), 0) < 0)
                    {
                        LogError("// CLIENT // Failed to send success message.");
                        return -1;
                    }
                    else
                    {
                        return 1;
                    }
                }
                else
                {
                    Log("// CLIENT // Failed to connect to specified server.");
                    string fail_msg = "Failed to connect to specified server.";
                    Log("// SENDING // " + fail_msg);
                    if (send(clientSock, fail_msg.data(), fail_msg.length(), 0) < 0)
                    {
                        LogError("// CLIENT // Failed to send failure message.");
                        return -1;
                    }
                    else
                    {
                        return 1;
                    }
                }
            }
            else
            {
                LogError(string("// COMMAND // IP address not valid, leaving."));
                return -1;
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

    string fail_msg = "Failed to use variables given, please try again.";
    Log("// SENDING // " + fail_msg);
    if (send(clientSock, fail_msg.data(), fail_msg.length(), 0) < 0)
    {
        LogError("// CLIENT // Failed to send failure message.");
        return -1;
    }
    else
    {
        return 1;
    }
}

int Server::RespondDOCSERVERS()
{
    string send_buffer = "SERVERS: \n";
    size_t pos = 0;
    string group_info;

    for (const auto& group_server : documented_servers) 
    {
        if (group_server.first == client_name)
        {
            continue;
        }
        group_info = group_server.first +", "+ group_server.second.first +", "+ to_string(group_server.second.second)+";\n";
        send_buffer += group_info;
    }

    Log(string("// SENDING // " + string(send_buffer)));
    if(send(clientSock, send_buffer.data(), send_buffer.length(), 0) < 0)
    {
        LogError(string("// COMMAND // Failed to send list of documented servers."));
        return -1;
    }
    else
    {
        Log(string("// COMMAND // Succeeded in sending list of documented servers."));
        return 1;
    }
}