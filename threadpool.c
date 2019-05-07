#include <stdio.h>
#include "threadPool.h"

#define ERROR_ALLOCATION "Error in system call\n"
#define ERROR_SIZE 21
#define STDERR 2
#define EXIT_FAIL -1
#define SUCCESS 0
#define YES 1

#define THREADS_COUNT 4
#define TASKS_PER_THREAD 30
#define TASKS_PRI_THREAD 10
#define TP_WORKER_THREADS 3

#define DEBUG 1 // Change to 1 for debug info

pthread_mutex_t TasksDoneLock;
pthread_mutex_t TasksInsertedLock;

volatile int tasksDoneCount;
volatile int tasksInsertedCount;


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
    if (pthread_mutex_init(&(threadPool->lockQueue), NULL) != 0
        || pthread_cond_init(&(threadPool->notify), NULL) != 0) {
        error();
    }
    threadPool->isStopped = 0;
    threadPool->executeTasks = executeTasks;
    threadPool->numOfThreads = numOfThreads;
    threadPool->destory = NoDestroy;
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
    if (threadPool->isStopped) {
        return;
    }
    if (shouldWaitForTasks != 0) {
        threadPool->destory = waitAll;
        freeThreadPool(threadPool);
    } else {
        threadPool->destory = waitOnlyRunning;
        freeThreadPool(threadPool);
    }

}

void error() {
    write(STDERR, ERROR_ALLOCATION, ERROR_SIZE);
    exit(EXIT_FAIL);
}

void executeTasks(void *args) {
    ThreadPool *threadPool = (ThreadPool *) args;
    while (threadPool->isStopped == 0) {
        pthread_mutex_lock(&threadPool->lockQueue);
        if (threadPool->destory == NoDestroy) {
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

            if (threadPool->destory == waitAll) {
                completeTask(threadPool);
            } else if (threadPool->destory == waitOnlyRunning) {
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


void incTaskAdded() {
    pthread_mutex_lock(&TasksInsertedLock);
    tasksInsertedCount++;
    pthread_mutex_unlock(&TasksInsertedLock);
}

void incTaskDone() {
    pthread_mutex_lock(&TasksDoneLock);
    tasksDoneCount++;
    pthread_mutex_unlock(&TasksDoneLock);
}

int getCurrentThread() {
    return ((long) pthread_self() % 1000);
}


void task1(void *_) {
    int r;
    r = (rand() % 100) + 20;
    if (DEBUG) printf("TASK1 thread %d. sleeping %dms\n", (getCurrentThread()), r);
    usleep(r);
    incTaskDone();
}

void task2(void *_) {
    if (DEBUG) printf("TASK2 thread %d\n", getCurrentThread());
    incTaskDone();
}

void *poolDestoyer(void *arg) {
    ThreadPool *pool = (ThreadPool *) arg;
    tpDestroy(pool, 1);
    return NULL;
}

void *tasksAdder(void *arg) {
    ThreadPool *pool = (ThreadPool *) arg;
    int i;

    for (i = 0; i < TASKS_PER_THREAD; ++i) {
        if (!tpInsertTask(pool, task1, NULL)) {
            incTaskAdded();
        }
    }

    return NULL;
}

int shouldWaitForTasksTest(int shouldWait) {
    pthread_t t1[THREADS_COUNT];
    int i, j, result;
    ThreadPool *tp = tpCreate(TP_WORKER_THREADS);
    for (j = 0; j < THREADS_COUNT; j++) {
        pthread_create(&t1[j], NULL, tasksAdder, tp);
    }

    for (i = 0; i < TASKS_PRI_THREAD; i++) {
        if (!tpInsertTask(tp, task2, NULL)) {
            incTaskAdded();
        }
    }

    if (DEBUG) printf("-->tp will be destroyed!<--\n");
    tpDestroy(tp, shouldWait);
    if (DEBUG) printf("-->tp destroyed!<--\n");

    if (DEBUG) printf("waiting for other threads to end..\n");
    for (j = 0; j < THREADS_COUNT; j++) {
        pthread_join(t1[j], NULL);
    }
    pthread_mutex_lock(&TasksInsertedLock);
    pthread_mutex_lock(&TasksDoneLock);
    if (DEBUG) printf("\nSUMMRAY:\nTasks inserted:%d\nTasks done:%d\n", tasksInsertedCount, tasksDoneCount);
    if (DEBUG) printf("Graceful? %d\n", shouldWait);
    if ((shouldWait && tasksInsertedCount == tasksDoneCount) ||
        (!shouldWait && tasksInsertedCount != tasksDoneCount)) {
        result = 0;
    } else {
        result = 1;
    }

    tasksDoneCount = 0;
    tasksInsertedCount = 0;
    pthread_mutex_unlock(&TasksInsertedLock);
    pthread_mutex_unlock(&TasksDoneLock);

    return result;
}

int insertAfterDestroyTest() {
    ThreadPool *tp = tpCreate(TP_WORKER_THREADS);
    int i;
    usleep(50);
    for (i = 0; i < TASKS_PRI_THREAD; i++) {
        tpInsertTask(tp, task1, NULL);
    }
    tpDestroy(tp, 1);
    return !tpInsertTask(tp, task1, NULL);
}

int doubleDestroy() {
    pthread_t t1;
    ThreadPool *tp = tpCreate(TP_WORKER_THREADS);
    int i;
    for (i = 0; i < TASKS_PRI_THREAD; i++) {
        tpInsertTask(tp, task1, NULL);
    }
    printf("Going to destroy pool from 2 different threads...\n");
    pthread_create(&t1, NULL, poolDestoyer, tp);
    tpDestroy(tp, 1);
    pthread_join(t1, NULL);
    printf("Done, did anything break?\n");
    return 0;
}

int main() {
    srand(time(NULL));
    pthread_mutex_init(&TasksDoneLock, NULL);
    pthread_mutex_init(&TasksInsertedLock, NULL);
    tasksDoneCount = 0;
    tasksInsertedCount = 0;
    int i;
    printf("---Tester Running---\n");

    for (i = 0; i < 10; i++) {
        if (insertAfterDestroyTest())
            printf("Could insert task after tp destroyed!\n");
        if (shouldWaitForTasksTest(0))
            printf("Failed on shouldWaitForTasks = 0, tasks created = tasks done. This should rarely happen..\n");
        if (shouldWaitForTasksTest(1))
            printf("Failed on destroy with shouldWaitForTasks = 1. tasks created != tasks done\n");
    }

    doubleDestroy();

    printf("---Tester Done---\n");
    return 0;
}

