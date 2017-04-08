#include <stdio.h>
#include <stdlib.h>
#include <gmisc.h>

int main()
{
    /*char ba[32];
    int er;
    if(er = gmisc_GetLine("Enter a line:\n", ba, sizeof(ba), NULL))
        printf("Err %d occured.\n", er);
    else
        printf("\n%s\n", ba);    

    printf("\n================================\n\n");
    */

    char kaka[16] = "bfia\n\n\r\r\n\xF1\xF2";
    printf("%s\n", kaka);
    gmisc_NullifyStringEnd(kaka, 15, NULL);
    printf("|%s|\n", kaka);


    printf("\n\nTime:\n");
    gmisc_PrintTimeByFormat( stdout, NULL );

    return 0;
}
