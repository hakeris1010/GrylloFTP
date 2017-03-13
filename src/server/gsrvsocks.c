#include "gsrvsocks.h"

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
            printf("WSAStartup failed with error: %d\n", iResult);
            return 1;
        }
    #endif // __WIN32
    return 0;
}

int gsockErrorCleanup(SOCKET sock, struct addrinfo* addrin, const char* msg, char cleanupEverything, int retval)
{
    if(msg)
        printf("%s : %d\n", msg, gsockGetLastError());

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
