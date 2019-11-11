#include <types.h>
#include <kern/errno.h>
#include <kern/ioctl.h>
#include <lib.h>
#include <thread.h>
#include <curthread.h>
#include <addrspace.h>
#include <vm.h>
#include <vfs.h>
#include <syscall.h>
#include <test.h>
#include <synch.h>
#include <uio.h>
#include <kern/unistd.h>
#include <kern/limits.h>
#include <vnode.h>
#include <generic/console.h>
#include <machine/trapframe.h>

 

int sys_read(int fd,void *buf, size_t buflen,int *retval_) {
	//lock_acquire(write_lock);
	int err = 0;
	// error checking
	if(fd != 0) {
		*retval_ = -1;
	//	lock_release(write_lock);
        return EBADF;
	} 
	
	char *kbuf = kmalloc(buflen*sizeof(char));
	char c;
	size_t ind = 0;
	while (ind < buflen) {
		c = getch();
		if (c == '\r') c = '\n';
		kbuf[ind] = c;
		ind++;
	}

	err = copyout((void*)kbuf, (userptr_t)buf, buflen);
	if(err){
		//kprintf("Error code: %d\n", err);
		*retval_ = -1;
		kfree(kbuf);
	//	lock_release(write_lock);
		return EFAULT;
	}

	kfree(kbuf);
	*retval_ = buflen;
//	lock_release(write_lock);
	return 0;

}


int sys_write(int fd,void *buf,size_t buflen, int* retval_){
 
//	lock_acquire(write_lock);
	 
	int err = 0;
	
	char *kbuf = kmalloc(buflen);

	if(fd != 1 && fd != 2){
			kfree(kbuf);
			*retval_ = -1;
		//	lock_release(write_lock);
          	return EBADF;	  
	}
	 
  	if((err = copyin((const_userptr_t)buf,kbuf,buflen) !=0)){
	   kfree(kbuf);
		*retval_ = -1;
	//	lock_release(write_lock);
	   return EFAULT;
	}	 

	size_t ind = 0;
	while (ind < buflen) {
		kprintf("%c",kbuf[ind]);
		ind++;
	}
  
	*retval_ = buflen;

	kfree(kbuf);
//	lock_release(write_lock);
	
    return 0;

}

