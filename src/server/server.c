
//#undef UNICODE
//#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

#include "gsrv.h"
#include "service.h"
#include "helperz.h"

// Need to link with Ws2_32.lib
//#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

int sendFile(SOCKET sock, const char* fname){
    // Try to open file.
    printf("Trying to open file: %s|\n", fname);

    FILE* inputFile = fopen(fname, "rb");
    char buffer[GSRV_DEFAULT_BUFLEN];
    size_t bufferLen = GSRV_DEFAULT_BUFLEN;
                
    if(!inputFile){
        printf("File requested can't be opened. Terminating.\n");
        strcpy(buffer, "File doesn't exist on this machine!");    
        
        int iSendResult = send( sock, buffer, strlen(buffer), 0 );
        if (iSendResult == SOCKET_ERROR) {
            printf("send failed with error: %d\n", WSAGetLastError());
            closesocket(sock);
            WSACleanup();
            return 1;
        }
        printf("Bytes sent: %d\n", iSendResult);
    }
    else{ // File exists.
        size_t bytesRead;
        do{
            size_t bytesRead = fread(buffer, 1, bufferLen, inputFile);

            if(bytesRead > 0){
                int iSendResult = send( sock, buffer, bytesRead, 0 );
                if (iSendResult == SOCKET_ERROR) {
                    printf("send failed with error: %d\n", WSAGetLastError());
                    closesocket(sock);
                    WSACleanup();
                    return 1;
                }
                printf("Bytes sent: %d\n", iSendResult);
            }
        } while(!feof(inputFile) && !ferror(inputFile));
    }

    fclose(inputFile);

    return 0;
}

int initSocks(){
    // Initialize Winsock
    WSAData wsaData;
    iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }
    return 0;
}

int performSockCleanup(SOCKET sock, struct addrinfo* addrin, const char* msg, int retval){
    printf("%s : %d\n", msg, WSAGetLastError());
    if(addrin)
        freeaddrinfo(addrin);
    if(sock != INVALID_SOCKET)
        closesocket(sock);
    WSACleanup();
    return retval;
}

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
    char recvbuf[GSRV_DEFAULT_BUFLEN];
    size_t recvbuflen = GSRV_DEFAULT_BUFLEN;
    
    // Init WinSocks.
    printf("Done.\nInit WinSock... ");
    if(initSocks() != 0)
        return 1;
    
    printf("Done.\nInit addrinfo's and SockBuffs ...");
    
    // Set the variables
    memset(ClientSocket, 0, sizeof(ClientSocket)); // Nullify da buffer.
    memset(&hints, 0, sizeof(hints));
    
    // Set the hints for the preferred sockaddr properties.
    hints.ai_family = AF_INET;       // Use IPv4 socket mode.
    hints.ai_socktype = SOCK_STREAM; // Stream mode. (For UDP, we use Datagram)
    hints.ai_protocol = IPPROTO_TCP; // Use TCP for communicating.
    hints.ai_flags = AI_PASSIVE;     // Use it for listening.

    // Resolve the server address and port with the specified hints.
    printf("Done.\nCalling getAddrInfo, with specified port number as a service... ");
    iResult = getaddrinfo(NULL, port, &hints, &result);

    if ( iResult != 0 ) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    // Create a listening SOCKET for connecting to a client.
    printf("Done.\nCreate a ListenSocket (socket())...");
    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    // Setup the TCP listening socket, bind it to a local server address.
    printf("Done.\nBinding ListenSocket... ");
    iResult = bind( ListenSocket, result->ai_addr, (int)(result->ai_addrlen));
    if (iResult == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }
    
    printf("Done.\nFreeAddrInfo()... ");
    freeaddrinfo(result);

    // Mark the ListenSocket as the socket willing to accept incoming connections.
    // Max pending connections: SOMAXCONN. This is defined by system.
    printf("Done.\nlisten(ListenSocket)... ");
    iResult = listen(ListenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        printf("listen failed with error: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

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

    printf("Done.\n\nStarting Loop... \n");
    while(connectionsAccepted < GSRV_CONNECTIONS_TO_ACCEPT) // Run a server loop.
    {
        // We will use the SELECT function to async'ly check which socks have pending conn's, and perform stuff if they don't.
        //clear the socket set 
        FD_ZERO(&readfds);  
    
        // Add ListenSocket to set 
        FD_SET(ListenSocket, &readfds);  
        max_fds = ListenSocket;
        
        char haveFileSendingJobs = 0;

        // Now add every of the ClientSockets to the fd_set
        for(int i=0; i<GSRV_MAX_CLIENTS; i++)
        {
            int clsd = ClientSocket[i].cli_sock; 

            if(clsd > 0){ // Valid socket
                FD_SET(clsd, &readfds); 
                
                if(clsd > max_fds) // This desc number is highest. 
                    max_fds = clsd;

                if(ClientSocket[i].currentFile != NULL)
                    haveFileSendingJobs = 1;
            }
        }
       
        // If we have jobs, SELECT timeouts after 1 ms. 
        tm.tv_sec = 0;
        tm.tv.usec = 1;

        // Perform SELECT to check the socket state.
        // We check only the read buffer fd_set, so writefds and exceptfds are NULL.
        // If we have some active clients to which files are being sent, we timeout after 1 ms to resume the sending work.
        // If not, we wait until one of the sockets get the data ready in the queue.
        // Returns total number of ready sockets in fds, or <0 if error.

        int activity = select(0, &readfds, NULL, NULL, (haveFileSendingJobs ? &tm : NULL));

        if(activity < 0){ // Error occured
            printf("Select error occured: %d\n", WSAGetLastError());
            if(selectErrCount > 10)
                break; // If more than 10 consecutive errors occured, break the loop.
            continue; // If not, try in the next loop;
        }
        else if(selectErrCount != 0)
            selectErrCount = 0; // If no error occured, clear the consecutive error counter.

        
        // Check if the master ListenSocket is ready to read (has a pending connection).
        if(FD_ISSET(ListenSocket, &readfds)){

            // Extract first request from a connection queue.
            // Blocks the thread until connection is received. However, it's not blocked here because we already know a request is pending.
            int newClient = accept(ListenSocket, (struct sockaddr*)&sin, &sinlen);
            if(newClient == INVALID_SOCKET){ 
                printf("accept failed with error: %d\n", WSAGetLastError());
                break;
            }

            // Perform new connection start tasks, like application-level handshakes, data receive and stuff.

            // Now add this client socket to the structure.
            for(int i=0; i < GSRV_MAX_CLIENTS; i++)
            {
                if(ClientSocket[i].cli_sock == 0 && ClientSocket[i].status == GSRV_STATUS_INACTIVE){ // Free position, can add!
                {
                    ClientSocket[i].cli_sock = newClient;
                    ClientSocket[i].status = GSRV_STATUS_COMMAND_WAITING;
                    ClientSocket[i].currentFile = NULL;
                }
            }
            
        }

        // Now check the remaining client socket descriptors if they are ready to read,
        // Or if they have other jobs unfinished, do those jobs.
        for(int i=0; i<GSRV_MAX_CLIENTS; i++)
        {
            int clsd = ClientSocket[i].cli_sock;
            if(FD_ISSET(clsd, &readfds){
                // Do stuff.
            }
        }

        /*
        printf("Connection Received!\nGetting the data...\n");
        // Receive until the peer shuts down the connection
        do {
            iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
            
            if (iResult > 0) { // Got bytes. iResult: how many bytes got.
                recvbuf[(recvbuf[iResult-1]=='\n' ? iResult-1 : iResult)] = 0; // Null-Terminated string.
                
                printf("Bytes received: %d\nPacket data:\n%s\n", iResult, recvbuf);
                
                sendFile(ClientSocket, (const char*)recvbuf);
            }
            else if (iResult == 0) // Client socket shut down'd properly.
                printf("Connection closing...\n");
            else  {  // Error occured.
                printf("recv failed with error: %d\n", WSAGetLastError());
                closesocket(ClientSocket);
                WSACleanup();
                return 1;
            }

        } while (iResult > 0);
        
        printf("Shutting down the connection...\n");
        // shutdown the connection since we're done
        iResult = shutdown(ClientSocket, SD_SEND);
        if (iResult == SOCKET_ERROR) {
            printf("shutdown failed with error: %d\n", WSAGetLastError());
            closesocket(ClientSocket);
            WSACleanup();
            return 1;
        }

        connectionsAccepted++;*/
    }

    // cleanup
    closesocket(ListenSocket);
    closesocket(ClientSocket);
    WSACleanup();

    return 0;
}

int main(int argc, char** argv)
{
    printf("Nyaaaa >.<\n");
    return runServer( argc>1 ? (const char*)argv[1] : GSRV_DEFAULT_PORT );
}

