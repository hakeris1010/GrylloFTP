
//#undef UNICODE
//#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

// Need to link with Ws2_32.lib
//#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

#define GSRV_DEFAULT_BUFLEN 512
#define GSRV_DEFAULT_PORT "27015"

// Accept this much connections
#define GSRV_CONNECTIONS_TO_ACCEPT 4


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

// Arg: Port number on which we'll listen.
int runServer(const char* port)
{
    printf("Init start vars... ");

    WSADATA wsaData;
    int iResult;

    SOCKET ListenSocket = INVALID_SOCKET;
    SOCKET ClientSocket = INVALID_SOCKET;

    struct addrinfo *result = NULL;
    struct addrinfo hints;

    int iSendResult;
    char recvbuf[GSRV_DEFAULT_BUFLEN];
    int recvbuflen = GSRV_DEFAULT_BUFLEN;

    // Initialize Winsock
    printf("Done.\nInit WinSock... ");

    iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    printf("Done.\nInit addrinfo hints...");
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;       // Use IPv4 socket mode.
    hints.ai_socktype = SOCK_STREAM; // Stream mode. (For UDP, we use Datagram)
    hints.ai_protocol = IPPROTO_TCP; // Use TCP for communicating.
    hints.ai_flags = AI_PASSIVE;     // Use it for listening.

    // Resolve the server address and port
    printf("Done.\nCalling getAddrInfo, with specified port number... ");
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
    struct sockaddr_in sin;
    socklen_t len = sizeof(sin);
    if (getsockname(ListenSocket, (struct sockaddr *)&sin, &len) == -1)
        printf("getsockname err. can't get port.\n");
    else
        printf("\nThe server is listening on port: %d\n", ntohs(sin.sin_port));

    // Initialize and start accept/receive loop.
    int connectionsAccepted = 0;
    
    printf("Done.\n\nStarting Loop... \n");
    while(connectionsAccepted < GSRV_CONNECTIONS_TO_ACCEPT) // Run a server loop.
    {
        // Extract first request from a connection queue.
        // Blocks the thread until connection is received.
        printf("------------\n\nWaiting for the connection.....\n");
        ClientSocket = accept(ListenSocket, NULL, NULL);
        if (ClientSocket == INVALID_SOCKET) {
            printf("accept failed with error: %d\n", WSAGetLastError());
            closesocket(ListenSocket);
            WSACleanup();
            return 1;
        }
        
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

        connectionsAccepted++;
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

