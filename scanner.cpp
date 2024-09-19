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
    if (argc != 4)
    {
        cout << "Missing arguments." << endl;
        return 0;
    }

    int lowport = atoi(argv[2]);
    int highport = atoi(argv[3]);
    struct sockaddr_in server_addr;

    server_addr.sin_family = AF_INET;

    // Setting own socket.
    int udpsock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    
    if (udpsock < 0)
    {
        cout << "Error on creating UDP socket." << endl;
        return 0;
    }

    int flags = fcntl(udpsock, F_GETFL, 0);       // Get current flags
    fcntl(udpsock, F_SETFL, flags | O_NONBLOCK);  // Set non-blocking flag

    // Setting servers IP address.
    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0)
    {
        cout << "Failed to set socket address." << endl;
        return 0;
    }
    
    fd_set set;
    struct timeval wait_time;
    int compare;
    char recv_buf[1024];  // Buffer to hold incoming messages
    socklen_t server_addr_len = sizeof(server_addr);
    bool success = false;

    for (int i = lowport; i < highport+1; i++)
    {
        memset(recv_buf, 0, sizeof(recv_buf));
        success = false;
        server_addr.sin_port = htons(i);
        FD_ZERO(&set);
        FD_SET(udpsock, &set);

        for (int j = 0; j < 5; j++)
        {
            wait_time.tv_sec = 0;
            wait_time.tv_usec = 20000; // 20 ms for each wait.
            
            // Sending an empty UDP packet to server
            sendto(udpsock, NULL, 0, 0, (sockaddr*)&server_addr, sizeof(server_addr));

            // Select begins checking what compare is, blocks until it either timesout or gets an answer.
            compare = select(udpsock + 1, &set, NULL, NULL, &wait_time);
            if (compare < 0)
            {
                cout << "Error on select()." << endl;
                continue;
            }
            else if (compare == 0) // Timeout or no response
            {
                continue;
            }
            else
            {
                // A response!
                int recv_len = recvfrom(udpsock, recv_buf, sizeof(recv_buf), 0, (sockaddr*)&server_addr, &server_addr_len);

                if (recv_len > 0)
                {
                    // Successfully received a response; port is open
                    cout << "Port: " << i << endl;
                    cout << recv_buf << endl;
                }
                else
                {
                    cout << "recv() failed" << endl;
                }
                success = true;
                break;
            }

            FD_ZERO(&set);
            FD_SET(udpsock, &set);
        }

        if (!success)
        {
            cout << "Failed: " << i << endl;
        }
        cout << endl;
    }
}