
//#define WIN32_LEAN_AND_MEAN

#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>
#include <gmisc.h>
#include <gsrvsocks.h>
#include <grylthread.h>
#include <hlog.h>
#include "../gftp/gftp.h"

#define MAX_DATA_THREADS 8

// FTP Structs

typedef struct 
{
    char onDataChannelSetup;
    char waitingForCommand;
    int numberOfDataThreads;
} FTPClientState;

typedef struct
{
    char dataType;      // Ascii, Image, Local, EbcDic
    char dataFormat;    // For Ascii - NonPrint, Telnet_FormatContol, CarriageControl
    char structure;     // File, record, page
    char transMode;     // Stream, block, compressed.
} FTPDataFormatInfo;

static GrThreadHandle DataThreadPool[MAX_DATA_THREADS];

// Helper funcs

int receiveAndPerformForEachPacket( SOCKET sock, char* buff, size_t bufsize, int flags, \
                                    void (*callbk)(char*, size_t, void*), void* callbkParam )
{
    int iRes, retval = 0;
    do {
        iRes = recv(sock, buff, bufsize, flags);

        if( iRes > 0){ // Bytes were on queue, read successfully.
            // Send this buffer for operation to callback.
            callbk(buff, iRes, callbkParam);
        }
        else if ( iRes == 0 ){ //  0 -> Connection stream mode is closing.
            hlogf("Recv loop: Connection closed\n");
            retval = 0;
        }
        else{ // < 0 - error has occured while receiving.
            hlogf("Recv loop: recv failed with error: %d\n", gsockGetLastError());
            retval = -1;
        }
    } while( iRes > 0);

    return retval;
}

void printBuffer(char* buff, size_t sz, void* param)
{
    //buff[sz] = 0; // Null-terminate the bufer for printing.

    fprintf((param ? (FILE*)param : stdout), "%.*s", sz, buff);
}

/*! The DataThread runner.
 *  Param - the structure holding all data needed for file data transfer (name, type, etc)
 */
void dataThreadRunner(void* param)
{
    if(!param) return;
        
    FTPDataFormatInfo* formInfo = (FTPDataFormatInfo*)param;
}

// The command interpreter

int executeCommand(SOCKET sock, const char* command, FTPClientState* state)
{
    //a
    return -1;
}

/*! Authorization of the connection.
 *  - Called just after initializing a connection.
 *  - Authorizes the user and starts a valid FTP session.
 *  - On error return < 0.
 */
int authorizeConnection(SOCKET sock)
{
    char recvbuf[FTP_DEFAULT_BUFLEN];
    // Get the response from s3rver. First reply - The welcome message
    iResult = recv(ControlSocket, recvbuf, sizeof(recvbuf), 0);
    if(iResult < 0){
        hlogf("Error receiving welcome message.\n");
        return -2;
    }
    // Print the message
    recvbuf[iResult] = 0;
    printf("\n%s\n", recvbuf);
    
    // Start user authorization. Straightforward, no encryption, just like the good ol' days :)
    strcpy(recvbuf, "USER ");
    gmisc_GetLine("Name: ", recvbuf+5, sizeof(recvbuf), stdin);
}

int __cdecl main(int argc, char **argv) 
{
    SOCKET ControlSocket = INVALID_SOCKET;

    struct addrinfo *result = NULL,
                    hints;

    char recvbuf[FTP_DEFAULT_BUFLEN];
    size_t recvbuflen = sizeof(recvbuf);
    int iResult, 
        oneCommand = 0;
    
    // Activate the logger
    hlogSetActive(1);

    // Validate the parameters
    if ((argc==2 ? strcmp(argv[1], "--help")==0 : 0) || argc<3) {
        printf("usage: %s server-name server-port [remote-command] [local-filename]\n \
        \nIf local-filename is set, command output will be redirected to the filename", basename(argv[0]));
        return 1;
    }

    //--------------- Connect to a SeRVeR ----------------//    

    printf("Will connect to a server \"%s\" on port %s\n\nInit WinSock...", argv[1], argv[2]);

    // Initialize Winsock
    iResult = gsockInitSocks();
    if (iResult != 0) {
        printf("gsockInitSocks() failed with error: %d\n", iResult);
        return 1;
    }
    
    printf("Done.\nNow trying to connect.\n");
    
    ControlSocket = gsockConnectSocket(argv[1], argv[2], SOCK_STREAM, IPPROTO_TCP);
    if(ControlSocket == INVALID_SOCKET)
        return gsockErrorCleanup(0, NULL, "Can't connect to a madafakkin' server....", 1, 1); 
    
    //---------------   Start a SessioN  ----------------// 

    // The state structure    
    FTPClientState ftpCliState = {0};    

    if( authorizeConnection(ControlSocket) < 0 )
        return gsockErrorCleanup(ControlSocket, NULL, "Error authorizing a connection!", 1, 1); 

    
    printf("Starting loop...\n");
    char canRun = 1;
    while(canRun)
    {
        gmisc_GetLine("\nftp> ", recvbuf, recvbuflen, stdin);

        if( executeCommand(ControlSocket, recvbuf, &ftpCliState) < 0 ) // Error or need to close sock.
            break;
    }

    printf("Connection closed. Exitting...\n");

    // cleanup. close the socket, and terminate the Winsock.dll instance bound to our app.
    gsockCloseSocket(ControlSocket);
    gsockSockCleanup();

    return 0;
}

