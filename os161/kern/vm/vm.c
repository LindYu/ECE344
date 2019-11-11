#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <vm.h>
#include <vfs.h>
#include <kern/unistd.h>
#include <synch.h>
#include <machine/coremap.h>
#include <machine/spl.h>
#include <machine/tlb.h>
#include <machine/vm.h>
#include <thread.h>
#include <curthread.h>

/*
 *  
 */

struct lock *vm_lock;
struct lock *write_lock;
void vm_bootstrap(void) {
	curthread->t_vmspace = as_create();

	coremap_bootstrap();
	vm_lock = lock_create("vm lock");
//	write_lock = lock_create("pt lock");
	
}

/* Fault handling function called by trap code */

int
vm_fault(int faulttype, vaddr_t faultaddr)
{
	vaddr_t faultaddress;
	//int index;
	u_int32_t PPN;
	struct addrspace *as;
	int spl;
	//int writable = 0; 
	u_int32_t pt1, pt2;
	//void * page_entries;
	struct pt_entry* secondary;
	//struct coremap_entry* dirty_page;
	

	//spl = splhigh();

	faultaddress = faultaddr & PAGE_FRAME;
	 

	//DEBUG(DB_VM, "dumbvm: fault: 0x%x\n", faultaddress);

	if(((faultaddress >> 22) == 0) || (faultaddress >= USERTOP)){
		return EFAULT;
	}
	
    as = curthread->t_vmspace;
	if (as == NULL) {
		return EFAULT;
	}

	// See if the function has access to w/r
    //writable = as->page_dir->protection;

	/*	 EX_TLBL - VM_FAULT_READ-TLB miss on load 
		 EX_TLBS - VM_FAULT_WRITE-TLB miss on store  */
	//lock_acquire(as->pt_lock);

	switch (faulttype) {
	    case VM_FAULT_READONLY:
		    //panic("TLB read only\n");
		    // HAS TO BE IN THE PAGE TABLE
		    
			pt1 = (int)(faultaddress >> 22); // master page table
			pt2 = (faultaddress>>12)&(0x3ff);
			
			assert ((as->page_dir[pt1].paddr>>1) != 0);
			secondary = (struct pt_entry*) PADDR_TO_KVADDR((as->page_dir[pt1].paddr)>>1);
			assert ((secondary[pt2].paddr>>1) != 0);
		    
			if (get_cm_nproc((secondary[pt2].paddr)>>1) > 1) {
				free_cm_entry((secondary[pt2].paddr)>>1);
				vaddr_t ptr = alloc_kpages(1);
				if (ptr == 0) {
					//panic("hhhh");
					return ENOMEM;
					//thread_yield();
					//ptr = alloc_kpages(1);
				}
				memcpy((void*)ptr, (void*)PADDR_TO_KVADDR((secondary[pt2].paddr)>>1),PAGE_SIZE);
				secondary[pt2].paddr = KVADDR_TO_PADDR(ptr)<<1;
			}
			secondary[pt2].paddr |= 1;
			PPN = (secondary[pt2].paddr)>>1;
            PPN = (PPN & PAGE_FRAME) | TLBLO_DIRTY | TLBLO_VALID;	
		    
		    lock_acquire(vm_lock);
		    
			spl = splhigh();
		    int index = TLB_Probe(faultaddress,0);
			TLB_Write(faultaddress,PPN,(u_int32_t)index);
			splx(spl);
		    lock_release(vm_lock);
		 	//assert(get_cm_nproc(secondary[pt2].paddr) != 0);
		break;	 
	    case VM_FAULT_READ:
			if (faultaddress == 0x40000000){
				return EFAULT;
					
			}
			//bring page back to tlb - check physical page # 
			pt1 = (int)(faultaddress >> 22); // master page table
			pt2 = (faultaddress>>12)&(0x3ff);	 		
			paddr_t page =  (as->page_dir[pt1].paddr)>>1;
			assert(curthread->t_vmspace!= NULL);
			// no pagetable mapped to the paddr
			if ( page == 0) {
				secondary = (struct pt_entry *)kmalloc(sizeof(struct pt_entry)*(1<<10));
				
			//	assert(secondary!= NULL);
				if (secondary == NULL){
					return ENOMEM; 
					//thread_yield();
					//secondary = (struct pt_entry *)kmalloc(sizeof(struct pt_entry)*(1<<10));
				}
				int i;
				for (i = 0; i < (1<<10); i++) secondary[i].paddr = 0;

				as->page_dir[pt1].paddr = (KVADDR_TO_PADDR((vaddr_t)secondary)<<1) + 1;
				
				vaddr_t ptr = alloc_kpages(1);
				//assert(ptr!=0);
				if (ptr == 0){
					return ENOMEM;
					//thread_yield();
					//ptr = alloc_kpages(1);
				}
				
				secondary[pt2].paddr = (KVADDR_TO_PADDR(ptr) << 1) + 1;	
				//secondary[pt2].protection = 1;
				PPN = (u_int32_t)(secondary[pt2].paddr)>>1;
				
	 			struct vnode *v;
				if (faultaddress >= as->vaddr_1 && faultaddress < as->vaddr_1 + as->filesize_1 ) {
					/* Open the file. */
					int result = vfs_open(as->prog_name, O_RDONLY, &v);
					//assert(result ==0);
					if (result) {
						return result;
					}
					if (faultaddress + PAGE_SIZE >= as->vaddr_1 + as->filesize_1) {
						result = load_segment(v, as->offset_1 + faultaddress - as->vaddr_1, faultaddress, 
									  as->filesize_1 - (faultaddress - as->vaddr_1), as->filesize_1 - (faultaddress - as->vaddr_1),
									  as->is_executable_1);
				//	assert(result ==0);
						if (result) {
							return result;
						}
					} else {
						result = load_segment(v, as->offset_1 + faultaddress - as->vaddr_1, faultaddress, 
									  PAGE_SIZE, PAGE_SIZE,
									  as->is_executable_1);
				//assert(result ==0);
						if (result) {
							return result;
						}
					}
					/* Done with the file now. */
					vfs_close(v);
				} else if (faultaddress >= as->vaddr_2 && faultaddress < as->vaddr_2 + as->filesize_2 ) {
					/* Open the file. */
					int result = vfs_open(as->prog_name, O_RDONLY, &v);
					//assert(result ==0);
					if (result) {
						return result;
					}
					if (faultaddress + PAGE_SIZE >= as->vaddr_2 + as->filesize_2) {
						result = load_segment(v, as->offset_2 + faultaddress - as->vaddr_2, faultaddress, 
									  as->filesize_2 - (faultaddress - as->vaddr_2), as->filesize_2 - (faultaddress - as->vaddr_2),
									  as->is_executable_2);
									  assert(result ==0);
						if (result) {
							return result;
						}
					} else {
						result = load_segment(v, as->offset_2 + faultaddress - as->vaddr_2, faultaddress, 
									  PAGE_SIZE, PAGE_SIZE,
									  as->is_executable_2);
									  assert(result ==0);
						if (result) {
							return result;
						}
					}	
					/* Done with the file now. */
					vfs_close(v);
				}
				//return EFAULT;
			} else {
				secondary = (struct pt_entry*)PADDR_TO_KVADDR(page);
 
				if((secondary[pt2].paddr>>1) == 0){
					vaddr_t ptr = alloc_kpages(1);
				//	assert(ptr!=0);
					if (ptr==0){
						return ENOMEM;
					}
					secondary[pt2].paddr = (KVADDR_TO_PADDR(ptr) << 1) + 1;
					//secondary[pt2].protection = 1;
					
		 			struct vnode *v;
					if (faultaddress >= as->vaddr_1 && faultaddress < as->vaddr_1 + as->filesize_1) {
						/* Open the file. */
						int result = vfs_open(as->prog_name, O_RDONLY, &v);
					//		assert(result ==0);
						if (result) {
							return result;
						}
						if (faultaddress + PAGE_SIZE >= as->vaddr_1 + as->filesize_1) {
							result = load_segment(v, as->offset_1 + faultaddress - as->vaddr_1, faultaddress, 
										  as->filesize_1 - (faultaddress - as->vaddr_1), as->filesize_1 - (faultaddress - as->vaddr_1),
										  as->is_executable_1);
										  assert(result ==0);
							if (result) {
								return result;
							}
						} else {
							result = load_segment(v, as->offset_1 + faultaddress - as->vaddr_1, faultaddress, 
										  PAGE_SIZE, PAGE_SIZE,
										  as->is_executable_1);
										  assert(result ==0);
							if (result) {
								return result;
							}
						}
						/* Done with the file now. */
						vfs_close(v);
					} else if (faultaddress >= as->vaddr_2 && faultaddress < as->vaddr_2 + as->filesize_2 ) {
						/* Open the file. */
						int result = vfs_open(as->prog_name, O_RDONLY, &v);
			//			assert(result ==0);
						if (result) {
							return result;
						}
						if (faultaddress + PAGE_SIZE >= as->vaddr_2 + as->filesize_2) {
							result = load_segment(v, as->offset_2 + faultaddress - as->vaddr_2, faultaddress, 
										  as->filesize_2 - (faultaddress - as->vaddr_2), as->filesize_2 - (faultaddress - as->vaddr_2),
										  as->is_executable_2);
										  assert(result ==0);
							if (result) {
								return result;
							}
						} else {
							result = load_segment(v, as->offset_2 + faultaddress - as->vaddr_2, faultaddress, 
										  PAGE_SIZE, PAGE_SIZE,
										  as->is_executable_2);
										  assert(result ==0);
							if (result) {
								return result;
							}
						}	
						/* Done with the file now. */
						vfs_close(v);
					}
				} 
				
				PPN = (u_int32_t)(secondary[pt2].paddr) >> 1;
				if ((secondary[pt2].paddr & 1) == 0)
					PPN = (PPN & PAGE_FRAME) | TLBLO_VALID;
				else
					PPN = (PPN & PAGE_FRAME) | TLBLO_VALID | TLBLO_DIRTY;
					
				lock_acquire(vm_lock);
				spl = splhigh();
				int i = TLB_Probe(faultaddress, 0);
				if (i > 0) TLB_Write (faultaddress, PPN, i);
				else TLB_Random(faultaddress, PPN);
				splx(spl);
				lock_release(vm_lock);
 			}
		break;	
	    case VM_FAULT_WRITE:
			// if faultaddress just within one page below the stack, lower the stack pointer
			if (faultaddress < as->user_stack && faultaddress >= as->user_stack - PAGE_SIZE) 
				as->user_stack -= PAGE_SIZE;
			if (faultaddress == 0x40000000){
				return EFAULT;
				}
			if (faultaddress > as->heap_end && faultaddress < as->user_stack) 
				as->user_stack = faultaddress / PAGE_SIZE * PAGE_SIZE;
			//bring page back to tlb - check physical page # 
			pt1 = (int)(faultaddress >> 22); // master page table
			pt2 = (faultaddress>>12)&(0x3ff);	 		
			// no pagetable mapped to the paddr
			if ((as->page_dir[pt1].paddr>>1) == 0){
				// page fault
				secondary = (struct pt_entry *)kmalloc(sizeof(struct pt_entry)*(1<<10));

				if (secondary == NULL){
					return ENOMEM;
					//thread_yield();
					//secondary = (struct pt_entry *)kmalloc(sizeof(struct pt_entry)*(1<<10));
				}
				int i;
				for (i = 0; i < (1<<10); i++) secondary[i].paddr = 0;

				as->page_dir[pt1].paddr = (KVADDR_TO_PADDR((vaddr_t)secondary) << 1) + 1;
				
				vaddr_t ptr = alloc_kpages(1);
				if (ptr == 0){
				 return ENOMEM;
				 //thread_yield();
				 //ptr = alloc_kpages(1);
				}
				
				secondary[pt2].paddr = (KVADDR_TO_PADDR(ptr) << 1) +1;	
				//secondary[pt2].protection = 1;
				
				PPN = (u_int32_t)(secondary[pt2].paddr) >> 1;
				
	 			struct vnode *v;
				if (faultaddress >= as->vaddr_1 && faultaddress < as->vaddr_1 + as->filesize_1) {
					/* Open the file. */
					int result = vfs_open(as->prog_name, O_RDONLY, &v);
					if (result) {
						return result;
					}
					if (faultaddress + PAGE_SIZE >= as->vaddr_1 + as->filesize_1) {
						result = load_segment(v, as->offset_1 + faultaddress - as->vaddr_1, faultaddress, 
									  PAGE_SIZE, as->filesize_1 - (faultaddress - as->vaddr_1),
									  as->is_executable_1);
						if (result) {
							return result;
						}
					} else {
						result = load_segment(v, as->offset_1 + faultaddress - as->vaddr_1, faultaddress, 
									  PAGE_SIZE, PAGE_SIZE,
									  as->is_executable_1);
						if (result) {
							return result;
						}
					}
					/* Done with the file now. */
					vfs_close(v);
				} else if (faultaddress >= as->vaddr_2 && faultaddress < as->vaddr_2 + as->filesize_2 ) {
					/* Open the file. */
					int result = vfs_open(as->prog_name, O_RDONLY, &v);
					if (result) {
						return result;
					}
					if (faultaddress + PAGE_SIZE >= as->vaddr_2 + as->filesize_2) {
						result = load_segment(v, as->offset_2 + faultaddress - as->vaddr_2, faultaddress, 
									  PAGE_SIZE, as->filesize_2 - (faultaddress - as->vaddr_2),
									  as->is_executable_2);
						if (result) {
							return result;
						}
					} else {
						result = load_segment(v, as->offset_2 + faultaddress - as->vaddr_2, faultaddress, 
									  PAGE_SIZE, PAGE_SIZE,
									  as->is_executable_2);
						if (result) {
							return result;
						}
					}	
					/* Done with the file now. */
					vfs_close(v);
				}
				
			}
								
			else {
				secondary = (struct pt_entry*)PADDR_TO_KVADDR((as->page_dir[pt1]).paddr>>1);
				
				// This entry has never been allocated any pages 
				if(secondary[pt2].paddr == 0){
					vaddr_t ptr = alloc_kpages(1);
					if (ptr == 0){
						 return ENOMEM;
						 //thread_yield();
						 //ptr = alloc_kpages(1);
					}
					secondary[pt2].paddr = (KVADDR_TO_PADDR(ptr) << 1) + 1;
//					secondary[pt2].protection = 1;
					
		 			struct vnode *v;
					if (faultaddress >= as->vaddr_1 && faultaddress < as->vaddr_1 + as->filesize_1  ) {
						/* Open the file. */
						int result = vfs_open(as->prog_name, O_RDONLY, &v);
						if (result) {
							return result;
						}
						if (faultaddress + PAGE_SIZE >= as->vaddr_1 + as->filesize_1) {
							result = load_segment(v, as->offset_1 + faultaddress - as->vaddr_1, faultaddress, 
										  PAGE_SIZE, as->filesize_1 - (faultaddress - as->vaddr_1),
										  as->is_executable_1);
							if (result) {
								return result;
							}
						} else {
							result = load_segment(v, as->offset_1 + faultaddress - as->vaddr_1, faultaddress, 
										  PAGE_SIZE, PAGE_SIZE,
										  as->is_executable_1);
							if (result) {
								return result;
							}
						}
						/* Done with the file now. */
						vfs_close(v);
					} else if (faultaddress >= as->vaddr_2 && faultaddress < as->vaddr_2 + as->filesize_2) {
						/* Open the file. */
						int result = vfs_open(as->prog_name, O_RDONLY, &v);
						if (result) {
							return result;
						}
						if (faultaddress + PAGE_SIZE >= as->vaddr_2 + as->filesize_2) {
							result = load_segment(v, as->offset_2 + faultaddress - as->vaddr_2, faultaddress, 
										  PAGE_SIZE, as->filesize_2 - (faultaddress - as->vaddr_2),
										  as->is_executable_2);
							if (result) {
								return result;
							}
						} else {
							result = load_segment(v, as->offset_2 + faultaddress - as->vaddr_2, faultaddress, 
										  PAGE_SIZE, PAGE_SIZE,
										  as->is_executable_2);
							if (result) {
								return result;
							}
						}	
						/* Done with the file now. */
						vfs_close(v);
					}
				} else {
				
					if ((secondary[pt2].paddr & 1) == 0) {
						if (get_cm_nproc(secondary[pt2].paddr >> 1) > 1) {
							free_cm_entry(secondary[pt2].paddr >> 1);
							vaddr_t ptr = alloc_kpages(1);
							if (ptr == 0){
								 return ENOMEM;
								// thread_yield();
								 //vaddr_t ptr = alloc_kpages(1);
							}
							memcpy((void*)ptr, (void*)PADDR_TO_KVADDR(secondary[pt2].paddr >> 1),PAGE_SIZE);
							
							secondary[pt2].paddr = (KVADDR_TO_PADDR(ptr) << 1) + 1;
							
						}
					} 
					
					secondary[pt2].paddr |= 1;
				}
				PPN = (u_int32_t)(secondary[pt2].paddr >> 1);
			}
		 	
 
			PPN &= PAGE_FRAME;
			PPN = PPN |TLBLO_VALID | TLBLO_DIRTY;
			lock_acquire(vm_lock);
			spl = splhigh();
			int i = TLB_Probe(faultaddress, 0);
			if (i > 0) TLB_Write (faultaddress, PPN, i);
			else TLB_Random(faultaddress, PPN);
			splx(spl);
			lock_release(vm_lock);
		 
			break;
	    default:
			//splx(spl);
			return EINVAL;
	}
		
	//splx(spl);
	return 0;
}
/*

void TLB_shootdown(vaddr_t faultaddress){	
	int index;
    int spl = splhigh();
	u_int32_t *entryhi,*entrylo;
	index = TLB_Probe(faultaddress, 0);
	TLB_Read(entryhi, entrylo, index);
	assert(entrylo & TLBLB_VALID != 0);
	TLB_Write(TLBHI_INVALID(index), TLBLO_INVALID());
	splx(spl);

}
*/
