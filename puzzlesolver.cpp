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
#include <vector>
#include <sstream>
#include <unistd.h>
#include <netinet/ip_icmp.h>

using namespace std;

int is_little_endian() {
    unsigned int x = 1;
    return (*(char *)&x == 1);  // If first byte is 1, it's little-endian
}

int Send_UDP_Packet(int udpsock, const void* data, int data_len, void* buffer, int buffer_size, sockaddr_in server_addr, socklen_t server_addr_len)
{
    // Sends a UDP packet and collects the response. 
    // If the packet is dropped by the network it resends up to a maximum of 5 times.
    for (int j = 0; j < 5; j++)
    {
        // Sending a UDP packet to server
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
                continue;
            }
            else
            {
                break;
            }
        }
    }

    return 0;
}

int Calculate_Checksum(uint32_t srcIp, uint32_t destIp, uint32_t srcPort, uint16_t destPort, uint16_t checksum)
{
    // Calculates data from a given checksum.

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

    if (comparison < checksum) // There was a carry on the data, needs to be reduce from the difference
    {
        data -= checksum - comparison;
    }
    
    return data;
}

int main(int argc, char* argv[])
{
    // Error check
    if (argc != 6)
    {
        cout << "Wrong number of arguments given." << endl;
        cout << "Please run the program like so" << endl;
        cout << "./puzzlesolver <IP address> <port 1> <port 2> <port 3> <port 4>" << endl;
        return 0;
    }

    // Taking in port numbers
    int* ports = new int[4];
    ports[0] = atoi(argv[2]);
    ports[1] = atoi(argv[3]);
    ports[2] = atoi(argv[4]);
    ports[3] = atoi(argv[5]);
    char buffer[1024];

    // Comparison strings
    string checksum_puzzle = "Send me";
    string evil_puzzle = "The dark";
    string secret_puzzle = "Greetings ";
    string knock_puzzle = "Greetings!";

    // Making server_addr
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    int wait; // Variable to try and mitigate IO/times for filling up the buffer, kinda doesn't have a purpose except to delay stuff

    // Storable variables
    int secretport1 = 0;
    string secretphrase = "";
    int secretport2 = 0;
    uint64_t Signature = 0;

    // Timeout for socket
    struct timeval wait_time;
    wait_time.tv_sec = 0;
    wait_time.tv_usec = 20000; // 20 ms for each wait.

    // Making UDP socket
    int udpsock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    setsockopt(udpsock, SOL_SOCKET, SO_RCVTIMEO, &wait_time, sizeof(wait_time));
    if (udpsock < 0)
    {
        cout << "Error on creating UDP socket." << endl;
        return 0;
    }

    // Translating server ip addr from string to decimal
    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0)
    {
        cout << "Failed to set socket address." << endl;
        return 0;
    }

    // Size of socket for later function calls
    socklen_t server_addr_len = sizeof(server_addr);

    for (int i = 0; i < 4; i++)
    { // Searches for S.E.C.R.E.T port to make correct signature.
        server_addr.sin_port = htons(ports[i]);

        memset(buffer, 0, sizeof(buffer));
        wait = Send_UDP_Packet(udpsock, NULL, 0, &buffer, sizeof(buffer), server_addr, server_addr_len);
        sleep(0.5);

        if (strncmp(buffer, secret_puzzle.c_str(), 10) == 0) // Looking for S.E.C.R.E.T phrase.
        {
            memset(buffer, 0, sizeof(buffer));

            // Setting some necessary variables.
            uint64_t group_number = 0x17; // Hardcoded for assignment
            uint32_t group_secret_mask = 0xc9baa9f8; // Hardcoded for assignment
            uint64_t response;
            uint64_t message = 0;

            // Don't need to convert group number to network byte order as it is only a single byte.
            wait = Send_UDP_Packet(udpsock, &group_number, 1, &response, sizeof(response), server_addr, server_addr_len);

            // Endian manipulation and bit twiddling, *hopefully* works on a big endian machine.
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

            // Sends signature + group number.
            wait = Send_UDP_Packet(udpsock, &message, 5, &buffer, sizeof(buffer), server_addr, server_addr_len);

            // Collects the secretport.
            string temp = buffer;
            temp = temp.substr(64, 4);
            secretport1 = stoi(temp);
            Signature = be64toh(Signature);
            Signature = htonl(Signature);
            cout << "First secret port: " << secretport1 << endl;
        }
    }

    for (int i = 0; i < 4; i++)
    {
        // Since we now have the necessary signature, we can do the Evil port and the Checksum port.
        server_addr.sin_port = htons(ports[i]);

        memset(buffer, 0, sizeof(buffer));
        wait = Send_UDP_Packet(udpsock, NULL, 0, buffer, sizeof(buffer), server_addr, server_addr_len);
        sleep(0.5);

        if (strncmp(buffer, checksum_puzzle.c_str(), 7) == 0) // Checksum port.
        {
            memset(buffer, 0, sizeof(buffer));
            // Send signature.
            wait = Send_UDP_Packet(udpsock, &Signature, 4, &buffer, sizeof(buffer), server_addr, server_addr_len);

            // Collect data for checksum problem.
            int length_of_message = strlen(buffer);
            uint32_t checksum;
            uint32_t ip;
            memcpy(&checksum, buffer + length_of_message - 6, 2);
            memcpy(&ip, buffer + length_of_message - 4, 4);
        
            // Endian manipulation
            checksum = ntohl(checksum);
            checksum = checksum >> 16;
            ip = ntohl(ip);

            // Making IPv4 and UDP headers.
            struct sockaddr_in sin;
            socklen_t len = sizeof(sin);
            if (getsockname(udpsock, (struct sockaddr *)&sin, &len) == -1)
                cout << "Getsockname failed." << endl;

            // Getting data for correct checksum
            uint32_t data = Calculate_Checksum(ip, ntohl(inet_addr(argv[1])), ntohs(sin.sin_port), ports[i], checksum);

            // IPv4 header.
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

            // UDP header.
            udphdr->check = htons(checksum);
            udphdr->dest = htons((unsigned int)ports[i]);
            udphdr->source = sin.sin_port; // Comes out in network byte order
            udphdr->len = htons(10);

            // Data
            memcpy(packet + sizeof(struct ip) + sizeof(struct udphdr), &data, 2);

            memset(buffer, 0, sizeof(buffer));
            // Sending packet
            wait = Send_UDP_Packet(udpsock, packet, 30, &buffer, sizeof(buffer), server_addr, server_addr_len);
            secretphrase = buffer;

            // Getting secret phrase.
            int first_location = secretphrase.find('"');
            int second_location = secretphrase.find('"', first_location + 1);
            secretphrase = secretphrase.substr(first_location + 1, second_location - first_location - 1);
            cout << "Secret phrase: " << secretphrase << endl;
        }

        if (strncmp(buffer, evil_puzzle.c_str(), 8) == 0) // Evil port
        {

            // Making IPv4 Header and UDP header.
            unsigned char* packet = new unsigned char[32];
            memset(packet, 0, 32);
            memset(buffer, 0, sizeof(buffer));

            if (getsockname(udpsock, (struct sockaddr *)&server_addr, &server_addr_len) == -1) // Getting UDP socket port
                cout << "Getsockname failed." << endl;

            // IPv4
            uint32_t reserved_bit_mask = 0x8000;
            struct ip *iphdr;

            iphdr = (struct ip*)packet;
            iphdr->ip_v = 4;
            iphdr->ip_hl = 5;
            iphdr->ip_len = htons(32);
            iphdr->ip_tos = 0;
            iphdr->ip_id = 0;
            iphdr->ip_off = htons(iphdr->ip_off | reserved_bit_mask);
            iphdr->ip_ttl = 64;
            iphdr->ip_p = IPPROTO_UDP;
            iphdr->ip_dst.s_addr = inet_addr(argv[1]);
            iphdr->ip_src.s_addr = htonl(server_addr.sin_addr.s_addr);
            iphdr->ip_sum = 0;

            // UDP header
            struct udphdr *udphdr = (struct udphdr*)(packet + sizeof(struct ip));

            udphdr->dest = htons((unsigned int)ports[i]);
            udphdr->source = server_addr.sin_port; // Comes out in network byte order
            udphdr->len = htons(12);
            udphdr->check = 0;

            // Setting data
            memcpy(packet + sizeof(struct ip) + sizeof(struct udphdr), &Signature, 4);

            // Making raw socket
            // NOTE requires root privilage to run! (Sudo)

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

            // Sending message multiple times in case of packet getting dropped.
            for (int i = 0; i < 5; i++)
            {

                int rsp = sendto(raw_sock, packet, 32, 0, (sockaddr*)&server_addr, sizeof(server_addr));
                if(rsp < 0)
                {
                    continue;
                }   
                else
                {
                    int recv_len = recvfrom(udpsock, buffer, 1024, 0, (sockaddr*)&server_addr, &server_addr_len);
                
                    if (recv_len <= 0)
                    {
                        continue;
                    }
                    else
                    {
                        break;
                    }
                
                }
            }

            // Getting port.
            string temp = buffer;
            temp = temp.substr(72, 5);
            secretport2 = stoi(temp);
            cout << "Second secret port: " << secretport2 << endl;
        }

    }
    
    for (int i = 0; i < 4; i++) // Cycling again through to find Knock puzzle, since all data is gathered.
    {
        server_addr.sin_port = htons(ports[i]);

        memset(buffer, 0, sizeof(buffer));
        wait = Send_UDP_Packet(udpsock, NULL, 0, buffer, sizeof(buffer), server_addr, server_addr_len);
        sleep(0.5);

        if (strncmp(buffer, knock_puzzle.c_str(), 10) == 0) // Knock port comparison
        {

            // Sending ports.
            string data = to_string(secretport1) + "," + to_string(secretport2);
            memset(buffer, 0, sizeof(buffer));

            wait = Send_UDP_Packet(udpsock, data.data(), data.length(), &buffer, sizeof(buffer), server_addr, server_addr_len);
            
            // Setting up knock pattern
            string knock = buffer;
            stringstream s(knock);
            string knock_port;

            // Secret phrase + Signature.
            unsigned char* packet = new unsigned char[4 + secretphrase.length()];
            memcpy(packet,&Signature,4);
            memcpy(packet + 4, secretphrase.c_str(), secretphrase.length());

            while (!s.eof()) // Sending until stringstream finishes.
            {
                getline(s, knock_port, ',');
                server_addr.sin_port = htons(stoi(knock_port));

                memset(buffer, 0, sizeof(buffer));
                wait = Send_UDP_Packet(udpsock, packet, 4 + secretphrase.length(), &buffer, sizeof(buffer), server_addr, server_addr_len);

            }
            cout << buffer;
            int recv_len = recvfrom(udpsock, buffer, 1024, 0, (sockaddr*)&server_addr, &server_addr_len);
        }
    }

    // Making IPv4 Header and ICMP header.
    unsigned char* packet = new unsigned char[18];
    memset(packet, 0, 18);
    memset(buffer, 0, sizeof(buffer));
    
    // ICMP header
    struct icmp *icmp_hdr = (struct icmp*)(packet);

    icmp_hdr->icmp_type = ICMP_ECHO;  
    icmp_hdr->icmp_code = 0;
    icmp_hdr->icmp_cksum = 0; 

    char group_data[10];
    group_data[0] = '$';
    group_data[1] = 'g';
    group_data[2] = 'r';
    group_data[3] = 'o';
    group_data[4] = 'u';
    group_data[5] = 'p';
    group_data[6] = '_';
    group_data[7] = '2';
    group_data[8] = '3';
    group_data[9] = '$';

    // Setting data
    memcpy(packet + 8, &group_data, 10);

    // Making raw socket
    // NOTE requires root privilage to run! (Sudo)

    int raw_sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if(raw_sock < 0)
    {
        cout << "Error on creating raw socket." << endl;
    }
    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0)
    {
        cout << "Failed to set socket address." << endl;
    }

    sendto(raw_sock, packet, 18, 0, (sockaddr*)&server_addr, sizeof(server_addr));
}