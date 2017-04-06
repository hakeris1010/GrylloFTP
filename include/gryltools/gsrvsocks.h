#ifndef GSRV_H_DEFINED
#define GSRV_H_DEFINED

// ======== Socket headers - OS dependent. ======== //
#if defined __WIN32
    #include <winsock2.h>
    #include <windows.h>
    #include <ws2tcpip.h>

#elif defined __linux__
    #include <unistd.h>
    #include <arpa/inet.h>  //close
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netdb.h>
    #include <netinet/in.h>
    #include <sys/time.h>   //FD_SET, FD_ISSET, FD_ZERO macros
    #include <errno.h>

    #define INVALID_SOCKET -1
    #define SOCKET_ERROR   -1
    
    typedef int SOCKET;

#endif // __linux__ || __WIN32

#define GSOCK_DEFAULT_BUFLEN 1500

/*! The socket data structure
 *  - Encapsulates a socket, a buffer of an initial size of GSOCK_DEFAULT_BUFLEN, and flags. 
 *  - Use for more convenience when transferring a buffer of each socket.
 */
typedef struct
{
    SOCKET sock;
    char dataBuff[GSOCK_DEFAULT_BUFLEN];
    char flags;
    short checksum;
} GSOCKSocketStruct;

int gsockGetLastError();
int gsockInitSocks();
int gsockErrorCleanup(SOCKET sock, struct addrinfo* addrin, const char* msg, char cleanupEverything, int retval);
int gsockCloseSocket(SOCKET sock);
void gsockSockCleanup();

SOCKET gsockConnectSocket(const char* addr, const char* port, int socktype, int protocol);
SOCKET gsockListenSocket(const char* port, int family, int socktype, int protocol);

int gsockReceive(SOCKET sock, char* buff, size_t bufsize, int flags);
int gsockSend(SOCKET sock, const char* buff, size_t bufsize, int flags); 

#endif
