#include <stdio.h>
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
#include <vector>
#include <thread>
#include <ctime>
#include <iostream>
#include <sstream>

using namespace std;

int main(int argc, char* argv[])
// Main function which runs all the necessary code for the client
{
    int server_sock;                           // Socket variable which is going to be used to send to server
    struct sockaddr_in server_addr;     // The socket address variable used to set the preset of the server
    int buffer_size = 1024;
    char buffer[buffer_size + 1];

    // Checking if right amount of arguments is given.
    if (argc != 3) 
    {
        perror("Wrong number of arguments given. First argument should be the IP address and second to should be the port.");
        return 0;
    }

    // Setting variables for connection
    server_addr.sin_family = AF_INET;  
    server_addr.sin_port = htons(atoi(argv[2]));

    cout << "Connecting to IP: " << argv[1] << endl;
    cout << "Connecting to port: " << atoi(argv[2]) << endl;

    // Getting socket for the client
    if ((server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    {
        perror("Failed to open socket");
        return(-1);
    }

    // Getting the servers socket address
    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0)
    {
        perror("Failed to set socket address.");
        exit(0);
    }

    // Connect to remote address
    if (connect(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Could not connect");
        return 0;
    }

    cout << "Server connected, please type a message:" << endl;
    string message_intake;

    while (true)
    {
        getline(cin, message_intake); // Get the users command
        string new_string;
        if (message_intake == "DOUBLE")
        {
            getline(cin, message_intake);
            new_string += message_intake + '\x04' + '\x01';
            getline(cin, message_intake);
            new_string += message_intake;
            message_intake = new_string;
        }

        if (message_intake == "Exit" || message_intake == "exit" || message_intake == "EXIT")
        {
            break;
        }

        new_string = '\x01' + message_intake + '\x04';
        cout << "Sending: " << new_string << endl;
        if (send(server_sock, new_string.c_str(), new_string.length(), 0) < 0)
        {
            cout << "Error on sending command, please try again." << endl;
        }

        int bytes = recv(server_sock, buffer, buffer_size, 0);
        cout << "Bytes received: " << bytes;
        buffer[bytes] = '\0';

        if (bytes <= 0)
        {
            cout << "Server disconnected, quitting." << endl;
            break;
        }
        else
        {
            char timebuffer[64];
            time_t timestamp = time(NULL);
            struct tm* localTime = localtime(&timestamp);
            strftime(timebuffer, sizeof(timebuffer), "[%Y-%m-%d %H:%M:%S] ", localTime);

            cout << timebuffer;
            cout << buffer << endl;
        }
    }
}