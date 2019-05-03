#ifndef __THREAD_POOL__
#define __THREAD_POOL__

#include <malloc.h>
#include <unistd.h>
#include <stdlib.h>
#include "osqueue.h"
#include <pthread.h>

typedef struct Task_t {
    void (*func) (void *);
    void * information;
}Task;

enum destoryState {NoDestroy,Destory,waitAll,waitOnlyRunning};

typedef struct thread_pool {
    pthread_t *threads;
    int numOfThreads;
    int isStopped;
    pthread_mutex_t stopLock;
    pthread_mutex_t lockQueue;
    pthread_cond_t notify;
    enum destoryState destory;
    void (*executeTasks)(void *args);
    OSQueue *osQueue;

} ThreadPool;

ThreadPool* tpCreate(int numOfThreads);

void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks);

int tpInsertTask(ThreadPool* threadPool, void (*computeFunc) (void *), void* param);

#endif
