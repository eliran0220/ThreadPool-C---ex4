#include <stdio.h>
#include "threadPool.h"

#define ERROR_MESSAGE "Error in system call\n"
#define ERROR_SIZE 21
#define STDERR 2
#define EXIT_FAIL -1
#define SUCCESS 0
#define YES 1
#define NO 0
#define TWIECE_DESTROYED_ERROR "Error - destroyed pool twice from same call\n"
#define TWICE_ERROR_SIZE 44

pthread_mutex_t lockThePool;

void hello (void* a)
{
    printf("hello\n");
}


void test_thread_pool_sanity()
{
    int i;

    ThreadPool* tp = tpCreate(5);

    for(i=0; i<5; ++i)
    {
        tpInsertTask(tp,hello,NULL);
    }

    tpDestroy(tp,1);
}



void errorTwiceDestroyed ();

void error();

void *runTasks(void *args);

void executeTasks(void *args);

void completeTask(ThreadPool *th);

void freeThreadPool(ThreadPool *threadPool);


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
    if (pthread_mutex_init(&(threadPool->lockQueue), NULL) != 0 ||
    pthread_cond_init(&(threadPool->notify), NULL) != 0 ||
    pthread_mutex_init(&(lockThePool), NULL) != 0) {
        error();
    }
    threadPool->isStopped = NO;
    threadPool->executeTasks = executeTasks;
    threadPool->numOfThreads = numOfThreads;
    threadPool->destroy = NoDestroy;
    int i;
    for (i = 0; i < numOfThreads; i++) {
        pthread_create(&threadPool->threads[i], NULL, runTasks, threadPool);

    }
    return threadPool;
}

int tpInsertTask(ThreadPool *threadPool, void (*computeFunc)(void *), void *param) {
    if (threadPool->isStopped) {
        return EXIT_FAIL;
    }
    Task *task = (Task *) malloc(sizeof(Task));
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
    pthread_mutex_lock(&lockThePool);
    if (threadPool->isStopped) {
        pthread_mutex_destroy(&lockThePool);
        errorTwiceDestroyed();
    }
    if (shouldWaitForTasks != NO) {
        threadPool->destroy = waitAll;
        freeThreadPool(threadPool);
    } else {
        threadPool->destroy = waitOnlyRunning;
        freeThreadPool(threadPool);
    }
    pthread_mutex_unlock(&lockThePool);
}

void error() {
    write(STDERR, ERROR_MESSAGE, ERROR_SIZE);
    exit(EXIT_FAIL);
}

void errorTwiceDestroyed () {
    write(STDERR,TWIECE_DESTROYED_ERROR,TWICE_ERROR_SIZE);
    exit(EXIT_FAIL);
}


void executeTasks(void *args) {
    ThreadPool *threadPool = (ThreadPool *) args;
    while (threadPool->isStopped == NO) {
        pthread_mutex_lock(&threadPool->lockQueue);
        if (threadPool->destroy == NoDestroy) {
            if (osIsQueueEmpty(threadPool->osQueue)) {
                // handle busy waiting for task to be inserted
                pthread_cond_wait(&(threadPool->notify), &(threadPool->lockQueue));
                pthread_mutex_unlock(&(threadPool->lockQueue));
            } else {
                completeTask(threadPool);
            }
        } else {
            // destroy state
            if (osIsQueueEmpty(threadPool->osQueue)) {
                pthread_mutex_unlock(&(threadPool->lockQueue));
                break;
            }

            if (threadPool->destroy == waitAll) {
                completeTask(threadPool);
            } else if (threadPool->destroy == waitOnlyRunning) {
                pthread_mutex_unlock(&threadPool->lockQueue);
                break;
            }

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

void freeThreadPool(ThreadPool *threadPool) {
    int i;
    pthread_mutex_lock(&(threadPool->lockQueue));
    pthread_cond_broadcast(&(threadPool->notify));
    pthread_mutex_unlock(&(threadPool->lockQueue));

    for (i = 0; i < threadPool->numOfThreads; i++) {
        pthread_join(threadPool->threads[i], NULL);
    }
    //stopped threads set to true
    threadPool->isStopped = YES;
    while (!osIsQueueEmpty(threadPool->osQueue)) {
        Task *task = osDequeue(threadPool->osQueue);
        free(task);
    }
    osDestroyQueue(threadPool->osQueue);
    //destroy mutexes and cond
    pthread_mutex_destroy(&(threadPool->lockQueue));
    pthread_cond_destroy(&(threadPool->notify));

    //destroy threads array
    free(threadPool->threads);
    //destroy thread pool
    free(threadPool);
}


int main() {
    test_thread_pool_sanity();
    return 0;
}

