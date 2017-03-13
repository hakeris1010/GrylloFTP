#ifndef SERVICE_H_INCLUDED
#define SERVICE_H_INCLUDED

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>

#include "gsrv.h"

// FTP Service Structures & Function.

// FTP Packet additional data.
struct GsrvAdditionalData
{
    FILE* currentFile;
    int command;
    char* dataString;
}

// The socket structure.
struct GsrvClientSocket
{
    volatile SOCKET cli_sock;
    volatile char status;
    volatile char sockDataBuffer[GSRV_DEFAULT_BUFLEN];
    volatile GrsvAdditionalData* otherData;
};

// Client Socket Data management funcs
void initGsrvClientSocket(GsrvClientSocket* sd, SOCKET clSock, char createAdditionalData);
void clearGsrvClientSocket(GsrvClientSocket* sd);

// Service funcs

/*  Parses the sockDataBuffer to an FTP header and data, and updates specific flags.
    Call when received data to a buffer. */
int parseFTPData(GsrvClientSocket* sd);

/*  Performs specific FTP operation from data present in GsrvClientSocket.
    - The function does not loop, returns after sending 1 packet if needs to send something.
    - Also performs any necessary file openings/readings/closings when needed.
    - If file data transmit has been started, status flags will be set according to the state.
    - When called again, will resume the operation.
    TODO: Return value might be a structure or a flag int.*/
int performFTPOperation(GsrvClientSocket* sd);



#endif
