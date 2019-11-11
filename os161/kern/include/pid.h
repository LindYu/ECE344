#ifndef _PID_H_
#define _PID_H_

#include <types.h>
#include <pid.h>
#include <thread.h>
#include <synch.h>
/*	 Macros for the state of process. 	*/
typedef enum {
	P_NEW,
	P_TERMINATED
} processstate_t;

/*
 * Structure for pid
 */
struct pid {
	char *p_name;
	pid_t pid;
	// Process related variables
	struct pid *ppid; //parent pid 
	int state;
	struct cv *p_cv;
	struct lock *p_lock;
	//struct semaphore *killMe;
//	int exited; //a boolean
	int exitcode;
	//struct thread *myThread; //used in wait	
};

/*
 * Functions for pid:
 * 	pid_bootstrap: 	called upon boot up
 * 	pid_create:	creates a new process, returns its pid
 * 			NULL if fail
 * 	pid_acquire:	acquire the next available pid, returns error code
 * 	pid_giveup:	gives up the current pid, if no threads on pid, set state to terminated
 * 	pid_destroy:	destroys pid, returns pid to available pids, needs to be called after pid is terminated
 * 	pid_shutdown:	called at shutdown, needs to be called after all processes are destroyed.
 * 	pid_newthread:	called when new thread is created in the process
 * 	pid_getstruct:	returns the pid structure corresponsding to the pid number
 */

/* Call during startup to allocate structures. */
struct pid *pid_bootstrap(void);

/* create a new process */
struct pid *pid_create(const char *name);

/* Get the next available pid and allocate. */
int pid_acquire(struct pid *process);

/* Deallocate the given pid */
void pid_destroy(struct pid *pid);

/* Terminates the given pid */
void pid_giveup(struct pid *pid);

/* Called at shutdown */
void pid_shutdown(void);

/* New thread created in the process */
void pid_newthread(struct pid *pid);

/* Returns the pid structure */
struct pid *pid_getstruct(int pid);

#endif /* _PID_H_ */
