
//#define WIN32_LEAN_AND_MEAN

#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>
#include <gmisc.h>
#include <gsrvsocks.h>
#include <grylthread.h>
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

int executeCommand(SOCKET sock, const char* command, struct FTPClientState* state)
{
    //a
}

int __cdecl main(int argc, char **argv) 
{
    hlogSetActive(1);

    SOCKET ControlSocket = INVALID_SOCKET,
    struct addrinfo *result = NULL,
                    hints;
    char recvbuf[FTP_DEFAULT_BUFLEN];
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
    
    printf("Done.\nNow trying to connect.\n");
    
    ControlSocket = gsockConnectSocket(argv[1], argv[2], SOCK_STREAM, IPPROTO_TCP);
    if(ControlSocket == INVALID_SOCKET)
        return gsockErrorCleanup(0, NULL, "Can't connect to a madafakkin' server....", 1, 1); 
     
    // Now we can start the session.
    // Get the response from s3rver. First reply - The welcome message
    
    // Now print when receiving for each message received.
    iResult = receiveAndPerformForEachPacket(ControlSocket, recvbuf, recvbuflen, 0, printBuffer, NULL);
    if(iResult != 0)
        printf("Err occurr'd.\n");

    printf("Starting loop...\n");

    char canRun = 1;
    while(canRun)
    {
        gmisc_GetLine("\nftp> ", recvbuf, recvbuflen, stdin);
        
        char* command = strtok(recvbuf, " \n");
        char* data = strtok(NULL, " \n");
        printf("\nCommand: |%s|\nData: |%s|\n\n", command, data);

        if(executeCommand(ConnectSocket, command, data, (data ? strlen(data) : 0)) < 0) // Error or need to close sock.
            break;
    }

    printf("Connection closed. Exitting...\n");

    // cleanup. close the socket, and terminate the Winsock.dll instance bound to our app.
    closesocket(ConnectSocket);
    gsockSockCleanup();

    return 0;
}

