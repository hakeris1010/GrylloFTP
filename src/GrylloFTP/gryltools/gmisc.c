#include "gmisc.h"
#include <string.h>
#include <ctype.h>

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

void gmisc_CStringToLower(char* str, size_t size) // If size==0, until '\0'
{
    size = (size==0 ? strlen(str) : size);
    for(size_t i=0; i<size; i++){
        str[i] = tolower(str[i]);
    }
}

void gmisc_strnSubst(char* str, size_t sz, const char* targets, char subst)
{
    for(char* i = str; *i!=0; i++)
    {
        for(const char* j = targets; *j!=0; j++){
            if(*i == *j){
                *i = subst;
                break;
            }
        }

        if(sz && i-str >= sz-1)
            break;
    }
}

void gmisc_strSubst(char* str, const char* targets, char subst)
{
    gmisc_strnSubst(str, 0, targets, subst);
}


