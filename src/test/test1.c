#include <grylthread.h>
#include <hlog.h>
#include <gmisc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*  This test demonstrates all the features of GrylloThread library:
 *  - Threads
 *  - Mutexes
 *  - Condition Variables
 *
 *  The background:
 *
 *  Main Thread creates Several worker threads, which:
 *    - performing PreProcessing on passed Data,
 *    - and start their main Job after being signaled.
 *    
 *  How it happens:
 *  
 *  1. Main Thread creates several Worker Threads, passing them some data on Creation.
 *
 *  2. Worker threads complete the PreProcessing jobs independently, and then all of them
 *     start waiting using a Condition Variable for the signal to start their main job.
 *
 *     However, as they PreProcess things, other stuff happens:
 *
 *  3. After thread creation, Main Thread starts waiting for the workers to complete PreProcessing,
 *     so that all workers would be on the Waiting State. 
 *
 *  4. For PreProcess state management, an Int - number of currently preprocessing threads,
 *     and a Condition Variable is used. 
 *
 *     As workers complete PreProcessing, they decrement the "Currently PreProcessing" counter, and
 *     Notify a CondVar main thread is waiting on.
 *
 *     The main thread Stops Waiting when the "Currently PreProcessing" counter becomes Zero.
 *
 *  5. When Main Thread wakes up, it sets the condition for Race Start, and 
 *     notifies All Workers to wake up.
 *
 *  6. Then all Workers wake up, and start performing their Main Job.  
 *
 */ 

/*  How Condition Variable waiting works:
 *
 *  1. The Wait function UNLOCKS the mutex (Disowns (un-acquires) the lock,
 *     and puts current thread to SLEEP simultaneously.
 *
 *     Then other threads can acquire a lock in the ABOVE statement (Mutex_lock(mutex)).
 *
 *  2. As the new thread acquires the lock, and no other threads can call the Wait function, 
 *     this thread can enter a sleep state too by calling Wait on that mutex.
 *
 *     When this happens, the mutex gets unlocked, and other threads can enter the waiting section.
 *
 *  3. The waiting threads (one or all) can be woken up when other thread calls "notify" 
 *     function on that lock.
 *
 *  4. When thread wakes up, it Re-Acquires the mutex lock it is waiting on, 
 *     so no other waiting threads could resume to the shared waiting section.
 *
 *  5. So, in summary:
 *     To start Waiting (enter a Sleep state), AND to Wake up, 
 *     THE THREAD NEEDS TO OWN THE MUTEX.
 *
 *     This is needed so that no threads could use the shared "waiting section" simultaneously.
 *
 *     The "waiting section" can be used by many things, associated with shared resource management.
 *
 *  6. To ensure the most efficient waiting, we must use a loop, because the condition not necessarily
 *     becomes true when the thread wakes up. 
 */

// Some constants
// The Param Protocol config
const int headerLen = sizeof(int)*3;
const size_t DataBufSiz = 100; 
const int loops = 20;

// How many workers will we spawn.
const int WorkerThreadCount = 4;

// The Synchronization Primitives
GrMutex mainJobMutex;
GrMutex preProcMutex;
GrCondVar mainJobCond;
GrCondVar preProcCond;

// Main Job start condition indicator.
char readyToRace = 0;

// The counter is set to the number of Worker Threads.
// When thread completes PreProcessing, it decrements the counter.
int preProcessingThreadCtr; 

void doShit(void* paramz0r)
{
    // Start PreProcessing stage
    if(!paramz0r)
        return;
    int id = *((int*)paramz0r);
    int count = *((int*)(paramz0r+sizeof(int)));
    int iterations = *((int*)(paramz0r+2*sizeof(int)));
    char* str0ng = (char*)(paramz0r+3*sizeof(int));

    printf("Thread %d: count: %d, iterations: %d\n", id, count, iterations);

    // Now, decrement working threads counter and signal 
    // the CondVar that we have completed the PreProcessing.
    preProcessingThreadCtr--;
    gthread_CondVar_notify(preProcCond);

    // Start the Waiting for Main Job stage
    // This is the mutex that protects the waiting section, since it is shared.
    gthread_Mutex_lock(mainJobMutex);

    while(!readyToRace) // Wait on the mutex
        gthread_CondVar_wait( mainJobCond, mainJobMutex );

    gthread_Mutex_unlock(mainJobMutex);

    // Start the Main Job stage
    // Print ID, and then payload, one by one, until a '\0' is reached.
    for(int j = 0; j<iterations; j++)
    {
        //printf("[%d] : ", id);
        for(char* i=str0ng; i!=0, i-str0ng < count; i++){
            printf("%c", *i);
        }
        printf("\n");
    }
}

void startThreads()
{
    // We set the condition to break the wait loop and notify all waiting threads to resume.
    readyToRace = 1;
    gthread_CondVar_notifyAll(mainJobCond);
}

int main(int argc, char** argv)
{
    hlogSetFile("gryltest1.log", HLOG_MODE_UNBUFFERED | HLOG_MODE_APPEND);

    // Print current time to a logger.
    hlogf("\n\n===============================\nGrylThread Test %s\n> > ", GTHREAD_VERSION);
    gmisc_PrintTimeByFormat( hlogGetFile(), NULL );
    hlogf("\n- - - - - - - - - - - - - - - - \n\n");

    // Init CondVars and Mutexes

    mainJobCond = gthread_CondVar_init();
    mainJobMutex = gthread_Mutex_init( 0 );

    preProcCond = gthread_CondVar_init();
    preProcMutex = gthread_Mutex_init( 0 );
    
    readyToRace = 0;
    preProcessingThreadCtr = WorkerThreadCount;

    GrThread WorkerPool[ WorkerThreadCount ];
    
    // Set thread parameters

    char data[DataBufSiz * WorkerThreadCount];
    char* payloadStart;
    
    for(int i=0; i<WorkerThreadCount; i++)
    {
        payloadStart = ( data + i*DataBufSiz ) + headerLen;  // Current Payload's Start position
        // Set payloads for different threads.
        if(i == 0)
            strncpy( payloadStart, "NyaaNyaa :3", DataBufSiz - headerLen );
        else if(i == 1)
            strncpy( payloadStart, "Kawaii~~ :3", DataBufSiz - headerLen );
        else if(i == 2)
            strncpy( payloadStart, "*.*.*.*.*.+", DataBufSiz - headerLen );
        else
            strncpy( payloadStart, "Desu desu~~", DataBufSiz - headerLen );

        payloadStart[ DataBufSiz - headerLen - 1 ] = 0; // Null-terminate in case of emergency.

        // Set Header Fields
        *((int*) ( data + i*DataBufSiz ))                  = i;     // Thread Id
        *((int*)(( data + i*DataBufSiz ) + sizeof(int)))   = (int)strlen(payloadStart); // payload lenght 
        *((int*)(( data + i*DataBufSiz ) + 2*sizeof(int))) = loops; // Number of loop iterations.
    }

    // Spawn all needed threads.

    for(int i=0; i < WorkerThreadCount; i++) {
        WorkerPool[ i ] = gthread_Thread_create( doShit, data + i*DataBufSiz );
    }

    // Now all threads are executing PreProcessing and then starting to wait on the mutex.
    // So it's time to wait until PreProc is completed.
    gthread_Mutex_lock(preProcMutex);

    while(preProcessingThreadCtr > 0) // While there still are PreProcessing threads.
        gthread_CondVar_wait(preProcCond, preProcMutex);

    gthread_Mutex_unlock(preProcMutex);

    // After all threads completed PreProcessing, we can release 
    // the lock above and signal the MainJob start
    printf("\n3 threads ready to race!\n\n");
    startThreads();

    // Join all threads safely after thay finish their work.
    for(int i=0; i < WorkerThreadCount; i++) {
        printf("[ Joining thread no %d ]\n", i); 
        gthread_Thread_join( WorkerPool[i], 1 ); // Join and destroy
    }

    // SuccSessFullY JoineD All ThreadS
    printf("\n[ ALL THREADS JOINED SUCCESSFULLY ]\n"); 

    // Perform cleanup.

    gthread_Mutex_destroy(&mainJobMutex);
    gthread_Mutex_destroy(&preProcMutex);

    gthread_CondVar_destroy(&mainJobCond);
    gthread_CondVar_destroy(&preProcCond);

    return 0;
}

