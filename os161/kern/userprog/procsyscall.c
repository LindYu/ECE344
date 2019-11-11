#include <types.h>
#include <syscall.h>
#include <lib.h>
#include <machine/pcb.h>
#include <machine/spl.h>
#include <machine/trapframe.h>
#include <thread.h>
#include <curthread.h>
#include <pid.h>
#include <synch.h>
#include <vm.h>
#include <vfs.h>
#include <addrspace.h>
#include <kern/limits.h>
#include <kern/unistd.h>
#include <kern/errno.h>
#include <kern/ioctl.h>
#include <machine/vm.h>
#include <machine/coremap.h>

/*
 * Syscall for fork().
 *
 * Returns error number if error, 0 if fork success.
 * Parameters: trapframe in which syscall is called, retval 
 * 	to store return value.
 */


int 
sys_fork(void *tf, int *retval) {
	struct thread *ret;

	struct trapframe* newtf = kmalloc(sizeof(struct trapframe));
	while (newtf==NULL) {
		*retval = -1;
		return ENOMEM;
		thread_yield();
		newtf = kmalloc(sizeof(struct trapframe));
	}

	*newtf = *(struct trapframe*)tf;
	
	//kprintf("sysfork: curthread->t_vmspace->prog_name is %s\n", curthread->t_vmspace->prog_name);

	int errno = thread_fork("forked process", newtf, (unsigned long)(curthread->t_vmspace), md_forkentry, &ret);

	if (errno) {
		*retval = -1;
	//	lock_release(write_lock);
		return errno;
	}
//	lock_acquire(write_lock);
	*retval = ret->t_pid->pid;
//	lock_release(write_lock);
	//memcpy(retval, &ret->t_pid->pid, sizeof(int));
	return 0;
}

/*
 * Syscall for getpid().
 */
int sys_getpid(int32_t *retval){
	*retval = curthread->t_pid->pid;
	return 0;
}
/*
 * Syscall for waitpid(). Here is implemented such that the current process can only wait on its child
 * Here exited is defined as terminated as a zombie, and does not exist is defined as pid recycled. 
 */
int sys_waitpid(pid_t pid, int *status, int options, int user){
//	lock_acquire(write_lock);
	if(pid <= 0 || pid > PROC_MAX || pid == curthread->t_pid->pid) {
	//	assert(curthread->t_pid->pid != pid);
//		lock_release(write_lock);
		// kprintf("I am %d, waiting for %d\n",curthread->t_pid->pid,pid);
		return EINVAL; // no such process error code not defined yet	 
    }

	// GET PID BLOCK 
	struct pid *child = pid_getstruct(pid);	
	if (child == NULL) {
//		lock_release(write_lock);
		return EINVAL;
	}

	if(options != 0) {
//		lock_release(write_lock);
		return EINVAL; // other than 0 not supported
	}
	/*REMINDER TO FIND OUT KERNEL ADDRESS SPACE AND CHECK IF STATUS FALLS WITHIN THE RANGE FOR ERROR CHECKING*/
	//have to parse in stack pointer again...
	if (user==1 && (uintptr_t)status >= USERTOP) {
//		lock_release(write_lock);
		return EFAULT;
	}
	//kprintf("sp = %d, status = %d\n", stackptr);
	if ((uintptr_t)status == (uintptr_t)0x40000000) {
//		lock_release(write_lock);
		return EFAULT;
	}
	if(status == NULL || (uintptr_t)status % 4 != 0) {
//		lock_release(write_lock);
		return EFAULT; // NULL or not aligned
	}

    if(curthread->t_pid->pid != child->ppid->pid) {
 //   	lock_release(write_lock);
		return EINVAL; //only parent can wait for child
	} 
//	lock_release(write_lock);
	// return immediately if the child has exited - if the thread has been destroyed? 
	
	lock_acquire(child->p_lock);
	while (child->state != P_TERMINATED) {
		
		cv_wait(child->p_cv, child->p_lock);
	}


	*status = child->exitcode;

	lock_release(child->p_lock);
	pid_destroy(child);
	
	return 0;
}
/*
 * Syscall for _exit(). Given exitcode, clean and give up the pid to use.
 * Here, we assume as long as the parent is alive, it's interested in the
 * exitcode of the child process. Therefore, if parent is terminated (dead/zombie)
 *  we may destroy the child.  
 */
int sys_exit(int exitcode){
	// Check parent state, if parent dead, pass exitcode to grandpa and die 
	//pid_giveup(curthread->t_pid);
	
	lock_acquire(curthread->t_pid->p_lock);	
	curthread->t_pid->state = P_TERMINATED;
		
	curthread->t_pid->exitcode = exitcode;

	cv_broadcast(curthread->t_pid->p_cv, curthread->t_pid->p_lock);
	lock_release(curthread->t_pid->p_lock);

	//if (curthread->t_pid->ppid->state == P_TERMINATED) pid_destroy(curthread->t_pid);
		
	thread_exit();
	// exit should not return
	return 0;
}	
/*
 * Syscall for execv. Execv replaces the currently executing program with a newly loaded
 * program image. 
 */
int sys_execv(char *program, char**args){
	if (program == NULL) return EFAULT;
	int err, argc = 0; 

	if ((uintptr_t)program == (uintptr_t)0x40000000) return EFAULT;
	 
	if((uintptr_t)program >= USERTOP) return EFAULT;
	if(strcmp(program,"") == 0) return EINVAL;
	if ((uintptr_t)args == (uintptr_t)0x40000000) return EFAULT;
	if (args==NULL) return EFAULT;
	if ((uintptr_t)args >= USERTOP) return EFAULT;
	// count argc	 
	while(args[argc]) argc++;
	
	char **argv = (char**)kmalloc((argc+1)*sizeof(char*));

	if (argv==NULL) return ENOMEM;
	
	// copy arguments
	int ind = 0;
	while (ind < argc) {
		if (args[ind]==NULL) return EFAULT;
		if ((uintptr_t)args[ind]==(uintptr_t)0x40000000) return EFAULT;
		if ((uintptr_t)args[ind] >= USERTOP) return EFAULT;
		int len = strlen(args[ind])+1;
		if (len%4!=0) len += 4-len%4;
		
		argv[ind] = (char*)kmalloc((len)*sizeof(char));
		if (argv[ind]==NULL) {
			kfree(argv);
			return ENOMEM;
		}

		len = strlen(args[ind])+1;
		err = copyin((const_userptr_t)(args[ind]), argv[ind], len);
		if (err) {
			while (ind >= 0) {
				kfree(argv[ind]);
				ind--;
			}
			kfree(argv);
			return EFAULT;
		}

		ind++;
	}
	argv[ind] = NULL;	// ind = argc

//	kprintf("finished copy\n");

	// Open the executable, create a new address space and load elf into it
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;
	char * prog_name = kstrdup (program);
	/* Open the file. */
	result = vfs_open(program, O_RDONLY, &v);
	if (result) {
		while (argc >= 0) {
			kfree(argv[argc]);
			argc--;
		}
		kfree(argv);
		return result;
	}

	if (curthread->t_vmspace != NULL)
		as_destroy(curthread->t_vmspace);

	/* Create a new address space. */
	curthread->t_vmspace = as_create();
	if (curthread->t_vmspace==NULL) {
		return ENOMEM;
	}

	/* Activate it. */
	as_activate(curthread->t_vmspace);

	//kprintf("activated as\n");

	curthread->t_vmspace->prog_name = kstrdup(prog_name);
//	assert (strlen(program) != 0);

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* thread_exit destroys curthread->t_vmspace */
		vfs_close(v);
		return result;
	}

	/* Done with the file now. */
	vfs_close(v);

	//Copy the arguments from kernel buffer into user stack

	/* Define the user stack in the address space */
	result = as_define_stack(curthread->t_vmspace, &stackptr);
	if (result) {
		/* thread_exit destroys curthread->t_vmspace */
		return result;
	}

	//kprintf("defined stack\n");

	userptr_t tempptr = (userptr_t)stackptr;

	ind = argc-1;
	while (ind >= 0) {
		int len = strlen(argv[ind])+1;
		if (len % 4)
			len += 4-len%4;

		tempptr -= len;
		err = copyoutstr(argv[ind],tempptr,len, NULL);
		if(err){
	//		kprintf("err exec\n");
			while (ind >= 0) {
				kfree(argv[ind]);
				ind--;
			}
			kfree(argv);
			return err;
		}
		kfree(argv[ind]);

		argv[ind] = (char*)tempptr;

		ind--;
	}

	tempptr -= (argc+1)*sizeof(char*);
	err = copyout(argv,tempptr,(argc+1)*sizeof(char*));
	if(err){
		kfree(argv);
		return err;
	}

	stackptr = (int)tempptr;

	//kprintf("copied out\n");
	kfree(argv);

	// Return user mode

	md_usermode(argc,(userptr_t)stackptr,stackptr, entrypoint);
	
	/* md_usermode does not return */
	panic("md_usermode returned\n");
	return EINVAL;

}

vaddr_t get_kvaddr (struct addrspace *as, vaddr_t vaddr_u) {
	vaddr_t pt1, pt2;
	pt1 = vaddr_u >> 22;
	pt2 = (vaddr_u >> 12) & (0x3ff);
	
	if (as->page_dir[pt1].paddr == 0) return 0;
	
	struct pt_entry *secondary = (struct pt_entry*) PADDR_TO_KVADDR((as->page_dir[pt1].paddr)>>1);
	if (secondary[pt2].paddr == 0) return 0;
	
	return PADDR_TO_KVADDR(secondary[pt2].paddr>>1);
}

 

int sys_sbrk(intptr_t amount, int* retval, intptr_t stackptr){
	struct addrspace *as;
	as = curthread->t_vmspace;
	//kprintf("heap_start - heap_end: 0x%x amount: %d\n", as->heap_start - as->heap_end, amount);
	if ((int)amount < (int)(as->heap_start - as->heap_end)) return EINVAL;
	*retval = as->heap_end;
	vaddr_t temp;
	//u_int32_t pt2;
	//int pt1;
	if (amount==0) {
		*retval = as->heap_end;
		assert(as->heap_end !=0);
		assert(*retval != 0);
		return 0;
	}
	
	/*if((amount%PAGE_SIZE)) {
		//return EINVAL;
		amount += PAGE_SIZE - amount%PAGE_SIZE;
	}*/
	as = curthread->t_vmspace;
	temp = as->heap_end + amount;


/*
	//user to kernel address space translation

	as->heap_end 
	as->page_dir
*/
	(void) stackptr;

	if((int)temp < (int)as->heap_start) return EINVAL;	
	if(temp >= (vaddr_t)as->user_stack || temp - (vaddr_t)as->heap_start > 20*PAGE_SIZE) return ENOMEM;
	
	vaddr_t kvaddr;
	while (amount < 0 && ((temp & PAGE_FRAME) != (as->heap_end & PAGE_FRAME))){
		kvaddr = get_kvaddr (as, as->heap_end);
		if (kvaddr != 0) {
			free_cm_entry(kvaddr);
		}
		as->heap_end -= PAGE_SIZE;
	}
	
	as->heap_end = temp;	 


	return 0;
}



































