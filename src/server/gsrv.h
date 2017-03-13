#ifndef GSRV_H_DEFINED
#define GSRV_H_DEFINED

// Defines for the GSRV server

#define GSRV_DEFAULT_BUFLEN 512
#define GSRV_DEFAULT_PORT "27015"

// Accept this much connections
#define GSRV_CONNECTIONS_TO_ACCEPT 128
#define GSRV_MAX_CLIENTS 32

// Client Status constnts
#define GSRV_STATUS_INACTIVE            0
#define GSRV_STATUS_SENDING_FILE        1
#define GSRV_STATUS_COMMAND_WAITING     2


#endif
