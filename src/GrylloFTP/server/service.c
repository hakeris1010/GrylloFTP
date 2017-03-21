#include "service.h"

// Specific helper funcs. Maybe should be put into another file.

int sendFile(SOCKET sock, const char* fname){
    // Try to open file.
    printf("Trying to open file: %s|\n", fname);

    FILE* inputFile = fopen(fname, "rb");
    char buffer[GSRV_DEFAULT_BUFLEN];
    size_t bufferLen = GSRV_DEFAULT_BUFLEN;

    if(!inputFile){
        printf("File requested can't be opened. Terminating.\n");
        strcpy(buffer, "File doesn't exist on this machine!");

        int iSendResult = send( sock, buffer, strlen(buffer), 0 );
        if (iSendResult == SOCKET_ERROR) {
            gsockErrorCleanup(sock, NULL, "send failed with error", 0, 0);
            return 1;
        }
        printf("Bytes sent: %d\n", iSendResult);
    }
    else{ // File exists.
        size_t bytesRead;
        do{
            size_t bytesRead = fread(buffer, 1, bufferLen, inputFile);

            if(bytesRead > 0){
                int iSendResult = send( sock, buffer, bytesRead, 0 );
                if (iSendResult == SOCKET_ERROR) {
                    gsockErrorCleanup(sock, NULL, "send failed with error", 0, 0);
                    return 1;
                }
                printf("Bytes sent: %d\n", iSendResult);
            }
        } while(!feof(inputFile) && !ferror(inputFile));

        fclose(inputFile);
    }

    return 0;
}

// ================ Actual Funcz ================ //

void gsrvInitClientSocket(GsrvClientSocket* sd, SOCKET clSock, char createAdditionalData)
{
    if(!sd) return;
    sd->cliSock = clSock;
    sd->dataSendSock = INVALID_SOCKET;
    sd->status = GSRV_STATUS_INACTIVE;
    sd->sockDataBuffLen = 0;
    if(createAdditionalData)
    {
        sd->otherData = (GsrvAdditionalData*)malloc( sizeof(GsrvAdditionalData) ); // MALLOC sd->otherData

        sd->otherData->command = 0; // TODO: FTP Commands.
        sd->otherData->responseCode = 0;
        sd->otherData->currentFile = NULL;
        sd->otherData->dataString = NULL;
    }
    else
        sd->otherData = NULL;
}

void gsrvClearClientSocket(GsrvClientSocket* sd, char closeSockets)
{
    if(!sd) return;
    if(closeSockets){
        if(sd->cliSock != INVALID_SOCKET){
            gsockCloseSocket(sd->cliSock);
            sd->cliSock = INVALID_SOCKET;
        }
        if(sd->dataSendSock != INVALID_SOCKET){
            gsockCloseSocket(sd->dataSendSock);
            sd->dataSendSock = INVALID_SOCKET;
        }
    }
    if(sd->otherData)
    {
        if(sd->otherData->currentFile)
            fclose(sd->otherData->currentFile);

        free(sd->otherData); // FREE sd->otherData
        sd->otherData = NULL;
    }
    sd->status = GSRV_STATUS_INACTIVE;
    sd->sockDataBuffLen = 0;
}

char gsrvIsClientSocketEmpty(GsrvClientSocket* sd)
{
    if(!sd) return -1;
    return (sd->cliSock == INVALID_SOCKET && sd->status == GSRV_STATUS_INACTIVE);
}

void gsrvSetupNewClientSocket(GsrvClientSocket* sd, SOCKET clSock)
{
    if(!sd) return;
    sd->cliSock = clSock;
    sd->status = GSRV_STATUS_IDLE;
}

char gsrvHaveActiveJobs(GsrvClientSocket* sd)
{
    if(!sd) return 0;
    return (sd->status & GSRV_STATUS_SENDING_FILE);
}

//============= FTP Service funcs =============//

int parseFTPData(GsrvClientSocket* sd)
{
    if(!sd) return;
    // Todo
}

int performFTPOperation(GsrvClientSocket* sd)
{
    if(!sd || sd->cliSock == INVALID_SOCKET) return;
    // TOdo
}

int runClientService(GsrvClientSocket* sd)
{
    if(!sd || sd->cliSock == INVALID_SOCKET) return;
    // Todo
}

//========== Service (current) end. ===========//

// Toy function. For testing.

int performToyOperation(GsrvClientSocket* sd)
{
    if(!sd || sd->cliSock == INVALID_SOCKET) return 1;
    int iResult;
    char closed = 0;

    // File send operation pending
    if(sd->status & GSRV_STATUS_SENDING_FILE)
    {
        printf("\nPerformToyOperation: sending data to sock: %d... ", sd->cliSock);
        iResult = send(sd->cliSock, sd->sockDataBuffer, sd->sockDataBuffLen, 0);
        if(iResult < 0){
            gsockErrorCleanup(sd->cliSock, NULL, "send failed with error", 0, 1);
            return -1;
        }
        sd->status &= ~GSRV_STATUS_SENDING_FILE; // Reset the flag.
        printf("Done.\n");
    }

    // Receive operation pending
    else if(sd->status & GSRV_STATUS_RECEIVE_PENDING)
    {
        printf("\nPerformToyOperation: receiving data from sock: %d...\n", sd->cliSock);
        iResult = recv(sd->cliSock, sd->sockDataBuffer, GSRV_FTP_DEFAULT_BUFLEN, 0);

        if(iResult > 0){ // Got bytes. iRes: how many bytes got.
            sd->sockDataBuffer[(sd->sockDataBuffer[iResult-1]=='\n' ? iResult-1 : iResult)] = 0; // Null-Terminated string.
            sd->sockDataBuffLen = iResult;

            printf("Bytes received: %d\nPacket data:\n%s\n", iResult, sd->sockDataBuffer);

            sd->status |= GSRV_STATUS_SENDING_FILE; // Now we will echo the data back to the client.

            //Check if quit message has been posted.
            if( strncmp(sd->sockDataBuffer, "exit", 4) == 0 )
                closed = 1;  // Close this connection.
            else if( strncmp(sd->sockDataBuffer, "shutdown", 8) == 0 )
                closed = 2;  // Shut down the server
        }
        else if (iResult == 0){ // Client socket shut down'd properly.
            printf("Close message posted...\n");
            closed = 1;

            //iResult = shutdown(sd->cliSock, SD_SEND); // Possible if we want to shutdown only 1 channel.
        }
        else {  // < 0 - Error occured.
            gsockErrorCleanup(sd->cliSock, NULL, "recv failed with error", 0, 1);
            return -1;
        }
        sd->status &= ~GSRV_STATUS_RECEIVE_PENDING;
    }

    if(closed)
    {
        printf("Closing this connection... ");
        gsrvClearClientSocket(sd, 1);
        printf("Done.\n");
    }

    return (closed==2 ? 2 : 0);
}

int runToyClientService(GsrvClientSocket* sd)
{
    if(!sd || sd->cliSock == INVALID_SOCKET) return 2;
}
