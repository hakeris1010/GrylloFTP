#ifndef SERVICE_H_INCLUDED
#define SERVICE_H_INCLUDED

#include "gsrvsocks.h"

#include <stdio.h>
#include <stdlib.h>

//=========== Defines for the GSRV server ===========//

// FTP BufLen & Default Ports
#define GSRV_FTP_DEFAULT_BUFLEN 1500
#define GSRV_DEFAULT_BUFLEN     1500

#define GSRV_FTP_DEFAULT_PORT "21"
#define GSRV_FTP_DATA_DEFAULT_PORT "20"

// Accept this much connections
#define GSRV_CONNECTIONS_TO_ACCEPT 128
#define GSRV_MAX_CLIENTS 32

// Client Status constnts
#define GSRV_STATUS_INACTIVE            0
#define GSRV_STATUS_SENDING_FILE        1
#define GSRV_STATUS_IDLE                2
#define GSRV_STATUS_RECEIVE_PENDING     16

// =========== Structures =========== //

// FTP Packet additional data.
typedef struct
{
    FILE* currentFile;
    int command;
    int responseCode;
    char* dataString;
} GsrvAdditionalData;

// The socket structure.
typedef struct
{
    volatile SOCKET cliSock;
    volatile SOCKET dataSendSock;
    volatile char status;
    volatile char sockDataBuffer[GSRV_FTP_DEFAULT_BUFLEN];
    volatile size_t sockDataBuffLen;
    volatile GsrvAdditionalData* otherData;
} GsrvClientSocket;

// =========== FTP Service functions =========== //

/*  Client Socket Data management funcs
    - Populate the structure GsrvClientSocket with valid values after creation
    - Clear when ending work. */
void gsrvInitClientSocket(GsrvClientSocket* sd, SOCKET clSock, char createAdditionalData);
void gsrvClearClientSocket(GsrvClientSocket* sd, char closeSockets);

/* Client Socket structure setups and checks. */
char gsrvIsClientSocketEmpty(GsrvClientSocket* sd);
void gsrvSetupNewClientSocket(GsrvClientSocket* sd, SOCKET clSock);
char gsrvHaveActiveJobs(GsrvClientSocket* sd);

// =========== Service funcs =============//

/*  Parses the sockDataBuffer to an FTP header and data, and updates specific flags.
    Call when received data to a buffer. */
int gsrvFTP_ParseData(GsrvClientSocket* sd);

/*  Performs specific FTP operation from data present in GsrvClientSocket.
    - Already initialized GsrvClientSocket must be passed, with the valid, and connected socket.
    - The function does not loop, returns after sending 1 packet if needs to send something.
    - Also performs any necessary file openings/readings/closings when needed.
    - If file data transmit has been started, status flags will be set according to the state.
    - When called again, will resume the operation.
    TODO: Return value might be a structure or a flag int. */
int gsrvFTP_PerformSingleOperation(GsrvClientSocket* sd);

/* Loop the client connection. Use when multithreading.
    - Already initialized GsrvClientSocket must be passed, with the valid, and connected socket.
    - The same as performFTPOperation, but loops until the connection is closed. */
int gsrvFTP_RunClientService(GsrvClientSocket* sd);

//************ Toys. For testing purposes. ************//
//----????
int gsrvPerformToyOperation(GsrvClientSocket* sd);
int gsrvRunToyClientService(GsrvClientSocket* sd);
//----????
//************             -*-             ************//

// =========== Another Trivial Socket Service Functions =========== //

/* Send file over the TCP socket.
    - SOCKET must be valid and connected to the client. */
int gsrvSendFile(SOCKET sock, const char* fname);

#endif
