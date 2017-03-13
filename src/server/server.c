
#ifdef __WIN32
    #undef UNICODE
    #define WIN32_LEAN_AND_MEAN
#endif // defined __WIN32

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "gsrvsocks.h"
#include "service.h"

// Need to link with Ws2_32.lib
// #pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

// App Data.

// Arg: Port number on which we'll listen.
int runServer(const char* port)
{
    printf("Init start vars... ");

    int iResult;

    SOCKET ListenSocket = INVALID_SOCKET;
    GsrvClientSocket ClientSocket[GSRV_MAX_CLIENTS];

    fd_set readfds; // The fd_set of the socket descriptors which we will check with SELECT.
    SOCKET max_fds; // The highest file desctiptor number, needed for SELECT to check.

    // Optimize the program work by allocating variable memory on the stack at the beginning of the program.
    struct sockaddr_in sin;
    socklen_t sinlen = sizeof(sin);

    struct addrinfo *result = NULL;
    struct addrinfo hints;

    int iSendResult;

    //============================================//
    // Init WinSocks.
    printf("Done.\nInit WinSock... ");
    if(gsockInitSocks() != 0)
        return 1;

    printf("Done.\nInit addrinfo's and SockBuffs ...");

    // Set the variables
    for(int i=0; i<GSRV_MAX_CLIENTS; i++){
        gsrvInitClientSocket(ClientSocket+i, INVALID_SOCKET, 0);
    }
    memset(&hints, 0, sizeof(hints));

    // Set the hints for the preferred sockaddr properties.
    hints.ai_family = AF_INET;       // Use IPv4 socket mode.
    hints.ai_socktype = SOCK_STREAM; // Stream mode. (For UDP, we use Datagram)
    hints.ai_protocol = IPPROTO_TCP; // Use TCP for communicating.
    hints.ai_flags = AI_PASSIVE;     // Use it for listening.

    //==============================================//
    // Resolve the server address and port with the specified hints.
    printf("Done.\nCalling getAddrInfo, with specified port number as a service... ");
    iResult = getaddrinfo(NULL, port, &hints, &result);

    if ( iResult != 0 ) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        gsockSockCleanup();
        return 1;
    }

    // Create a listening SOCKET for connecting to a client.
    printf("Done.\nCreate a ListenSocket (socket())...");
    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        return gsockErrorCleanup(INVALID_SOCKET, result, "socket failed with error", 1, 1);
    }

    // Setup the TCP listening socket, bind it to a local server address.
    printf("Done.\nBinding ListenSocket... ");
    iResult = bind( ListenSocket, result->ai_addr, (int)(result->ai_addrlen));
    if (iResult == SOCKET_ERROR) {
        return gsockErrorCleanup(ListenSocket, result, "bind failed with error", 1, 1);
    }

    printf("Done.\nFreeAddrInfo()... ");
    freeaddrinfo(result);

    // Mark the ListenSocket as the socket willing to accept incoming connections.
    // Max pending connections: SOMAXCONN. This is defined by system.
    printf("Done.\nlisten(ListenSocket)... ");
    iResult = listen(ListenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        return gsockErrorCleanup(ListenSocket, NULL, "listen failed with error", 1, 1);
    }

    //===================================================//
    // Print on which port the socket is listening.
    printf("Done.\nGetSockName()... ");
        if (getsockname(ListenSocket, (struct sockaddr *)&sin, &sinlen) == -1)
            printf("getsockname err. can't get port.\n");
    else
        printf("\nThe server is listening on port: %d\n", ntohs(sin.sin_port));

    // Initialize and start accept/receive loop.
    // Some state vars. Timeout to use in SELECT and how many conns were acceptz0red.
    int connectionsAccepted = 0;
    int selectErrCount = 0;
    struct timeval tm;

    char exitLoop = 0;

    printf("Done.\n\nStarting Loop... \n");
    while(!exitLoop) // Run a server loop.
    {
        // We will use the SELECT function to async'ly check which socks have pending conn's, and perform stuff if they don't.
        //clear the socket set
        FD_ZERO(&readfds);

        // If we have jobs, SELECT timeouts after 1 ms.
        tm.tv_sec = 0;
        tm.tv_usec = 1;

        // Add ListenSocket to set
        FD_SET(ListenSocket, &readfds);
        max_fds = ListenSocket;

        char haveActiveJobs = 0;

        // Now add every of the ClientSockets to the fd_set
        for(int i=0; i<GSRV_MAX_CLIENTS; i++)
        {
            SOCKET clsd = ClientSocket[i].cliSock;

            if(clsd != INVALID_SOCKET){ // Valid socket
                FD_SET(clsd, &readfds);

                if(clsd > max_fds) // This desc number is highest.
                    max_fds = clsd;

                if(gsrvHaveActiveJobs(ClientSocket+i))
                    haveActiveJobs = 1;
            }
        }

        // Perform SELECT to check the socket state.
        // We check only the read buffer fd_set, so writefds and exceptfds are NULL.
        // If we have some active clients to which files are being sent, we timeout after 1 ms to resume the sending work.
        // If not, we wait until one of the sockets get the data ready in the queue.
        // Returns total number of ready sockets in fds, or <0 if error.

        int activity = select(0, &readfds, NULL, NULL, (haveActiveJobs ? &tm : NULL));

        if(activity < 0){ // Error occured
            printf("Select error occured: %d\n", gsockGetLastError());
            if(selectErrCount > 10)
                break; // If more than 10 consecutive errors occured, break the loop.
            continue; // If not, try in the next loop;
        }
        else if(selectErrCount != 0)
            selectErrCount = 0; // If no error occured, clear the consecutive error counter.


        // Check if the master ListenSocket is ready to read (has a pending connection).
        if(FD_ISSET(ListenSocket, &readfds))
        {
            // Extract first request from a connection queue.
            // Blocks the thread until connection is received. However, it's not blocked here because we already know a request is pending.
            SOCKET newClient = accept(ListenSocket, (struct sockaddr*)&sin, &sinlen);
            if(newClient == INVALID_SOCKET){
                printf("accept failed with error: %d\n", gsockGetLastError());
                break;
            }
            printf("New connection: \n SOCKET fd: %d\n ip: %s\n port : %d \n\n" , newClient , inet_ntoa(sin.sin_addr) , ntohs(sin.sin_port));

            // -- Check if IP is banned and stuff.

            // Now add this client socket to the structure.
            char added = 0;
            for(int i=0; i < GSRV_MAX_CLIENTS; i++)
            {
                if(gsrvIsClientSocketEmpty(ClientSocket+i)) // Free position, can add!
                {
                    gsrvSetupNewClientSocket(ClientSocket+i, newClient);

                    added = 1;
                    break;
                }
            }
            if(added)
                connectionsAccepted++;
            else{
                printf("Client can't be added, maximum number reached.\n");
                gsockCloseSocket(newClient);
            }

            // -- Perform new connection start tasks, like application-level handshakes, data receive and stuff.

        }

        // Now check the remaining client socket descriptors if they are ready to read,
        // Or if they have other jobs unfinished, do those jobs.
        for(int i=0; i<GSRV_MAX_CLIENTS; i++)
        {
            if(FD_ISSET( ClientSocket[i].cliSock, &readfds ))
               ClientSocket[i].status |= GSRV_STATUS_RECEIVE_PENDING;

            // === Do the Test Toy Stuff === //
            if(ClientSocket[i].status)
            {
                int retStat = performToyOperation(ClientSocket+i);
                if(retStat < 0) // <0 - error happened.
                {
                    printf("Error occured while performing client operation. Closing...");
                    exitLoop = 1;
                    break;
                }
                else if(retStat == 2) // Server shutdown requested
                {
                    printf("Server shutdown requested. Closing...\n");
                    exitLoop = 1;
                    break;
                }
            }
        }
    }

    for(int i=0; i<GSRV_MAX_CLIENTS; i++){
        gsrvClearClientSocket(ClientSocket + i, 1);
    }
    gsockCloseSocket(ListenSocket);
    gsockSockCleanup();

    return 0;
}

int main(int argc, char** argv)
{
    printf("Nyaaaa >.<\n");
    return runServer( argc>1 ? (const char*)argv[1] : GSRV_FTP_DEFAULT_PORT );
}

