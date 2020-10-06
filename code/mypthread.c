// File:	mypthread.c

// List all group member's name: Zachary Londono
// username of iLab: zcl6
// iLab Server: ilab1.cs.rutgers.edu

#include "mypthread.h"

// INITAILIZE ALL YOUR VARIABLES HERE
static mypthread_t current_thread = -1;
static ucontext_t main_ctx;
static Queue* runqueue = NULL;
static tcb** threads = NULL;
static uint thread_array_size = 0;

static const uint THREAD_SIZE_INCRIMENT = 100;

static int init_tcb(tcb* control, mypthread_t thread_id) {
	// Allocates the context which will store the state of the thread
    control->context = malloc(sizeof(ucontext_t));
	if (!(control->context)) return -1;
	
	// Creates the stack, parameters of which should be gotten from the pthread_attr_t ?
	control->stack = malloc(SIGSTKSZ);
	if (!(control->stack)) return -1;
	
	control->thread_id = thread_id;
	control->thread_status = Ready;
	control->will_be_joined = 0;
}

static int next_thread_id() {

	// TODO: The thread array should be initilized so that the main thread is ID 0 and the scheduling thread is id 1

	if (threads == NULL) {
		// allocate array and assign each element to NULL
		threads = malloc(sizeof(tcb*) * THREAD_SIZE_INCRIMENT);
		thread_array_size = THREAD_SIZE_INCRIMENT;	
		for (int j = 0; j < thread_array_size; j++)
			threads[j] = NULL;
		
			
		threads[0] = malloc(sizeof(tcb));
		int err = -1;
		if (!threads[0] || (err = init_tcb(&threads[0], 0)) != 0) return err;
		getcontext(&(threads[0]->context));
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


void funciton_handler(void *(*function)(void*), void* args) {
	void* ret_val = function(args);
	mypthread_exit(ret_val);	
}

/* create a new thread */
int mypthread_create(mypthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg) {
	// create Thread Control Block
	// create and initialize the context of this thread
	// allocate space of stack for this thread to run
	// after everything is all set, push this thread into queue

	// This function will initilize the thread array if necessary and get the next available thread ID
	mypthread_t new_id = next_thread_id();
	if (new_id == -1) return -1;

	if (runqueue == NULL) {
		runqueue = malloc(sizeof(Queue));
		initQueue(runqueue);
	}

	// Set up new control block
	threads[new_id] = malloc(sizeof(tcb));
	int err = -1;
	if (!threads[new_id] || (err = init_tcb(&threads[new_id], new_id)) != 0) return err;

	// Set up the new context
    threads[new_id]->context->uc_stack.ss_sp = threads[new_id]->stack;
    threads[new_id]->context->uc_stack.ss_size = SIGSTKSZ;
    threads[new_id]->context->uc_stack.ss_flags = 0;
	makecontext(threads[new_id]->context, (void (*)())function, 1, arg);

	// Add the new thread to the runqueue
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

	// Save return value in value_ptr
	threads[current_thread]->exit_status = value_ptr;

	// Flag the thread as exited
	threads[current_thread]->thread_status = Exited;

	free(threads[current_thread]->context);
	free(threads[current_thread]->stack);

};


/* Wait for thread termination */
int mypthread_join(mypthread_t thread, void **value_ptr) {

	// wait for a specific thread to terminate
	// de-allocate any dynamic memory created by the joining thread

	// thread with id thread does not exist
	if (thread >= thread_array_size || threads[thread] == NULL) return ESRCH;

	tcb* thread_control = threads[thread];

	// Check if thread is joinable (can't detach in this library so we ignore this)
	// if (thread_controll->isdetached) return EINVAL

	if (thread_control->will_be_joined) return EINVAL;
	else thread_control->will_be_joined = current_thread; 	// flag it as being waited on so that other threads do not wait for it to join

	while(thread_control->thread_status != Exited) {}	// Block current thread until the thread it's witing on exits

	*value_ptr = thread_control->exit_status;

	free(thread_control);	// dealocate threads control block
	threads[thread] = NULL;	// set it's position to NULL so that the ID can be reused? maybe should change this to be sure that thread id's are unique within the process

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
