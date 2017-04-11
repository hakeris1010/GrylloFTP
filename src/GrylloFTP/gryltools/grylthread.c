#include "grylthread.h"

#if defined __WIN32
    #include <windows.h>
    #include <WinBase.h>
    #include <Synchapi.h>
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
#include "hlog.h"

//==========================================================//
// - - - - - - - - - - Thread section  - - - - - - - - - - -//

/* Thread implementation modes
 * On POSIX, 2 options: spawn process (fork), or use pthreads.
 */ 
#define GRYLTHREAD_THREADMODE_LINUX_FORK     1 // by now only this mode is implemented.
#define GRYLTHREAD_THREADMODE_LINUX_PTHREAD  2

/* DEPRECATED, UNSUPPORTED:
 * Mutex on Windows modes
 * If this is defined also use the kernel-level mutex
 * If not defined, use CRITICAL_SECTION only.
 * Note that CRITICAL_SECTION will always be defined in the structure, just the impl will differ. 
 */ 
//#define GRYLTHREAD_MUTEXMODE_WINDOWS_USE_KERNELMUTEX  1 

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

GrThread gthread_Thread_create(void (*proc)(void*), void* param)
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

    return (GrThread)thread_id;
}

void gthread_Thread_sleep(unsigned int millisecs)
{
    #if defined __WIN32
        Sleep(millisecs);
    #elif defined __linux__
        sleep(millisecs);
    #endif
}

void gthread_Thread_join(GrThread hnd)
{
    if(!hnd) return;
    if(gthread_Thread_isRunning(hnd))
    {
        #if defined __WIN32
            WaitForSingleObject( ((struct ThreadHandlePriv*)hnd)->hThread, INFINITE );
            CloseHandle( ((struct ThreadHandlePriv*)hnd)->hThread ); // Close native handle (OS recomendation)
        #elif defined __linux__
            waitpid( ((struct ThreadHandlePriv*)hnd)->pid, NULL, 0 );
        #endif
    }
    // Now thread is no longer running, we can free it's handle.
    free( (struct ThreadHandlePriv*)hnd );
}

char gthread_Thread_isRunning(GrThread hnd)
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
        // If error occured, now check if process doesn't exist.
        if( kill( phnd->pid, 0 ) < 0 ){ 
            if(errno == ESRCH){ // Process doesn't exist.
	    	    phnd->flags &= ~GRYLTHREAD_FLAG_ACTIVE; // Clear the active flag.
                    return 0; // Not running.
	        }
        }
        return 1; // Thread running.
    #endif

    return 0;
}


//==========================================================//
// - - - - - - - - - -  Mutex section  - - - - - - - - - - -//

/*! The Mutex private structure
 *  Holds the native handle to the OS-specific Mutex primitive.
 */
struct GThread_MutexPriv
{
    #if defined __WIN32
        CRITICAL_SECTION critSect;
        HANDLE hMutex;
    #elif defined __linux__
        pthread_mutex_t mtx;
    #endif
    int flags;
};

GrMutex gthread_Mutex_init(int flags)
{
    // Zero-initialize the structure. ZERO because when checking, ZERO means NonExistent.
    struct GThread_MutexPriv* mpv = (struct GThread_MutexPriv*) calloc( sizeof(struct GThread_MutexPriv), 1 );
    if(!mpv){
        hlogf("Calloc() failz0red!\n");
        return NULL;
    }
    mpv->flags = flags;

    #if defined __WIN32
        // If this mode is defined, we will use the Kernel-level mutex handle, which works across processes. 
        // If not, then only the process-specific CRITICAL_SECTION will be used.
        if( flags & GTHREAD_MUTEX_SHARED )
        {
            SECURITY_ATTRIBUTES attrs = { 0 };
            attrs.nLength = sizeof(SECURITY_ATTRIBUTES);
            attrs.lpSecurityDescriptor = NULL; // Access control specified by Default Access Token of this process.
            attrs.bInheritHandle = TRUE; // Child processes can inherit this mutex.
            
            mpv->hMutex = CreateMutex( &attrs, FALSE, NULL );
            if( !mpv->hMutex ){
                hlogf("gthread: CreateMutex() failz0red to initialize Win32 Mutex. ErrCode: 0x%p\n", GetLastError());
                free(mpv);
                return NULL;
            }
        }
        else{
            // This function never fails!
            InitializeCriticalSection( &(mpv->critSect) ); 
        }
        
    #elif defined __linux__
        res = pthread_mutex_init( &(mpv->mtx), NULL );
        if( res != 0 ){ // Error occur'd when initializing.
            hlogf("gthread: Error initializing Mutex (%d) !\n", res);
            free(mpv);
            return NULL;
        }
    #endif

    return (GrMutex) mpv;
}

/* Mutex must be UNOWNED when calling this.
 * If some threads are still owning a mutex, their behavior is undefined.
 */ 
void gthread_Mutex_destroy(GrMutex* mtx)
{
    if(!mtx || !*mtx) return;
    struct GThread_MutexPriv* mpv = (struct GThread_MutexPriv*)(*mtx);
    #if defined __WIN32
        // Free all resources of the structure.
        if( mpv->hMutex ){
            if( !CloseHandle( mpv->hMutex ) ) // If error, retval is NonZero
                hlogf("gthread: failed to CloseHandle() on mutex.\n");
        }
        else{ // If not hMutex, it's non-shared, use CRITICAL_SECTION. 
            DeleteCriticalSection( &(mpv->critSect) ); 
        }

    #elif defined __linux__
        if( pthread_mutex_destroy( &(mpv->mtx) ) != 0 ){
            hlogf("gthread: pthread_mutex_destroy() failed to destroy a mutex.\n");
    #endif
    // At the end, free the dynamically allocated private structure.
    free( (struct GThread_MutexPriv*)(*mtx) );
    *mtx = NULL;
}

// Returns 0 if lock acquired successfully, NonZero otherwise.
char gthread_Mutex_lock(GrMutex mtx)
{
    if(!mtx) return 1;
    #if defined __WIN32
        if( (((struct GThread_MutexPriv*)mtx)->flags) & GTHREAD_MUTEX_SHARED ){ // Lock hMUTEX
            // The WaitForSinleObject(), waits until the handle is signaled
            // If called on a Mutex, after waiting also takes Ownership of this mutex (Ackquires a lock).
            DWORD waitRes = WaitForSingleObject( ((struct GThread_MutexPriv*)mtx)->hMutex , INFINITE );
            if(waitRes == WAIT_FAILED){
                hlogf("gthread: Error on WaitForSingleObject() : 0x%p\n", GetLastError());
                return -1;
            }
        }
        else{ // Lock CRITICAL_SECTION
            EnterCriticalSection( &( ((struct GThread_MutexPriv*)mtx)->critSect ) );
        }

    #elif defined __linux__
        int res = pthread_mutex_lock( &( ((struct GThread_MutexPriv*)mtx)->mtx ) );
        if(res != 0){
            hlogf("gthread: Error locking mutex (%d)\n", res);
            return -1;
        }
    #endif
    return 0; // Lock acquired.
}

/* Returns:
 *  0, if Lock acquired successfully,
 *  1, if already Locked
 *  < 0, if Error occured.
 */ 
char gthread_Mutex_tryLock(GrMutex mtx)
{
    if(!mtx) return -3;
    #if defined __WIN32
        if( (((struct GThread_MutexPriv*)mtx)->flags) & GTHREAD_MUTEX_SHARED ){
            hlogf("gthread: TryLock can't be called on a Shared Win32 mutex. \n");
            return -2;
        }
        else{ // Lock CRITICAL_SECTION
            // If another thread already owns a mutex, returns ZERO.
            if(TryEnterCriticalSection( &( ((struct GThread_MutexPriv*)mtx)->critSect ) ) == 0 ){
                //hlogf("gthread: TryLock: mutex already locked.\n");
                return 1;
            }
        }

    #elif defined __linux__
        int res = pthread_mutex_trylock( &( ((struct GThread_MutexPriv*)mtx)->mtx ) );
        if(res != 0 && res != EBUSY){
            hlogf("gthread: Error locking mutex (%d)\n", res);
            return -1; // Error
        }
        else if(res == EBUSY){
            //hlogf("gthread: TryLock: mutex already locked.\n");
            return 1; // Already locked
        }

    #endif
    return 0; // Lock acquired.
}

/* Return:
 *  0 - succesfully unlocked
 *  NonZero - error occured.
 */  
char gthread_Mutex_unlock(GrMutex mtx)
{
    if(!mtx) return 1;
    #if defined __WIN32
        if( (((struct GThread_MutexPriv*)mtx)->flags) & GTHREAD_MUTEX_SHARED ){ // UnLock hMUTEX
            // If the function fails, the return value is ZERO.
            if( ReleaseMutex( ((struct GThread_MutexPriv*)mtx)->hMutex ) == 0 ){
                hlogf("gthread: Error on ReleaseMutex() : 0x%p\n", GetLastError());
                return -1;
            }
        }
        else{ // UnLock CRITICAL_SECTION
            LeaveCriticalSection( &( ((struct GThread_MutexPriv*)mtx)->critSect ) );
        }

    #elif defined __linux__
        int res = pthread_mutex_unlock( &( ((struct GThread_MutexPriv*)mtx)->mtx ) );
        if(res != 0){
            hlogf("gthread: Error pthread_unlocking mutex (%d)\n", res);
            return -1;
        }
    #endif
    return 0;
}


//==========================================================//
// - - - - - - - - -   CondVar section   - - - - - - - - - -//

/*! The Condition variable private structure
 *  Holds the native handle to the native cond.var primitive.
 */ 
struct GThread_CondVarPriv
{
    #if defined __WIN32
        CONDITION_VARIABLE cond;
    #elif defined __linux__
        pthread_cond_t cond;
    #endif
};

GrCondVar gthread_CondVar_init()
{
    // Zero-initialize the structure. ZERO because when checking, ZERO means NonExistent.
    struct GThread_CondVarPriv* mpv = (struct GThread_CondVarPriv*) calloc( sizeof(struct GThread_CondVarPriv), 1 );
    if(!mpv){
        hlogf("Calloc() failz0red!\n");
        return NULL;
    }

    #if defined __WIN32
        // Init a condVar. The function does not fail!
        InitializeConditionVariable( &(mpv->cond) );    

    #elif defined __linux__
        // Init with default attributes.
        res = pthread_cond_init( &(mpv->cond), NULL );
        if( res != 0 ){ // Error occur'd when initializing.
            hlogf("gthread: Error initializing pthread_CondVar (%d) !\n", res);
            free(mpv);
            return NULL;
        }
    #endif

    return (GrCondVar) mpv;
}

void gthread_CondVar_destroy(GrCondVar* cond)
{
    if(!cond || !*cond)
        return;
    struct GThread_CondVarPriv* mpv = (struct GThread_CondVarPriv*)(*cond);
    #if defined __WIN32
        // On Windows, we don't need to destroy a CondVar!!!
        // DEBUG@
        CRITICAL_SECTION crit;
        SleepConditionVariableCS( &(mpv->cond), &crit, INFINITE );     

    #elif defined __linux__
        // Init with default attributes.
        res = pthread_cond_destroy( &(mpv->cond) );
        if( res != 0 ){ // Error occur'd when initializing.
            hlogf("gthread: Error destroying pthread_CondVar (%d) !\n", res);
            return NULL;
        }
    #endif
    free( mpv );
}

char gthread_CondVar_wait(GrCondVar cond)
{
     
}

char gthread_CondVar_notify(GrCondVar cond)
{
    
}

void gthread_CondVar_notifyAll(GrCondVar cond)
{
    
}


//end.
