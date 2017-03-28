
#define WIN32_LEAN_AND_MEAN

/*#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>*/
#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>
#include <gmisc.h>
#include <gsrvsocks.h>
#include "../gftp/gftp.h"

// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
// If using GCC only needs Ws2_32
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")


#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"
/*
//
int executeCommand(SOCKET sock, const char* command, char* data, size_t datalen)
{
    struct GrylBangProtoData pd;
    //memset(&pd, 0, sizeof(GrylBangProtoData));
    datalen = (datalen > sizeof(pd.data) ? sizeof(pd.data) : datalen);

    pd.version = GBANG_VERSION;

    strncpy((pd.commandString), command, (strlen(command) > sizeof(pd.commandString) ? sizeof(pd.commandString) : strlen(command)));
    if(data)
        strncpy(pd.data, data, datalen);
    
    char onLargeDataBlock = 0, onFile = 0;
    FILE* outFile = NULL;
    if(strncmp(command, GBANG_REQUEST_FILE, strlen(GBANG_REQUEST_FILE))==0){
        onFile = 1;
        nullifyFnameEnd(data, datalen);
        outFile = fopen(basename(data), "wb");
    }

    int iResult = send( sock, (char*)&pd, GBANG_HEADER_SIZE + datalen, 0 );
    if (iResult == SOCKET_ERROR) {
        printf("send failed with error: %d\n", gsockGetLastError());
        if(outFile)
            fclose(outFile);
        return -1;
    }
    printf("Bytes Sent: %ld\n", iResult);
    int retval = 0;
    printf("\nReceiving the response...\n");
    // Receive until the peer closes the connection
    do {
        // Wait until some packet appears in the socket's receive queue on a port.
        // When it appears, extract it from queue into the buffer (recvbuf) with the specified lenght.
        // Flags are set to null, we perform a basic receive.
        // This call blocks the thread until first packet appears on a queue, unless we specify NOBLOCK on flags.
        iResult = recv(sock, (char*)&pd, sizeof(struct GrylBangProtoData), 0);
        if ( iResult > 0 ){                  // > 0 - iResult Bytes received.
            printf("Bytes received: %d.\n", iResult);
            if(strncmp(pd.commandString, GBANG_ERROR, strlen(GBANG_ERROR))==0){
                // Error occured.
                printf("Error response received: %.*s", sizeof(pd.commandString), pd.commandString);
                break;
            }
            else if(strncmp(pd.commandString, GBANG_DATA_START, strlen(GBANG_DATA_START))==0)
                onLargeDataBlock = 1;
            
            else if(strncmp(pd.commandString, GBANG_DATA_END, strlen(GBANG_DATA_END))==0){
                printf("End of data stream. \n");
                break;
            }
            else if(strncmp(pd.commandString, GBANG_DATA_SENDING, strlen(GBANG_DATA_SENDING))==0){
                if(onFile){
                    if(outFile)
                        fwrite(pd.data, 1, iResult, outFile);
                }
                else
                    printf("\n%.*s\n", iResult, pd.data);
            }

            else break;    
        }
        else if ( iResult == 0 ){            // == 0 - Connection stream mode is closing. FIN handshake has been made.
            printf("Connection closed\n");
            retval = -1;
        }
        else{                                // < 0 - error has occured while receiving.
            printf("recv failed with error: %d\n", gsockGetLastError());
            retval = -1;
        }

    } while( iResult > 0 );
    fclose(outFile);

    return retval;
}

int __cdecl main(int argc, char **argv) 
{
    SOCKET ConnectSocket = INVALID_SOCKET; // Socket for connection to da server.
    struct addrinfo *result = NULL,
                    hints;
    char recvbuf[DEFAULT_BUFLEN];
    int iResult, 
        oneCommand = 0;
    size_t recvbuflen = sizeof(recvbuf);
    
    FILE* outputFile = NULL;
    FILE* g_inputFd = stdin;
    FILE* g_outputFd = stdout;
    // Validate the parameters
    if ((argc==2 ? strcmp(argv[1], "--help")==0 : 0) || argc<3) {
        printf("usage: %s server-name server-port [remote-command] [local-filename]\n \
        \nIf local-filename is set, command output will be redirected to the filename", basename(argv[0]));
        return 1;
    }

    if(argc>3) // Command name specified.
        oneCommand = 1;

    if(argc==5){ // Local file path has been passed.
        if(!(outputFile = fopen(argv[4], "wb")));
        printf("error.\n");
    }
    //--------------- Connect to a SeRVeR ----------------//    

    printf("Will connect to a server \"%s\" on port %s\n\nInit WinSock...", argv[1], argv[2]);

    // Initialize Winsock
    iResult = gsockInitSocks();
    if (iResult != 0) {
        printf("gsockInitSocks() failed with error: %d\n", iResult);
        return 1;
    }
    
    printf("Done.\nSet AddrInfo hints: ai_family=AF_UNSPEC, ai_socktype=SOCK_STREAM, ai_protocol=IPPROTO_TCP\n");
    
    memset( &hints, 0, sizeof(hints) );
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
        return gsockErrorCleanup(0, NULL, "getaddrinfo failed with error: ", 1, 1);
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
            return gsockErrorCleanup(0, NULL, "socket failed with error: ", 1, 1);
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
            continue; // Resume to the next entity.
        }
        // We found a connectable address entity, so we can quit now.
        break;
    }
    // Free the result ADDRINFO* structure, we no longer need it.
    freeaddrinfo(result);
    
    // The server might refuse a connection, so we must check if ConnectSocket is INVALID.
    if (ConnectSocket == INVALID_SOCKET) {
        return gsockErrorCleanup(0, NULL, "Unable to connect to server! ", 1, 1);
    }
    // Now we can start the session.

    if(oneCommand)
    {
        // Parse command.
        char* command = strtok(argv[3], " ");
        char* data = strtok(NULL, " ");
        
        iResult = executeCommand(ConnectSocket, command, data, (data ? strlen(data) : 0));
    }
    else // run loop with user inputing data.
    {
        printf("Starting loop...\n");
        char canRun = 1;
        while(canRun)
        {
            printf("\n>");
            getLine(NULL, recvbuf, recvbuflen);
            
            char* command = strtok(recvbuf, " \n");
            char* data = strtok(NULL, " \n");
			printf("\nCommand: |%s|\nData: |%s|\n\n", command, data);

            if(executeCommand(ConnectSocket, command, data, (data ? strlen(data) : 0)) < 0) // Error or need to close sock.
                break;
        }
    }

    printf("Connection closed. Exitting...\n");

    // cleanup. close the socket, and terminate the Winsock.dll instance bound to our app.
    closesocket(ConnectSocket);
    gsockSockCleanup();

    return 0;
}

*/

int main()
{
    return 0;
}
