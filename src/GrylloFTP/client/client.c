
//#define WIN32_LEAN_AND_MEAN

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
int ftpComplexComProc( struct FTPCallbackCommand, FTPClientState*);
int ftpParamComProc(   struct FTPCallbackCommand, FTPClientState*);

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
    
    // Complex commands (more than one raw FTP command required)
    { 3, "get",    0x0E, ftpComplexComProc},
    { 3, "send",   0x0F, ftpComplexComProc},
    { 1, "dir",    0x1C, ftpComplexComProc},
    
    // Setting altering commands
    { 4, "passive",   0, ftpParamComProc} 
};


// Helper funcs

/*! Function receives data until connection is closed, and for each received buffer calls the Callbk() function.
 *  TODO: Select-based stuff.
 */

int receiveAndPerformForEachPacket( SOCKET sock, char* buff, size_t bufsize, int flags, \
                                    void (*callbk)(char*, size_t, void*), void* callbkParam )
{
    int iRes, retval = 0;
    FD_SET readSet; // We'll use SELECT to check if something is inside the buffer.

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

// Simple callback-format function, which can print a buffer to a specified file.
void printBuffer(char* buff, size_t sz, void* param)
{
    //buff[sz] = 0; // Null-terminate the bufer for printing.

    fprintf((param ? (FILE*)param : stdout), "%.*s", sz, buff);
}

/*! The DataThread runner.
 *  Thread makes a Data connection to server and executes the transfer by the 
 *  options specified in the FTPDataFormatInfo* structure.
 *  Param:
 *  - Pointer to a FTPDataFormatInfo structure,
 *    holding all data needed for file data transfer (name, type, etc)
 */
void dataThreadRunner(void* param)
{
    if(!param) return;
        
    FTPDataFormatInfo* formInfo = (FTPDataFormatInfo*)param;
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
#define FTOOL_RECVRESP_NOSEND         2
#define FTOOL_RECVRESP_NORECEIVE      4 

int sendMessageGetResponse(SOCKET sock, const char* command, char* respondbuf, size_t respbufsiz, char flags)
{
    fd_set readSet;
    fd_set writeSet;
    int iRes = 0;
    int lastRespCode = 0;
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

        iRes = select(0, NULL, &writeSet, NULL, &tv);
        if(iRes == SOCKET_ERROR){
            hlogf("SELECT returned error when SENDing.\n");
            return -1;
        }
        else if(iRes == 0){
            hlogf("Its's unavailable to send data on this socket!\n");
            return 0;
        }
        
        if(FD_ISSET(sock, &writeSet)) // There's data for reading on this sock.
        {  
            hlogf("FD's are OK for sending. Trying to send...\n");

            // Send the command
            if((iRes = send(sock, command, strlen(command), 0)) < 0){
                hlogf("Error sending a message.\n");
                return -2;
            }
            hlogf("Bytes sent: %d\n", iRes);
        }
    }

    if(! (flags & FTOOL_RECVRESP_NORECEIVE))
    {    
        hlogf("\nReceive flag is set. Checking FD's for receiving.\n");

        // Now receive the Response
        while(1){
            tv.tv_sec = 1; // Wait 1 second until request arrives
            tv.tv_usec = 0;
            
            FD_ZERO(&readSet);
            FD_SET(sock, &readSet); // Add this socket to a set of socks checked for readability

            iRes = select(0, &readSet, NULL, NULL, &tv); // Return - no.of socks available for reading/writing.
            if(iRes == SOCKET_ERROR){
                hlogf("SELECT returned error.\n");
                return -1;
            }
            else if(iRes == 0){
                hlogf("Timeout: No more readable data on sockets!\n");
                return lastRespCode;
            }
            
            if(FD_ISSET(sock, &readSet)) // There's data for reading on this sock.
            {    
                hlogf("Data is available for receiving on this socket.\n");

                iRes = recv(sock, respondbuf, respbufsiz, 0);
                if( iRes < 0 ){
                    hlogf("Error receiving  a response.\n");
                    return -2;
                }
                if( iRes == 0 ){
                    hlogf("recv returned 0 - FIN.\n");
                    return -1;
                }
                // iRes > 0 = size of buffer.
                hlogf("Received bytes: %d\n", iRes);
               
                // Parse the response, and null-terminate it.
                respondbuf[iRes] = 0;

                if(flags & FTOOL_RECVRESP_PRINTBUFFER)
                    printf("\n%s\n", respondbuf); 
                
                // Return the response code as int.
                lastRespCode = ( (respondbuf[0]-'0')*100 + (respondbuf[1]-'0')*10 + (respondbuf[2]-'0') );
                lastRespCode = (lastRespCode<0 ? 0 : lastRespCode);
            }
        }
    }

    return lastRespCode;
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

    size_t curlen, totallen = strlen((command.commInfo)->name) + 1;
    // Create the command string. Append RawName and params.
    strcpy( (state->controlSocket).dataBuff, FTP_getRawNameFromID( (command.commInfo)->rawCommandID ) );

    for(char** str=command.params; str < command.params + sizeof(command.params)/sizeof(char*); str++){
        if(!*str) break;
        curlen = strlen(*str);
        if(totallen+curlen+1 > GSOCK_DEFAULT_BUFLEN-2) // Buffer is too small
            break;
        strcat( (state->controlSocket).dataBuff, " ");    
        strcat( (state->controlSocket).dataBuff, *str);    
        
        totallen += curlen+1;
    }
    // And add CRLF at the end.
    strcat( (state->controlSocket).dataBuff, "\r\n");

    //Execute that command.
    if(sendMessageGetResponse((state->controlSocket).sock, (state->controlSocket).dataBuff,
       (state->controlSocket).dataBuff, GSOCK_DEFAULT_BUFLEN, 1 ) < 0)
    {
        hlogf("Error on sendMessageGetResponse()\n");
        return -3;
    }
    return 0;
}

/*! Processes commands which consists of multiple requests and replies,
 *  and opens/closes data connections, modifies state.
 */
int ftpComplexComProc(struct FTPCallbackCommand command, FTPClientState* state)
{
    //a
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
    // Print the message
    recvbuf[iResult] = 0;
    printf("\n%s\n", recvbuf);
    
    int attempts = 3;
    //Authorize username and passwd with 3 attempts each.
    // i<3: UName, i>=3:passwd
    char success = 0;
    for(int i=0; i<=2*attempts; i++){
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

    hlogf("\n-----------------------------------\nProcessing a new command.\n");

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
                }

                if( cmd->procedure(cbst, state) < 0 ){ // Call the specified processing function. If < 0, must end. 
                    hlogf("Quit signal received from a procedure. Time to quit.\n");
                    retval = -1; // Quit sign.
                }
                break; // We executed a valid command, now end loop.    
            }
        }
    }
    if(!valid)
        printf("Command is invalid!\n"); // retval=1 - command invalid.
    hlogf("\nEnded command processing.\n-----------------------------------\n");
    return retval;
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
    hlogSetFile("grylogz.log", HLOG_MODE_UNBUFFERED);
    hlogSetActive(1);

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
    
    /*hlogf("Setting the socket to NonBlocking");
    if(fnctl(ControlSocket, F_SETFL, O_NONBLOCK))
        return gsockErrorCleanup(ControlSocket, NULL, "Can't make a socket non-blocking!", 1, 1); 
    */

    //---------------   Start a SessioN  ----------------// 

    // The state structure    
    FTPClientState ftpCliState = {0};    
    ftpCliState.controlSocket.sock = ControlSocket;
    
    // Authorize, and start loop.
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

