// File:	mypthread.c

// List all group member's name: Zachary Londono
// username of iLab: zcl6
// iLab Server: ilab1.cs.rutgers.edu

#include "mypthread.h"

// INITAILIZE ALL YOUR VARIABLES HERE
// YOUR CODE HERE
static Queue* runqueue = NULL;
static tcb** threads = NULL;
static uint thread_array_size = 0;

static const uint THREAD_SIZE_INCRIMENT = 100;

static int next_thread_id() {

	if (threads == NULL) {
		// allocate array and assign each element to NULL
		threads = malloc(sizeof(tcb*) * THREAD_SIZE_INCRIMENT);
		thread_array_size = THREAD_SIZE_INCRIMENT;	
		for (int j = 0; j < thread_array_size; j++)
			threads[j] = NULL;
	}

	int i = 0;
	for (i = 0; i < thread_array_size; i++)
		if (threads[i] == NULL) return i;

	tcb** new_array = realloc(threads, sizeof(tcb) * (thread_array_size + THREAD_SIZE_INCRIMENT));
	if (!new_array) return -1;
	threads = new_array; 
	for (int j = thread_array_size; j < thread_array_size + THREAD_SIZE_INCRIMENT; j++)
		threads[j] = NULL;
	thread_array_size += THREAD_SIZE_INCRIMENT;

	return thread_array_size;

}


/* create a new thread */
int mypthread_create(mypthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg) {
	// create Thread Control Block
	// create and initialize the context of this thread
	// allocate space of stack for this thread to run
	// after everything is all set, push this thread into queue

	mypthread_t new_id = next_thread_id();
	if (new_id == -1) return -1;

	if (runqueue == NULL) {
		runqueue = malloc(sizeof(Queue));
		initQueue(runqueue);
	}

	// Set up new control block
	threads[new_id] = malloc(sizeof(tcb));
	if (!threads[new_id]) return -1;

    threads[new_id]->context = malloc(sizeof(ucontext_t));
	if (!(threads[new_id]->context)) return -1;
	
	threads[new_id]->stack = malloc(SIGSTKSZ);
	if (!(threads[new_id]->stack)) return -1;
	
	threads[new_id]->thread_id = new_id;
	threads[new_id]->thread_status = Ready;
    threads[new_id]->context->uc_stack.ss_sp = threads[new_id]->stack;
    threads[new_id]->context->uc_stack.ss_size = SIGSTKSZ;
    threads[new_id]->context->uc_stack.ss_flags = 0;

	makecontext(threads[new_id]->context, (void (*)())function, 1, arg);

	mypthread_t* id_heap = malloc(sizeof(mypthread_t));
	*id_heap = new_id;
    enqueue(runqueue, id_heap);

    return 0;
};

/* give CPU possession to other user-level threads voluntarily */
int mypthread_yield() {

	// change thread state from Running to Ready
	// save context of this thread to its thread control block
	// switch from thread context to scheduler context

	// YOUR CODE HERE
	return 0;
};

/* terminate a thread */
void mypthread_exit(void *value_ptr) {
	// Deallocated any dynamic memory created when starting this thread

	// YOUR CODE HERE
};


/* Wait for thread termination */
int mypthread_join(mypthread_t thread, void **value_ptr) {

	// wait for a specific thread to terminate
	// de-allocate any dynamic memory created by the joining thread

	// YOUR CODE HERE
	return 0;
};

/* initialize the mutex lock */
int mypthread_mutex_init(mypthread_mutex_t *mutex,
                          const pthread_mutexattr_t *mutexattr) {
	//initialize data structures for this mutex

	// YOUR CODE HERE
	return 0;
};

/* aquire the mutex lock */
int mypthread_mutex_lock(mypthread_mutex_t *mutex) {
        // use the built-in test-and-set atomic function to test the mutex
        // if the mutex is acquired successfully, enter the critical section
        // if acquiring mutex fails, push current thread into block list and //
        // context switch to the scheduler thread

        // YOUR CODE HERE
        return 0;
};

/* release the mutex lock */
int mypthread_mutex_unlock(mypthread_mutex_t *mutex) {
	// Release mutex and make it available again.
	// Put threads in block list to run queue
	// so that they could compete for mutex later.

	// YOUR CODE HERE
	return 0;
};


/* destroy the mutex */
int mypthread_mutex_destroy(mypthread_mutex_t *mutex) {
	// Deallocate dynamic memory created in mypthread_mutex_init

	return 0;
};

/* scheduler */
static void schedule() {
	// Every time when timer interrup happens, your thread library
	// should be contexted switched from thread context to this
	// schedule function

	// Invoke different actual scheduling algorithms
	// according to policy (STCF or MLFQ)

	// if (sched == STCF)
	//		sched_stcf();
	// else if (sched == MLFQ)
	// 		sched_mlfq();

	// YOUR CODE HERE

// schedule policy
#ifndef MLFQ
	// Choose STCF
#else
	// Choose MLFQ
#endif

}

/* Preemptive SJF (STCF) scheduling algorithm */
static void sched_stcf() {
	// Your own implementation of STCF
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE
}

/* Preemptive MLFQ scheduling algorithm */
static void sched_mlfq() {
	// Your own implementation of MLFQ
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE
}

// Feel free to add any other functions you need

// YOUR CODE HERE
