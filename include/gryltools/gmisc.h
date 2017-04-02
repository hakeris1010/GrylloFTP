#ifndef GMISC_H_INCLUDED
#define GMISC_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>

#define GMISC_GETLINE_OK       0
#define GMISC_GETLINE_NO_INPUT 1
#define GMISC_GETLINE_TOO_LONG 2

int gmisc_GetLine( const char *prmpt, char *buff, size_t sz, FILE* file );
void gmisc_NullifyStringEnd(char* str, size_t size, const char* delim);

#endif // GMISC_H_INCLUDED
