#include "clientcommands.h"

const FTPClientUICommand ftpClientCommands[]=
{
    { 0, "system", ftpSimpleCommandProc},
    { 0, "quit"}, ftpSimpleCommandProc}
};

int ftpSimpleCommandProc(const char* command, FTPClientState* state)
{
    //a
}
