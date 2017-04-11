#include <grylthread.h>
#include <stdio.h>

void doShit(void* paramz0r)
{
    if(!paramz0r)
        return;
    int id = *((int*)paramz0r);
    int count = *((int*)(paramz0r+sizeof(int)));
    char* str0ng = (char*)(paramz0r+2*sizeof(int));

    for(int i=0; i < count; i++){
        //printf("LoopNo: %d | Id: %d | %s\n", i, id, str0ng);
	printf("%d ", id);
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
    int loops = 100;

    char data1[32] = "\0\0\0\0\0\0\0\0NyaaNyaa :3\0";
    *((int*)data1)     = 1; // Thread Id
    *((int*)(data1+4)) = loops; // Number of loop iterations.

    GrThread h1 = gthread_Thread_create(doShit, &data1);
//aaaaa
    char data2[32] = "\0\0\0\0\0\0\0\0Kawaii~~ :3\0";
    *((int*)data2)     = 2; // Thread Id
    *((int*)(data2+4)) = loops; // Number of loop iterations.
    GrThread h2 = gthread_Thread_create(doShit, &data2);
    
    for(int i=0; i<loops; i++){
        //printf(" I am very cute :3 \n");
	printf("m ");
    }
    
    //sleep(10000);
    gthread_Thread_join(h1);
    gthread_Thread_join(h2);

    return 0;
}

