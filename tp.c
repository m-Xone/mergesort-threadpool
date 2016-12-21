// =========================================== //
// Date:        27 September 2016              //
// File:        tp.c                           //
// Assignment:  Project 1                      //
// Course:      OS6456 Operating Systems, F16  // 
// Name:        William Young                  //
// Description: Threadpool Implementation      //
// =========================================== //
/* Detailed description:
   Threadpools need a few things:
   1) Initialization
   2) Termination & cleanup
   3) Ability to receive tasking orders
   4) Queue to hold tasks
   5) Ability to dispatch threads for each task
   This file provides for these functionalities.
   References:
   "The Linux Programming Interface" by Michael Kerrisk;
   "Thread pools and Work Queues" by Brian Goetz <ibm.com/developerworks/library/j-jtp0730/>;
 */

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include "tp.h"

// LOCKS AND CONDITION VARIABLES
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t condition = PTHREAD_COND_INITIALIZER;

// THREADPOOL TASK
struct threadpool_task {
  void (*task_function)(void*);
  void* args;
};

// THREADPOOL
struct threadpool {
  int threads_in_pool; // number of threads in threadpool
  pthread_t* all_threads; // threads 
  threadpool_task* task_queue; // task queue
  pthread_mutex_t mtx_lock; // mutex lock
  pthread_cond_t cond; // condition variable for task signaling  
  int front; // front of queue (first out)
  int back; // back of queue (first in)
  int tasks_in_queue; // number of pending tasks in queue
  int exit; // indicate termination so we can empty the pool
  int active; // tasked threads
};

/*###############################################################################*/
/*############################# CREATE THREADPOOL  ##############################*/
/*###############################################################################*/
/* Called when MAIN needs a threadpool.
   1) Initialize all threadpool variables.
   2) Create thread array and task queue.
   NOTE: queue size = 2 * num_threads  (n + n/2 + n/4 + ... + n/k -> 2) 
   3) Create N threads and send them to get_task.
   4) Return threadpool to caller.           */
threadpool* threadpool_create(int num_threads) {
  // initialize variables
  int i;
  threadpool* tp = (threadpool*)malloc(sizeof(threadpool)); // threadpool
  tp->threads_in_pool = 0;
  tp->all_threads = NULL;
  tp->task_queue = NULL;
  tp->active = 0;
  tp->front = 0;
  tp->back = -1;
  tp->mtx_lock = lock;
  tp->cond = condition;
  tp->tasks_in_queue = 0;
  tp->exit = 0;
  tp->all_threads = (pthread_t*)malloc(sizeof(pthread_t) * num_threads); // pthread_t array
  tp->task_queue = (threadpool_task*)malloc(2 * sizeof(threadpool_task) * num_threads); // queue
  
  for(i = 0; i < num_threads; i++) {  // initialize threads and get a task or idle
    pthread_create(&tp->all_threads[i], NULL, threadpool_get_task, (void*)tp);
    tp->threads_in_pool++; // one more thread in threadpool
    tp->active++; // thread is now active
  }
  return tp;  // send initialized threadpool back to caller
}


/*###############################################################################*/
/*################################### GET TASK ##################################*/
/*###############################################################################*/
/* Every new thread comes here to idle. 
   1) Check if there are available tasks.
   2) If task available, pop off the queue and execute.
   3) If task not available, idle until signaled.
   4) If threadpool receives termination call,
   wake-up from idle and terminate thread.            */

void* threadpool_get_task(void* tpv) {
  threadpool* tp = (threadpool*)tpv; // recast from (void*) 
  threadpool_task task; // initialize task 
  while(1) {
    pthread_mutex_lock(&tp->mtx_lock); // lock
    
    while(tp->tasks_in_queue == 0 && tp->exit == 0) { // wait until task is available/exit is called
      pthread_cond_wait(&tp->cond, &tp->mtx_lock); }
    if(tp->exit == 1) { // threadpool terminating; don't get task
      break; }

    task.task_function = (tp->task_queue[tp->front]).task_function; // get task
    task.args = (tp->task_queue[tp->front]).args; // get args for task
    tp->front++; // advance front of queue
    tp->tasks_in_queue--; // one less task to complete
    
    pthread_mutex_unlock(&tp->mtx_lock); 
    (*(task.task_function))(task.args); // execute task
    }

  // once threadpool_terminate is invoked, every thread dies here
  tp->active--; // one less active thread
  pthread_mutex_unlock(&tp->mtx_lock); // unlock (safety)
  pthread_exit(NULL); // goodbye, cruel world
}


/*###############################################################################*/
/*#################################### ADD TASK #################################*/
/*###############################################################################*/
/* Called when mergeSort in MAIN adds a task. 
   1) Add new task to the back of the queue.
   2) Signal to idling threads that a task is available (at least one will wake up) */

int threadpool_add_task(threadpool* tp, void (*function)(void*), void* args) {
  pthread_mutex_lock(&tp->mtx_lock);
  tp->back++; // advance back of queue
  (tp->task_queue[tp->back]).task_function = function; // enqueue new task
  (tp->task_queue[tp->back]).args = args;
  tp->tasks_in_queue++; // one more task to complete
  pthread_cond_signal(&tp->cond); // signal that task is available
  pthread_mutex_unlock(&tp->mtx_lock); 
  return 0;
}

/*###############################################################################*/
/*################################# TERMINATE ###################################*/
/*###############################################################################*/
/* Called when MAIN is done with the threadpool. 
   1) Set exit flag to 1 (this prevents threads from getting new tasks).
   2) Wake up all idling threads so that they see the exit flag.
   3) Have all threads join the main thread. 
   4) Deallocate everything we used.                   */

int threadpool_exit(threadpool* tp) {
  int i;
  pthread_mutex_lock(&tp->mtx_lock); // lock
  tp->exit = 1; // termination = TRUE
  pthread_cond_broadcast(&tp->cond); // wakeup all idling threads
  pthread_mutex_unlock(&tp->mtx_lock);
  while(tp->active > 0) {}
  for(i = 0; i < (tp->threads_in_pool); i++) { // join threads to caller
    pthread_join(tp->all_threads[i], NULL); }
  free(tp->all_threads);
  free(tp->task_queue); 
  free(tp);
  return 0;
}
