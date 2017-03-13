#include "../server/helperz.h"
#include <stdio.h>

void doShit(void* paramz0r)
{
    if(!paramz0r)
        return;
    int id = *((int*)paramz0r);
    int count = *((int*)(paramz0r+sizeof(int)));
    char* str0ng = (char*)(paramz0r+2*sizeof(int));

    for(int i=0; i < count; i++){
        printf("LoopNo: %d | Id: %d | %s\n", i, id, str0ng);
    }
}

void printThread(void* param)
{
    int* id = (int*)param;
    for(int i=0; i<1000; i++)
    {
        printf("%d ", *id);
    }
}

int main(int argc, char** argv)
{
    char data1[32] = "\0\0\0\0\0\0\0\0NyaaNyaa :3\0";
    *((int*)data1)     =   1; // Thread Id
    *((int*)(data1+4)) = 100; // Number of loop iterations.

    GrThreadHandle h1 = procToThread(doShit, &data1);

    char data2[32] = "\0\0\0\0\0\0\0\0Kawaii~~ :3\0";
    *((int*)data2)     =   2; // Thread Id
    *((int*)(data2+4)) = 100; // Number of loop iterations.
    GrThreadHandle h2 = procToThread(doShit, &data2);

    joinThread(h1);
    joinThread(h2);

    return 0;
}
