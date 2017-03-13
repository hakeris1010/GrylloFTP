#ifndef GRYLTHREAD_H_INCLUDED
#define GRYLTHREAD_H_INCLUDED

typedef void *GrThreadHandle;

GrThreadHandle procToThread(void (*proc)(void*), void* param);
void joinThread(GrThreadHandle hnd);

void sleep(unsigned int millisecs);

#endif //GRYLTHREAD_H_INCLUDED
