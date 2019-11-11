/*
 * Sample/test code for running a user program.  You can use this for
 * reference when implementing the execv() system call. Remember though
 * that execv() needs to do more than this function does.
 */

#include <types.h>
#include <kern/unistd.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <thread.h>
#include <curthread.h>
#include <vm.h>
#include <vfs.h>
#include <test.h>

/*
 * Load program "progname" and start running it in usermode.
 * Does not return except on error.
 *
 * Calls vfs_open on progname and thus may destroy it.
 */
int
runprogram(char *progname, unsigned long argc, char** args)
{
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;
	char** argv;

	/* Open the file. */
	result = vfs_open(progname, O_RDONLY, &v);
	if (result) {
		return result;
	}

	/* We should be a new thread. */
	//assert(curthread->t_vmspace == NULL);
	if (curthread->t_vmspace != NULL)
		as_destroy(curthread->t_vmspace);

	/* Create a new address space. */
	curthread->t_vmspace = as_create();
	if (curthread->t_vmspace==NULL) {
		vfs_close(v);
		return ENOMEM;
	}

	/* Activate it. */
	as_activate(curthread->t_vmspace);

	curthread->t_vmspace->prog_name = kstrdup(progname);

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* thread_exit destroys curthread->t_vmspace */
	//	kprintf("load_elf fail\n");
		vfs_close(v);
		return result;
	}

	/* Done with the file now. */
	vfs_close(v);

	/* Define the user stack in the address space */
	result = as_define_stack(curthread->t_vmspace, &stackptr);
	if (result) {
		/* thread_exit destroys curthread->t_vmspace */
	//	kprintf("as_define_stack fail\n");
		return result;
	}

	assert(argc > 0);

		int err;
		/* Define userspace arguments */
		argv = (char**)kmalloc((argc + 1)*sizeof(char*));
		if (argv==NULL) return ENOMEM;

		// copy arguments
		int ind = 0;
		while (ind < (long)argc) {
			int len = strlen(args[ind])+1;
			if (len%4!=0) len += 4-len%4;
		
			argv[ind] = kstrdup(args[ind]);

			if (argv[ind]==NULL) {
				ind--;
				while (ind >= 0) {
					kfree(argv[ind]);
					ind--;
				}
				kfree(argv);
				return ENOMEM;
			}

			ind++;
		}
		argv[ind] = NULL;	// ind = argc

		/* Copy onto stack */
		ind = argc-1;
		while (ind >= 0) {
			int len = strlen(argv[ind])+1;
			if (len % 4 != 0)
				len += 4-len%4;

			stackptr -= len;
			err = copyoutstr(argv[ind],(userptr_t)stackptr,len, NULL);

			if (err) {
				while (ind >= 0) {
					kfree(argv[ind]);
					ind--;
				}
		//		kprintf("copyoutstr err\n");
				kfree(argv);
				return err;
			}
			kfree(argv[ind]);

			argv[ind] = (char*)stackptr;

			ind--;
		}

		stackptr -= (argc+1)*sizeof(char*);
		err = copyout(argv,(userptr_t)stackptr,(argc+1)*sizeof(char*));
		if(err){
		//	kprintf("copyout err\n");
			kfree(argv);
			return err;
		}

		kfree(argv);

		/* Warp to user mode. */
		md_usermode(argc,(userptr_t)stackptr,stackptr, entrypoint);

	/* md_usermode does not return */
	panic("md_usermode returned\n");
	return EINVAL;
}

