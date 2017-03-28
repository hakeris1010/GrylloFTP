#ifndef GFTP_H_INCLUDED
#define GFTP_H_INCLUDED

/** 
 *  GFTP FTP protocol implementation by GrylloTron
 */

#define GFTP_VERSION "0.1 pre"
#define FTP_VERSION "unimpl"

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

/** FORMAT:
 *  - Byte 0: ID        
 *  - Byte 1: Flags
 *  - Byte 2+: Command string.
 */
struct GFTPCommandInfo
{
     char id;
     char flags;
     char comString[6];
};

//------   Access control   -------//
#define FTP_COM_USERNAME        {0x01, 0, "USER"} //BASIC
#define FTP_COM_PASSWORD        {0x02, 0, "PASS"}
#define FTP_COM_QUIT            {0x03, 0, "QUIT"} //BASIC
#define FTP_COM_ACCOUNT         {0x04, 0, "ACCT"}
#define FTP_COM_CHANGE_DIR      {0x05, 0, "CWD"}
#define FTP_COM_CHANGE_DIR_UP   {0x06, 0, "CDUP"}
#define FTP_COM_S_MOUNT         {0x07, 0, "SMNT"}
#define FTP_COM_REINITIALIZE    {0x08, 0, "REIN"}

//------ Transfer parameters -------//
#define FTP_COM_OPEN_DATAPORT   {0x09, 0, "PORT"} //BASIC
#define FTP_COM_LISTEN_DATAPORT {0x0A, 0, "PASV"}
#define FTP_COM_DATA_TYPE       {0x0B, 0, "TYPE"} //BASIC
#define FTP_COM_STRUCTURE       {0x0C, 0, "STRU"} //BASIC
#define FTP_COM_TRANSFER_MODE   {0x0D, 0, "MODE"} //BASIC

//------ FTP Service Comm's -------//
#define FTP_COM_RETRIEVE        {0x0E, 0, "RETR"} //BASIC
#define FTP_COM_STORE           {0x0F, 0, "STOR"} //BASIC
#define FTP_COM_NOOP            {0x10, 0, "NOOP"} //BASIC
#define FTP_COM_STORE_UNIQUE    {0x11, 0, "STOU"}
#define FTP_COM_APPEND          {0x12, 0, "APPE"}
#define FTP_COM_ALLOCATE        {0x13, 0, "ALLO"}
#define FTP_COM_RESTART         {0x14, 0, "REST"}
#define FTP_COM_RENAME_FROM     {0x15, 0, "RNFR"}
#define FTP_COM_RENAME_TO       {0x16, 0, "RNTO"}
#define FTP_COM_ABORT           {0x17, 0, "ABOR"}
#define FTP_COM_DELETE          {0x18, 0, "DELE"}
#define FTP_COM_REMOVE_DIR      {0x19, 0, "RMD"}
#define FTP_COM_MAKE_DIR        {0x1A, 0, "MKD"}
#define FTP_COM_PRINT_DIR       {0x1B, 0, "PWD"}
#define FTP_COM_LIST            {0x1C, 0, "LIST"}
#define FTP_COM_NAME_LIST       {0x1D, 0, "NLST"}
#define FTP_COM_SITE_PARAMS     {0x1E, 0, "SITE"}
#define FTP_COM_SYSTEM          {0x1F, 0, "SYST"}
#define FTP_COM_STATUS          {0x20, 0, "STAT"}
#define FTP_COM_HELP            {0x21, 0, "HELP"}

#define CRLF "\r\n"



#endif //GFTP_H_INCLUDED
