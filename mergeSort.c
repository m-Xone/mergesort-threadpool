// =========================================== //
// Date:        27 September 2016              //
// File:        mergeSort.c                    //
// Assignment:  Project 1                      //
// Course:      OS6456 Operating Systems, F16  // 
// Name:        William Young                  //
// Description: Concurrent Mergesort           //
// =========================================== //

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
#include <math.h>
#include "tp.h"

#define MAX_ITEMS_TO_SORT 4096

static pthread_mutex_t mainlock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t done = PTHREAD_COND_INITIALIZER;
static pthread_cond_t shutdown = PTHREAD_COND_INITIALIZER;

typedef struct ms_input ms_input;

struct ms_input {
  int start, level;
};

int save = 0;
int num_items = 0;
int complete = 0;
int tasked = 0;
int finished = 0;
int items[MAX_ITEMS_TO_SORT];
int temp[MAX_ITEMS_TO_SORT];

void merge(void*);
void mSort(threadpool* tp);
void usage(int);
void read_file(char*);
void pad_input(void);
void output(void);
int near_pow_2(int);
double num_levels(void);

int main(int argc, char** argv, char** env) {
  // intialization and setup
  threadpool *tp;
  usage(argc);
  read_file(argv[1]);
  pad_input();

  // create threadpool
  tp = threadpool_create(num_items/2);

  // merge sort helper function
  mSort(tp);

  // wait for all tasks to finish (avoids killing pending tasks in threadpool)
  while(1) { 
    pthread_mutex_lock(&mainlock);  
    while(finished == 0) { pthread_cond_wait(&shutdown, &mainlock); }
    if(finished == 1) { break; }
    pthread_mutex_unlock(&mainlock); }
  pthread_mutex_unlock(&mainlock);

  // output sorted items
  output();

  // shutdown threadpool
  threadpool_exit(tp);

  return 0;
}

/*###############################################################################*/
/*#################################### MSORT ####################################*/
/*###############################################################################*/
/* This is a helper function that prevents MAIN from becoming cluttered and indecipherable.
   1) Determine the number of levels we'll need to execute on.
   2) The outer loop keeps track of which level we're in; the inner loop manages how many 
      subarrays we need to merge in each level ( first level: num_items/2 )
   3) "Concurrent tasks" is a function of the level we're in and the total number of 
      input integers.
   4) The step is a function of the number of concurrent tasks running
      (e.g. 4 concurrent tasks on an input of 8 items implies we're merging single 
      items to produce sorted pairs; thus, the step size is 2)
   5) Inside the inner loop (concurrent tasks), the threadpool is tasked to sort a subset of
      the input.
   6) Once the threadpool has been tasked, update the tasked count so we can keep track
      of completion status per level.
   7) Once all levels have been tasked, wait until all threads finish and then report status to MAIN. */
void mSort(threadpool* tp) {
  // setup
  int x, y, z;
  int lvl = num_levels();

  // outer loop ( log2(n) levels of merging )
  for(x = 0; x < lvl; x++) {

    // these variables help divide merging into taskable chunks
    int concurrent_tasks = (num_items/2) / (int)pow(2,(double)x); // n/2, n/4, n/8, ... n/n concurrent threads
    int step = num_items/concurrent_tasks;

    // each individual merge operation
    for(y = 0, z = 0; y < concurrent_tasks; y++, z += step) { 

      // argument setup
      ms_input *msi = malloc(sizeof(ms_input));
      msi->start = z; // start = 0, 0+1*(step), 0+2*(step), 0+4*(step), ..., 0+log(n)*(step)
      msi->level = x; // level = 0, 1, 2, ..., level-1

      // Task the threadpool
      threadpool_add_task(tp, merge, (void*)msi); 

      // task dispatched to threadpool; update internal count for book-keeping
      pthread_mutex_lock(&mainlock);
      tasked++; 
      pthread_mutex_unlock(&mainlock);
    }

    // wait for all merges in a level to complete before starting next round
    while(1) { 
      pthread_mutex_lock(&mainlock);
      while(complete == 0 || complete < tasked) { 
	pthread_cond_wait(&done, &mainlock); }
      if(complete == tasked) 
	break; // level finished; move to next level
      pthread_mutex_unlock(&mainlock);
    }
    pthread_mutex_unlock(&mainlock); // because of BREAK
  }

  // this code ONLY reached once we're done.
  // signal completion to main thread
  pthread_mutex_lock(&mainlock);
  finished = 1;
  pthread_cond_signal(&shutdown); 
  pthread_mutex_unlock(&mainlock);
}

/*###############################################################################*/
/*#################################### MERGE ####################################*/
/*###############################################################################*/
/* This is the crux of the assignment: the actual sorting algorithm.
   1) Receive and recast function arguments from void*
   2) Calculate the subarray size of the elements we're merging (using LEVEL argument)
   3) Calculate the bounds of our merge operations. These are functions of the start index
      and the subarray size.
   4) Merge and store in TEMP array (global).
   5) Update global ITEMS array with sorted integers. (No indices overlap in our sort, so
      concurrent read/write operations are permitted)
   6) Report completion to mSort, which begins a new level once the current LEVEL is complete.
   7) Deallocate the arguments struct.                                           */
void merge(void* input) {
  // setup
  ms_input *msi = (ms_input*)input; // arguments
  int start = msi->start; // start index
  int num_in_subarr = (int)pow(2,(double)(msi->level)); // subarray to sort
  int k = start; // index in temp array
  int i = start; // index in subarray 1
  int j = start + num_in_subarr; // index in subarray 2
  int i_max = j; // end of subarray 1
  int j_max = start + 2 * num_in_subarr; // end of subarray 2 

  
  // SORTING ===============================================================
  // start merging (items in order require 1 less branch -> faster)
  while (i < i_max && j < j_max) {
    if (items[i] < items[j])
      temp[k++] = items[i++];
    else
      temp[k++] = items[j++]; }
  // One a subarray has been exhausted, use the other subarray to fill-out the temp array
  while(i < i_max)
    temp[k++] = items[i++];
  while(j < j_max)
    temp[k++] = items[j++];
  // =========================================================================

  
  // save results
  for (i = start; i < j_max; i++)
  items[i] = temp[i];
  
  // merge complete; report status
  pthread_mutex_lock(&mainlock);
  complete++;
  free(msi);
  pthread_cond_signal(&done);
  pthread_mutex_unlock(&mainlock);
}

// display results
void output(void) {
  int i, j;
  if(num_items == 0) {exit(1);}
  for(i = 0; i < save; i += 10) {
    for(j = i; j < (i + 10) && j < save; j++) {
      printf("%d ",items[j]);
    }
    printf("\n");
  }
}

// dummy integers to force valid log2(n)
void pad_input(void) { // decision for 9000+i made because it moves through the merge function faster
  int i;
  int j = near_pow_2(num_items);
  for(i = 0; i < j; i++)
    items[num_items++] = 9000+i; }

// custom log2(n)
double num_levels(void) {
  if(num_items == 0) {return 0;}
  return log((double)num_items)/log(2); }

// closest higher power of 2
int near_pow_2(int num_items) {
  if(num_items == 0) {return 0;}
  int pow = 1;
  while(pow < num_items && pow <= 4096) {pow*=2;} 
  return pow - num_items; }

// read integers from argument file
void read_file(char* input) {
  FILE* inFile = fopen(input, "r");
  if(inFile == NULL) {
    exit(1); }
  int i=0, v;
  while((fscanf(inFile, "%d", &v) != EOF) && i < 4096) {
    items[i++] = v;
    }
  num_items = save = i;
  fclose(inFile); }

// "man" page
void usage(int args) {
  if(args < 2) {
    fprintf(stdout, "\nNAME\n\tmergeSort - sort a file of integers in non-decreasing order\n\nSYNOPSIS\n\t#include \"threadpool.h\"\n\n\t./mergeSort [input_file]\n\nDESCRIPTION\n\tThe mergeSort program uses the concept of concurrent execution achieved \n\twith the POSIX pthread library to sort a file of space-delimited integers.\n\nRETURN VALUE\n\tReturns 0 on success.\n\n"); 
    exit(1); } }
