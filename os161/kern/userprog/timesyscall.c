#include <types.h>
#include <syscall.h>
#include <kern/errno.h>
#include <clock.h>
#include <lib.h>
#include <thread.h>
#include <curthread.h>
#include <addrspace.h>

int sys__time(userptr_t seconds, userptr_t nanoseconds, int stackptr, int* retval) {
	//if ((uintptr_t)seconds < (uintptr_t)stackptr || (uintptr_t)seconds > curthread->t_vmspace->heap_end) return EFAULT;
	//if ((uintptr_t)nanoseconds < (uintptr_t)stackptr || (uintptr_t)seconds > curthread->t_vmspace->heap_end) return EFAULT;
	(void) stackptr;
	time_t sec;
	u_int32_t nano;
	gettime(&sec, &nano);
	//kprintf("time: %d.%d\n",sec,nano);
	int err;
	if (seconds != 0) {
		err = copyout(&sec, seconds, sizeof(time_t));
		if (err) return err;
	}

	if (nanoseconds != 0) {
		err = copyout(&nano, nanoseconds, sizeof(u_int32_t));
		if (err) return err;
	}

	*retval = sec;

	return 0;
}
