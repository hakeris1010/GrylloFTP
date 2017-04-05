#include "gftp.h"
#include <string.h>

/*! Here is defined the Raw Command Database, with IDs and flags.
 *
 */

const struct GFTPCommandInfo FTP_RawCommandDatabase[] =
{
    //------   Access control   -------//
    {0x01, 3, "USER"}, //BASIC
    {0x02, 5, "PASS"}, //BASIC
    {0x03, 1, "QUIT"}, //BASIC
    {0x04, 2, "ACCT"},
    {0x05, 2, "CWD"},
    {0x06, 0, "CDUP"},
    {0x07, 2, "SMNT"},
    {0x08, 0, "REIN"},

    //------ Transfer parameters -------//
    {0x09, 3, "PORT"}, //BASIC
    {0x0A, 1, "PASV"}, //BASIC
    {0x0B, 3, "TYPE"}, //BASIC
    {0x0C, 3, "STRU"}, //BASIC
    {0x0D, 3, "MODE"}, //BASIC

    //------ FTP Service Comm's -------//
    {0x0E, 3, "RETR"}, //BASIC
    {0x0F, 3, "STOR"}, //BASIC
    {0x10, 1, "NOOP"}, //BASIC
    {0x11, 0, "STOU"},
    {0x12, 2, "APPE"},
    {0x13, 6, "ALLO"},
    {0x14, 2, "REST"},
    {0x15, 2, "RNFR"},
    {0x16, 2, "RNTO"},
    {0x17, 0, "ABOR"},
    {0x18, 2, "DELE"},
    {0x19, 2, "RMD"},
    {0x1A, 2, "MKD"},
    {0x1B, 0, "PWD"},
    {0x1C, 4, "LIST"},
    {0x1D, 4, "NLST"},
    {0x1E, 2, "SITE"},
    {0x1F, 0, "SYST"},
    {0x20, 4, "STAT"},
    {0x21, 4, "HELP"}
};

const char* FTP_getRawNameFromID(char id)
{
    for(const struct GFTPCommandInfo* cmd = FTP_RawCommandDatabase; 
        cmd < FTP_RawCommandDatabase + sizeof(FTP_RawCommandDatabase)/sizeof(struct GFTPCommandInfo);
        cmd++)
    {
        if((char)id == cmd->id)
            return cmd->comString;
    }
    return NULL;
}
