#ifndef GBANG_H_INCLUDED
#define GBANG_H_INCLUDED

// GBANG protocol info
#define GBANG_VERSION 1

#define GBANG_ERROR         "E_"
#define GBANG_ERROR_NOFILE  "E_NOFILE_"
#define GBANG_ERROR_SOCKS   "E_SOCKS_"

#define GBANG_DATA_START    "START_DATA_"
#define GBANG_DATA_SENDING  "DATA_"
#define GBANG_DATA_END      "END_DATA_"
#define GBANG_INVALID_COMMAND "INVALID_"

#define GBANG_REQUEST_FILE      "FILE_"
#define GBANG_REQUEST_DIR       "DIR_"
#define GBANG_REQUEST_SYSINFO   "SYST_"
#define GBANG_REQUEST_EXIT      "EXIT_"
#define GBANG_REQUEST_SHUTDOWN  "SHUTDOWN_"
 
#define GBANG_HEADER_SIZE   16
#define GBANG_DATA_SIZE     1472

// Protocol data structure

struct __attribute__((__packed__)) GrylBangProtoData // Set no padding for the accurate packet size.
{
    //Header
    char version;
    char commandString[ GBANG_HEADER_SIZE - 1 ];
    //Data
    char data[ GBANG_DATA_SIZE ];
};

#endif //GBANG_H_INCLUDED
