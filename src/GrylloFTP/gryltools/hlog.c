#include "hlog.h"
#include <stdarg.h>

#define HLOG_DEFAULT_LOGFILE stdout

static FILE* curFile = NULL;
static char active = 1;

FILE* hlogSetFile(const char* fname, char mode)
{
    hlogCloseFile();
    if( (curFile = fopen(fname, "w"))!=NULL && (mode & HLOG_MODE_UNBUFFERED) ) // check for UNBUFFERED bit
        setbuf(curFile, NULL); // Disable buffering.
    return curFile;
}

void hlogSetFileFromFile(FILE* file, char mode)
{
    hlogCloseFile();
    curFile = file;
    if(mode & HLOG_MODE_UNBUFFERED)
        setbuf(curFile, NULL); // Disable buffering.
}

void hlogCloseFile()
{
    if(curFile && curFile!=stdout && curFile!=stderr)
        fclose(curFile);
}

FILE* hlogGetFile()
{
    return curFile;
}

void hlogf(const char* fmt, ...)
{
    if(!active)
        return;
    if(!curFile)
        curFile = HLOG_DEFAULT_LOGFILE;

    va_list vl;
    va_start(vl, fmt);

    vfprintf( curFile, fmt, vl ); // Call vith Variadic Arguments.

    va_end(vl);
}

char hlogIsActive(){
    return active;
}

void hlogSetActive(char val){
    active = val;
}

