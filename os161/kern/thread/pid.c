#include <types.h>
#include <lib.h>
#include <array.h>
#include <pid.h>
#include <thread.h>
#include <synch.h>
#include <kern/limits.h>
#include <kern/errno.h>

struct lock *pid_lock;
struct array *pid_array;
struct array *pid_map;

int num_proc;

/* Function called during bootup, creating the first process */
struct pid *pid_bootstrap() {
	struct pid *start_pid = pid_create("Start process");
	if (start_pid==NULL) panic("Cannot create starting pid\n");

	start_pid->pid = 1;
	start_pid->ppid = NULL;
	num_proc = 1;

	pid_array = array_create();
	if (pid_array == NULL) {
		panic("Cannot create pid array\n");
		//return NULL;
	}
	pid_map = array_create();
	if (pid_map == NULL) {
		panic("Cannot create pid mapping array\n");
		//return NULL;
	}
	int err = array_preallocate(pid_array, PROC_MAX+1);
	if (err) {
		panic("Cannot preallocate pid array\n");
		//return NULL;
	}
	err = array_preallocate(pid_map, PROC_MAX+1);
	if (err) {
		panic("Cannot preallocate pid mapping array\n");
		//return NULL;
	}

	// initialize array
	array_add(pid_array, 0);
	array_add(pid_map, NULL);
	array_add(pid_array, (void*)1); 
	//array_add(pid_array, (void*)1);
	//array_add(pid_map, (void*)start_pid);
	array_add(pid_map, (void*)start_pid);
	int i;
	for (i = 2; i <= PROC_MAX; i++) {
	       array_add(pid_array, (void*)(-1));
	       array_add(pid_map, NULL);
	}

	// create lock for pid operations
	pid_lock = lock_create("pid lock");

	return start_pid;
	
}

struct pid *pid_create(const char *name) {
	struct pid *pid = kmalloc(sizeof(struct pid));
	if (pid==NULL) {
		return NULL;
	}
	pid->p_name = kstrdup(name);
	 
	if (pid->p_name==NULL){
		kfree(pid);
		return NULL;
	}
	pid->pid = -1;
	pid->ppid = NULL;

	pid->state = P_NEW;
	pid->p_cv = NULL;

	pid->p_lock = NULL;
	return pid;
}

/* Acquires a valid pid from the list of available pids */
int pid_acquire(struct pid *pid) {
	assert(pid != NULL);

	lock_acquire(pid_lock);

	if (num_proc == PROC_MAX) {
		lock_release(pid_lock);
		return EAGAIN;
	}

	int i;
	for (i = 2; i <= PROC_MAX; i++) {
		int available = (int) array_getguy(pid_array,i);
		if (available==-1) {
			// pid i is available for use
			array_setguy(pid_array, i, (void*)1);
			
			num_proc++;
			
			pid->pid = i;
			array_setguy(pid_map, i, (void*) pid);
			//ADDED- ONLY AFTER i valid I create sem
			//so I don't need to free it in case of error		   	
			//pid->killMe = sem_create("Exited",0);
			pid->p_cv = cv_create("Process CV");
			pid->p_lock = lock_create("Process Lock");

			lock_release(pid_lock);
			return 0;
		}
	}

	// if gets to here, something went wrong

	lock_release(pid_lock);
	panic("Problem in pid acquire");
	return -1;
}

/*
 * Reminder to write pid_destroy
 */
void
pid_destroy(struct pid *pid) {
	assert(pid!=NULL);
	assert(pid->state == P_TERMINATED);

	lock_acquire(pid_lock);
	num_proc--;
	array_setguy(pid_array,pid->pid, (void*)(-1));
	array_setguy(pid_map, pid->pid, NULL);
	kfree(pid->p_name);
	//sem_destroy(pid->killMe); // warned implicit declaration. WHY
	if (pid->p_cv)cv_destroy(pid->p_cv);
	if (pid->p_lock) lock_destroy(pid->p_lock);
	kfree(pid);
	lock_release(pid_lock);
}

/* Gives up current process, not destroying it */
void pid_giveup(struct pid *pid) {
	lock_acquire(pid_lock);
	int num = (int) array_getguy(pid_array, pid->pid);
	assert(num > 0);
	array_setguy(pid_array, pid->pid, (void*)(num-1));
	
	if (num-1==0){
		pid->state = P_TERMINATED;
	}
	lock_release(pid_lock);
}

/*
 * Reminder to write pid_shutdown
 */
void
pid_shutdown(void) {
	assert(num_proc==0);

	array_destroy(pid_array);
	array_destroy(pid_map);
	lock_destroy(pid_lock);
}

/**/
void pid_newthread(struct pid* pid) {
	lock_acquire(pid_lock);
	int num = (int)array_getguy(pid_array, pid->pid);
	array_setguy(pid_array, pid->pid, (void*)(num+1));
	lock_release(pid_lock);
}

/**/
struct pid *pid_getstruct(int pid) {
	if ((int)array_getguy(pid_array, pid)==-1) return NULL;
	return (struct pid*)array_getguy(pid_map, pid);
}


