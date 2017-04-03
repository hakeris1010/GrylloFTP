#include "clientcommands.h"

const FTPClientUICommand ftpClientCommands[]=
{
    // Simple commands
    { 0, "system",  ftpSimpleComProc},
    { 0, "cdup",    ftpSimpleComProc},
    { 0, "pwd",     ftpSimpleComProc},
    { 0, "shelp",   ftpSimpleComProc},
    { 0, "abort",   ftpSimpleComProc},
    { 0, "status",  ftpSimpleComProc},
    { 0, "quit",    ftpSimpleComProc},
    
    // Complex commands (more than one raw FTP command required)
    { 3, "get",     ftpComplexComProc},
    { 3, "send",    ftpComplexComProc},
    { 1, "dir",     ftpComplexComProc},
    
    // Setting altering commands
    { 4, "passive", ftpParamComProc} 
};

int ftpSimpleCommandProc(const char* command, FTPClientState* state)
{
    //a
}
