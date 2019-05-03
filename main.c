#include <stdio.h>

#include "threadPool.h"

#define ERROR_ALLOCATION "Error in system call\n"
#define ERROR_SIZE 21
#define STDERR 2
#define EXIT_FAIL -1
#define SUCCESS 0


void error();

void *runTasks(void *args);

void executeTasks(void *args);

void completeTask(ThreadPool *th);


int main() {
    ThreadPool *tp = tpCreate(5);
    return 0;
}

ThreadPool *tpCreate(int numOfThreads) {
    ThreadPool *threadPool = (ThreadPool *) malloc(sizeof(ThreadPool));
    if (threadPool == NULL) {
        error();
    }
    threadPool->threads = (pthread_t *) malloc(sizeof(pthread_t) * numOfThreads);
    if (threadPool->threads == NULL) {
        error();
    }
    threadPool->osQueue = osCreateQueue();
    if (threadPool->osQueue == NULL) {
        error();
    }
    if (pthread_mutex_init(&(threadPool->lockQueue), NULL) != 0 || pthread_mutex_init(&(threadPool->stopLock), NULL) != 0
        || pthread_cond_init(&(threadPool->notify), NULL) != 0) {
        error();
    }
    threadPool->isStopped = 0;
    threadPool->executeTasks = executeTasks;
    threadPool->numOfThreads = numOfThreads;
    threadPool->destory = NoDestroy;
    int i;
    for (i = 0; i < numOfThreads; i++) {
        pthread_create(&threadPool->threads[i], NULL, ץ., threadPool);

    }
    return threadPool;
}

int tpInsertTask(ThreadPool *threadPool, void (*computeFunc)(void *), void *param) {
    if (threadPool->isStopped) {
        return EXIT_FAIL;
    }
    Task *task = (Task *) malloc(sizeof(task));
    if (task == NULL) {
        error();
    }
    task->func = computeFunc;
    task->information = param;
    pthread_mutex_lock(&threadPool->lockQueue);
    osEnqueue(threadPool->osQueue, task);
    if (pthread_cond_signal(&(threadPool->notify)) != 0) {
        error();
    }
    pthread_mutex_unlock(&threadPool->lockQueue);
    return SUCCESS;
}

void tpDestroy(ThreadPool *threadPool, int shouldWaitForTasks) {


}

void error() {
    write(STDERR, ERROR_ALLOCATION, ERROR_SIZE);
    exit(EXIT_FAIL);
}

void executeTasks(void *args) {
    ThreadPool *threadPool = (ThreadPool *) args;
    while (!threadPool->isStopped) {
        pthread_mutex_lock(&(threadPool->lockQueue));
        switch (Destory) {
            case NoDestroy:
                if (osIsQueueEmpty(threadPool->osQueue)) {
                    // handle busy waiting for task to be inserted
                    pthread_cond_wait(&(threadPool->notify), &(threadPool->lockQueue));
                    pthread_mutex_unlock(&(threadPool->lockQueue));
                } else {
                    completeTask(threadPool);
                }
                break;
            case Destory:
                if (osIsQueueEmpty(threadPool->osQueue)) {
                    pthread_mutex_unlock(&(threadPool->lockQueue));
                    break;
                } else {
                    if (threadPool->destory == waitAll) {
                        completeTask(threadPool);
                    } else if (threadPool->destory == waitOnlyRunning) {
                        pthread_mutex_unlock(&(threadPool->lockQueue));
                    }
                }
                break;
            default:
                break;
        }

    }

}

void *runTasks(void *args) {
    ThreadPool *threadPool = (ThreadPool *) args;
    threadPool->executeTasks(args);
}

void completeTask(ThreadPool *th) {
    Task *task = osDequeue(th->osQueue);
    pthread_mutex_unlock(&th->lockQueue);
    task->func(task->information);
    free(task);
}