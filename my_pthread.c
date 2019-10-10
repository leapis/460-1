#include "my_pthread.h"
#include <errno.h>
#define NUM       1000
#define STACKSIZE 32768

/* Scheduler State */
 // Fill in Here //

my_pthread_t tid = 0;

my_pthread_tcb tList[NUM];

//my_pthread_tcb current;

struct myNode {
  my_pthread_t tid;
  struct myNode* next;
};

struct myNode *head;
struct myNode *tail;
struct myNode *current;

void schedule(int);

void timer_handler(int);

void disableTime(void);

void timer_handler(int sigint){
  //printf("hit\n");
  //exit(0);
  schedule(-1);
}


/* Scheduler Function
 * Pick the next runnable thread and swap contexts to start executing
 */
void schedule(int signum){
  disableTime();
  struct itimerval timer;
  timer.it_value.tv_usec = TIME_QUANTUM_MS;
  timer.it_value.tv_sec = 0;
  timer.it_interval.tv_sec = 0;
  timer.it_interval.tv_usec = 0;
  if (signum == -2){ //initial setup
    signal(SIGPROF, timer_handler);
    int errnum = setitimer(ITIMER_PROF, &timer, NULL);
    if (errnum == -1) {
      printf("Timer err: %d\n", errno);
      exit(0);
    }
  }
  else if (signum == -1){ //called by timer or thread exit, go next
    struct myNode *target = current;
    do {
      if (target->next == NULL) {
        target = head;
      }
      else {
        target = target->next;
      }
    }
    while(tList[target->tid].status != RUNNABLE);
    
    //swap contexts
    int oldContext = current->tid;
    int newContext = target->tid;
    current = target;
    int errnum = setitimer(ITIMER_PROF, &timer, NULL);
    if (errnum == -1) {
      printf("Timer err: %d\n", errno);
      exit(0);
    }
    signal(SIGPROF, timer_handler);
    swapcontext(&tList[oldContext].context, &tList[newContext].context);
  }
  else{
    exit(0);
  }
}

/* Create a new TCB for a new thread execution context and add it to the queue
 * of runnable threads. If this is the first time a thread is created, also
 * create a TCB for the main thread as well as initalize any scheduler state.
 */
void my_pthread_create(my_pthread_t *thread, void*(*function)(void*), void *arg){

  ucontext_t newStack;
  //printf("starting\n");
  if (tid == 0) { //initial run
    //create context for current thread
    ucontext_t* currentContext = malloc(sizeof(ucontext_t));
    getcontext(currentContext);
    my_pthread_tcb start = tList[0];
    start.context = *currentContext;
    start.tid = tid;
    start.status = RUNNABLE;

    //setup scheduler
    tail = malloc(sizeof(struct myNode*));
    tail->tid = tid;
    tail->next = tail;
    head = tail;
    current = tail;
    tid++;
    //schedule(-2);

  } 

  //make new thread
  tList[tid].tid = tid;
  tList[tid].status = RUNNABLE;
  ucontext_t* newContext = malloc(sizeof(ucontext_t));
  getcontext(newContext);
  newContext->uc_stack.ss_size = STACKSIZE;
  newContext->uc_stack.ss_sp = malloc(STACKSIZE);
  makecontext(newContext, (void(*)(void)) function, 0);
  tList[tid].context = *newContext;
  *thread = tid;

  //add to queue
  struct myNode* temp = malloc(sizeof(struct myNode*));
  temp->tid = tid;
  temp->next = NULL;
  tail->next = temp;
  tail = tail->next;


  schedule(-1);

  tid++;

  //printf("%d\n",my_pthread_self());
  //exit(0);
  // Implement Here

}

/* Give up the CPU and allow the next thread to run.
 */
void my_pthread_yield(){
  //disable signal
  disableTime();

  //hit schduler up
  schedule(-1);

}

/* The calling thread will not continue until the thread with tid thread
 * has finished executing.
 */
void my_pthread_join(my_pthread_t thread){

  //check if joined thread is alive
  if (tList[thread].status != RUNNABLE)
    return;

  //stop scheduler
  disableTime();

  //sleep current thread
  tList[my_pthread_self()].status = SLEEP;

  //add to wake queue
  my_pthread_tcb* prev = tList[thread].next;
  tList[thread].next = &tList[my_pthread_self()];
  tList[my_pthread_self()].next = prev;

  //printf("joined\n");

  schedule(-1);  

}


/* Returns the thread id of the currently running thread
 */
my_pthread_t my_pthread_self(){

  // Implement Here //
  
  return current->tid; // temporary return, replace this

}

/* Thread exits, setting the state to finished and allowing the next thread
 * to run.
 */

void disableTime(){
  signal(SIGPROF, SIG_IGN);
}

void my_pthread_exit(){

  //disable timer
  disableTime();
  
  //kill thread
  tList[my_pthread_self()].status = FINISHED;

  my_pthread_tcb* wakeUp = tList[my_pthread_self()].next;
  while(wakeUp != NULL){
    wakeUp->status = RUNNABLE;
    wakeUp = wakeUp->next;
  }
  
  //printf("%d\n", tList[0].status);
  schedule(-1);

}
