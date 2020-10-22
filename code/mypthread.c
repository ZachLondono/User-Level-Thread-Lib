// File:	mypthread.c

// List all group member's name:
// username of iLab:
// iLab Server:

#include "mypthread.h"
#include <valgrind/valgrind.h>

static int is_init = 0;
static tcb** threads;
static int thread_array_size = 100;
static int curr_thread_id = 0;
static mypthread_mutex_t* library_mutex = NULL;

static StackNode** waiting_queue = NULL;
static StackNode** run_queue = NULL;

static void schedule();
static void push(void* data, StackNode** root);
static void delete(void* data, StackNode** root);
static void* pop(StackNode** root);
static void push_queue(tcb* thread, StackNode** top);

static const int QUANTUM = 100;

static void signal_action(int a, siginfo_t *b, void *current_context) {
	schedule();
}

static int restart_timer() {
	
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sigemptyset(&sa.sa_mask);
	sa.sa_sigaction = signal_action;
	sa.sa_flags = SA_SIGINFO;
	// if (sigaction(SIGALRM, &sa, NULL) != 0) return -1;
	if (sigaction(SIGPROF, &sa, NULL) != 0) return -1;

	struct itimerval tv;
	tv.it_value.tv_sec = 0;
	tv.it_value.tv_usec = QUANTUM;
	tv.it_interval.tv_sec = 0;
	tv.it_interval.tv_usec = QUANTUM;
	// if (setitimer(ITIMER_REAL, &tv, NULL) == -1) return -1;
	if (setitimer(ITIMER_PROF, &tv, NULL) == -1) return -1;

	return 0;
}

static int next_available() {

	if (!is_init) {
		threads = calloc(thread_array_size, sizeof(tcb*));

		library_mutex = malloc(sizeof(mypthread_mutex_t));
		mypthread_mutex_init(library_mutex, NULL);
		mypthread_mutex_lock(library_mutex);

		// NULL out array so theycan be chosen when creating new threads
		int i = 0;
		for (i = 0; i < thread_array_size; i++) {
			threads[i] = NULL;
		}

		// stetup timer & signal catcher
		if (restart_timer() == -1) return -1;

		// allocate tcb for main thread
		threads[0] = malloc(sizeof(tcb));
		threads[0]->context = calloc(1, sizeof(ucontext_t));
		threads[0]->ticks = 1;
		threads[0]->ret_val = NULL;
		threads[0]->to_join = 0;
		threads[0]->status = Ready;
		threads[0]->id = 0;

		run_queue = malloc(sizeof(StackNode*));
		*run_queue = NULL;
		push_queue(threads[0], run_queue);

		is_init = 1;

		mypthread_mutex_unlock(library_mutex);
	}
	
	// Finds first NULL spot in the thread array that we can use for the new thread
	int i = 0;
	for (i = 0; i < thread_array_size; i++)
		if (threads[i] == NULL) return i;


	mypthread_mutex_lock(library_mutex);
	// If a NULL spot has not been found, we need to increase the size of the array
	const uint THREAD_SIZE_INCRIMENT = 100;
	tcb** new_array = realloc(threads, sizeof(tcb*) * (thread_array_size + THREAD_SIZE_INCRIMENT));
	
	// NULL new slots in thread pool
	if (!new_array) return -1;
	threads = new_array;
	for (int j = thread_array_size; j < thread_array_size + THREAD_SIZE_INCRIMENT; j++)
		threads[j] = NULL;


	int next_available = thread_array_size;
	thread_array_size += THREAD_SIZE_INCRIMENT;

	mypthread_mutex_unlock(library_mutex);

	return next_available;

}

void function_handler(intptr_t callback, intptr_t arg) {

	// cast the arguments into their correct types
	void* (*real_callback)(void*) = (void*(*)(void*)) callback;
	void* real_arg = (void*) arg;

	// save the return value so that it can be returned when another thread calls join
	void* ret_val = real_callback(real_arg);
	mypthread_exit(ret_val);
}

/* create a new thread */
int mypthread_create(mypthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg) {

	// retreive the next available slot in the thread array
	mypthread_t new_thread_id = next_available();
	if (new_thread_id == -1) return -1;

	mypthread_mutex_lock(library_mutex);

	// assign pointer to new id
	if(thread != NULL)
		*thread = new_thread_id;

	// threads[new_thread_id] = malloc(sizeof(tcb));
	tcb* new_thread = calloc(1, sizeof(tcb));
	if (!new_thread) return -1;

	threads[new_thread_id] = new_thread;
	
	// tcb* new_thread = threads[new_thread_id];
	new_thread->ticks = 0;
	new_thread->ret_val = NULL;
	new_thread->to_join = 0;
	new_thread->status = Ready;
	new_thread->id = new_thread_id;

	// create new context for new thread
	new_thread->context = malloc(sizeof(ucontext_t));
	getcontext(new_thread->context);
	ucontext_t* new_context = new_thread->context;
	new_context->uc_stack.ss_sp = calloc(SIGSTKSZ, sizeof(char));
	if (!new_context) return -1;
	new_context->uc_stack.ss_size = SIGSTKSZ;
	new_context->uc_stack.ss_flags = 0;

	VALGRIND_STACK_REGISTER(new_context->uc_stack.ss_sp, new_context->uc_stack.ss_sp + SIGSTKSZ);

	// pass the function and argument as arguments into the function handler
	makecontext(new_context, (void (*)(void)) function_handler, 2, (intptr_t) function, (intptr_t) arg);

	push_queue(threads[new_thread_id], run_queue);

	mypthread_mutex_unlock(library_mutex);

	return 0;
};

/* give CPU possession to other user-level threads voluntarily */
int mypthread_yield() {

	// change thread state from Running to Ready
	// save context of this thread to its thread control block
	// switch from thread context to scheduler context
	threads[curr_thread_id]->status = Yielding;
	push_queue(threads[curr_thread_id], run_queue);
	schedule();

	// YOUR CODE HERE
	return 0;
};

/* terminate a thread */
void mypthread_exit(void *value_ptr) {
	// Deallocated any dynamic memory created when starting this thread
	
	tcb* curr = threads[curr_thread_id];

	mypthread_mutex_lock(library_mutex);

	if (waiting_queue != NULL && *waiting_queue != NULL) {
		// Check waiting threads if their targets have returned
		StackNode** node = waiting_queue;
		StackNode* prev_node = NULL; // prev_node is the previous node in the iteration, not the previously added node in the "stack"
		while (*node != NULL) {
			WaitingPair* pair = (WaitingPair*) (*node)->data;
			if (threads[pair->target_thread]->status == Returned || pair->target_thread == curr_thread_id) {
				threads[pair->waiting_thread]->status = Ready;
				push_queue(threads[pair->waiting_thread], run_queue);
				delete(pair, waiting_queue);
				free(pair);
				pair = NULL;
			} else {
				prev_node = (*node);
				StackNode* next = ((*node)->previous);
				node = &next;
			}
		}
	}

	curr->ret_val = value_ptr; 
	curr->status = Returned;

	mypthread_mutex_unlock(library_mutex);


	// wait for another thread to join this context, after setting status to returned, this thread should never run again
	schedule();
	printf("SHOULD NEVER GEt HERE\n");

};


/* Wait for thread termination */
int mypthread_join(mypthread_t thread, void **value_ptr) {

	//TODO: add error cases (ie. thread does not exist)
	
	if (threads[thread] == NULL) {
		return -1;
	}
	tcb* to_be_joined = threads[thread];

	while (to_be_joined->status != Returned) {
		// wait for the thread to return something;
		//mypthread_yield();


		mypthread_mutex_lock(library_mutex);

		WaitingPair* new_pair = malloc(sizeof(WaitingPair));
		new_pair->waiting_thread = curr_thread_id;
		new_pair->target_thread = thread;
		// push this thread into stack of threads waiting to be joined
		// in schedule function, check each thread that is waiting to see if it's thread has been returned.
		threads[curr_thread_id]->status = Waiting;

		if (waiting_queue == NULL) {
			waiting_queue = malloc(sizeof(StackNode*));
			*waiting_queue = NULL;
		}

		push(new_pair, waiting_queue);

		mypthread_mutex_unlock(library_mutex);
		
		schedule();
	}

	// save return value from other thread
	if (value_ptr != NULL) *value_ptr = to_be_joined->ret_val;

	// free the other thread
	mypthread_mutex_lock(library_mutex);
	VALGRIND_STACK_DEREGISTER(to_be_joined->context->uc_stack.ss_sp);
	free(to_be_joined->context->uc_stack.ss_sp);	
	free(to_be_joined->context);	
	free(to_be_joined);
	threads[thread] = NULL;
	mypthread_mutex_unlock(library_mutex);
	
	return 0;
};

/* initialize the mutex lock */
int mypthread_mutex_init(mypthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr) {
	if (mutex == NULL) return -1;
	mutex->lock = calloc(1, sizeof(char));
	if(!mutex->lock) return -1;
	mutex->blocked = malloc(sizeof(StackNode*));
	*(mutex->blocked) = NULL;
	return 0;
};

/* aquire the mutex lock */
int mypthread_mutex_lock(mypthread_mutex_t *mutex) {
	// use the built-in test-and-set atomic function to test the mutex
	// if the mutex is acquired successfully, enter the critical section
	// if acquiring mutex fails, push current thread into block list and //
	// context switch to the scheduler thread
	int ret = -1;
	while ((ret = __atomic_test_and_set(mutex->lock, __ATOMIC_RELAXED)) != 0) {
		threads[curr_thread_id]->status = Blocked;
		push(threads[curr_thread_id], (mutex->blocked));
		schedule();
	}
	return 0;
};

/* release the mutex lock */
int mypthread_mutex_unlock(mypthread_mutex_t *mutex) {
	// Release mutex and make it available again.
	// Put threads in block list to run queue
	// so that they could compete for mutex later.
	tcb* blocked_thread;
	while ((blocked_thread = pop((mutex->blocked))) != NULL) {
		blocked_thread->status = Ready;
		push_queue(blocked_thread, run_queue);
	}

	__atomic_clear(mutex->lock, __ATOMIC_RELAXED);	
	return 0;
};


/* destroy the mutex */
int mypthread_mutex_destroy(mypthread_mutex_t *mutex) {
	// Deallocate dynamic memory created in mypthread_mutex_init
	free(mutex->lock);
	tcb* blocked_thread;
	while ((blocked_thread = pop((mutex->blocked))) != NULL) {
		blocked_thread->status = Ready;
		push_queue(blocked_thread, run_queue);
	}
	return 0;
};

void print_status() {
	printf("///////////////////////////\n");	
	int i = 0;
	for (i = 0; i < thread_array_size; i++) {
		if (threads[i] == NULL) continue;
		printf("threads[%d]->status: %s\n", i, threads[i]->status == Ready ? "Ready" : threads[i]->status == Running ? "Running" : threads[i]->status == Yielding ? "Yielding" : threads[i]->status == Blocked ? "Blocked" : threads[i]->status == Waiting ? "Waiting" : "Returning");	
	}
	printf("///////////////////////////\n");	

}

static void sched_mlfq();
static void sched_stcf();
/* scheduler */
static void schedule() {
	// Every time when timer interrup happens, your thread library
	// should be contexted switched from thread context to this
	// schedule function
	// schedule policy
	#ifndef MLFQ
		// Choose STCF
		sched_stcf();
	#else
		// Choose MLFQ
	#endif

}

/* Preemptive SJF (STCF) scheduling algorithm */
static void sched_stcf() {
	// Your own implementation of STCF
	// (feel free to modify arguments and return types)

	signal(SIGPROF, SIG_IGN);

	if (threads[curr_thread_id]->status == Running)
		threads[curr_thread_id]->status = Ready;

	if (threads[curr_thread_id]->status != Returned && threads[curr_thread_id]->status != Blocked) 
		push_queue(threads[curr_thread_id], run_queue);
	
	tcb* next_thread;
	do {
		next_thread = (tcb*) pop(run_queue);
	} while (next_thread->status == Yielding);
	
	int curr_thread_holder = curr_thread_id;
	curr_thread_id = next_thread->id;
	
	next_thread->status = Running;

	restart_timer();
	
	// if the scheduler was called by sig, then it will pass context in, otherwise take current context
	swapcontext(threads[curr_thread_holder]->context, next_thread->context);
}

/* Preemptive MLFQ scheduling algorithm */
static void sched_mlfq() {
	// Your own implementation of MLFQ
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE
}

static StackNode* create_node(void* data) {
	StackNode* new_node = malloc(sizeof(StackNode));
	new_node->data = data;
	new_node->previous = NULL;
	return new_node;
}

static void push(void* data, StackNode** top) {
	StackNode* new_node = create_node(data);

	if (top == NULL || *top == NULL) *top = new_node;
	else {
		new_node->previous = *top;
		*top = new_node;
	}
	return;
}

static void delete(void* data, StackNode** top) {
	
	if (*top == NULL) return;
	StackNode* node = *top;
	StackNode* prev = *top;

	if (node->data == data) {
		*top = node->previous;	
		free(node);
		return;
	}

	while (node != NULL && node->data != data) {
		prev = node;
		node = node->previous;
	}
	
	if (node == NULL) return; // data not found
	prev = node->previous;
	free(node);

}

static void* pop(StackNode** top) {
    	if (top == NULL || *top == NULL) return NULL;
	void* data = (*top)->data;
	delete(data, top);
	return data;
}

static void push_queue(tcb* thread, StackNode** top) {

	StackNode* node = *top;

	StackNode* new_node = create_node(thread);

	if (top == NULL) return;
	if (*top == NULL) *top = new_node;
	else if (((tcb*)(*top)->data)->ticks > thread->ticks) {
		new_node->previous = *top;
		(*top) = new_node;
	} else {
		while (node->previous != NULL && ((tcb*)node->previous->data)->ticks < thread->ticks) {
			node = node->previous;	
		}

		new_node->previous = node->previous;
		node->previous = new_node;
	}

}
