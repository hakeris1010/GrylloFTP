#include "gsrvsocks.h"
#include "hlog.h"
#include <stdio.h>
#include <stdlib.h>

int gsockGetLastError()
{
    #if defined __WIN32
        return WSAGetLastError();
    #elif defined __linux__
        return errno;
    #endif // defined
    return 0;
}

int gsockInitSocks()
{
    #ifdef __WIN32
        // Initialize Winsock
        struct WSAData wsaData;
        int iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
        if (iResult != 0) {
            hlogf("WSAStartup failed with error: %d\n", iResult);
            return 1;
        }
    #endif // __WIN32
    return 0;
}

int gsockErrorCleanup(SOCKET sock, struct addrinfo* addrin, const char* msg, char cleanupEverything, int retval)
{
    if(msg)
        hlogf("%s : %d\n", msg, gsockGetLastError());

    gsockCloseSocket(sock);

    if(addrin)
        freeaddrinfo(addrin);

    if(cleanupEverything)
        gsockSockCleanup();

    return retval;
}

void gsockSockCleanup()
{
    #if defined __WIN32
        WSACleanup();
    #endif // __WIN32
}

int gsockCloseSocket(SOCKET sock)
{
    if(sock != INVALID_SOCKET)
    {
        #if defined __WIN32
            return closesocket(sock);
        #elif defined __linux__
            return close(sock);
        #endif // __linux__ || __WIN32
    }
}

// Easy connect

SOCKET gsockConnectSocket(const char* address, const char* port, int socktype, int protocol)
{
    hlogf("\ngsockConnectSocket(): Trying to connect to: %s, on port: %s.\n", addr, port);
    hlogf("Set AddrInfo hints: ai_family=AF_UNSPEC, ai_socktype=%d, ai_protocol=%d\n", socktype, protocol);
    
    SOCKET ConnectSocket = INVALID_SOCKET;
    struct addrinfo hints = { 0 };
                    *result;
    //memset( &hints, 0, sizeof(hints) );
    hints.ai_family = AF_UNSPEC;        // Use IPv4 or IPv6, respectively.
    hints.ai_socktype = socktype;    // Stream mode (Connection-oriented)
    hints.ai_protocol = protocol;    // Use TCP (Guaranteed delivery n stuff).
    
    hlogf("Resolve server address & port (GetAddrInfo)... ");

    // Resolve the server address and port
    // 1: Server address (ipv4, ipv6, or DNS)
    // 2: Port of the server.
    // 3: hints (above)
    // 4: Result addrinfo struct. The server address might return more than one connectable entities, so ADDRINFO uses a linked list.
    int iResult = getaddrinfo(address, port, &hints, &result);
    if ( iResult != 0 ) {
        return INVALID_SOCKET;
    }

    hlogf("Done.\nTry to connect to the first connectable entity of the server...\n");
    // Attempt to connect to an address until one succeeds
    int cnt=0;
    for(struct addrinfo* ptr=result; ptr != NULL; ptr=ptr->ai_next) {
        hlogf(" Trying to connect to entity #%d\n", cnt);
        cnt++;

        // Create a SOCKET for connecting to server
        ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (ConnectSocket == INVALID_SOCKET) {
            return ConnectSocket;
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
            continue; // If can't connect, close sock, and Resume to the next entity.
        }
        // We found a connectable address entity, so we can quit now.
        hlogf("Success!\n");
        break;
    }
    // Free the result ADDRINFO* structure, we no longer need it.
    freeaddrinfo(result);
    
    // The server might refuse a connection, so we must check if ConnectSocket is INVALID.
    if (ConnectSocket == INVALID_SOCKET) {
        hlogf("ERROR: Can't connect to this server!\n");
    }
    
    return ConnectSocket;
}

SOCKET gsockListenSocket(SOCKET sock, const char* port, int family, int socktype, int protocol)
{
    //a
}

// Functions for sending and receiving multipacket buffers.
int gsockReceive(SOCKET sock, char* buff, size_t bufsize, int flags);

int gsockSend(SOCKET sock, const char* buff, size_t bufsize, int flags); 


