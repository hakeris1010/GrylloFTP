
/************************************************************ 
 *                 FTP Client by GrylloTron                 *
 *                       Version 0.1                        *
 *            -  -  -  -  -  -  -  -  -  -  -  -            *
 *                                                          *   
 *  Features:                                               *
 *  - 1980s FTP Plain text model                            *
 *  - Working USER/PASS authentication                      *           
 *  - All the basic commands (except Upload)                *   
 *  - Using only NAT/Firewall-friendly Passive mode         *
 *  - Multithreaded download/control                        *
 *  - Efficient command-handling                            *
 *  - Easily implementable new commands                     *
 *  - Uses Cross-Platform GrylTools framework               *
 *                                                          *
 *  TODOS:                                                  *
 *  - Synchronized Console I/O using Mutex-CondVars         *
 *  - Support for Upload                                    *
 *  - Support for different Data Modes                      *
 *  - (Far future) Support for FTPS                         *
 *                                                          *   
 ***********************************************************/ 

#define FTP_CLIENT_VERSION  "v0.1"

#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>
#include <gmisc.h>
#include <gsrvsocks.h>
#include <grylthread.h>
#include <hlog.h>
#include "clientcommands.h"

// Command parser procedures

int ftpSimpleComProc(  struct FTPCallbackCommand, FTPClientState*);
int ftpParamComProc(   struct FTPCallbackCommand, FTPClientState*);
int ftpDataConComProc( struct FTPCallbackCommand, FTPClientState*);

// Available command database

const struct FTPClientUICommand ftpClientCommands[]=
{
    // Simple commands
    { 0, "system", 0x1F, ftpSimpleComProc},
    { 0, "cdup",   0x06, ftpSimpleComProc},
    { 0, "pwd",    0x1B, ftpSimpleComProc},
    { 0, "shelp",  0x21, ftpSimpleComProc},
    { 0, "abort",  0x17, ftpSimpleComProc},
    { 0, "status", 0x20, ftpSimpleComProc},
    { 0, "quit",   0x03, ftpSimpleComProc},
    { 0, "help",   0x00, ftpSimpleComProc},

    // Complex commands (more than one raw FTP command required)
    { 3, "get",    0x0E, ftpDataConComProc},
    { 3, "send",   0x0F, ftpDataConComProc},
    { 1, "dir",    0x1C, ftpDataConComProc},

    // Setting altering commands
    { 4, "passive",   0, ftpParamComProc}
};

/*! Default ClientState value setter.
 */
void FTP_setDefaultClientState(FTPClientState* state)
{
    /*if(!state) return;
    state->passiveModeOn =  1;
    state->defType =    FTP_DATATYPE_ASCII;
    state->defaultFormat =  FTP_DATAFORMAT_NONPRINT;
    state->defaultStrucure= FTP_STRUCTURE_FILE;
    state->defaultMode =    FTP_TRANSMODE_STREAM;*/
}

// Helper funcs

// Simple callback-format function, which can print a buffer to a specified file.
void printBuffer(char* buff, size_t sz, void* param)
{
    //buff[sz] = 0; // Null-terminate the bufer for printing.
    hlogf("helper_printBuffer(): printing %d bytes...\n", sz);

    fwrite( buff, 1, sz, (param ? (FILE*)param : stdout));
}


/*! FTP request-response handler.
 *  Sends a client request, and returns server response.
 *  By FTP conventions, 1 packet each.
 *  Params:
 *  - Command - fully ready C-String buffer to send (with a CRLF at the end).
 *  - Result in respondbuf
 *  Retval:
 *  - Fatal technical err: <0 (Need to exit a program)
 *  - If >0, then server response code.
 */
#define FTOOL_RECVRESP_PRINTBUFFER  1
#define FTOOL_RECVRESP_NOSEND       2
#define FTOOL_RECVRESP_NORECEIVE    4

int sendMessageGetResponse_Extended(SOCKET sock, const char* command, char* respondbuf, size_t respbufsiz, 
                        char flags, void (*responseBufferCallback)(char*, size_t, void*), void* callbackParam)
{
    fd_set readSet;
    fd_set writeSet;
    int iRes = 0;
    int lastRespCode = 0;
    int maxFds; // Linux-only argument to the "select()", the highest value of the FD's in sets + 1.
    //Set the timeout
    struct timeval tv;

    hlogf("\nsendMessageGetResponse(): start\n");

    if(! (flags & FTOOL_RECVRESP_NOSEND))
    {
        hlogf("Send flag is set. Data to send:\n%s\nChecking FD's for sending.\n", command);

        tv.tv_sec = 1; // Wait 1 second until it's possible to send
        tv.tv_usec = 0;

        FD_ZERO(&writeSet);
        FD_SET(sock, &writeSet);
        // Reserve the max-numbered SockFD for Loonix
        maxFds = (int)sock + 1;

        iRes = select(maxFds, NULL, &writeSet, NULL, &tv);
        if(iRes == SOCKET_ERROR){
            hlogf("SELECT returned error when SENDing.\n\n");
            return -1;
        }
        else if(iRes == 0){
            hlogf("Timeout: Its's unavailable to send data on this socket!\n\n");
            return 0;
        }

        if(FD_ISSET(sock, &writeSet)) // There's data for reading on this sock.
        {
            hlogf("FD's are OK for sending. Trying to send...\n");

            // Send the command
            if((iRes = send(sock, command, strlen(command), 0)) < 0){
                hlogf("Error sending a message.\n\n");
                return -2;
            }
            hlogf("Bytes sent: %d\n", iRes);
        }
    }

    if(! (flags & FTOOL_RECVRESP_NORECEIVE))
    {
        hlogf("Receive flag is set. Checking FD's for receiving.\n");

        // Now receive the Response
        while(1){
            tv.tv_sec = 1; // Wait 1 second until request arrives
            tv.tv_usec = 0;

            FD_ZERO(&readSet);
            FD_SET(sock, &readSet); // Add this socket to a set of socks checked for readability

            // Reserve the max-numbered SockFD for Loonix
            maxFds = (int)sock + 1;

            iRes = select(maxFds, &readSet, NULL, NULL, &tv); // Return - no.of socks available for reading/writing.
            if(iRes == SOCKET_ERROR){
                hlogf("SELECT returned error.\n\n");
                return -1;
            }
            else if(iRes == 0){
                hlogf("Timeout: No more readable data on sockets!\n\n");
                return 1;
            }

            if(FD_ISSET(sock, &readSet)) // There's data for reading on this sock.
            {
                hlogf("Data is available for receiving on this socket.\n");

                iRes = recv(sock, respondbuf, respbufsiz, 0);
                if( iRes < 0 ){
                    hlogf("Error receiving  a response.\n\n");
                    return -2;
                }
                if( iRes == 0 ){
                    hlogf("recv returned 0 - FIN.\n\n");
                    return -1;
                }
                // iRes > 0 = size of buffer.
                hlogf("Received bytes: %d\n", iRes);

                if((flags & FTOOL_RECVRESP_PRINTBUFFER) && !responseBufferCallback){ // No func passed, just print.
                    // Parse the response, and null-terminate it.
                    respondbuf[iRes] = 0;
                    printf("%s", respondbuf);
                }
                else if(responseBufferCallback)
                    responseBufferCallback( respondbuf, iRes, callbackParam );

                // Return the response code as int.
                lastRespCode = ( (respondbuf[0]-'0')*100 + (respondbuf[1]-'0')*10 + (respondbuf[2]-'0') );
                lastRespCode = (lastRespCode<0 ? 0 : lastRespCode);
            }
        }
    }
    hlogf("\n");
    return lastRespCode;
}

int sendMessageGetResponse(SOCKET sock, const char* command, char* respondbuf, size_t respbufsiz, char flags)
{
    if(flags & FTOOL_RECVRESP_PRINTBUFFER)
        printf("\n");

    int retval = sendMessageGetResponse_Extended(sock, command, respondbuf, respbufsiz, flags, NULL, NULL);
    
    if(flags & FTOOL_RECVRESP_PRINTBUFFER)
        printf("\n");

    return retval;
}

// Help printer
void ftpPrintHelp(FILE* outf)
{
    if(!outf) outf = stdout;
    fprintf(outf, "\nAll available UI commands:\n");
    for(const struct FTPClientUICommand* cmd = ftpClientCommands;
        cmd < ftpClientCommands + sizeof(ftpClientCommands)/sizeof(struct FTPClientUICommand);
        cmd++)
    {
        fprintf(outf, "%s ", cmd->name);
    }
    fprintf(outf, "\n");
}


/*! The Data-connection thread procedures.
 *  Thread makes a Data connection to server and executes the transfer by the
 *  options specified in the FTPDataFormatInfo* structure.
 *
 *  There are currently sending and receiving procedures defined.
 *
 *  Param:
 *  - Pointer to a FTPDataFormatInfo structure,
 *    holding all data needed for file data transfer (name, type, etc)
 */
void ftpThreadRunner_receive(void* param)
{
    if(!param) return;
    hlogf("\n* - * - * - * - * - * - *\n ftpThreadRunner_receive(): start\n");

    FTPDataFormatInfo* formInfo = (FTPDataFormatInfo*)param;
    FTP_printDataFormInfo(formInfo, hlogGetFile());
        
    if(!formInfo->passiveOn){
        hlogf("Active mode is not supported. Aborting.\n");
        FTP_freeDataFormInfo(formInfo);
        return;
    }

    char port[8];
    sprintf(port, "%d", formInfo->port);

    if(!formInfo->outFile && !formInfo->fname){
        hlogf("NO file and filename specified. Aborting data transfer.\n");
        FTP_freeDataFormInfo(formInfo);
        return;
    }
    
    //return; //DEBUG

    // Connect to the server on specified sock and port.
    SOCKET dataSocket = gsockConnectSocket(formInfo->ipAddr, port, SOCK_STREAM, IPPROTO_TCP);
    if(dataSocket == INVALID_SOCKET){
        hlogf("Can't connect to the server on Data Port! Aborting...\n");
        FTP_freeDataFormInfo(formInfo);
        return;
    }
    printf("Successfully connected. Creating a File: %s\n", formInfo->fname);
    
    if(formInfo->fname && !formInfo->outFile){
        if(! (formInfo->outFile = fopen(formInfo->fname, "wb"))){
            hlogf("Can't open file: %s\nAborting...\n", formInfo->fname);
            gsockCloseSocket(dataSocket);
            FTP_freeDataFormInfo(formInfo);
            return;
        }
    }

    // Allocate the buffer to which we'll receive
    char dataBuffer[GSOCK_DEFAULT_BUFLEN];

    //hlogf("Testing ouput file.\n");
    //fprintf(formInfo->outFile, "Testing.\n");

    // Execute the receiving and writing into buff. Specify that No sending should be done, only receiving.
    hlogf("Starting the receiving procedure.....\n");
    if( sendMessageGetResponse_Extended(dataSocket, dataBuffer, dataBuffer, sizeof(dataBuffer),
             FTOOL_RECVRESP_NOSEND, printBuffer, (void*)(formInfo->outFile) ) < 0 ){ 
         hlogf("FIN or error while sending and receiving.\n");
    }
    
    // Cleanup. Close files, sockets, and free structures.
    gsockCloseSocket(dataSocket);

    //fclose(formInfo->outFile);

    hlogf("Freeing formInfo.\n");
    FTP_freeDataFormInfo(formInfo);
    free(formInfo);

    hlogf("ftpThreadRunner_receive(): end\n* - * - * - * - * - * - *\n");
}

void ftpThreadRunner_send(void* param)
{
    if(!param) return;
    hlogf("\n* - * - * - * - * - * - *\n ftpThreadRunner_send(): start\n");

    FTPDataFormatInfo* formInfo = (FTPDataFormatInfo*)param;
    FTP_printDataFormInfo(formInfo, hlogGetFile());

    // Do work here.

    hlogf("Freeing formInfo.\n");
    //FTP_freeDataFormInfo(formInfo);
    free(formInfo);

    hlogf("ftpThreadRunner_send(): end\n* - * - * - * - * - * - *\n");
}

/*! Convert the UI main command to a raw one.
 *
 */

int ftpConvertUItoRaw(char* buff, size_t sz, struct FTPCallbackCommand command)
{
    if(!buff || sz < 4)
        return 2; //Bad

    strcpy( buff, FTP_getRawNameFromID( (command.commInfo)->rawCommandID ) );
    size_t curlen, totallen = strlen(buff) + 1;

    for(char** str=command.params; str < command.params + sizeof(command.params)/sizeof(char*); str++){
        if(!*str) break;
        curlen = strlen(*str);
        if(totallen+curlen+1 > sz-2) // Buffer is too small
            break;
        strcat( buff, " ");
        strcat( buff, *str);

        totallen += curlen+1;
    }
    // And add CRLF at the end.
    strcat( buff, "\r\n");

    return 0;
}

/*! Callback functions which execute specific type of UI command.
 *  Used in a Valid UI command database, and should be called from there.
 *  Assumes that the command passed is valid.
 *  Params:
 *  - command: a command entered by user. Uses a syntax of space-separated params.
 *  - state: a ptr to state variable, containing current ControlSocket and other data.
 *  Retval:
 *    < 0 - must terminate session
 *    = 0 - good, can continue
 *    > 0 (4, 5) - the FTP response value error number.
 *  Also modifies state buffer:
 *    - The response string of the server is stored in a statebuff.
 */

/*! Simple command processor.
 *  Executes commands which are 1-request-1-reply only, without state changes.
 *  Can be with params.
 */
int ftpSimpleComProc(struct FTPCallbackCommand command, FTPClientState* state)
{
    hlogf("ftpSimpleComProc() called. Name:%s\n", (command.commInfo)->name);
    GSOCKSocketStruct* ss = &(state->controlSocket);

    // Check for terminating commands.
    if(strcmp((command.commInfo)->name, "quit") == 0){
        hlogf("ftpSimpleComProc(): QUIT identified.\n");
        return -1;
    }
    
    // Check for help command.
    if(strcmp((command.commInfo)->name, "help") == 0){
        hlogf("ftpSimpleComProc(): HELP identified. Printing Help msg.\n");
        ftpPrintHelp(stdout);
        return 0;
    }

    // Create the command string. Append RawName and params.
    if( ftpConvertUItoRaw( (state->controlSocket).dataBuff, GSOCK_DEFAULT_BUFLEN, command ) < 0){
        hlogf("Couldn't convert UI command to Raw.\n");
        return 1;
    }

    //Execute that command.
    //Last result will be in the socket DataBuff.
    if(sendMessageGetResponse((state->controlSocket).sock, (state->controlSocket).dataBuff,
       (state->controlSocket).dataBuff, GSOCK_DEFAULT_BUFLEN, 1 ) < 0)
    {
        hlogf("Error on sendMessageGetResponse()\n");
        return -3;
    }
    return 0;
}

// Helpers for extended command processor

void FTP_freeDataFormInfo(FTPDataFormatInfo* info)
{
    if(!info) return;
    /*if(info->ipAddr)
        free(info->ipAddr);
    if(info->fname)
        free(info->fname);*/
    if(info->outFile && info->outFile!=stdout && info->outFile!=stderr && info->outFile!=stdin)
        fclose(info->outFile);
}

void FTP_printDataFormInfo(const FTPDataFormatInfo* fi, FILE* outFile)
{
    if(!outFile || !fi) return;
    fprintf(outFile, "FTPDataFormatInfo (0x%p) contents:\n", fi);
    fprintf(outFile, " dataType: %c\n dataFormat: %c\n structure: %c\n transMode: %c\n", 
                     fi->dataType, fi->dataFormat, fi->structure, fi->transMode);
    fprintf(outFile, " passiveOn: %d\n ipAddr: %s\n port: %d\n outFile: 0x%p\n fname: %s\n\n",
                     fi->passiveOn, fi->ipAddr, fi->port, fi->outFile, fi->fname);
}

int ftpDataConProc_checkError(int iResult, char* dataBuf, FTPDataFormatInfo* formInfo, const char* infoErr)
{
    // Check if technical error occured or if server returned an error code.
    if(iResult < 0 ? 1 : (dataBuf ? (dataBuf[0]=='4' || dataBuf[0]=='5') : 0) ){
        hlogf( (iResult<0 ? "Technical FATAL Error on function() : \"%s\"\n" :
                            "Error on \"%s\": Server returned error response. Aborting.\n"),
               infoErr );
        if(formInfo)
            FTP_freeDataFormInfo(formInfo);
        return (iResult<0 ? iResult : (dataBuf ? dataBuf[0]-'0' : 1)); // Return negative if fatal or server respose.
    }
    return 0;
}

int ftpExtractIpPortPasv( char** ip, short* port, const char* dataBuf, char extended )
{
    if(!ip || !port || !dataBuf)
        return -4;

    hlogf("ftpExtractIpPortPasv(): start\n");

    const char* start = NULL;
    const char* end = NULL;
    const char* ipEnd = NULL;
    int sepCount=0;
    //Find the beginning of IPv4
    for(const char* i = dataBuf; i!=0; i++){
        if(*i == '(')
            start = i+1;
        if(*i == ')'){
            end = i;
            break;
        }

        if(start && *i==','){ //Separator
            sepCount++;
            if(sepCount == 4) // First separator after IP address
                ipEnd = i;
        }
    }
    if(!start || !end || sepCount!=5 || ((int)(end-start) < 12) || ((int)(end-start) > 24)) // Invalid format.
        return -1;

    // Copy the ip in the buffer to the specified one.
    *ip = (char*) malloc((size_t)(ipEnd-start)+1);
    strncpy(*ip, start, (size_t)(ipEnd-start));
    *(*ip + (size_t)(ipEnd-start)) = 0;

    gmisc_strSubst(*ip, ",", '.'); // Change the separators into dots.

    // Now work with port.
    unsigned int n1, n2;
    // If wrong format, scanf failed to read 2 args, or read wrong values.
    if( (sscanf(ipEnd+1, "%d,%d", &n1, &n2) != 2) || (n1 > 255 || n2 > 255) ){ 
        free(*ip);
        return -2;
    }

    //*port = (short)n1 * 256 + (short)n2;
    *port = ( (((unsigned char)n1) << 8) | (unsigned char)n2 );

    hlogf("Successfully extracted IP and Port:\n IP: %s\n port: %d\n", *ip, *port);
}

/*! Processes commands which consists of multiple requests and replies,
 *  and opens/closes data connections, modifies state.
 */
int ftpDataConComProc(struct FTPCallbackCommand command, FTPClientState* state)
{
    hlogf("ftpDataConComProc() called. Name:%s\n", (command.commInfo)->name);

    //Allocate a dynanic FTPDataFormatInfo structure, because it will be passed to thread.
    FTPDataFormatInfo* formInfo = (FTPDataFormatInfo*) calloc( sizeof(FTPDataFormatInfo), 1 );
    if(!formInfo){ //Calloc failed
        hlogf("Oh no. Can't allocate dynamic memory!\n");
        return -1;
    }

    const char* cname = (command.commInfo)->name;
    char* dataBuf = (state->controlSocket).dataBuff;
    void (*threadProc)(void*); // The procedure which we'll be spawning as thread.
    int iRes, filenameParam= -1;

    hlogf("Setting individual parameters for command...\n");

    // Determine the individual params for each command.
    if(strcmp(cname, "get")==0){
        threadProc = ftpThreadRunner_receive;
        filenameParam = 0; // Indicate which command parameter contains a filename to be passed.
    }
    else if(strcmp(cname, "dir")==0){
        threadProc = ftpThreadRunner_receive;
        formInfo->outFile = stdout; // Just set the output file directly.
    }
    else if(strcmp(cname, "send")==0){
        threadProc = ftpThreadRunner_send;
        filenameParam = 0;
    }

    // If need to copy a filename, do it.
    if(filenameParam >= 0)
    {
        if(!command.params[0]){ // Must have a filename parameter.
            hlogf("get: no filename specified!\n");
            free(formInfo);
            return 1;
        }
        formInfo->fname = (char*)malloc( strlen(command.params[0]) );
        strcpy(formInfo->fname, command.params[0]);
    }

    hlogf("Starting a format negotiation with the server...\n");

    // Starting the transfer options negotiations.
    // Check for data transfer options in State. If zero, skip.

    // Data type and format (TYPE x y)
    if(state->defDataType){
        hlogf("Negotiating Data Type-format: %c %c\n", state->defDataType,
                (state->defDataFormat ? state->defDataFormat : ' ') );
        snprintf( dataBuf, GSOCK_DEFAULT_BUFLEN, "TYPE %c %c\r\n", state->defDataType,
            (state->defDataType != 'I' ? (state->defDataFormat ? state->defDataFormat : 'N') : ' ') );

        // Execute request, and check for errors, performing cleanup if needed.
        if( (iRes = ftpDataConProc_checkError(
                sendMessageGetResponse((state->controlSocket).sock, dataBuf, dataBuf, GSOCK_DEFAULT_BUFLEN, 1),
                dataBuf, formInfo, "Data type-format" )) != 0 )
            return iRes;

        formInfo->dataType = state->defDataType;
        formInfo->dataFormat = state->defDataFormat;
    }

    // Transmission mode (MODE x)
    if(state->defTransMode){
        hlogf("Negotiating TransMode: %c\n", state->defTransMode);
        snprintf( dataBuf, GSOCK_DEFAULT_BUFLEN, "MODE %c\r\n", state->defTransMode );

        // Execute request, and check for errors, performing cleanup if needed.
        if( (iRes = ftpDataConProc_checkError(
                sendMessageGetResponse((state->controlSocket).sock, dataBuf, dataBuf, GSOCK_DEFAULT_BUFLEN, 1),
                dataBuf, formInfo, "transmission mode" )) != 0 )
            return iRes;

        formInfo->transMode = state->defTransMode;
    }

    // Structure (STRU x)
    if(state->defStructure){
        hlogf("Negotiating structure: %c\n", state->defStructure);
        snprintf( dataBuf, GSOCK_DEFAULT_BUFLEN, "STRU %c\r\n", state->defStructure );

        // Execute request, and check for errors, performing cleanup if needed.
        if( (iRes = ftpDataConProc_checkError(
                sendMessageGetResponse((state->controlSocket).sock, dataBuf, dataBuf, GSOCK_DEFAULT_BUFLEN, 1),
                dataBuf, formInfo, "structure" )) != 0 )
            return iRes;

        formInfo->structure = state->defStructure;
    }

    //TODO: implement PORT mode
    //By now we just set to PASv implicitly
    state->passiveModeOn = 1;

    // Set the passive option in formInfo
    formInfo->passiveOn = state->passiveModeOn;

    if(state->passiveModeOn) // PASV mode
    {
        // Execute request, and check for errors, performing cleanup if needed.
        if( (iRes = ftpDataConProc_checkError(
                sendMessageGetResponse((state->controlSocket).sock, "PASV\r\n", dataBuf, GSOCK_DEFAULT_BUFLEN, 1),
                dataBuf, formInfo, "setting passive mode" )) != 0 )
            return iRes;

        // Set IP and port in a structure and check for errors at the same time.
        if( (iRes = ftpDataConProc_checkError(
                ftpExtractIpPortPasv( &(formInfo->ipAddr), &(formInfo->port), dataBuf, 0),
                NULL, formInfo, "Can't extract IP and port from PASV response." )) != 0 )
            return 2;
    }
    else  // PORT mode
    {
        //Not yet impl'd
    }

    // Now execute the command specified.
    // Create the command string. Append RawName and params.
    if( ftpConvertUItoRaw( dataBuf, GSOCK_DEFAULT_BUFLEN, command ) < 0){
        hlogf("Couldn't convert UI command to Raw.\n");
        return 1;
    }
    
    // Execute the final data connection opening request, and check for errors, performing cleanup if needed.
    if( (iRes = ftpDataConProc_checkError(
            sendMessageGetResponse((state->controlSocket).sock, dataBuf, dataBuf, GSOCK_DEFAULT_BUFLEN, 1),
            dataBuf, formInfo, "Final Data Connection Opening" )) != 0 )
    {
        hlogf("Couldn't open the data connection.\nAborting everything.\n");

        return iRes;
    }

    // At this point everything is configured.

    hlogf("Spawning a Data Connection Thread!\n");
    int threadError = 0;
    for(int i = 0; i<FTP_MAX_DATA_THREADS; i++)
    {
        if(!gthread_Thread_isRunning( (state->DataThreadPool[i]).thrHand )){
            // Crete a new thread in this position and check if error occured.
            if( ! ((state->DataThreadPool[i]).thrHand = gthread_Thread_create( threadProc, (void*)formInfo )) )
                threadError = 1;
            hlogf("[main thread]: Successfully spawned thread, in position %d\n", i);   
            break;
        }
        if(i==FTP_MAX_DATA_THREADS-1)
            threadError = 2; // No free threads.
    }
    if(threadError){
        hlogf(threadError==1 ? "Error creating thread!\n" : "No free threads!\n");
        FTP_freeDataFormInfo(formInfo);
        return 1;
    }

    return 0;
}

/*! Processes commands which change the client local options
 *  For example, turns passive mode on or off.
 */
int ftpParamComProc(struct FTPCallbackCommand command, FTPClientState* state)
{
    // a
    return 0;
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
    int iResult = sendMessageGetResponse(sock, recvbuf, recvbuf, sizeof(recvbuf),
                   FTOOL_RECVRESP_PRINTBUFFER | FTOOL_RECVRESP_NOSEND );
    if(iResult < 0){
        hlogf("Error on sendMessageGetResponse() while receiving welcome message.\n");
        return -2;
    }

    int attempts = 3;
    //Authorize username and passwd with 3 attempts each.
    // i<3: UName, i>=3:passwd
    char success = 0;
    for(int i=0; i<2*attempts; i++){
        // Start user authorization. Straightforward, no encryption, just like the good ol' days :)
        strcpy(recvbuf, (i<attempts ? "USER " : "PASS ")); // Command
        gmisc_GetLine((i<attempts ? "Name: " : "Password: "), recvbuf+5, sizeof(recvbuf)-7, stdin); // Get parameter from user
        strcpy(recvbuf+strlen(recvbuf), "\r\n"); // CRLF terminator at the end

        if(sendMessageGetResponse(sock, recvbuf, recvbuf, sizeof(recvbuf), 1) < 0){
            hlogf("Error on sendMessageGetResponse()\n");
            return -3;
        }

        //Print the response
        //printf("\n%s\n", recvbuf);

        if((recvbuf[0]=='5' || recvbuf[0]=='4') && !((i+1) % attempts)){ // Haven't authenticated for all the attempts
            hlogf("Can't authenticate. Max attempts reached. Aborting...\n");
            return -1;
        }

        if(recvbuf[0]=='2' || recvbuf[0]=='3'){ //OK
            if(i<attempts)
                i=attempts; //Move to password
            else
                break; // Password auth'd, end loop.
        }
    }

    // Now it has been authenticated.
    return 0;
}

/*! The raw command interpreter
 *  Just sends a raw command to a server.
 *  Params:
 *  - command: null-terminated standard ftp command string, with CRLF at the end.
 *  - checkmode: sets if command is to be checked locally.
 *  Retval:
 *  - Standard (As below).
 */
int executeRawFtpCommand(const char* command, FTPClientState* state, char checkMode)
{
    int retval=0, valid=1;

    // TODO: Perform local checking if set

    if(valid) // Command can be set to server.
    {
        //TODO: for Data-Connection openers, fill info and start their thread.

        // Copy to the socket send buffer, and append CRLF in the end.
        /*strncpy((state->controlSocket).dataBuff, command, GSOCK_DEFAULT_BUFLEN-3);
        (state->controlSocket).dataBuff[GSOCK_DEFAULT_BUFLEN-3] = 0; // Null-terminate, then add CRLF in the end.
        strcpy( (state->controlSocket).dataBuff + strlen( (state->controlSocket).dataBuff ), "\r\n" );*/

        if( sendMessageGetResponse( (state->controlSocket).sock, command, \
            (state->controlSocket).dataBuff, GSOCK_DEFAULT_BUFLEN, 1 ) < 0 )
        {
            hlogf("Error on sendMessageGetResponse()\n");
            return -3;
        }

        // Print the receive message
        //printf("\n%s\n", (state->controlSocket).dataBuff);
    }
    return retval;
}

/*! The user-input command interpreter.
 *  Params:
 *  - command (User input):
 *    A User-entered command: null-terminated string, which has format:
 *    lowercase command word, space, parameter list, separated by spaces.
 *    Example: get file1.txt
 *  Retval:
 *  - < 0 - must terminate session (fatal error occured or quit request sent)
 *  - = 0 - function succeeded, must continue.
 *  - > 0 - non-fatal errors, can continue. (Invalid command(1), Bad params(2), etc.)
 */
int executeCommand(SOCKET sock, char* command, size_t comBufLen, FTPClientState* state)
{
    if(!command && !state) return 2; // 2 - bad params.

    int retval = 0; // Return 0 (good) by default.
    int valid = 0; // Set invalid now, later if found match, change it to valid.
    size_t comlen;

    hlogf("\n-----------------------------------\nProcessing a new command:\n%s\n", command);

    // Check if it is a raw command (# at the beginning)
    if( command[0]=='#' )
    {
        hlogf("Identified a raw command. Passing to RawProcessor.\n");

        comlen = strlen(command);
        // Check for CRLF at the end, and set if possible
        if(strcmp(command + comlen - 2, "\r\n") != 0){
            if(comlen + 2 > comBufLen)
                return 1; // Can't set CRLF --> bad.
            strcpy(command + comlen, "\r\n");
        }

        retval = executeRawFtpCommand(command+1, state, FTP_CHECKRAW_DEFAULT); // Call the raw command processor.

        hlogf("\nEnded command processing.\n-----------------------------------\n\n");
        return retval;
    }

    comlen = strcspn(command, gmisc_whitespaces);
    //Lowercase-ify the command name
    gmisc_CStringToLower(command, comlen);

    if(comlen <= FTPUI_COMMAND_NAME_LENGHT && comlen > 0) // Len good, we can do the loop.
    {
        // Iterate through all the available commands until we find a valid match
        for(const struct FTPClientUICommand* cmd = ftpClientCommands;
            cmd < ftpClientCommands + sizeof(ftpClientCommands)/sizeof(struct FTPClientUICommand);
            cmd++)
        {
            if( strncmp(command, cmd->name, comlen) == 0 ) // Found matching valid command.
            {
                hlogf("Identified a Valid command! Name: %s\n", cmd->name);

                valid = 1;

                // Setup the structure to pass to the Proc.
                struct FTPCallbackCommand cbst = { 0 };
                cbst.commInfo = cmd;

                // Tokenize the command, separating params.
                strtok(command, gmisc_whitespaces); // The name of command
                for(int j=0; j < FTPUI_COMMAND_MAXPARAMS; j++)
                {
                    cbst.params[j] = strtok(NULL, gmisc_whitespaces); // After the last token, all calls return NULL.
                    if(cbst.params[j])
                        hlogf("Found a param: %s\n", cbst.params[j]);
                }

                if( cmd->procedure(cbst, state) < 0 ){ // Call the specified processing function. If < 0, must end.
                    hlogf("Quit signal received from a procedure. Time to quit.\n");
                    retval = -1; // Quit sign.
                }
                break; // We executed a valid command, now end loop.
            }
        }
    }
    if(!valid){
        printf("Command is invalid!\n"); // retval=1 - command invalid.
        hlogf("Command is InValid.\n");
    }
    hlogf("\nEnded command processing.\n-----------------------------------\n");
    return retval;
}

int main(int argc, char **argv)
{
    SOCKET ControlSocket = INVALID_SOCKET;

    struct addrinfo *result = NULL,
                    hints;

    char recvbuf[FTP_DEFAULT_BUFLEN];
    size_t recvbuflen = sizeof(recvbuf);
    int iResult,
        oneCommand = 0;

    // Activate the logger
    hlogSetFile("grylogz.log", HLOG_MODE_UNBUFFERED | HLOG_MODE_APPEND);
    hlogSetActive(1);

    // Print current time to a logger.
    hlogf("\n\n===============================\nGrylloFTP %s\n> > ", FTP_CLIENT_VERSION);
    gmisc_PrintTimeByFormat( hlogGetFile(), NULL );
    hlogf("\n- - - - - - - - - - - - - - - - \n\n");

    //------------- Validate the parameters --------------//

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
    //FTP_setDefaultClientState( &ftpCliState );
    ftpCliState.controlSocket.sock = ControlSocket;

    // Authorize this connection.
    if( authorizeConnection(ControlSocket) < 0 )
        hlogf("Error authorizing a connection!\n");

    else // If authorization succeeded (>=0), let's start a command loop.
    {
        printf("Starting loop...\n");
        while(1)
        {
            gmisc_GetLine("\nftp> ", recvbuf, recvbuflen, stdin);

            if( executeCommand(ControlSocket, recvbuf, recvbuflen, &ftpCliState) < 0 ) // Error or need to quit.
                break;
        }
        printf("Connection closed. Exitting...\n");
    }

    // Execute QUIT command - safely terminate an FTP session.
    sendMessageGetResponse(ControlSocket, "QUIT\r\n", recvbuf, recvbuflen, 1);

    // cleanup. close the socket, and terminate the Winsock.dll instance bound to our app.
    gsockCloseSocket(ControlSocket);
    gsockSockCleanup();

    return 0;
}

