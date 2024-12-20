#include "server.h"

int main(int argc, char* argv[])
{
    if(argc != 2)
    {
        printf("Usage: chat_server <port>\n");
        exit(0);
    }

    int port = atoi(argv[1]);
    freopen("ErrorLog.txt", "a", stderr);
    freopen("Log.txt", "a", stdout);

    Server main_server(port);
    main_server.InitializeServer();

    bool finished = false;
    while(!finished)
    {
        // TODO: Add KEEPALIVE timer which at a rate of once per 5~ minutes sends to all connected servers that we have messages.
        main_server.CheckForMoreConnections();
        main_server.CheckTimeouts();
        main_server.CheckKeepalive();
        int check = main_server.CheckMessages();
        if (check < 0)
        {
            finished = true;
        }
    }
}