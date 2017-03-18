#ifndef HLOG_H_INCLUDED
#define HLOG_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>

// If set, write()'s to file won't store data in buffer, but will immediately write data into file.
#define HLOG_MODE_UNBUFFERED  1

/* Set, Close and Get the current LogFile */
FILE* hlogSetFile(const char* fname, char mode);
void hlogSetFileFromFile(FILE* file, char mode);
void hlogCloseFile();
FILE* hlogGetFile();

/* Write to the LogFile (Printf style) */
void hlogf( const char* fmt, ... );

#endif // HLOG_H_INCLUDED
