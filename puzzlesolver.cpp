#include <netinet/ip.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <cstdint>
#include <iostream>
#include <fcntl.h>
#include <string>
#include <cstring>
#include <endian.h>

using namespace std;

int is_little_endian() {
    unsigned int x = 1;
    return (*(char *)&x == 1);  // If first byte is 1, it's little-endian
}

int Send_UDP_Packet(int udpsock, void* data, int data_len, void* buffer, int buffer_size, sockaddr_in server_addr, socklen_t server_addr_len)
{
    fd_set set;
    int compare;

    FD_ZERO(&set);
    FD_SET(udpsock, &set);

    for (int j = 0; j < 5; j++)
    {
        // Sending an empty UDP packet to server
        int rsp = sendto(udpsock, data, data_len, 0, (sockaddr*)&server_addr, sizeof(server_addr));

        if (rsp < 0)
        {
            continue;
        }
        else
        {
            // A response!
            int recv_len = recvfrom(udpsock, buffer, buffer_size, 0, (sockaddr*)&server_addr, &server_addr_len);

            if (recv_len <= 0)
            {
                cout << "recv() failed" << endl;
            }
            else
            {
                break;
            }
        }

        FD_ZERO(&set);
        FD_SET(udpsock, &set);
    }

    return 0;
}

int Calculate_Checksum(uint32_t srcIp, uint32_t destIp, uint32_t srcPort, uint32_t destPort, uint32_t checksum, uint32_t data)
{
    uint64_t sum = 0;

    uint32_t reserved = 17;
    
    return 0;
}

int main(int argc, char* argv[])
{
    if (argc != 6)
    {
        cout << "Wrong number of arguments given." << endl;
        cout << "Please run the program like so" << endl;
        cout << "./puzzlesolver <IP address> <port 1> <port 2> <port 3> <port 4>" << endl;
        return 0;
    }

    int* ports = new int[4];
    ports[0] = atoi(argv[2]);
    ports[1] = atoi(argv[3]);
    ports[2] = atoi(argv[4]);
    ports[3] = atoi(argv[5]);
    char buffer[1024];

    string first_puzzle = "Send me";
    string second_puzzle = "The dark";
    string third_puzzle = "Greetings ";
    string fourth_puzzle = "Greetings!";

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    int wait;

    int secretport1 = 0;
    int secretport2 = 0;
    struct timeval wait_time;

    wait_time.tv_sec = 0;
    wait_time.tv_usec = 20000; // 20 ms for each wait.

    int udpsock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    setsockopt(udpsock, SOL_SOCKET, SO_RCVTIMEO, &wait_time, sizeof(wait_time));
    
    if (udpsock < 0)
    {
        cout << "Error on creating UDP socket." << endl;
        return 0;
    }

    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0)
    {
        cout << "Failed to set socket address." << endl;
        return 0;
    }

    socklen_t server_addr_len = sizeof(server_addr);
    uint64_t Signature = 0;

    for (int i = 0; i < 4; i++)
    {
        server_addr.sin_port = htons(ports[i]);

        memset(buffer, 0, sizeof(buffer));
        wait = Send_UDP_Packet(udpsock, NULL, 0, &buffer, sizeof(buffer), server_addr, server_addr_len);

        if (strncmp(buffer, third_puzzle.c_str(), 10) == 0)
        {
            memset(buffer, 0, sizeof(buffer));
            uint64_t group_number = 0x17;
            uint32_t group_secret_mask = 0xc9baa9f8;
            uint64_t response;
            uint64_t message = 0;

            // Don't need to convert group number as it is only a single byte.

            wait = Send_UDP_Packet(udpsock, &group_number, 1, &response, sizeof(response), server_addr, server_addr_len);

            group_number = group_number << 32;
            group_number = htobe64(group_number);
            response = ntohl(response);
            Signature = response ^ group_secret_mask;
            Signature = htobe64(Signature);
            message = group_number | Signature;

            if (is_little_endian())
            {
                message = message >> 24;
            }

            wait = Send_UDP_Packet(udpsock, &message, 5, &buffer, sizeof(buffer), server_addr, server_addr_len);

            cout << buffer << endl;
            string temp = buffer;
            temp = temp.substr(64, 4);
            secretport1 = stoi(temp);
            Signature = be64toh(Signature);
            Signature = htonl(Signature);
        }
    }

    for (int i = 0; i < 4; i++)
    {
        server_addr.sin_port = htons(ports[i]);

        memset(buffer, 0, sizeof(buffer));
        wait = Send_UDP_Packet(udpsock, NULL, 0, buffer, sizeof(buffer), server_addr, server_addr_len);

        if (strncmp(buffer, first_puzzle.c_str(), 7) == 0)
        {
            memset(buffer, 0, sizeof(buffer));
            wait = Send_UDP_Packet(udpsock, &Signature, 4, &buffer, sizeof(buffer), server_addr, server_addr_len);

            int length_of_message = strlen(buffer);
            uint32_t mask;
            uint32_t ip;
            memcpy(&mask, buffer + length_of_message - 6, 2);
            memcpy(&ip, buffer + length_of_message - 4, 4);
        
            mask = ntohl(mask);
            mask = mask >> 16;
            ip = ntohl(ip);

            cout << hex;
            cout << ip << endl;
            cout << mask << endl;
        }

        if (strncmp(buffer, second_puzzle.c_str(), 8) == 0)
        {

            // Don't need to convert group number as it is only a single byte.

            wait = Send_UDP_Packet(udpsock, &group_number, 1, &response, sizeof(response), server_addr, server_addr_len);




        }
        if (strncmp(buffer, fourth_puzzle.c_str(), 10) == 0)
        {
            // Needs to be moved, have to first get secret ports from first and second puzzle.
            continue;
        }
    }
}