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
#define FTP_DATATYPE_ASCII  1  // Only this one should be used.
#define FTP_DATATYPE_IMAGE  2
#define FTP_DATATYPE_LOCAL  3
#define FTP_DATATYPE_EBCDIC 4 

// For ASCII type, optional format second argument.
#define FTP_DATAFORMAT_NONPRINT              1 // Default
#define FTP_DATAFORMAT_TELNET_FORMATCONTROL  2
#define FTP_DATAFORMAT_CARRIAGECONTROL       3

// File structures (not used)
#define FTP_STRUCTURE_FILE      1 // Default
#define FTP_STRUCTURE_RECORD    2
#define FTP_STRUCTURE_PAGE      3 // Requires page header (more defines)

// Transmission modes 
#define FTP_TRANSMODE_STREAM    1 // Default
#define FTP_TRANSMODE_BLOCK     2 // With a header (More defines incoming)
#define FTP_TRANSMODE_COMPRESS  3

/**
 *  FTP Commands (Sent from user-PI to server-PI)
 */
//------ Access control -------
#define FTP_COM_USER "USER "
#define FTP_COM_PASS "PASS "
#define FTP_COM_QUIT "QUIT "
// Not important  
#define FTP_COM_ACCT "ACCT "
#define FTP_COM_CWD  "CWD "
#define FTP_COM_CDUP "CDUP "
#define FTP_COM_SMNT "SMNT "
#define FTP_COM_REIN "REIN "

//------ Transfer parameter -------
#define FTP_COM_PORT "PORT "
#define FTP_COM_


#define CRLF "\r\n"

#endif //GFTP_H_INCLUDED
