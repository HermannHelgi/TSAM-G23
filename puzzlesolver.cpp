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

//Returns in network byte order !
int Calculate_checksum_ipv4(ip* my_header)
{

    uint32_t sum;

    sum += my_header->ip_v;
    sum += my_header->ip_hl;
    sum += my_header->ip_tos;

    sum += my_header->ip_len;

    sum += my_header->ip_id;
    
    sum += my_header->ip_off;
    sum += my_header->ip_ttl;

    //Check if need to carry the front bit
    if(sum & 0xFFFF0000)
    {
        sum = (sum & 0xFFFF) + (sum >> 16);
    };

    sum += my_header->ip_src.s_addr;
    if(sum & 0xFFFF0000)
    {
        sum = (sum & 0xFFFF) + (sum >> 16);
    };
    sum += my_header->ip_dst.s_addr;


    return htonl(sum);
}

int Calculate_Checksum(uint32_t srcIp, uint32_t destIp, uint32_t srcPort, uint16_t destPort, uint16_t checksum, uint32_t data)
{
    uint64_t sum = 0;
    sum += 0x11;

    uint32_t srcIp2 = (srcIp >> 16) & 0xFFFF; // top half
    srcIp = srcIp & 0xFFFF;
    sum += srcIp2 + srcIp;

    uint32_t destIp2 = (destIp >> 16) & 0xFFFF; // top half
    destIp = destIp & 0xFFFF;
    sum += destIp2 + destIp;

    sum += destPort;
    sum += srcPort;

    if (data == 0) // Need to find data
    {
        sum += 10; // Length in pseudo header
        sum += 10; // Length in header

        uint32_t sumTemp = (sum >> 16) & 0xFFFF;
        sum = sum & 0xFFFF;
        sum += sumTemp;
        sumTemp = 0;

        // Could potentially overflow from the carry-bit, safety check.
        sumTemp = (sum >> 16) & 0xFFFF;
        sum = sum & 0xFFFF;
        sum += sumTemp;
        sumTemp = 0;

        uint64_t data = ((~checksum) - sum) & 0xFFFF;
        uint64_t comparison = sum + data;
        sumTemp = (comparison >> 16) & 0xFFFF;
        comparison = comparison & 0xFFFF;
        comparison += sumTemp;

        comparison = (~comparison) & 0xFFFF;

        if (comparison < checksum) // There was a carry on the data, needs to get a +1
        {
            data -= checksum - comparison;
        }
        
        return data;
    }
    else if (checksum == 0) // need to find checksum
    {
        // THIS COULD POTENTIALLY LEAD TO BUGS IF DATA IS SMALLER THAN 4 BYTES
        sum += 12; // Length in pseudo header
        sum += 12; // Length in header

        uint32_t sumTemp = (sum >> 16) & 0xFFFF;
        sum = sum & 0xFFFF;
        sum += sumTemp;
        sumTemp = 0;

        // Could potentially overflow from the carry-bit, safety check.
        sumTemp = (sum >> 16) & 0xFFFF;
        sum = sum & 0xFFFF;
        sum += sumTemp;
        sumTemp = 0;

        sum += (data >> 16) & 0xFFFF;
        sum += data & 0xFFFF;

        sumTemp = (sum >> 16) & 0xFFFF;
        sum = sum & 0xFFFF;
        sum += sumTemp;
        sumTemp = 0;

        // Could potentially overflow from the carry-bit, safety check.
        sumTemp = (sum >> 16) & 0xFFFF;
        sum = sum & 0xFFFF;
        sum += sumTemp;

        sum = (~sum) & 0xFFFF;

        return sum;
    }
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
    string secretphrase = "";
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
            uint32_t checksum;
            uint32_t ip;
            memcpy(&checksum, buffer + length_of_message - 6, 2);
            memcpy(&ip, buffer + length_of_message - 4, 4);
        
            checksum = ntohl(checksum);
            checksum = checksum >> 16;
            ip = ntohl(ip);

            struct sockaddr_in sin;
            socklen_t len = sizeof(sin);
            if (getsockname(udpsock, (struct sockaddr *)&sin, &len) == -1)
                cout << "Getsockname failed." << endl;

            uint32_t data = Calculate_Checksum(ip, ntohl(inet_addr(argv[1])), ntohs(sin.sin_port), ports[i], checksum, 0);

            unsigned char* packet = new unsigned char[30];
            memset(packet, 0, 30);
            struct ip *iphdr = (struct ip*)packet;
            struct udphdr *udphdr = (struct udphdr*)(packet + sizeof(struct ip));

            data = htons(data);

            iphdr->ip_hl = 5;
            iphdr->ip_v = 4;
            iphdr->ip_tos = 0;
            iphdr->ip_len = htons(30);
            iphdr->ip_off = 0;
            iphdr->ip_ttl = 64;
            iphdr->ip_p = IPPROTO_UDP;
            iphdr->ip_dst.s_addr = inet_addr(argv[1]); // Comes out in network byte order
            iphdr->ip_src.s_addr = htonl(ip);
            iphdr->ip_sum = 0;

            udphdr->check = htons(checksum);
            udphdr->dest = htons((unsigned int)ports[i]);
            udphdr->source = sin.sin_port; // Comes out in network byte order
            udphdr->len = htons(10);

            memcpy(packet + sizeof(struct ip) + sizeof(struct udphdr), &data, 2);

            memset(buffer, 0, sizeof(buffer));
            wait = Send_UDP_Packet(udpsock, packet, 30, &buffer, sizeof(buffer), server_addr, server_addr_len);
            secretphrase = buffer;
            int first_location = secretphrase.find('"');
            int second_location = secretphrase.find('"', first_location + 1);
            secretphrase = secretphrase.substr(first_location + 1, second_location - first_location - 1);
        }

        if (strncmp(buffer, second_puzzle.c_str(), 8) == 0)
        {
            unsigned char* packet = new unsigned char[32];
            memset(packet, 0, 32);
            memset(buffer, 0, sizeof(buffer));

            if (getsockname(udpsock, (struct sockaddr *)&server_addr, &server_addr_len) == -1)
                cout << "Getsockname failed." << endl;

            uint32_t reserved_bit_mask = 0x8000;
            struct ip *iphdr;

            //Note what ever is larger then one byte NEEDS to be in network byte order
            iphdr = (struct ip*)packet;
            iphdr->ip_v = 4;
            iphdr->ip_hl = 5;
            iphdr->ip_len = 32;
            iphdr->ip_tos = 0;
            iphdr->ip_id = 0;
            iphdr->ip_off = iphdr->ip_off | reserved_bit_mask;
            iphdr->ip_ttl = 64;
            iphdr->ip_p = IPPROTO_UDP;
            iphdr->ip_dst.s_addr = inet_addr(argv[1]);
            iphdr->ip_src.s_addr = htonl(server_addr.sin_addr.s_addr);
            iphdr->ip_sum = 0;

            struct udphdr *udphdr = (struct udphdr*)(packet + sizeof(struct ip));

            udphdr->dest = htonl(ports[i]);
            udphdr->source = server_addr.sin_port; // Comes out in network byte order
            udphdr->len = htons(12);
            udphdr->check = 0;

            memcpy(packet + sizeof(struct ip) + sizeof(struct udphdr), &Signature, 4);

            //TODO create the raw socket so that we can use our own ip and udp headers.
            //NOTE requires root privlage to run !!
            //  sudo ./puzzlesolver ....

            cout << hex;
            for (int i = 0; i < 32; i++)
            {
                cout << (unsigned int)packet[i] << endl;
            }
            cout << Signature << endl;

            int raw_sock = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
            if(raw_sock < 0)
            {
                cout << "Error on creating raw socket." << endl;
            }
            int opt = 1;
            if(setsockopt(raw_sock, IPPROTO_IP, IP_HDRINCL, &opt, sizeof(opt)) < 0)
            { 
                cout << "Failed to set socket options" << endl;
            }
            if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0)
            {
                cout << "Failed to set socket address." << endl;
            }

            //TODO send & recv data
            int rsp = sendto(raw_sock, packet, 32, 0, (sockaddr*)&server_addr, sizeof(server_addr));
            if(rsp > 0)
            {
                cout << "We got a respone :). Time to see what it says..." << endl;
            }
            
            int recv_len = recvfrom(udpsock, buffer, 1024, 0, (sockaddr*)&server_addr, &server_addr_len);
            cout << endl;
            cout << buffer << endl;

        }
        if (strncmp(buffer, fourth_puzzle.c_str(), 10) == 0)
        {
            // Needs to be moved, have to first get secret ports from first and second puzzle.
            continue;
        }
    }
}