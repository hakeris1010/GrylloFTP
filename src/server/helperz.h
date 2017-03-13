#ifndef HELPERZ_H_INCLUDED
#define HELPERZ_H_INCLUDED

typedef void *GrThreadHandle;

GrThreadHandle procToThread(void (*proc)(void*), void* param);
void joinThread(GrThreadHandle hnd);

void sleep(unsigned int millisecs);


#endif
