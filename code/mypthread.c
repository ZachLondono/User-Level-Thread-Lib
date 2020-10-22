// File:	mypthread.c

// List all group member's name:
// username of iLab:
// iLab Server:

#include "mypthread.h"

static int is_init = 0;

static tcb** threads;
static int last_claimed_id = 0;
static int thread_array_size = 100;
static int curr_thread_id = 0;

static mypthread_mutex_t* threads_mutex = NULL;
static mypthread_mutex_t* queue_mutex = NULL;

static LLNode** waiting_queue = NULL;
static LLNode** run_queue = NULL;

static void schedule();
static void push(void* data, LLNode** root);
static void delete(void* data, LLNode** root);
static void* pop(LLNode** root);
static void push_queue(tcb* thread, LLNode** top);

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
	if (sigaction(SIGPROF, &sa, NULL) != 0) return -1;

	struct itimerval tv;
	tv.it_value.tv_sec = 0;
	tv.it_value.tv_usec = QUANTUM;
	tv.it_interval.tv_sec = 0;
	tv.it_interval.tv_usec = QUANTUM;
	if (setitimer(ITIMER_PROF, &tv, NULL) == -1) return -1;

	return 0;
}

static int next_available() {

	if (!is_init) {

		threads = calloc(thread_array_size, sizeof(tcb*));

		// this mutex will lock operations on the thread array
		threads_mutex = malloc(sizeof(mypthread_mutex_t));
		mypthread_mutex_init(threads_mutex, NULL);

		// this mutex will lock operations on the runqueue
		queue_mutex = malloc(sizeof(mypthread_mutex_t));
		mypthread_mutex_init(queue_mutex, NULL);

		// NULL out array so theycan be chosen when creating new threads
		int i = 0;
		for (i = 0; i < thread_array_size; i++) {
			threads[i] = NULL;
		}

		// stetup timer & signal handler
		if (restart_timer() == -1) return -1;

		// allocate tcb for main thread
		threads[0] = malloc(sizeof(tcb));
		threads[0]->context = calloc(1, sizeof(ucontext_t));
		threads[0]->ticks = 1;
		threads[0]->ret_val = NULL;
		threads[0]->to_join = -1;
		threads[0]->status = Ready;
		threads[0]->id = 0;

		run_queue = malloc(sizeof(LLNode*));
		*run_queue = NULL;
		push_queue(threads[0], run_queue);

		is_init = 1;

	}
	
	mypthread_mutex_lock(threads_mutex);
	int i = 0;
	for (i = last_claimed_id + 1; i < thread_array_size; i++)
		if (threads[i] == NULL) {			
			last_claimed_id = i;
			mypthread_mutex_unlock(threads_mutex);
			return i;
		}

	// If a NULL spot has not been found, we need to increase the size of the array
	const uint THREAD_SIZE_INCRIMENT = 100;
	tcb** new_array = realloc(threads, sizeof(tcb*) * (thread_array_size + THREAD_SIZE_INCRIMENT));
	
	// NULL new slots in thread pool
	if (!new_array) {
		mypthread_mutex_unlock(threads_mutex);
		return -1;
	}
	threads = new_array;
	for (int j = thread_array_size; j < thread_array_size + THREAD_SIZE_INCRIMENT; j++)
		threads[j] = NULL;


	int next_available = thread_array_size;
	thread_array_size += THREAD_SIZE_INCRIMENT;
	last_claimed_id = next_available;

	mypthread_mutex_unlock(threads_mutex);

	return next_available;

}

static void function_handler(intptr_t callback, intptr_t arg) {

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

	mypthread_mutex_lock(threads_mutex);

	// assign pointer to new id
	if(thread != NULL)
		*thread = new_thread_id;

	// threads[new_thread_id] = malloc(sizeof(tcb));
	tcb* new_thread = calloc(1, sizeof(tcb));
	if (!new_thread) {
		mypthread_mutex_unlock(threads_mutex);
		return -1;
	}

	threads[new_thread_id] = new_thread;
	new_thread->ticks = 0;
	new_thread->ret_val = NULL;
	new_thread->to_join = -1;
	new_thread->status = Ready;
	new_thread->id = new_thread_id;

	mypthread_mutex_unlock(threads_mutex);

	// create new context for new thread
	new_thread->context = malloc(sizeof(ucontext_t));
	getcontext(new_thread->context);
	ucontext_t* new_context = new_thread->context;
	new_context->uc_stack.ss_sp = calloc(SIGSTKSZ, sizeof(char));
	if (!new_context) return -1;
	new_context->uc_stack.ss_size = SIGSTKSZ;
	new_context->uc_stack.ss_flags = 0;

	// VALGRIND_STACK_REGISTER(new_context->uc_stack.ss_sp, new_context->uc_stack.ss_sp + SIGSTKSZ);

	// enqueue the new thread
	mypthread_mutex_lock(queue_mutex);
	push_queue(threads[new_thread_id], run_queue);
	mypthread_mutex_unlock(queue_mutex);

	// pass the function and argument as arguments into the function handler
	makecontext(new_context, (void (*)(void)) function_handler, 2, (intptr_t) function, (intptr_t) arg);

	return 0;
};

/* give CPU possession to other user-level threads voluntarily */
int mypthread_yield() {
	// Change state and call scheduler to go to another thread
	threads[curr_thread_id]->status = Yielding;
	schedule();
	return 0;
};

/* terminate a thread */
void mypthread_exit(void *value_ptr) {

	mypthread_mutex_lock(threads_mutex);

	tcb* curr = threads[curr_thread_id];

	if (curr->to_join != -1 && threads[curr->to_join]->status == Waiting) {
		// If there is a thread waiting on this thread to exit, put it back into run queue
		threads[curr->to_join]->status = Ready; 
		mypthread_mutex_lock(queue_mutex);
		push_queue(threads[curr->to_join], run_queue);
		mypthread_mutex_unlock(queue_mutex);
	}

	// Store return value to be retrieved by joined thread
	curr->ret_val = value_ptr; 
	curr->status = Returned;

	mypthread_mutex_unlock(threads_mutex);

	// wait for another thread to join this context, after setting status to returned, this thread should never run again
	schedule();
	printf("SHOULD NEVER GEt HERE\n");

};


/* Wait for thread termination */
int mypthread_join(mypthread_t thread, void **value_ptr) {
		
	if (threads[thread] == NULL) return ESRCH;
	if (threads[curr_thread_id]->to_join == thread) return EDEADLK;
	if (threads[thread]->to_join != -1) return EINVAL;

	mypthread_mutex_lock(threads_mutex);

	tcb* to_be_joined = threads[thread];
	to_be_joined->to_join = curr_thread_id;

	while (to_be_joined->status != Returned) {
		// If thread has not returned, this thread will wait until it has
		threads[curr_thread_id]->status = Waiting;
		mypthread_mutex_unlock(threads_mutex);		// unlock the mutex when it schedules next thread
		schedule();
		mypthread_mutex_lock(threads_mutex);		// relock mutex when this thread is woken
	}

	// save return value from other thread
	if (value_ptr != NULL) *value_ptr = to_be_joined->ret_val;

	// free the other thread
	// VALGRIND_STACK_DEREGISTER(to_be_joined->context->uc_stack.ss_sp);
	free(to_be_joined->context->uc_stack.ss_sp);	
	free(to_be_joined->context);	
	free(to_be_joined);
	threads[thread] = NULL;
	mypthread_mutex_unlock(threads_mutex);
	
	return 0;
};

/* initialize the mutex lock */
int mypthread_mutex_init(mypthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr) {
	if (mutex == NULL) return -1;

	// lock will track whether mutex is locked/unlocked
	mutex->lock = calloc(1, sizeof(char));
	if(!mutex->lock) return -1;

	// blocked is a LL of blocked nodes
	mutex->blocked = malloc(sizeof(LLNode*));
	*(mutex->blocked) = NULL;
	return 0;
};

/* aquire the mutex lock */
int mypthread_mutex_lock(mypthread_mutex_t *mutex) {
	int ret = -1;
	while ((ret = __atomic_test_and_set(mutex->lock, __ATOMIC_RELAXED)) != 0) {
		// push current thread into block list and
		threads[curr_thread_id]->status = Blocked;
		push(threads[curr_thread_id], (mutex->blocked));
		// call scheduler
		schedule();
	}
	// if ret == 0, lock was aquired
	return 0;
};

/* release the mutex lock */
int mypthread_mutex_unlock(mypthread_mutex_t *mutex) {
	tcb* blocked_thread;
	while ((blocked_thread = pop((mutex->blocked))) != NULL) {
		// Put threads in block list to run queue
		blocked_thread->status = Ready;
		// Maybe need a mutex lock here?
		push_queue(blocked_thread, run_queue);
	}

	// Release mutex and make it available again.
	__atomic_clear(mutex->lock, __ATOMIC_RELAXED);	
	return 0;
};


/* destroy the mutex */
int mypthread_mutex_destroy(mypthread_mutex_t *mutex) {
	// Deallocate dynamic memory created in mypthread_mutex_init
	free(mutex->lock);
	tcb* blocked_thread;
	while ((blocked_thread = pop((mutex->blocked))) != NULL) {
		// put all blocked queues back onto the run queue
		// will only happen if the mutex is destroyed while locked
		blocked_thread->status = Ready;
		// Maybe need a mutex lock here?
		push_queue(blocked_thread, run_queue);
	}
	free(mutex->blocked);
	return 0;
};


// debugging function
static void print_status() {
	mypthread_mutex_lock(threads_mutex);
	printf("///////////////////////////\n");	
	int i = 0;
	for (i = 0; i < thread_array_size; i++) {
		if (threads[i] == NULL) continue;
		printf("threads[%d]->status: %s\n", i, threads[i]->status == Ready ? "Ready" : threads[i]->status == Running ? "Running" : threads[i]->status == Yielding ? "Yielding" : threads[i]->status == Blocked ? "Blocked" : threads[i]->status == Waiting ? "Waiting" : "Returning");	
	}
	printf("///////////////////////////\n");	
	mypthread_mutex_unlock(threads_mutex);

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

	// if the scheduler was started in a way other than the signal handler
	// we must make sure that the timer does not interrupt 
	signal(SIGPROF, SIG_IGN);

	tcb* curr = threads[curr_thread_id];
	tcb* next_thread = NULL;

	// if scheduler is called by a thread which is in a yielding, waiting, or blocked state, do not enqueue it yet
	if (curr->status == Ready || curr->status == Running) {
		threads[curr_thread_id]->status = Ready;
		push_queue(threads[curr_thread_id], run_queue);
	}
	
	// poping from the run queue (a priority queue) will return the thread with the 
	// least number of "ticks" (number of quantums it has run for)
	next_thread = (tcb*) pop(run_queue);

	// if the thread queue is empty, we must just start this thread again
	// this may result in undefined behavior (unless current thread is just yielding, in which case the yield will basically be ignored)
	// because the current thread might have just exited or is waiting for another thread to join it which will never happen if queue is empty
	if (next_thread == NULL) next_thread = curr;

	// if current thread is yielding, don't enque it until after new thread is chosen so it does not get called again (unless queue is empty)
	if (threads[curr_thread_id]->status == Yielding) push_queue(threads[curr_thread_id], run_queue);

	curr_thread_id = next_thread->id;	
	next_thread->status = Running;

	// restarting the timer will ensure the next thread will get a full time quantum
	restart_timer();
	
	// if the scheduler was called by sig, then it will pass context in, otherwise take current context
	swapcontext(curr->context, next_thread->context);
}

static void sched_mlfq() {};

/* 
	LL and priority queue functions below
*/

static LLNode* create_node(void* data) {
	LLNode* new_node = malloc(sizeof(LLNode));
	new_node->data = data;
	new_node->next = NULL;
	return new_node;
}

static void push(void* data, LLNode** top) {
	LLNode* new_node = create_node(data);

	if (top == NULL || *top == NULL) *top = new_node;
	else {
		new_node->next = *top;
		*top = new_node;
	}
	return;
}

static void delete(void* data, LLNode** top) {
	
	if (*top == NULL) return;
	LLNode* node = *top;
	LLNode* prev = *top;

	if (node->data == data) {
		*top = node->next;	
		free(node);
		return;
	}

	while (node != NULL && node->data != data) {
		prev = node;
		node = node->next;
	}
	
	if (node == NULL) return; // data not found
	prev = node->next;
	free(node);

}

static void* pop(LLNode** top) {
    if (top == NULL || *top == NULL) return NULL;
	void* data = (*top)->data;
	delete(data, top);
	return data;
}

static void push_queue(tcb* thread, LLNode** top) {

	LLNode* node = *top;

	LLNode* new_node = create_node(thread);

	if (top == NULL) return;
	if (*top == NULL) *top = new_node;
	else if (((tcb*)(*top)->data)->ticks > thread->ticks) {
		new_node->next = *top;
		(*top) = new_node;
	} else {
		while (node->next != NULL && ((tcb*)node->next->data)->ticks < thread->ticks) {
			node = node->next;	
		}

		new_node->next = node->next;
		node->next = new_node;
	}

}
