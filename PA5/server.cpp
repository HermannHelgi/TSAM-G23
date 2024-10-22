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
                    if ((new_socket = accept(listenSock, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
                        LogError(string("// CONNECT // Failed to make new connection."));
                        continue;
                    }

                    // New connection made
                    struct pollfd new_pollfd = {new_socket, POLLIN, 0};
                    file_descriptors.push_back(new_pollfd);
                    Log(string("// CONNECT // New connection made: " + to_string(new_pollfd.fd)));
                    helo_received[new_pollfd.fd] = -1;
                    SendHELO(new_pollfd.fd);
                }
                else
                {
                    Log(string("// MESSAGE // New message received from: " + to_string(file_descriptors[i].fd)));
                    int valread = recv(file_descriptors[i].fd, buffer, buffer_size, MSG_DONTWAIT);

                    if (valread <= 0) 
                    {
                        // TODO: NEED TO REMOVE NAMES FROM MAP AND VECTOR
                        if (file_descriptors[i].fd == clientSock)
                        {
                            Log(string("// CLIENT // Client Disconnected: " + to_string(file_descriptors[i].fd)));

                            helo_received.erase(file_descriptors[i].fd);
                            list_of_connections.erase(fd_to_group_name[file_descriptors[i].fd]);
                            connection_names.erase(find(connection_names.begin(), connection_names.end(), fd_to_group_name[file_descriptors[i].fd]));
                            group_name_to_fd.erase(fd_to_group_name[file_descriptors[i].fd]);
                            fd_to_group_name.erase(file_descriptors[i].fd);
                            
                            close(file_descriptors[i].fd);
                            file_descriptors.erase(file_descriptors.begin() + i);
                            i--;
                            clientSock = INT32_MAX;
                        }
                        else
                        {
                            Log(string("// DISCONNECT // Server Disconnected: " + to_string(file_descriptors[i].fd)));
                            
                            // In case someone disconnects without saying HELO
                            if (helo_received[file_descriptors[i].fd])
                            {
                                helo_received.erase(file_descriptors[i].fd);
                                list_of_connections.erase(fd_to_group_name[file_descriptors[i].fd]);
                                connection_names.erase(find(connection_names.begin(), connection_names.end(), fd_to_group_name[file_descriptors[i].fd]));
                                group_name_to_fd.erase(fd_to_group_name[file_descriptors[i].fd]);
                                fd_to_group_name.erase(file_descriptors[i].fd);
                                
                                close(file_descriptors[i].fd);
                                file_descriptors.erase(file_descriptors.begin() + i);
                                i--;
                            }
                            else
                            {
                                helo_received.erase(file_descriptors[i].fd);
                                close(file_descriptors[i].fd);
                                file_descriptors.erase(file_descriptors.begin() + i);
                                i--;
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
    if (comma_index != string::npos)
    {
        command = main.substr(0, comma_index);
        while (comma_index != string::npos)
        {
            old_comma_index = comma_index;
            comma_index = main.find(',', (old_comma_index+1));
            
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
        connection_names.emplace_back("CLIENT");
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
            fd_to_group_name[clientSock] = "CLIENT";
            group_name_to_fd["CLIENT"] = clientSock;
            string ip_address = inet_ntoa(sin.sin_addr);
            list_of_connections["CLIENT"] = {ip_address, ntohs(sin.sin_port)};
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
    
    if (command == "HELO")
    {
        Log(string("// COMMAND // HELO detected. Taking in data."));
        return RespondHELO(fd, variables);
    }
    else if (command == "SERVERS" && helo_received[fd])
    {
        Log(string("// COMMAND // SERVERS detected. Taking in data."));
        // TODO
        // Don't really know what we should do with this stuff? Maybe check if we're not ocnnected to enough servers then send out some connections?
    }
    else if (command == "KEEPALIVE" && helo_received[fd])
    {
        Log(string("// COMMAND // KEEPALIVE detected. Taking in data."));
        return RespondKEEPALIVE(fd, variables);
    }
    else if (command == "GETMSGS" && helo_received[fd])
    {
        Log(string("// COMMAND // GETMSGS detected. Sending data."));
        return RespondGETMSGS(fd, variables);
    }
    else if (command == "SENDMSG" && helo_received[fd])
    {
        // TODO
        Log(string("// COMMAND // SENDMSG detected. Sending data"));
        if(variables.size() == 3)
        {
            return SendSENDMSG(fd,variables[0],variables[1],variables[2]);
        }
        else
        {
            LogError("// COMMAND// SENDMSG has inncorrect ammount of variables");
            return -1;
        }
    }
    else if (command == "STATUSREQ" && helo_received[fd])
    {
        Log(string("// COMMAND // STATUSREQ detected. Sending STATUSRESP."));
        return SendSTATUSRESP(fd);
    }
    else if (command == "STATUSRESP" && helo_received[fd])
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

int Server::SendSENDMSG(int fd, string to_group_name, string from_group_name, string data)
{
    char send_buffer[5121];
    //Begin by storing the message.
    if(to_group_name == group_name) // The msg is addressed to us
    {
        our_message_buffer[from_group_name].push(data); //Store in private buffer
        Log(string("// COMMAND // Message is addressed to us. Storing message."));
        return 1;
    }
    else
    {
        //check if to_group_name is connected
        if(find(connection_names.begin(),connection_names.end(),to_group_name) != connection_names.end())
        {
            Log(string("// COMMAND // Conected to group: " + to_group_name + " attempting to send message"));
            strcat(buffer,data.c_str());
            if(send(group_name_to_fd[to_group_name],buffer,sizeof(buffer),0) < 0)
            {
                LogError(string("// COMMAND // Failed sending massge to group: " + to_group_name));
                Log(string("// COMMAND // Storring message"));
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
            Log(string("// COMMAND // Storring message"));
            other_groups_message_buffer[to_group_name].push_back({from_group_name,data});
            return -1;
        }
    }


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
}

int Server::SendSTATUSRESP(int fd)
{
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
            return SendGETMSG(fd, group_name);
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

int Server::SendGETMSG(int fd, string var)
{
    return 1;
}

//Sends a list servers to the given file_descriptor
int Server::SendSERVERS(int fd)
{
    char send_buffer[5121];
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

        // LOG
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

        //Check if enough variables revived. i.e GETMSG,Group_id

    }
    else if (message.substr(0, 7) == "SENDMSG")
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
        Log(string("// COMMAND // Attempting to list of servers to client"));
        //We can just reuse our servers function for this
        return SendSERVERS(clientSock); 
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