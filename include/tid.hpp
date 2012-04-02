#ifndef THREAD_ID
#define THREAD_ID

//Thread local id
//Note: __thread is GCC specific
static __thread unsigned int thread_num;

#endif
