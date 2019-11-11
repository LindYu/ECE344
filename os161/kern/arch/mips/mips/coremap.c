#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <lib.h>
#include <vm.h>
#include <addrspace.h>
#include <machine/coremap.h>
#include <machine/spl.h>
#include <machine/tlb.h>
#include <array.h>
#include <thread.h>
#include <curthread.h>
#include <vfs.h>
#include <vnode.h>
#include <kern/stat.h>
#include <bitmap.h>
#include <synch.h>
#include <uio.h>
#include <machine/vm.h>

/* 
 * Coremap is a way the OS manages the page. It lets OS know which process 
 * ownes the page. The physical address is given and linked to the process
 * information and virtual address. It's indexed by the physical page #
 * and tells which process and what's the virtual page #
 */

struct coremap_entry *coremap;
int num_freepage = 0;

int num_entry;

u_int32_t bottom=0, top=0;
 
struct lock* coremap_lock;

void coremap_bootstrap () {
	
	u_int32_t low, high;
	ram_getsize(&low, &high);
	u_int32_t ptr = low;
	bottom = low;
	top = high;
	//kprintf("In boot: free pages are %d\n",num_freepage);
	//ptr += PAGE_SIZE;
	coremap = (struct coremap_entry*)PADDR_TO_KVADDR(ptr);

	num_entry = (high-bottom)/PAGE_SIZE;
	
	ptr += (num_entry)*(sizeof(struct coremap_entry));

	int i = 0;
	num_freepage = 0;
	while (i < num_entry) {

		// mark pages containing coremap as fixed, align ptr to the next free page
		if (low + i*PAGE_SIZE < ptr) {
			coremap[i].state = FIXED;
			if (low + i*PAGE_SIZE+PAGE_SIZE > ptr) ptr = low + i*PAGE_SIZE+PAGE_SIZE;
		} else { 
			coremap[i].state = FREE;
			//coremap[i].pid = 0;
			num_freepage++;
		}
		coremap[i].nproc = 0;
		
		i++;
	}
	
	coremap_lock = lock_create("Coremap lock");

/*
	bottom = *lower_bound;
	top = upper_bound;
	
	*lower_bound = ptr;
*/
}

static
paddr_t
getppages(unsigned long npages)
{
	int spl;
	paddr_t addr;

	spl = splhigh();

	addr = ram_stealmem(npages);
	
	splx(spl);
	return addr;
}

/* Allocate/free kernel heap pages (called by kmalloc/kfree) */
vaddr_t alloc_kpages(int npages) {
	if (bottom==0 && top==0) {
		paddr_t pa;
		pa = getppages(npages);
		if (pa==0) {
			return 0;
		}
		return PADDR_TO_KVADDR(pa);
	}
	//assert (npages == 1);
	if (num_freepage==0) return 0;
	lock_acquire(coremap_lock);

	int i = 0;
	while (bottom + (i + npages)*PAGE_SIZE <= top) {
		int j = 0;
		int check = 1;
		while (j < npages && check) {
			if (coremap[i+j].state != FREE) {
				check = 0;
				i = i+j+1;
			} else {
				j++;
			}
		}
		
		if (check) {
		//if (coremap[i].state == FREE) {
			// can allocate npages
			j = 0;
			while (j < npages) {
				coremap[i+j].state = DIRTY;
				coremap[i+j].nproc = 1;
				//coremap[i+j].pid = curthread->t_pid->pid;
				coremap[i+j].npage = npages-j;
				j++;
			}
			coremap[i].npage = npages;
			num_freepage -= npages;
			lock_release(coremap_lock);
			return PADDR_TO_KVADDR(bottom + i*PAGE_SIZE);
		}
		//i++;
	}

	// cannot allocate npages	
	lock_release(coremap_lock);
	return 0;
}

/* Frees the npages allocated by alloc_kpages. */
void free_kpages(vaddr_t addr) {
	u_int32_t ptr = KVADDR_TO_PADDR(addr);
	
	//struct pt_entry *page_table = PADDR_TO_KVADDR((struct pt_entry *)(curthread->t_vmspace->page_dir[addr>>22].paddr));
	 
	//u_int32_t ptr = page_table[(addr>>12)&(0x3ff)].paddr;
	
	int ind = (ptr-bottom)/PAGE_SIZE;
	//coremap[ind].state = FREE;

	// assume always frees on the addr returned by alloc_kpages
	assert(coremap[ind].npage != 0);
	//if (coremap[ind].npage==0) return;

	lock_acquire(coremap_lock);

	int i = 0;
	int npages = coremap[ind].npage;
	coremap[ind].nproc--;
	if (coremap[ind].nproc == 0) {
		while (i < npages) {
			coremap[i+ind].state = FREE;
			//coremap[i+ind].v_page = PADDR_TO_KVADDR(coremap[ind].p_page);
			coremap[i+ind].npage = 0;
			num_freepage++;
			coremap[i+ind].nproc = 0;
			i++;
		}
		coremap[ind].npage = 0;
	}

	lock_release(coremap_lock);
}

/* Returns coremap entry from physical address */
struct coremap_entry* get_cm_entry(u_int32_t paddr) {
	//kprintf("paddr is %d,bottom is %d, top is %d",paddr,bottom,top);
	//out of space 
	//if(paddr >= bottom && paddr <= top) ;
	assert(paddr >= bottom && paddr <= top);

	int ind = (paddr - bottom)/PAGE_SIZE;
	return &coremap[ind];
}

int get_cm_nproc(u_int32_t paddr) {
	assert(paddr >= bottom && paddr <= top);
	int ind = (paddr - bottom)/PAGE_SIZE;
	return coremap[ind].nproc;
}

/*
void set_cm_vaddr(vaddr_t vaddr, paddr_t paddr) {
	//kprintf("In set_cm_vaddr: free pages are %d\n",num_freepage);
	assert(paddr >= bottom && paddr <= top);
	lock_acquire(coremap_lock);
	int ind = (paddr - bottom)/PAGE_SIZE;
	coremap[ind].v_page = vaddr;
	coremap[ind].pid = curthread->t_pid->pid;
	lock_release(coremap_lock);
}*/

void free_cm_entry(paddr_t paddr) {
	if(!(paddr >= bottom && paddr <= top))
		return;
	assert ((paddr - bottom)%PAGE_SIZE == 0);
	int ind = (paddr - bottom)/PAGE_SIZE;
	
	if(coremap[ind].state==FREE) {
		return;
	}
	lock_acquire(coremap_lock);
	coremap[ind].nproc--;
	if (coremap[ind].nproc == 0) {
		coremap[ind].state = FREE;
		num_freepage++;
		//coremap[ind].pid = 0;
		//coremap[ind].v_page = PADDR_TO_KVADDR(coremap[ind].p_page);
		coremap[ind].npage = 0;
	}
	lock_release(coremap_lock);
	//kprintf("In free_cm_vaddr: free pages are %d\n",num_freepage);
}

void inc_cm_nproc(paddr_t paddr) {
	assert(paddr >= bottom && paddr <= top);
	int ind = (paddr - bottom)/PAGE_SIZE;
	lock_acquire(coremap_lock);
	coremap[ind].nproc++;
	lock_release(coremap_lock);
}

void print_cm () {
	int i;
	kprintf("Print coremap, num_freepage = %d\n", num_freepage);
	for (i = 0; i < num_entry; i++) {
		kprintf("coremap[%d]	state: %d\n", i, coremap[i].state);
	}
}


