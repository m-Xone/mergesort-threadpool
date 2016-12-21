// =========================================== //
// Date:        27 September 2016              //
// File:        tp.h                           //
// Assignment:  Project 1                      //
// Course:      OS6456 Operating Systems, F16  // 
// Name:        William Young                  //
// Description: Threadpool Headers             //
// =========================================== //
// -> Headers for threadpool based on task queue

typedef struct threadpool threadpool;
typedef struct threadpool_task threadpool_task;

// Initialize a threadpool
// num_threads = number of threads in threadpool
threadpool* threadpool_create(int);

// Get a task from the threadpool
// tp = threadpool (typecasted void* from pthread_create)
void* threadpool_get_task(void*);

// Task a job to the threadpool
// start = function to call
// args = arguments to function
int threadpool_add_task(threadpool*, void(*function)(void*), void*);

// Terminate a threadpool
// threapool = threadpool to terminate
int threadpool_exit(threadpool*);

