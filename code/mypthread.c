// File:	mypthread.c

// List all group member's name:
// username of iLab:
// iLab Server:

#include "mypthread.h"

static int is_init = 0;
static tcb** threads;
static int thread_array_size = 100;
static int curr_thread_id = 0;
static StackNode** waiting_queue = NULL;

static void schedule();
static void push(void* data, StackNode** root);
static void* pop(StackNode** root);
static void* peek(StackNode** root);

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
	
	struct itimerval tv;
	tv.it_value.tv_sec = 0;
	tv.it_value.tv_usec = 500;
	tv.it_interval.tv_sec = 0;
	tv.it_interval.tv_usec = 500;
	if (setitimer(ITIMER_REAL, &tv, NULL) == -1) return -1;
	
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
		threads[0]->ticks = 1;
		threads[0]->ret_val = NULL;
		threads[0]->to_join = 0;
		// TODO: this thread control block should be freed when main returns

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
	mypthread_exit(ret_val);
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
	new_thread->ticks = 0;
	new_thread->ret_val = NULL;
	new_thread->to_join = 0;

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
	threads[curr_thread_id]->status = Yielding;
	schedule();

	// YOUR CODE HERE
	return 0;
};

/* terminate a thread */
void mypthread_exit(void *value_ptr) {
	// Deallocated any dynamic memory created when starting this thread
	
	tcb* curr = threads[curr_thread_id];

	curr->ret_val = value_ptr; 
	curr->status = Returned;

	if (waiting_queue != NULL) {
		// Check waiting threads if their targets have returned
		StackNode* node = *waiting_queue;
		StackNode* prev_node = node; // prev_node is the previous node in the iteration, not the previously added node in the "stack"
		while (node != NULL) {
			WaitingPair* pair = (WaitingPair*) node->data;
			if (threads[pair->target_thread]->status == Returned) {
				threads[pair->waiting_thread]->status = Ready;
				prev_node->previous = node->previous;
				free(node->data);
				free(node);
			}
			prev_node = node;
			node = node->previous;
		}
	}

	// wait for another thread to join this context, after setting status to returned, this thread should never run again
	schedule();
	printf("SHOULD NEVER GEt HERE\n");

/*
	// Free this threads context, it is no longer needed
	free(curr->context->uc_stack.ss_sp);
	free(curr->context);

	// Tag as exited so that it is no longer run. This tcb will be freed by the thread which joins it
	curr->status = Exited;

	// loop endlessly until this thread is freed 
	while(1){mypthread_yield();}
*/
};


/* Wait for thread termination */
int mypthread_join(mypthread_t thread, void **value_ptr) {

	//TODO: add error cases (ie. thread does not exist)
	
	if (threads[thread] == NULL) printf("Attempting to join thread that doesn't exist\n");
	tcb* to_be_joined = threads[thread];

	while (to_be_joined->status != Returned) {
		// wait for the thread to return something;
		//mypthread_yield();
		
		// push this thread into stack of threads waiting to be joined
		// in schedule function, check each thread that is waiting to see if it's thread has been returned.
		threads[curr_thread_id]->status = Waiting;
		
		WaitingPair* new_pair = malloc(sizeof(WaitingPair));
		new_pair->waiting_thread = curr_thread_id;
		new_pair->target_thread = thread;

		if (waiting_queue == NULL) waiting_queue = malloc(sizeof(StackNode*));

		push(new_pair, waiting_queue);
		schedule();
	}

	// save return value from other thread
	if (value_ptr != NULL) *value_ptr = to_be_joined->ret_val;

	// free the other thread
	free(to_be_joined->context->uc_stack.ss_sp);	
	free(to_be_joined->context);	
	free(to_be_joined);
	
	return 0;
};

/* initialize the mutex lock */
int mypthread_mutex_init(mypthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr) {
	if (mutex == NULL) return -1;
	mutex->lock = malloc(sizeof(char));
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
		// mypthread_yield();
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
	}
	return 0;
};

void print_status() {
	printf("///////////////////////////\n");	
	int i = 0;
	for (i = 0; i < thread_array_size; i++) {
		if (threads[i] == NULL) continue;
		printf("threads[%d]->status: %s\n", i, threads[i]->status == Ready ? "Ready" : threads[i]->status == Running ? "Running" : threads[i]->status == Yielding ? "Yielding" : "Returning");	
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

	int lowest_ticks = -1;
	int next_thread_id = -1;
	
	if (threads[curr_thread_id]->status == Running) threads[curr_thread_id]->status = Ready;

	int i = 0;
	for (i = 0; i < thread_array_size; i++) {
		if (threads[i] == NULL) continue;
		if (threads[i]->status == Returned || 
			threads[i]->status == Blocked ||
			threads[i]->status == Waiting) continue;
		if (threads[i]->status == Yielding) {
			threads[i]->status = Ready;
			continue;
		}else if (lowest_ticks != -1 && (threads[i]->ticks >= lowest_ticks || threads[i]->status != Ready)) continue;
		next_thread_id = i;
		lowest_ticks = threads[i]->ticks;
	}

	threads[next_thread_id]->ticks++;
	
	int curr_thread_holder = curr_thread_id;
	curr_thread_id = next_thread_id;

	threads[next_thread_id]->status = Running;

	swapcontext(threads[curr_thread_holder]->context, threads[next_thread_id]->context);
}

/* Preemptive MLFQ scheduling algorithm */
static void sched_mlfq() {
	// Your own implementation of MLFQ
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE
}

static void push(void* data, StackNode** top) {

    StackNode* new_node = malloc(sizeof(StackNode));
    new_node->data = data;
    new_node->previous = NULL;

    if (top == NULL || *top == NULL) *top = new_node;
    else {
        new_node->previous = *top;
        *top = new_node;
    }

    return;

}

static void* pop(StackNode** top) {

    if (top == NULL || *top == NULL) return NULL;

    StackNode* new_top = (*top)->previous;
    void* temp_data = (*top)->data;
    //free(*top);

    *top = new_top;

    return temp_data;

}
