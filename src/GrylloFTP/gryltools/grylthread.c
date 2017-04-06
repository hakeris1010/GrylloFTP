#include "grylthread.h"

#if defined __WIN32
    #include "windows.h"
#elif defined __linux__
    #include <sys/types.h>
    #include <sys/wait.h>
    #include <unistd.h>
    #include <errno.h>
    #include <signal.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GRYLTHREADMODE_LINUX_FORK     1 // by now only this mode is implemented.
#define GRYLTHREADMODE_LINUX_PTHREAD  2

//TODO: Implement flags, e.g. set inactive flag when execution ends.

// Thread flags.
#define GRYLTHREAD_FLAG_ACTIVE  1


// Threading utilities.
struct ThreadFuncAttribs_Extended
{
    void* funcPtr;  // Pointer to a memory location to call
    void* funcSig;  // Pointer to a function signature
                    // - structure version (1 byte)
                    // - count (2 bytes): number of arguments
                    // - paramInfo (count*6 bytes):
                    //   - type&flags (2 bytes):
                    //     - unique id of a data type (12 bits)
                    //     - flags (4 bits)
                    //   - size (4 bytes): size of the data type.

    void* params; // param data&info.
                    // - count (2 bytes): number of params actually passed.
                    // - data (count * sum(funcSig.paramInfo[i].size, i=[0, count]))
};

struct ThreadFuncAttribs
{
	void (*proc)(void*);
	void* param;
};

struct ThreadHandlePriv
{
    #if defined __WIN32
        HANDLE hThread;
    #elif defined __linux__
        pid_t pid;
    #endif
    char flags;
};

#if defined __WIN32
DWORD WINAPI ThreadProc( LPVOID lpParameter )
{
    if(!lpParameter) // Error. Bad ptr.
        return 1;
    // Now retrieve the pointer to procedure and call it.
    struct ThreadFuncAttribs* attrs = (struct ThreadFuncAttribs*)lpParameter;
    attrs->proc( attrs->param );

    // Cleanup after the proc returns - the structure ThreadFuncAttribs is malloc'd on the heap, so we must free it. The pointer belongs only to this thread at this moment.
    free(attrs);
    return 0;
}
#endif

GrThreadHandle gthread_procToThread(void (*proc)(void*), void* param)
{
    struct ThreadHandlePriv* thread_id = NULL;
    // OS-specific code
    #if defined __WIN32
        struct ThreadFuncAttribs* attr = malloc( sizeof(struct ThreadFuncAttribs) );
		attr->proc = proc;
        attr->param = param;

        HANDLE h = CreateThread(NULL, 0, ThreadProc, (void*)attr, 0, NULL);
        if(h){
            thread_id = malloc(sizeof(struct ThreadHandlePriv));
            thread_id->hThread = h;
        }
    #elif defined __linux__
        // Call fork - spawn process.
        // On a child process execution resumes after FORK,
        // For a child fork() returns 0, and for the original, returns 0 on good, < 0 on error.
        pid_t pid = fork();

        if(pid == 0){ // Child process
            proc(param); // Call the client servicer function
        }
        else if(pid > 0){ // Parent and no error
            thread_id = malloc(sizeof(struct ThreadHandlePriv));
            thread_id->pid = pid;
        }
    #endif
    // Set the "active" flag on thread.	
    if(thread_id) // No error
    	thread_id->flags |= GRYLTHREAD_FLAG_ACTIVE;	

    return (GrThreadHandle)thread_id;
}

void gthread_sleep(unsigned int millisecs)
{
    #if defined __WIN32
        Sleep(millisecs);
    #elif defined __linux__
        sleep(millisecs);
    #endif
}

void gthread_joinThread(GrThreadHandle hnd)
{
    if(!hnd) return;
    if(gthread_isThreadRunning(hnd))
    {
        #if defined __WIN32
            WaitForSingleObject( ((struct ThreadHandlePriv*)hnd)->hThread, INFINITE );
        #elif defined __linux__
            waitpid( ((struct ThreadHandlePriv*)hnd)->pid, NULL, 0 );
        #endif
    }
    // Now thread is no longer running, we can free it's handle.
    free( (struct ThreadHandlePriv*)hnd );
}

char gthread_isThreadRunning(GrThreadHandle hnd)
{
    if(!hnd) return 0;
    struct ThreadHandlePriv* phnd = (struct ThreadHandlePriv*)hnd;
    if(! (phnd->flags & GRYLTHREAD_FLAG_ACTIVE)) // Active flag is not set.
        return 0;

    #if defined __WIN32
        DWORD status;
        if( GetExitCodeThread( phnd->hThread, &status ) ){
            if(status == STILL_ACTIVE)
                return 1; // Still running!
            // Not running
	    phnd->flags &= ~GRYLTHREAD_FLAG_ACTIVE; // Clear the active flag.
        }
    #elif defined __linux__
        // Here we use kill (send signal to process), with signal as 0 - don't send, just check process state.
        // Error occured, now check if process doesn't exist.
        if( kill( ((struct ThreadHandlePriv*)hnd)->pid, 0 ) < 0 ){ 
            if(errno == ESRCH){ // Process doesn't exist.
	    	phnd->flags &= ~GRYLTHREAD_FLAG_ACTIVE; // Clear the active flag.
                return 0; // Not running.
	    }
        }
        return 1; // Thread running.
    #endif

    return 0;
}

//end.
