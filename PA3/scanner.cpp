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

using namespace std;

int main(int argc, char* argv[])
{
    // Checking inputs
    if (argc != 4)
    {
        cout << "Missing arguments." << endl;
        cout << "Please run the program like so" << endl;
        cout << "./scanner <IP address> <low port> <high port>" << endl;
        return 0;
    }

    // Taking ports
    int lowport = atoi(argv[2]);
    int highport = atoi(argv[3]);

    // Creating socket with timeout
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;

    struct timeval wait_time;
    wait_time.tv_sec = 0;
    wait_time.tv_usec = 20000; // 20 ms for each wait.

    // Setting own socket.
    int udpsock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    setsockopt(udpsock, SOL_SOCKET, SO_RCVTIMEO, &wait_time, sizeof(wait_time));
    if (udpsock < 0)
    {
        cout << "Error on creating UDP socket." << endl;
        return 0;
    }

    // Setting servers IP address.
    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0)
    {
        cout << "Failed to set socket address." << endl;
        return 0;
    }
    
    // Additional Variables
    socklen_t server_addr_len = sizeof(server_addr);
    char recv_buf[1024];  // Buffer to hold incoming messages
    int all_ports[4]; // Buffer to store ports
    int current_index_of_ports = 0;
    int rsp; // Response check for recv
    int resend_amount = 5; // Repeats five times, can be set for higher if UDP packets are consistently being dropped.

    for (int i = 0; i < 4; i++)
    {
        all_ports[i] = 0;
    }

    cout << "Starting search." << endl;
    // Cycles through all ports given.
    for (int i = lowport; i < highport+1; i++)
    {
        memset(recv_buf, 0, sizeof(recv_buf));
        server_addr.sin_port = htons(i);

        if (i == (lowport + highport) / 2) // For clarity.
        {
            cout << "Halfway done." << endl;
        }

        for (int j = 0; j < resend_amount; j++)
        {
            // Sending an empty UDP packet to server
            rsp = sendto(udpsock, NULL, 0, 0, (sockaddr*)&server_addr, sizeof(server_addr));

            // Continues if it doesn't get a proper response within the set timeout.
            if (rsp < 0)
            {
                continue;
            }
            else
            {
                // A response has been received!
                int recv_len = recvfrom(udpsock, recv_buf, sizeof(recv_buf), 0, (sockaddr*)&server_addr, &server_addr_len);

                if (recv_len <= 0)
                {
                    continue; // Recv failed, try again next cycle.
                }
                else
                {
                    // Successfully received the response, writing down which port was open.
                    all_ports[current_index_of_ports] = i;
                    current_index_of_ports++;
                    break;
                }
            }
        }
    }

    // Printing.
    cout << "Ports: ";
    for (int i = 0; i < 4; i++)
    {
        cout << all_ports[i] << " ";
    }
    cout << endl;
}