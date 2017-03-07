
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>

// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
// If using GCC only needs Ws2_32
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")


#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"

int __cdecl main(int argc, char **argv) 
{
    WSADATA wsaData; // Winsock instance data.
    SOCKET ConnectSocket = INVALID_SOCKET; // Socket for connection to da server.
    struct addrinfo *result = NULL,
                    hints;
    char recvbuf[DEFAULT_BUFLEN];
    int iResult;
    size_t recvbuflen = sizeof(recvbuf);
    
    char* localFname;

    // Validate the parameters
    if (argc < 4 || (argc>1 ? strcmp(argv[1], "--help")==0 : 0)) {
        printf("usage: %s server-name server-port remote-filename [local-filename]\n", basename(argv[0]));
        return 1;
    }

    if(argc==5) // Local file path has been passed.
        localFname = argv[4];
    else // hasn't been passed.
        localFname = basename(argv[3]); // we'll save it on executable's dir.

    FILE* fff = fopen(localFname, "wb");
    if(!fff){
        printf("Bad local filename given. Maybe directory doesn't exist?\nWill save on local a.dat\n");
        if(!(fff = fopen("a.dat", "wb")))
            return 1;
    }
    // At this point the output local file has been created successfully.
    
    //--------------- Connect to a SeRVeR ----------------//    

    printf("Will connect to a server \"%s\" on port %s\nFile requested: %s\n\nInit WinSock...", argv[1], argv[2], argv[3]);

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        fclose(fff);
        return 1;
    }
    
    printf("Done.\nSet AddrInfo hints: ai_family=AF_UNSPEC, ai_socktype=SOCK_STREAM, ai_protocol=IPPROTO_TCP\n");
    ZeroMemory( &hints, sizeof(hints) );
    hints.ai_family = AF_UNSPEC;        // Use IPv4 or IPv6, respectively.
    hints.ai_socktype = SOCK_STREAM;    // Stream mode (Connection-oriented)
    hints.ai_protocol = IPPROTO_TCP;    // Use TCP (Guaranteed delivery n stuff).
    
    printf("Resolve server address & port (GetAddrInfo)... ");

    // Resolve the server address and port
    // 1: Server address (ipv4, ipv6, or DNS)
    // 2: Port of the server.
    // 3: hints (above)
    // 4: Result addrinfo struct. The server address might return more than one connectable entities, so ADDRINFO uses a linked list.
    iResult = getaddrinfo(argv[1], argv[2], &hints, &result);
    if ( iResult != 0 ) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        fclose(fff);
        return 1;
    }

    printf("Done.\nTry to connect to the first connectable entity of the server...\n");
    // Attempt to connect to an address until one succeeds
    int cnt=0;
    for(struct addrinfo* ptr=result; ptr != NULL; ptr=ptr->ai_next) {
        printf(" Trying to connect to entity #%d\n", cnt);
        cnt++;

        // Create a SOCKET for connecting to server
        ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (ConnectSocket == INVALID_SOCKET) {
            printf("socket failed with error: %ld\n", WSAGetLastError());
            WSACleanup();
            fclose(fff);
            return 1;
        }

        // Connect to server.
        // 1: the socket handle for maintaining a connection
        // 2: server address (IPv4, IPv6, or other (if using UNIX sockets))
        // 3: address lenght in bytes.
        // This makes the TCP/UDP SYN handshake.
        iResult = connect( ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(ConnectSocket);
            ConnectSocket = INVALID_SOCKET;
            continue;
        }
        // We found a connectable address entity, so we can quit now.
        break;
    }
    // Free the result ADDRINFO* structure, we no longer need it.
    freeaddrinfo(result);
    
    // The server might refuse a connection, so we must check if ConnectSocket is INVALID.
    if (ConnectSocket == INVALID_SOCKET) {
        printf("Unable to connect to server!\n");
        WSACleanup();
        fclose(fff);
        return 1;
    }
    
    printf("\nDone.\nSending the initial buffer (File name)... ");

    // Send a buffer.
    // We send the file name (on the server), expecting the server to send the file contents back.
    // Arg4: Flags. No flags specified, perform a basic send over TCP/UDP.
    // Result: Byte count actually sent.
    iResult = send( ConnectSocket, argv[3], (int)strlen(argv[3]), 0 );
    if (iResult == SOCKET_ERROR) {
        printf("send failed with error: %d\n", WSAGetLastError());
        closesocket(ConnectSocket);
        WSACleanup();
        fclose(fff);
        return 1;
    }

    printf("Bytes Sent: %ld\n", iResult);

    // shutdown the connection since no more data will be sent
    // We pass SD_SEND, because we are a client that is sending data, and we signal that we won't send data to the server.
    // However, the socket remains active and can still receive connections on it's assigned port.
    // A FIN handshake will be performed to end the stream send connection mode.
    iResult = shutdown(ConnectSocket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        printf("shutdown failed with error: %d\n", WSAGetLastError());
        closesocket(ConnectSocket);
        WSACleanup();
        fclose(fff);
        return 1;
    }
    
    printf("\nReceiving the response...\n");

    // Receive until the peer closes the connection
    do {
        // Wait until some packet appears in the socket's receive queue on a port.
        // When it appears, extract it from queue into the buffer (recvbuf) with the specified lenght.
        // Flags are set to null, we perform a basic receive.
        // This call blocks the thread until first packet appears on a queue, unless we specify NOBLOCK on flags.
        iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
        if ( iResult > 0 ){                  // > 0 - iResult Bytes received.
            printf("Bytes received: %d. Writing data to file...\n", iResult);
            fwrite(recvbuf, 1, iResult, fff); // Write the data to our "fff" file.
        }
        else if ( iResult == 0 )            // == 0 - Connection stream mode is closing. FIN handshake has been made.
            printf("Connection closed\n");
        else                                // < 0 - error has occured while receiving.
            printf("recv failed with error: %d\n", WSAGetLastError());

    } while( iResult > 0 );
    
    printf("Exitting...\n");
    // cleanup. close the socket, and terminate the Winsock.dll instance bound to our app.
    closesocket(ConnectSocket);
    WSACleanup();
    fclose(fff);

    return 0;
}


