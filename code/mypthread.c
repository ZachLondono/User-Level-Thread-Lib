// File:	mypthread.c

// List all group member's name:
// username of iLab:
// iLab Server:

#include "mypthread.h"

static int is_init = 0;
static tcb** threads;
static int thread_array_size = 100;
static int curr_thread_id = 0;

static void schedule();
static void signal_action(int a, siginfo_t *b, void *c) {
	schedule();
}

static int init_timer() {
	
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sigemptyset(&sa.sa_mask);
	sa.sa_sigaction = signal_action;
	sa.sa_flags = SA_SIGINFO;
	if (sigaction(SIGALRM, &sa, NULL) != 0) return -1;
	
	return 0;
}

static int next_available() {

	if (!is_init) {
		threads = malloc(sizeof(tcb*) * thread_array_size);

		// NULL out array so theycan be chosen when creating new threads
		int i = 0;
		for (i = 0; i < thread_array_size; i++) {
			threads[i] = NULL;
		}

		// stetup timer & signal catcher
		if (init_timer() == -1) return -1;

		// allocate tcb for main thread
		threads[0] = malloc(sizeof(tcb));
		threads[0]->context = malloc(sizeof(ucontext_t));

		is_init = 1;
	}
	
	// Finds first NULL spot in the thread array that we can use for the new thread
	int i = 0;
	for (i = 0; i < thread_array_size; i++)
		if (threads[i] == NULL) return i;

	// If a NULL spot has not been found, we need to increase the size of the array
	const uint THREAD_SIZE_INCRIMENT = 100;
	tcb** new_array = realloc(threads, sizeof(tcb*) * (thread_array_size + THREAD_SIZE_INCRIMENT));
	
	// NULL new slots in thread pool
	if (!new_array) return -1;
	threads = new_array;
	for (int j = thread_array_size; j < thread_array_size + THREAD_SIZE_INCRIMENT; j++)
		threads[j] = NULL;
	thread_array_size += THREAD_SIZE_INCRIMENT;


	// TODO grow threads[] array as needed
	return thread_array_size;

}

void function_handler(intptr_t callback, intptr_t arg) {

	// cast the arguments into their correct types
	void* (*real_callback)(void*) = (void*(*)(void*)) callback;
	void* real_arg = (void*) arg;

	// save the return value so that it can be returned when another thread calls join
	void* ret_val = real_callback(real_arg);
	//mypthread_exit(ret_val)
}

/* create a new thread */
int mypthread_create(mypthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg) {

	// retreive the next available slot in the thread array
	mypthread_t new_thread_id = next_available();
	if (new_thread_id == -1) return -1;

	// assign pointer to new id
	if(thread != NULL)
		*thread = new_thread_id;

	
	threads[new_thread_id] = malloc(sizeof(tcb));
	if (!threads[new_thread_id]) return -1;

	tcb* new_thread = threads[new_thread_id];
	// create new context for new thread
	new_thread->context = malloc(sizeof(ucontext_t));
	getcontext(new_thread->context);
	ucontext_t* new_context = new_thread->context;
	new_context->uc_stack.ss_sp = malloc(SIGSTKSZ);
	if (!new_context) return -1;
	new_context->uc_stack.ss_size = SIGSTKSZ;
	new_context->uc_stack.ss_flags = 0;

	// pass the function and argument as arguments into the function handler
	makecontext(new_context, function_handler, 2, (intptr_t) function, (intptr_t) arg);

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
int mypthread_mutex_init(mypthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr) {
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

	int next_thread_id = curr_thread_id;
	while (++next_thread_id <= thread_array_size) {
		if (next_thread_id == thread_array_size) next_thread_id = 0;
		if (threads[next_thread_id] != NULL) break;
	}

	int curr_thread_holder = curr_thread_id;
	curr_thread_id = next_thread_id;

	threads[next_thread_id]->status = Running;

	swapcontext(threads[curr_thread_holder]->context, threads[next_thread_id]->context);

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
