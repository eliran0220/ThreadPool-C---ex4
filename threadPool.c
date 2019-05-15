/*
 * Eliran Darshan
 * 311451322
 * 07
 */

#include "threadPool.h"

#define ERROR_MESSAGE "Error in system call\n"
#define DESTROYED_TWICE "TRYING TO DESTROY POOL TWICE!\n"
#define ERROR_SIZE 21
#define ERROR_TWICE 31
#define STDERR 2
#define EXIT_FAIL -1
#define ERROR -1
#define SUCCESS 0
#define YES 1
#define NO 0

/*
 * Function name:displayError
 * The input: none
 * The output: void
 * The function operation: the function displays an error message when
 * an error in system call occured, and exists the program.
 */
void displayError();

/*
 * Function name:twiceDestroyMsg
 * The input: none
 * The output: void
 * The function operation: the function displays an error message when
 * the threadpool was destroyed twice.
 */
void twiceDestroyMsg();

/*
 * Function name:runTasks
 * The input: *args
 * The output: void
 * The function operation: the function is given as a parameter to every
 * pthread that was created, which runs the executeTask function
 */
void *runTasks(void *args);

/*
 * Function name:executeTask
 * The input: *args
 * The output: void
 * The function operation: the function executes a task. first locks the thread pool
 * it checks if the state is NoDestroy, if so, we wait for a task if the queue is
 * empty, if not, we deque a task and complete it.
 * If we are not in destroy mode, we unlock the queue, and check if we need to
 * wait for all the task, or only those who are running, and then work
 * accordingly.
 */
void executeTask(void *args);

/*
 * Function name:completeTask
 * The input: ThreadPool*
 * The output: void
 * The function operation: the function deques a task, runs it, and frees it's
 * allocated memory
 */
void completeTask(ThreadPool *th);

/*
 * Function name:freeThreadPool
 * The input: ThreadPool*
 * The output: void
 * The function operation: the function frees all the data allocated in the
 * thread pool struct. also, frees all the tasks in the osqueue.
 */
void freeThreadPool(ThreadPool *threadPool);

/*
 * Function name:tpCreate
 * The input: int numOfThreads
 * The output: Threadpool*
 * The function operation: the function allocates space for the threadpool struct, and all
 * it's needed data, and creates the pthreads with the runTask's function
 * as parameter
 */

ThreadPool *tpCreate(int numOfThreads) {
    int i;
    ThreadPool *threadPool = (ThreadPool *) malloc(sizeof(ThreadPool));
    if (threadPool == NULL) {
        displayError();
    }
    threadPool->threads = (pthread_t *) malloc(sizeof(pthread_t) * numOfThreads);
    if (threadPool->threads == NULL) {
        free(threadPool);
        displayError();
    }
    threadPool->osQueue = osCreateQueue();
    if (threadPool->osQueue == NULL) {
        freeThreadPool(threadPool);
        displayError();
    }
    if (pthread_mutex_init(&(threadPool->lockQueue), NULL) != SUCCESS ||
        pthread_cond_init(&(threadPool->notify), NULL) != SUCCESS ||
        pthread_mutex_init(&(threadPool->lockPool), NULL) != SUCCESS) {
        freeThreadPool(threadPool);
        displayError();
    }
    (threadPool->isStopped) = NO;
    threadPool->executeTasks = executeTask;
    threadPool->numOfThreads = numOfThreads;
    threadPool->destroy = NoDestroy;
    for (i = 0; i < numOfThreads; i++) {
        if (pthread_create(&threadPool->threads[i], NULL, runTasks, threadPool) == ERROR) {
            freeThreadPool(threadPool);
            displayError();
        }
    }
    return threadPool;
}

/*
 * Function name:tpInsertTask
 * The input: ThreadPool *threadPool, void (*computeFunc)(void *), void *param
 * The output: int
 * The function operation: the function inserts a new task to the thread pool, if the
 * threadpool is already stopped, we exit with a fail
 */
int tpInsertTask(ThreadPool *threadPool, void (*computeFunc)(void *), void *param) {

    if (threadPool->isStopped) {
        return EXIT_FAIL;
    }
    Task *task = (Task *) malloc(sizeof(Task));
    if (task == NULL) {
        freeThreadPool(threadPool);
        displayError();
    }
    task->func = computeFunc;
    task->information = param;
    if (pthread_mutex_lock(&threadPool->lockQueue) != SUCCESS) {
        freeThreadPool(threadPool);
        displayError();
    }
    osEnqueue(threadPool->osQueue, task);
    //signal to the the thread pool that a new task has been added to the queue
    if (pthread_cond_signal(&threadPool->notify) != SUCCESS) {
        freeThreadPool(threadPool);
        displayError();
    }
    if (pthread_mutex_unlock(&threadPool->lockQueue) != SUCCESS) {
        freeThreadPool(threadPool);
        displayError();
    }
    return SUCCESS;
}

/*
 * Function name:tpDestroy
 * The input: ThreadPool *threadPool, int shouldWaitForTasks
 * The output: void
 * The function operation: the function begins the destroy mode of the
 * threadpool, we first declare if we wait for all the tasks that are
 * running to be completed or only those who currently run (not including in the
 * queue), then join all pthreads and call freeThreadPool function.
 */
void tpDestroy(ThreadPool *threadPool, int shouldWaitForTasks) {
    int i;
    if (pthread_mutex_lock(&(threadPool->lockPool)) != SUCCESS) {
        freeThreadPool(threadPool);
        displayError();
    }
    //check if the threadpool is already stopped
    if (threadPool->isStopped) {
        twiceDestroyMsg();
        return;
    }
    pthread_mutex_unlock(&(threadPool->lockPool));
    if (shouldWaitForTasks != NO) {
        threadPool->destroy = waitAll;
    } else {
        threadPool->destroy = waitOnlyRunning;
    }
    if (pthread_mutex_lock(&threadPool->lockQueue) != SUCCESS) {
        freeThreadPool(threadPool);
        displayError();
    }
    //broadcast to wait until queue is empty
    if (pthread_cond_broadcast(&threadPool->notify) != SUCCESS) {
        freeThreadPool(threadPool);
        displayError();
    }
    if (pthread_mutex_unlock(&threadPool->lockQueue) != SUCCESS) {
        freeThreadPool(threadPool);
        displayError();
    }
    for (i = 0; i < threadPool->numOfThreads; i++) {
        if (pthread_join(threadPool->threads[i], NULL) != SUCCESS) {
            freeThreadPool(threadPool);
            displayError();
        }
    }
    if (pthread_mutex_trylock(&threadPool->lockPool) != SUCCESS) {
        freeThreadPool(threadPool);
        displayError();
    }
    //threadpool is changed to stop
    threadPool->isStopped = YES;
    if (pthread_mutex_unlock(&threadPool->lockPool) != SUCCESS) {
        freeThreadPool(threadPool);
        displayError();
    }
    freeThreadPool(threadPool);
}

void freeThreadPool(ThreadPool *threadPool) {
    while (!osIsQueueEmpty(threadPool->osQueue)) {
        Task *task = osDequeue(threadPool->osQueue);
        if (task != NULL) {
            free(task);
            task = NULL;
        }
    }
    if (threadPool->osQueue != NULL) {
        osDestroyQueue(threadPool->osQueue);
        threadPool->osQueue = NULL;
    }
    pthread_mutex_destroy(&threadPool->lockQueue);
    pthread_cond_destroy(&threadPool->notify);
    pthread_mutex_destroy(&threadPool->lockPool);
    if (threadPool->threads != NULL) {
        free(threadPool->threads);
        threadPool->threads = NULL;
    }
    //destroy thread pool
    free(threadPool);
    threadPool = NULL;
}


void executeTask(void *args) {
    ThreadPool *threadPool = (ThreadPool *) args;
    while ((threadPool->isStopped) <= NO) {
        if (pthread_mutex_lock(&threadPool->lockQueue) != 0) {
            printf("in locking\n");
            freeThreadPool(threadPool);
            displayError();
        }
        // if we are in no destory mode, we need to wait for tasks
        if (threadPool->destroy == NoDestroy) {
            if (osIsQueueEmpty(threadPool->osQueue)) {
                // handle busy waiting for task to be inserted
                if (pthread_cond_wait(&threadPool->notify, &threadPool->lockQueue) != SUCCESS ||
                    pthread_mutex_unlock(&threadPool->lockQueue) != SUCCESS) {
                    freeThreadPool(threadPool);
                    displayError();
                }

            } else {
                // complete task because queue isn't empty
                completeTask(threadPool);
            }
        } else {
            // we are in destroy state, so fi empty break
            if (osIsQueueEmpty(threadPool->osQueue)) {
                if (pthread_mutex_unlock(&threadPool->lockQueue) != 0) {
                    freeThreadPool(threadPool);
                    displayError();
                }
                break;
            }
            //if we need to wait for all tasks, we need to complete them
            if (threadPool->destroy == waitAll) {
                completeTask(threadPool);
            } else if (threadPool->destroy == waitOnlyRunning) {
                if (pthread_mutex_unlock(&threadPool->lockQueue) != 0) {
                    freeThreadPool(threadPool);
                    displayError();
                }
                break;
            }

        }

    }
}


void *runTasks(void *args) {
    ThreadPool *threadPool = (ThreadPool *) args;
    threadPool->executeTasks(args);
    return NULL;
}

void completeTask(ThreadPool *th) {
    Task *task = osDequeue(th->osQueue);
    if (pthread_mutex_unlock(&th->lockQueue) != 0) {
        freeThreadPool(th);
        displayError();
    }
    task->func(task->information);
    free(task);
}


void twiceDestroyMsg() {
    write(STDERR, DESTROYED_TWICE, ERROR_TWICE);
}

void displayError() {
    write(STDERR, ERROR_MESSAGE, ERROR_SIZE);
    exit(EXIT_FAIL);
}
