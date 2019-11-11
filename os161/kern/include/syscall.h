#ifndef _SYSCALL_H_
#define _SYSCALL_H_

//#include <machine/trapframe.h>
/*
 * Prototypes for IN-KERNEL entry points for system call implementations.
 */

int sys_reboot(int code);

/*
 * Syscall prototypes.
 */
int sys_fork(void *tf, int *retval);
int sys_exit(int code);
int sys_execv(char *program, char**args);
int sys_read(int fd, void *buf,size_t buflen, int* retval_);
int sys_write(int fd,void *buf,size_t buflen, int* retval_);
int sys_getpid(int* retval);
int sys_waitpid(pid_t pid, int *status, int options, int user);

int sys__time(userptr_t seconds, userptr_t nanoseconds, int stackptr, int *retval);
int sys_sbrk(intptr_t amount, int* retval, intptr_t stackptr);

#endif /* _SYSCALL_H_ */
