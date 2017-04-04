#include "gmisc.h"
#include <string.h>

const char* gmisc_whitespaces = " \t\n\v\f\r";

void gmisc_NullifyStringEnd(char* fname, size_t size, const char* delim)
{
    if(!fname || size<=0) return;
    fname[size] = 0;

    for(size_t i=size-1; i>0; i--){
        if(fname[i] < 32) // invalid
            fname[i] = 0;
    }
}

int gmisc_GetLine(const char *prmpt, char *buff, size_t sz, FILE* file ) {
    int ch, extra;
    if(!file)
        file=stdin;

    // Get line with buffer overrun protection.
    if (prmpt && file==stdin) {
        printf ("%s", prmpt);
        fflush (stdout);
    }
    if (fgets (buff, sz, file) == NULL){
        buff[0] = 0; // Nullify
        return GMISC_GETLINE_NO_INPUT;
    }

    // If it was too long, there'll be no newline. In that case, we flush
    // to end of line so that excess doesn't affect the next call.
    if (buff[strlen(buff)-1] != '\n') {
        buff[sz] = 0;
        extra = 0;
        while (( (ch = fgetc(file)) != '\n') && (ch != EOF))
            extra = 1;
        return (extra) ? GMISC_GETLINE_TOO_LONG : GMISC_GETLINE_OK;
    }

    // Otherwise remove newline and give string back to caller.
    buff[strlen(buff)-1] = '\0';
    return GMISC_GETLINE_OK;
}


