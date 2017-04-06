#ifndef GRYLTHREAD_H_INCLUDED
#define GRYLTHREAD_H_INCLUDED

typedef void *GrThreadHandle;

GrThreadHandle gthread_procToThread(void (*proc)(void*), void* param);
void gthread_joinThread(GrThreadHandle hnd);
char gthread_isThreadRunning(GrThreadHandle hnd);

void gthread_sleep(unsigned int millisecs);

#endif //GRYLTHREAD_H_INCLUDED
