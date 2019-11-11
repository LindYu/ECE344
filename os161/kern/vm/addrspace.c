#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <vm.h>
#include <synch.h>
#include <thread.h>
#include <curthread.h>

#include <machine/spl.h>
#include <machine/tlb.h>
#include <machine/coremap.h>

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */

/* Allocate address space and set stuff in address space struct*/

struct addrspace *
as_create(void)
{
	struct addrspace *as = kmalloc(sizeof(struct addrspace));
	if (as==NULL) {
		return NULL;
	}

	/*
	 * Initialize as needed.
	 */
	as->page_dir = NULL;	
	//as->user_static = 0x0;		 
 	as->user_stack = USERSTACK;
	as->heap_start = 0x10000000;
	as->heap_end = as->heap_start;
	as->page_dir = kmalloc(sizeof(struct pt_entry)*(1<<10));
//	as->pt_lock = lock_create("as pt lock");
	if (as->page_dir==0) {
		kfree(as);
		return NULL;
	}
//	assert(as->pt_lock!=NULL);
	as->prog_name = "";
	as->offset_1 = 0;
	as->offset_2 = 0;
	as->vaddr_1 = 0;
	as->vaddr_2 = 0;
	as->filesize_1 = 0;
	as->filesize_2 = 0;
	as->is_executable_1 = 0;
	as->is_executable_2 = 0;

	int i = 0;
	while (i < 1<<10) {
		as->page_dir[i].paddr = 0;
	//	as->page_dir[i].valid = 0;
		i++;
	}
	
	return as;
}

/* Copy a process's address space. Go through page directory of old address
 * space, creating page tables and copy pages that are allocated. 
 */

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *newas;
	
	//assert (*ret == curthread->t_vmspace);

	newas = as_create();
	while (newas==NULL) {
		//return ENOMEM;
		thread_yield();
		newas = as_create();
	}
	*ret = newas;
	
	newas->heap_start = old->heap_start;
	newas->heap_end = old->heap_end;
	newas->user_stack = old->user_stack;
	
	newas->prog_name = kstrdup(old->prog_name);
	newas->offset_1 = old->offset_1;
	newas->offset_2 = old->offset_2;
	newas->vaddr_1 = old->vaddr_1;
	newas->vaddr_2 = old->vaddr_2;
	newas->filesize_1 = old->filesize_1;
	newas->filesize_2 = old->filesize_2;
	newas->is_executable_1 = old->is_executable_1;
	newas->is_executable_2 = old->is_executable_2;
	int i,j;
	struct pt_entry *pte;
	struct pt_entry *newpte;
	for (i = 0; i < (1<<10); i++) {
		if ((old->page_dir[i].paddr>>1) != 0) {
			pte = (struct pt_entry *) PADDR_TO_KVADDR(old->page_dir[i].paddr >> 1);

			newpte = (struct pt_entry*)kmalloc(sizeof(struct pt_entry)*(1<<10));
			if (newpte==NULL) {
				return ENOMEM;
			}
			newas->page_dir[i].paddr = (KVADDR_TO_PADDR((vaddr_t)newpte) << 1) + 1;

			for (j = 0; j < (1<<10); j++) {
				if ((pte[j].paddr>>1) != 0) {
					// copy on write
					
					newpte[j].paddr = (pte[j].paddr) & (~(1));
					
					inc_cm_nproc((pte[j].paddr)>>1);
					
					pte[j].paddr = newpte[j].paddr;
				} else {newpte[j].paddr = 0; }
			}
		} else {
			newas->page_dir[i].paddr = 0;
		}
	}
	 
	return 0;
}

/* Destroy a process's address space. Walk through page table, free pages, 
 * then free its data structures.*/

void
as_destroy(struct addrspace *as)
{
	/*
	 * Clean up as needed.
	 */
	int i,j;
	for (i = 0; i < (1<<10); i++) {
		if ((as->page_dir[i].paddr >> 1) != 0) {
			struct pt_entry * pte = (struct pt_entry*) PADDR_TO_KVADDR(as->page_dir[i].paddr >> 1);
			for (j = 0; j < (1<<10); j++) {
				if ((pte[j].paddr >> 1) != 0) {
					free_cm_entry((pte[j].paddr) >> 1);
					pte[j].paddr = 0;
				}
			}
			kfree (pte);
		}
	}
//	lock_destroy(as->pt_lock);
	kfree(as->prog_name);
	kfree(as->page_dir);
	kfree(as);
	//kprintf("free page %d\n",num_freepage);
}


// Invalidate all TLB entries 
void
as_activate(struct addrspace *as)
{	int i;
	(void)as;
 	int spl;
 
	if (vm_lock->curThread != curthread) {
		lock_acquire(vm_lock);
		spl = splhigh();
		for (i=0; i<NUM_TLB; i++) {
			TLB_Write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
		}
		splx(spl);
		lock_release(vm_lock);
	}	 
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */

// called in load_elf and simply sets up the bookkeeping for the data and text regions
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
		 int readable, int writeable, int executable)
{//	print_cm();
	vaddr_t seg_end;
	size_t npages; 
	
	//int permissions;
	(void) readable;
	(void) writeable;
	(void) executable;

	/* Align the region. First, the base... */
	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */
	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;
	
	npages = sz / PAGE_SIZE; 

	DEBUG(DB_VM, "Pages required: %d\n",npages);
	DEBUG(DB_VM, "segment starts at %p\n", (void*)vaddr);

/*	if(as->user_static == 0x0){ // new start
		as->user_static = vaddr;
	}
*/
	//adjust heap  - realign just in case
	seg_end = vaddr + npages*PAGE_SIZE;
	if(seg_end > as->heap_start){
       as->heap_start = seg_end;
	   as->heap_end = seg_end;
	}
 
	/*int offset = (int)(as->heap_start % PAGE_SIZE);
	if(offset){
		as->heap_start += offset;
		as->heap_end += offset;
	}*/

 	return 0;
	 
}

/* Called from load_elf before loading segments. Need to load into 
 * read only TLB 
 */
int
as_prepare_load(struct addrspace *as)
{   

	(void) as;

	return 0;
}

/* Called from load_elf after segments are loaded. The the system to 
 * make permission back and flush the TLB usign activate. 
 */

int
as_complete_load(struct addrspace *as)
{	// back to checking r/w/exec!
	//as->user_permission = 1;
	//as_activate(as);
	(void)as;
	return 0;
}

/* Allocate one page for the stack for now. Align heap to page boundary.*/

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	//as->user_stack = (vaddr_t)(USERSTACK - PAGE_SIZE); 

	/* Initial user-level stack pointer */
	as->user_stack = USERSTACK;
	*stackptr = USERSTACK;
	
	return 0;
}

