#ifndef GFTP_H_INCLUDED
#define GFTP_H_INCLUDED

/** 
 *  GFTP FTP protocol implementation by GrylloTron
 */

#define GFTP_VERSION "0.1 pre"
#define FTP_VERSION "unimpl"

// Buffer lenght
#define FTP_DEFAULT_BUFLEN 1500

// Default ports
#define FTP_CONTROL_PORT    21
#define FTP_DATA_PORT       20

// Communication modes (Flag-style):
#define FTP_PI_CONTROL_ACTIVE 1
#define FTP_DATA_TRANSFER_ACTIVE 2

// Data types (Var-style)
#define FTP_DATATYPE_ASCII  1  // BASIC, Default
#define FTP_DATATYPE_IMAGE  2
#define FTP_DATATYPE_LOCAL  3
#define FTP_DATATYPE_EBCDIC 4 

// For ASCII type, optional format second argument.
#define FTP_DATAFORMAT_NONPRINT              1 // BASIC, Default
#define FTP_DATAFORMAT_TELNET_FORMATCONTROL  2
#define FTP_DATAFORMAT_CARRIAGECONTROL       3

// File structures (not used)
#define FTP_STRUCTURE_FILE      1 // BASIC, Default
#define FTP_STRUCTURE_RECORD    2 // BASIC
#define FTP_STRUCTURE_PAGE      3 // Requires page header (more defines)

// Transmission modes 
#define FTP_TRANSMODE_STREAM    1 // BASIC, Default
#define FTP_TRANSMODE_BLOCK     2 // With a header (More defines incoming)
#define FTP_TRANSMODE_COMPRESS  3

/**
 *  FTP Commands (Sent from user-PI to server-PI)
 */
/** FLAGS
 *  The flags are held in the first byte of the command's #define. 
 *  Denotes the command's properties.
 */
//#define FTP_COMFLAG_ID                  0x1F // Command's ID, 32 values.
#define FTP_COMFLAG_COMMAND_BASIC       0x01 // The command is basic, must have all implementations.
#define FTP_COMFLAG_HAS_PARAMS          0x02 // Gotta expect params after a space. Error if not found
#define FTP_COMFLAG_HAS_OPTIONAL_PARAMS 0x04 // Optional params, no error if not found.

//Command namelen
#define FTP_COMNAME_LENGHT  6

/** FORMAT:
 *  - Byte 0: ID        
 *  - Byte 1: Flags
 *  - Byte 2+: Command string.
 */
struct GFTPCommandInfo
{
     char id;
     char flags;
     char comString[FTP_COMNAME_LENGHT];
};

extern const struct GFTPCommandInfo FTP_RawCommandDatabase[];

#define CRLF "\r\n"


#endif //GFTP_H_INCLUDED
