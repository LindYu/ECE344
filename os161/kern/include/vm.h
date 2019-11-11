#ifndef _VM_H_
#define _VM_H_

#include <machine/vm.h>

/*
 * VM system-related definitions.
 *
 * You'll probably want to add stuff here.
 */

enum {RONLY, R_W, EXEC}PROTECTION;
/* Fault-type arguments to vm_fault() */
#define VM_FAULT_READ        0    /* A read was attempted */
#define VM_FAULT_WRITE       1    /* A write was attempted */
#define VM_FAULT_READONLY    2    /* A write to a readonly page was attempted*/

extern struct lock *vm_lock;
extern struct lock *write_lock;
struct pt_entry {
	// to get paddr: paddr>>1; to get protection: paddr & 1.
	paddr_t paddr;			// 32 bits
	//int protection;
};

/* Initialization function */
void vm_bootstrap(void);

/* Fault handling function called by trap code */
int vm_fault(int faulttype, vaddr_t faultaddress);

/* Allocate/free kernel heap pages (called by kmalloc/kfree) */
vaddr_t alloc_kpages(int npages);
void free_kpages(vaddr_t addr);
void TLB_shootdown(vaddr_t faultaddress);

#endif /* _VM_H_ */
