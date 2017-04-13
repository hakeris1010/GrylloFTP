#ifndef GRYLTHREAD_H_INCLUDED
#define GRYLTHREAD_H_INCLUDED

/************************************************************  
 *         GrylloThreads C-Multithreading Framework         *
 *                ~ ~ ~ By GrylloTron ~ ~ ~                 *
 *                       Version 0.2                        *
 *        -  -  -  -  -  -  -  -  -  -  -  -  -  -  -       *      
 *                                                          *
 *  Features:                                               *
 *  - Cross-Platform concurrent execution framework         *
 *    - Currently supports POSIX and Win32                  *
 *  - Currently supported Native concurrency entities:      *
 *    - Thread                                              *
 *    - Mutex                                               * 
 *    - Condition Variable                                  *
 *                                                          *
 *  TODOS:
 *  - On POSIX, Ability to choose between pthreads or fork().
 *  - Extension to above, a Process_create() function.
 *  
 *  BUGS:
 *  - Must check bugs on POSIX.                             
 *                                                          *
 ***********************************************************/ 

#define GTHREAD_VERSION "v0.2"

/*! The typedef'd primitives
 *  Implementation is defined in their respective source files.
 */ 
typedef void *GrThread;
typedef void *GrMutex;
typedef void *GrCondVar;

/*! Specific attribute flags
 */
// If set, mutex will be shared among processes.
#define GTHREAD_MUTEX_SHARED  1

/*! Threading functions
 *  Supports creation, checking if running and joining.
 */ 
GrThread gthread_Thread_create(void (*proc)(void*), void* param);
void gthread_Thread_join(GrThread hnd);
char gthread_Thread_isRunning(GrThread hnd);

void gthread_Thread_sleep(unsigned int millisecs);

/*! Mutex functions.
 *  Allow all basic mutex operations.
 */ 
GrMutex gthread_Mutex_init(int attribs);
void gthread_Mutex_destroy(GrMutex* mtx);

char gthread_Mutex_lock(GrMutex mtx);
char gthread_Mutex_tryLock(GrMutex mtx);
char gthread_Mutex_unlock(GrMutex mtx);

/*! CondVar functions
 */ 
GrCondVar gthread_CondVar_init();
void gthread_CondVar_destroy(GrCondVar* cond);

char gthread_CondVar_wait(GrCondVar cond, GrMutex mtp);
char gthread_CondVar_wait_time(GrCondVar cond, GrMutex mtp, long millisec);
void gthread_CondVar_notify(GrCondVar cond);
void gthread_CondVar_notifyAll(GrCondVar cond);


#endif //GRYLTHREAD_H_INCLUDED
