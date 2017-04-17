#include "grylsocks.h"
#include "hlog.h"
#include <stdio.h>
#include <stdlib.h>

int gsockGetLastError()
{
    #if defined _GRYLTOOL_WIN32
        return WSAGetLastError();
    #elif defined _GRYLTOOL_POSIX
        return errno;
    #endif // defined
    return 0;
}

int gsockInitSocks()
{
    #ifdef _GRYLTOOL_WIN32
        // Initialize Winsock
        struct WSAData wsaData;
        int iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
        if (iResult != 0) {
            hlogf("WSAStartup failed with error: %d\n", iResult);
            return 1;
        }
    #endif // _GRYLTOOL_WIN32
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
    #if defined _GRYLTOOL_WIN32
        WSACleanup();
    #endif // _GRYLTOOL_WIN32
}

int gsockCloseSocket(SOCKET sock)
{
    if(sock != INVALID_SOCKET)
    {
        #if defined _GRYLTOOL_WIN32
            return closesocket(sock);
        #elif defined _GRYLTOOL_POSIX
            return close(sock);
        #endif // _GRYLTOOL_POSIX || _GRYLTOOL_WIN32
    }
}

// Easy connect

SOCKET gsockConnectSocket(const char* address, const char* port, int family, int socktype, int protocol, int flags)
{
    // Set the DEFAULT values if ZERO.
    if(!family)
        family = AF_UNSPEC; // Whatever family the GetAddrInfo() gives us.
    if(!socktype)
        socktype = SOCK_STREAM; // TCP Stream mode
    // Protocol is 0 anyways (unless explicitly specified by user).

    hlogf("\ngsockConnectSocket(): Trying to connect to: %s, on port: %s.\n", address, port);
    hlogf("Set AddrInfo hints: ai_family=AF_UNSPEC, ai_socktype=%d, ai_protocol=%d\n", socktype, protocol);
    
    SOCKET ConnectSocket = INVALID_SOCKET;
    struct addrinfo hints = { 0 },
                    *result;

    // Set the Socket Option hints.  // For Defaults:
    hints.ai_family   = family;      // AF_UNSPEC - Use IPv4 or IPv6, respectively.
    hints.ai_socktype = socktype;    // SOCK_STREAM - TCP Stream mode (Connection-oriented)
    hints.ai_protocol = protocol;    // 0 - Use default protocol for the given SockType.
    
    hlogf("Resolve server address & port (GetAddrInfo)... ");

    // Resolve the server address, port, and Socket Options (Preferred in Hints)
    // 1: Server address (ipv4, ipv6, or DNS)
    // 2: Service ID, or Port Number of the server.
    // 3: hints (above)
    // 4: Result addrinfo struct. The server address might return more than one
    //    connectable entities, so ADDRINFO uses a linked list.
    int iResult = getaddrinfo(address, port, &hints, &result);
    if ( iResult != 0 ) {
        hlogf("ERROR on getaddrinfo() : %d\n", gsockGetLastError() );
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
            hlogf("ERROR on socket() : %d\n", gsockGetLastError() );
            return ConnectSocket;
        }

        // Connect to server.
        // 1: the socket handle for maintaining a connection
        // 2: server address (IPv4, IPv6, or other (if using UNIX sockets))
        // 3: address lenght in bytes.
        // This makes the TCP/UDP SYN handshake.
        iResult = connect( ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            gsockCloseSocket(ConnectSocket);
            ConnectSocket = INVALID_SOCKET;
            continue; // If can't connect, close sock, and Resume to the next entity.
        }
        // We found a connectable address entity, so we can quit now.
        break;
    }
    // Free the result ADDRINFO* structure, we no longer need it.
    freeaddrinfo(result);
    
    // The server might refuse a connection, so we must check if ConnectSocket is INVALID.
    if (ConnectSocket == INVALID_SOCKET) {
        hlogf("ERROR: Can't connect to this server!\n");
    }
    hlogf("Success!\n");

    // Set the Socket IOCTL or SockOpt flags. - Currently None Supported.
    
    return ConnectSocket;
}

SOCKET gsockListenSocket(int port, const char* localBindAddr, int family, int socktype, int protocol, int flags)
{
    // Set the DEFAULT values if ZERO.
    if(!family)
        family = AF_INET; // IPv4
    if(!socktype)
        socktype = SOCK_STREAM; // TCP Stream mode
    // Protocol is 0 anyways (unless explicitly specified by user).

    SOCKET sockFd;
    struct sockaddr_in localAddr = { 0 }; // Local Address in Connection Tuple.
    int iRes = 0;

    hlogf("\ngsockListenSocket(): Port: %d, family: %d\nCreating Socket:");

    sockFd = socket(family, socktype, protocol);
    if(sockFd == INVALID_SOCKET){
        hlogf("ERROR on socket(): %d\n", gsockGetLastError());
        return INVALID_SOCKET;
    }
    
    // Setup the Bind Structure
    localAddr.sin_family = family;
    localAddr.sin_addr.s_addr = (localBindAddr ? inet_addr(localBindAddr) : INADDR_ANY);
    localAddr.sin_port = htons(port);
    
    // Bind to socket to Local Address (or interface).
    // (Connection Tuple: LocalAddr-LocalPort-RemoteAddr-RemotePort)
    iRes = bind(sockFd, (struct sockaddr*)&localAddr, sizeof(localAddr));
    if(iRes == SOCKET_ERROR){
        hlogf("ERROR on Bind() : %d\n", gsockGetLastError());
        gsockCloseSocket(sockFd);
        return INVALID_SOCKET;
    }

    // Mark for Listening if Connection-Stream oriented socket.
    if(socktype == SOCK_STREAM || socktype == SOCK_SEQPACKET)
    { 
        iRes = listen(sockFd, SOMAXCONN);
        if(iRes == SOCKET_ERROR){
            hlogf("ERROR on listen() : %d\n", gsockGetLastError());
            gsockCloseSocket(sockFd);
            return INVALID_SOCKET;
        }
    }

    // Setup the needed flags (NonBlock, etc.) - Currently not supported.

    return sockFd;
}

// Functions for sending and receiving multipacket buffers.
int gsockReceive(SOCKET sock, char* buff, size_t bufsize, int flags)
{
    return recv(sock, buff, bufsize, flags);
}

int gsockSend(SOCKET sock, const char* buff, size_t bufsize, int flags)
{
    return send(sock, buff, bufsize, flags);
}


