#include "grylthread.h"
#include "systemcheck.h"

// Include OS-specific needed headers
#if defined _GRYLTOOL_WIN32
    // If Win32 also define the required Windows version
    // Windows Vista (0x0600) - required for full functionality.
    #define _WIN32_WINNT 0x0600 

    #include <windows.h>
    #include <WinBase.h>

#elif defined _GRYLTOOL_POSIX
    #include <sys/types.h>
    #include <sys/wait.h>
    #include <unistd.h>
    #include <errno.h>
    #include <signal.h>
    #include <pthread.h>
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
#define GRYLTHREAD_FLAG_ACTIVE      1
#define GRYLTHREAD_FLAG_DETACHED    2


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

struct ThreadHandlePriv
{
    #if defined _GRYLTOOL_WIN32
        HANDLE hThread; 
    #elif defined _GRYLTOOL_POSIX
        pthread_t tid;
        pthread_attr_t attribs;
    #endif
    volatile char flags;
    volatile long threadID;
    GrMutex flagtex; // Thread-Safety guarantee'd.
};

struct ThreadFuncAttribs
{
	void (*proc)(void*);
	void* param;
    struct ThreadHandlePriv* threadInfo;
};


// Win32 Threading procedure
#if defined _GRYLTOOL_WIN32
DWORD WINAPI ThreadProc( LPVOID lpParameter )
{
    // Now retrieve the pointer to procedure and call it.
    struct ThreadFuncAttribs* attrs = (struct ThreadFuncAttribs*)lpParameter;
    attrs->proc( attrs->param );

    // Cleanup after the proc returns - the structure ThreadFuncAttribs is malloc'd on the heap, 
    // so we must free it. The pointer belongs only to this thread at this moment.
    free(attrs);
    return 0;
}
#endif

// POSIX threading procedures. In POSIX there is no direct mechanism for checking 
// if thread has terminated, so we must use flags.
#if defined _GRYLTOOL_POSIX

// A POSIX Cleanup Handler, setting the ACTIVE flag to false when thread exits;
void pEndFlagSetProc( void* param )
{
    struct ThreadFuncAttribs* attrs = (struct ThreadFuncAttribs*)param;

    // Set the flag to InActive, protected by Mutex.
    gthread_Mutex_lock( attrs->threadInfo->flagtex );
    (attrs->threadInfo->flags) &= ~GRYLTHREAD_FLAG_ACTIVE;
    gthread_Mutex_unlock( attrs->threadInfo->flagtex );

    // It's curren't thread's responsibility to free the memory used by ThreadFuncAttribs parameter.
    free(attrs); // Gets called no matter what, because it's a Cleanup Handler.
}

// Function must return the State of the thread, which later could be got into a variable.
void* pThreadProc( void* param )
{
    // Retrieve the pointer to procedure and parameters.
    struct ThreadFuncAttribs* attrs = (struct ThreadFuncAttribs*)param;
    // Push the cleanup handler which must be executed when thread exits.
    pthread_cleanup_push( &pEndFlagSetProc, (void*)attrs );

    // Set the thread-specific variables, with thread-safety.
    gthread_Mutex_lock( attrs->threadInfo->flagtex );
    attrs->threadInfo->threadID = (long)gettid();

    gthread_Mutex_unlock( attrs->threadInfo->flagtex );

    // Actually invoke the procedure.
    attrs->proc( attrs->param );

    // When thread returns, attrs will be cleaned automatically in the pEndFlagSetProc().
    // But the cleanup_pop() must be here because of the macro nature of these functions.
    pthread_cleanup_pop(1);
    return NULL; // Status of returning  -  null.
}
#endif

// Actual Threading API implementation.
// Private function definitions
static void gthread_Thread_terminate_priv( struct ThreadHandlePriv* pv );

// API Public funcs

GrThread gthread_Thread_create(void (*proc)(void*), void* param)
{
    struct ThreadHandlePriv* thread_id = calloc( 1, sizeof(struct ThreadHandlePriv) );
    struct ThreadFuncAttribs* attr = malloc( sizeof(struct ThreadFuncAttribs) );
    if(!thread_id || !attr){
        hlogf("gthread: ERROR on malloc()...\n");
        return NULL;
    }

	attr->proc = proc;
    attr->param = param;
    attr->threadInfo = thread_id;

    // Set the "active" flag on thread.	If error occurs on creation, this memory will just be "free'd"
    thread_id->flags |= GRYLTHREAD_FLAG_ACTIVE;	
    // (Maybe UnNecessary) Init the mutex which protects the FlagVar 
    thread_id->flagtex = gthread_Mutex_init(0);

    int errr = 0;
    // OS-specific code
    #if defined _GRYLTOOL_WIN32
        HANDLE h = CreateThread(NULL, 0, ThreadProc, (void*)attr, 0, NULL);
        if(h)
            thread_id->hThread = h;
        else{ // h == NULL, error occured.
            hlogf("gthread: ERROR when creating Win32 thread: 0x%0x\n", GetLastError());
            errr = 1;
        }

    #elif defined _GRYLTOOL_POSIX
        // Create with the DeFaUlt attribs
        pthread_attr_init( &(thread_id->attribs) );
        int res = pthread_create( &(thread_id->tid), &(thread_id->attribs), pThreadProc, (void*)attr );
        if( res != 0 ){ // Error OccurEd.
            hlogf("gthread: ERROR on pthread_create() : %d\n", res);
            errr = 1;
            pthread_attr_destroy( &(thread_id->attribs) );
        }
    #endif

    if( errr ){ // Error occured
        free( thread_id );
        free( attr );
        return NULL;
    }

    return (GrThread)thread_id;
}

void gthread_Thread_destroy(GrThread hnd)
{
    struct ThreadHandlePriv* pv = (struct ThreadHandlePriv*)hnd;
    gthread_Mutex_lock( pv->flagtex );

    // If thread hasn't been joined (is still active) and hasn't been detached, terminate.
    if( !(pv->flags & GRYLTHREAD_FLAG_DETACHED) /* && isRunning(pv) */ )
        gthread_Thread_terminate_priv( pv ); // Thread's active state will be checked in this function.
    
    // destroy the no-more-needed thread attribs and close Win32 handles.
    #if defined _GRYLTOOL_WIN32
        CloseHandle( pv->hThread ); 
    #elif defined _GRYLTOOL_POSIX
        pthread_attr_destroy( &(pv->attribs) );
    #endif

    // Unlock and Destroy that mutex.
    gthread_Mutex_unlock( pv->flagtex );
    gthread_Mutex_destroy( &(pv->flagtex) );

    // Now thread is no longer running, we can free it's handle.
    free( (struct ThreadHandlePriv*)hnd );
}

void gthread_Thread_join(GrThread hnd, char destroy)
{
    struct ThreadHandlePriv* pv = (struct ThreadHandlePriv*)hnd;
    gthread_Mutex_lock( pv->flagtex );
    
    // Here we assume that thread is joinable and don't check.
    // It's programmer's responsibility to keep track of which threads are joinable, after all.
    #if defined _GRYLTOOL_WIN32
        WaitForSingleObject( pv->hThread, INFINITE );
    #elif defined _GRYLTOOL_POSIX
        int res = pthread_join( pv->tid , NULL );
        if( res != 0 )
            hlogf("gthread: ERROR on pthread_join() : %s\n", strerror(res));
    #endif

    // Unlock the mutex, and call destroy() if specified.
    gthread_Mutex_unlock( pv->flagtex );

    if(destroy)
        gthread_Thread_destroy( hnd );
}

/* Function checks if thread is still running, and returns:
 *  0 - Thread has terminated.
 *  NoZero - Thread is still running.
 */ 
char gthread_Thread_isRunning(GrThread hnd)
{ 
    if(!hnd) return 0;
    char retval = 0; // Return value. Default - not running (0).
    struct ThreadHandlePriv* phnd = (struct ThreadHandlePriv*)hnd;
    gthread_Mutex_lock( phnd->flagtex );

    if((phnd->flags) & GRYLTHREAD_FLAG_ACTIVE) // If "Active" flag is set...
    {    
        #if defined _GRYLTOOL_WIN32
            DWORD status;
            if( GetExitCodeThread( phnd->hThread, &status ) ){
                if(status == STILL_ACTIVE)
                    retval = 1; // Still running!
                else // Not running
                    phnd->flags &= ~GRYLTHREAD_FLAG_ACTIVE; // Clear the active flag.
            }
            else // Error occured
                hlogf("gthread: ERROR: GetExitCodeThread() failed: 0x%0x\n", GetLastError());

        #elif defined _GRYLTOOL_POSIX
            // On POSIX, the GRYLTHREAD_FLAG_ACTIVE is automatically cleared by the
            // Thread Cleanup Handler (set by us) when thread terminates.
            // ---
            // So if ACTIVE flag is set, it means thread is actually running now.
            retval = 1; 
        #endif
    }

    gthread_Mutex_unlock( phnd->flagtex );
    return retval;
}

// TODO: Define a model of joinable thread:
//       Rely on Flags Only, or Rely on Implementation???
//
// Private function, assumes that lock is already acquired.
char gthread_Thread_isJoinable_priv(struct ThreadHandlePriv* prv)
{
    #if defined _GRYLTOOL_WIN32
        if( (prv->flags) & GRYLTHREAD_FLAG_DETACHED )
            return 0; // Detached --> non-joinable.
    #elif defined _GRYLTOOL_POSIX
        // TODO: Check if this works on a real POSIX environment.
        // We can also do this by just checking the flag.
        int joinState;
        pthread_attr_getdetachstate( &(prv->attribs), &joinState );
        if(joinState != PTHREAD_CREATE_JOINABLE)
            return 0;
    #endif
    return 1; // Joinable
}

char gthread_Thread_isJoinable(GrThread hnd)
{
    /*gthread_Mutex_lock( ((struct ThreadHandlePriv*)hnd)->flagtex );
    gthread_Thread_isJoinable_priv( (struct ThreadHandlePriv*)hnd );

    gthread_Mutex_unlock( ((struct ThreadHandlePriv*)hnd)->flagtex );*/
    
    //TODO: Now we just use the flag. (If not detached --> joinable)
    return !( (((struct ThreadHandlePriv*)hnd)->flags) & GRYLTHREAD_FLAG_DETACHED );
}

void gthread_Thread_detach(GrThread hnd)
{
    gthread_Mutex_lock( ((struct ThreadHandlePriv*)hnd)->flagtex );
    if( !( (((struct ThreadHandlePriv*)hnd)->flags) & GRYLTHREAD_FLAG_DETACHED) ) // If Not yet DeTaCheD 
    {
        #if defined _GRYLTOOL_WIN32
            // No mechanism to detach on Win32. We just set the flag.
        #elif defined _GRYLTOOL_POSIX
            int res = pthread_detach( ((struct ThreadHandlePriv*)hnd)->tid ); 
            if( res != 0 )
                hlogf("gthread: ERROR on pthread_detach() : %s\n", strerror(res));
        #endif
        ((struct ThreadHandlePriv*)hnd)->flags |= GRYLTHREAD_FLAG_DETACHED; 
    }
    gthread_Mutex_unlock( ((struct ThreadHandlePriv*)hnd)->flagtex );
}

// Private function, Lock is Acquired.
static void gthread_Thread_terminate_priv( struct ThreadHandlePriv* pv )
{
    #if defined _GRYLTOOL_WIN32
        TerminateThread( pv->hThread, 0 ); 
    #elif defined _GRYLTOOL_POSIX
        // TODO: We should probably use pthread_kill(), after installing signal handler on the specified thread.
        // It is known that pthread_cancel() terminates thread only after thread calls a SysCall which is a
        // Cancellation Point. This should not work immediatly.
        pthread_cancel( pv->tid );

        // After Cancellation we could join. Wait until the thread closes itself (it's optional).
    #endif
    // Set the activity flag to false.
    (pv->flags) &= ~GRYLTHREAD_FLAG_ACTIVE;
}

void gthread_Thread_terminate(GrThread hnd)
{
    gthread_Mutex_lock( ((struct ThreadHandlePriv*)hnd)->flagtex );
    gthread_Thread_terminate_priv( (struct ThreadHandlePriv*)hnd );

    gthread_Mutex_unlock( ((struct ThreadHandlePriv*)hnd)->flagtex );
}

void gthread_Thread_exit()
{
    #if defined _GRYLTOOL_WIN32
        ExitThread(0);
    #elif defined _GRYLTOOL_POSIX
        pthread_exit(NULL);
    #endif
}

// Other Thread funcs

void gthread_Thread_sleep(unsigned int millisecs)
{
    #if defined _GRYLTOOL_WIN32
        Sleep(millisecs);
    #elif defined _GRYLTOOL_POSIX
        sleep(millisecs);
    #endif
}

long gthread_Thread_getID(GrThread hnd)
{
    #if defined _GRYLTOOL_WIN32
        if(hnd) // Return specified pid
            return (long) GetThreadId( ((struct ThreadHandlePriv*)hnd)->hThread );    
        return (long) GetCurrentThreadId(); // Else, return current pid.

    #elif defined _GRYLTOOL_POSIX
        if(hnd) 
            return (long)( ((struct ThreadHandlePriv*)hnd)->threadID );
        return (long) gettid(); // This actually returns a TID.
    #endif
    return 0;
}

char gthread_Thread_equal(GrThread t1, GrThread t2)
{
    if(!t1 || !t2) return 0;
    #if defined _GRYLTOOL_WIN32
        return ( gthread_Thread_getID(t1) == gthread_Thread_getID(t2) );
    #elif defined _GRYLTOOL_POSIX
        return (char)pthread_equal( ((struct ThreadHandlePriv*)t1)->tid,
                                    ((struct ThreadHandlePriv*)t2)->tid );    
    #endif
}


//==========================================================//
// - - - - - - - - -   Process section   - - - - - - - - - -//

// Process structure
struct GThread_ProcessHandlePriv
{
    #if defined _GRYLTOOL_WIN32
        HANDLE hProcess;
    #elif defined _GRYLTOOL_POSIX
        pid_t pid;
    #endif
    volatile char flags;
    GrMutex flagtex; 
};

/* Process Functions.
 *  Allow process creation, joining, exitting, and Pid-operations.
 */ 
GrProcess gthread_Process_create(const char* pathToFile, const char* commandLine)
{
     ;
}

GrProcess gthread_Process_fork(void (*proc)(void*), void* param)
{
    struct GThread_ProcessHandlePriv* procHand = NULL;

    #if defined _GRYLTOOL_WIN32
        // TODO:
        hlogf("gthread: ERROR: Fork()'ing on Windows is Not (yet) Supported!\n");
        return NULL;
        
    #elif defined _GRYLTOOL_POSIX
        // Call fork - spawn process.
        // On a child process execution resumes after FORK,
        // For a child fork() returns 0, and for the original, returns 0 on good, < 0 on error.
        pid_t pid = fork();

        if(pid == 0){ // Child process
            proc(param); // Call the client servicer function
        }
        else if(pid > 0){ // Parent and no error
            procHand = malloc(sizeof(struct GThread_ProcessHandlePriv));
            procHand->pid = pid;
            // Maybe initialize the mutex (if we'll ever use it).
        }
    #endif

    return (GrProcess) procHand;
}

void gthread_Process_join(GrProcess hnd)
{
    if(!hnd) return;
    if( gthread_Process_isRunning(hnd) )
    {
        #if defined _GRYLTOOL_WIN32
            WaitForSingleObject( ((struct GThread_ProcessHandlePriv*)hnd)->hProcess, INFINITE );
            CloseHandle( ((struct GThread_ProcessHandlePriv*)hnd)->hProcess ); // Close native handle (OS recomendation)
     
        #elif defined _GRYLTOOL_POSIX
            // Just wait for termination of the PID.
            int res = waitpid( ((struct GThread_ProcessHandlePriv*)hnd)->pid, NULL, 0 );
            if( res < 0 ) // Error
                hlogf("gthread: ERROR on waitpid(): %s\n", strerror(res));
        #endif
    }
    free( (struct GThread_ProcessHandlePriv*)hnd );
}

char gthread_Process_isRunning(GrProcess hnd)
{
    if(!hnd) return 0;
    struct GThread_ProcessHandlePriv* phnd = (struct GThread_ProcessHandlePriv*)hnd;
    if(! ((phnd->flags) & GRYLTHREAD_FLAG_ACTIVE)) // Active flag is not set.
        return 0;

    #if defined _GRYLTOOL_WIN32
        DWORD status;
        if( GetExitCodeProcess( phnd->hProcess, &status ) ){
            if(status == STILL_ACTIVE)
                return 1; // Still running!
            // Not running
	        phnd->flags &= ~GRYLTHREAD_FLAG_ACTIVE; // Clear the active flag.
        }
        else // Error occured
            hlogf("gthread: ERROR: GetExitCodeProcess() failed: 0x%0x\n", GetLastError());

    #elif defined _GRYLTOOL_POSIX
        // Here we use kill (send signal to process), with signal as 0 - don't send, just check process state.
        // If error occured, now check if process doesn't exist.
        if( kill( phnd->pid, 0 ) < 0 ){ 
            if(errno == ESRCH){ // Process doesn't exist.
	    	    phnd->flags &= ~GRYLTHREAD_FLAG_ACTIVE; // Clear the active flag.
                return 0; // Not running.
	        }
        }
        return 1; // Process running.
    #endif

    return 0;
}

long gthread_Process_getID(GrProcess hnd)
{
    #if defined _GRYLTOOL_WIN32
        if(hnd) // Return specified pid
            return (long) GetProcessId( ((struct GThread_ProcessHandlePriv*)hnd)->hProcess );    
        return (long) GetCurrentProcessId(); // Else, return current pid.

    #elif defined _GRYLTOOL_POSIX
        if(hnd)
            return (long)( ((struct GThread_ProcessHandlePriv*)hnd)->pid );
        return (long) getpid();
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
    #if defined _GRYLTOOL_WIN32
        CRITICAL_SECTION critSect;
        HANDLE hMutex;
    #elif defined _GRYLTOOL_POSIX
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

    #if defined _GRYLTOOL_WIN32
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
                hlogf("gthread: CreateMutex() failz0red to initialize Win32 Mutex. ErrCode: 0x%0x\n", GetLastError());
                free(mpv);
                return NULL;
            }
        }
        else{
            // This function never fails!
            InitializeCriticalSection( &(mpv->critSect) ); 
        }
        
    #elif defined _GRYLTOOL_POSIX
        int res = pthread_mutex_init( &(mpv->mtx), NULL );
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
    #if defined _GRYLTOOL_WIN32
        // Free all resources of the structure.
        if( mpv->hMutex ){
            if( !CloseHandle( mpv->hMutex ) ) // If error, retval is NonZero
                hlogf("gthread: failed to CloseHandle() on mutex.\n");
        }
        else{ // If not hMutex, it's non-shared, use CRITICAL_SECTION. 
            DeleteCriticalSection( &(mpv->critSect) ); 
        }

    #elif defined _GRYLTOOL_POSIX
        if( pthread_mutex_destroy( &(mpv->mtx) ) != 0 )
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
    #if defined _GRYLTOOL_WIN32
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

    #elif defined _GRYLTOOL_POSIX
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
    #if defined _GRYLTOOL_WIN32
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

    #elif defined _GRYLTOOL_POSIX
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
    #if defined _GRYLTOOL_WIN32
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

    #elif defined _GRYLTOOL_POSIX
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
    #if defined _GRYLTOOL_WIN32
        CONDITION_VARIABLE cond;
    #elif defined _GRYLTOOL_POSIX
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

    #if defined _GRYLTOOL_WIN32
        // Init a condVar. The function does not fail!
        InitializeConditionVariable( &(mpv->cond) );    

    #elif defined _GRYLTOOL_POSIX
        // Init with default attributes.
        int res = pthread_cond_init( &(mpv->cond), NULL );
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
    #if defined _GRYLTOOL_WIN32
        // On Windows, we don't need to destroy a CondVar!!!

    #elif defined _GRYLTOOL_POSIX
        // Init with default attributes.
        int res = pthread_cond_destroy( &(mpv->cond) );
        if( res != 0 ){ // Error occur'd when initializing.
            hlogf("gthread: Error destroying pthread_CondVar (%s) !\n", strerror(res));
        }
    #endif
    free( mpv );
}

char gthread_CondVar_wait_time(GrCondVar cond, GrMutex mutex, long millisec)
{
    if(!cond || !mutex) return -2;
    struct GThread_CondVarPriv* cvp = (struct GThread_CondVarPriv*)cond;
    struct GThread_MutexPriv* mtp = (struct GThread_MutexPriv*)mutex;
    #if defined _GRYLTOOL_WIN32
        if( (mtp->flags) & GTHREAD_MUTEX_SHARED ){ // Only unshared mutex can be used to wait 
            hlogf("gthread: ERROR: CondVar can only wait on a non-shared Windows mutex (CRITICAL_SECTION)\n");
            return -3;
        }
            
        if(SleepConditionVariableCS( &(cvp->cond), &(mtp->critSect), (DWORD)millisec ) == 0)
        {
            DWORD error = GetLastError();
            if(error != ERROR_TIMEOUT){ // Actual error happened.
                hlogf("gthread: SleepConditionVariableCS() returned Error: 0x%0x\n", error);
                return -1; // Error occur'd
            }
            return 1; // Timeout
        }

    #elif defined _GRYLTOOL_POSIX
        struct timespec tims;
        tims.tv_sec = millisec / 1000; // Seconds
        tims.tv_nsec = (millisec % 1000) * 1000; // Nanoseconds

        int res = pthread_cond_timedwait( &(cvp->cond), &(mtp->mtx), &tims );
        if( res != 0 ){
            if( res == ETIMEDOUT ) // Timeout occured.
                return 1; // Timeout
            hlogf("gthread: ERROR when trying to pthread_cond_timedwait() : %d\n", res);
            return -1;
        }
    #endif
    return 0; // Wait successful, variable is signaled.
}

char gthread_CondVar_wait(GrCondVar cond, GrMutex mtp)
{
    if(!cond || !mtp) return -2;
    #if defined _GRYLTOOL_WIN32
        return gthread_CondVar_wait_time(cond, mtp, INFINITE);

    #elif defined _GRYLTOOL_POSIX
        int res = pthread_cond_wait( &(((struct GThread_CondVarPriv*)cond)->cond),
			             &(((struct GThread_MutexPriv*)mtp)->mtx) );
        if( res != 0 ){
            hlogf("gthread: ERROR when trying to pthread_cond_wait() : %d\n", res);
            return -1; // Only error can occur, no timeout.
        }
    #endif
    return 0; // Wait successful, variable is signaled.
}

void gthread_CondVar_notify(GrCondVar cond)
{
    if(!cond) return;
    #if defined _GRYLTOOL_WIN32
        WakeConditionVariable( &( ((struct GThread_CondVarPriv*)cond)->cond ) );

    #elif defined _GRYLTOOL_POSIX
        int res = pthread_cond_signal( &( ((struct GThread_CondVarPriv*)cond)->cond ) );
        if( res != 0 ){
            hlogf("gthread: ERROR when trying to pthread_cond_signal() : %d\n", res);
        }
    #endif
}

void gthread_CondVar_notifyAll(GrCondVar cond)
{
    if(!cond) return;
    #if defined _GRYLTOOL_WIN32
        WakeAllConditionVariable( &( ((struct GThread_CondVarPriv*)cond)->cond ) );

    #elif defined _GRYLTOOL_POSIX
        int res = pthread_cond_broadcast( &( ((struct GThread_CondVarPriv*)cond)->cond ) );
        if( res != 0 ){
            hlogf("gthread: ERROR when trying to pthread_cond_signal() : %d\n", res);
        }
    #endif
}

//end.
