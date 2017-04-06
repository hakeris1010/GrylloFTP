#ifndef CLIENTCOMMANDS_H_INCLUDED
#define CLIENTCOMMANDS_H_INCLUDED

/*! The client side structures.
 *
 */

#include <gsrvsocks.h> 
#include "../gftp/gftp.h"

#define FTPUI_COMFLAG_COMPLEX       1
#define FTPUI_COMFLAG_HASPARAMS     2
#define FTPUI_COMFLAG_SETTING_EDIT  4
#define FTPUI_COMFLAG_INTERMEDIATE  8

#define FTPUI_COMMAND_NAME_LENGHT 10
#define FTPUI_COMMAND_MAXPARAMS 5
#define FTP_CHECKRAW_DEFAULT 0

#define FTP_MAX_DATA_THREADS 8


// FTP Structs
/*! The structure to be passed to the Data Thread. 
 *  Holds all data needed to initiate data transfer
 */ 
typedef struct
{
    char dataType;      // Ascii, Image, Local, EbcDic
    char dataFormat;    // For Ascii - NonPrint, Telnet_FormatContol, CarriageControl
    char structure;     // File, record, page
    char transMode;     // Stream, block, compressed.

    char passiveOn; // PASV or PORT
    char* ipAddr;   // Must be free'd
    short port;

    FILE* outFile;  // Maybe closed, if has been opened.
    char* fname;    // Muse be free'd
} FTPDataFormatInfo;

/*! Client state structure.
 *  Holds current control socket, Data threads, and options.
 *  If option value is 0, don't use it (or use default).
 */ 
typedef struct 
{
    GSOCKSocketStruct controlSocket;

    GrThreadHandle DataThreadPool[FTP_MAX_DATA_THREADS];
    
    // Options.
    char passiveModeOn;
    char defDataFormat; 
    char defDataType;
    char defTransMode;
    char defStructure;

} FTPClientState;

struct FTPClientUICommand;

/*! A command structure which is sent to callbacks.
 *  Has a param string array, and a pointer to a database strucure. 
 */
struct FTPCallbackCommand
{
    char* params[ FTPUI_COMMAND_MAXPARAMS ];
    const struct FTPClientUICommand* commInfo;
};

/*! UI Command info structure.
 *  - Contains data about command name, specific flags,
 *    and a procedure to call to process that command.
 */
struct FTPClientUICommand
{
    char flags;
    char name[ FTPUI_COMMAND_NAME_LENGHT ];
    char rawCommandID;
    int (*procedure)(struct FTPCallbackCommand, FTPClientState*);
}; 

/*! A database-like buffer, storing data about all the supported commands.
 */
extern const struct FTPClientUICommand cftpClientCommand[];

void FTP_freeDataFormInfo(FTPDataFormatInfo* info);
void FTP_printDataFormInfo(const FTPDataFormatInfo* fi, FILE* outFile);
int ftpExtractIpPortPasv( char** ip, short* port, const char* dataBuf, char extended );

#endif // CLIENTCOMMANDS_H_INCLUDED
