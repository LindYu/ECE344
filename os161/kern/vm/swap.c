#include <swap.h>
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
#include <coremap.h> 
#include <machine/vm.h>
#include <kern/stat.h>

 
struct bitmap *swap_bits;
struct vnode *swap_file;
struct lock *swap_lock;
int clock_stay = 0;
int free_pages;
int total_pages;
struct coremap_entry* clock_hand = coremap[0];
 
void swap_bootstrap(){
	int error;
	int size;
	struct stat* ptr;
	error = vfs_open("lhd0raw", O_RDWR, &swap_file);
	if (error) panic("Swap failed");

	VOP_STAT(swap_file,ptr);

	//get file size 
	size = (int)(ptr->st_size);
	kprintf("size is %i",size); 
	
	// IS THIS PART NECESSARY
	int offset = size % PAGE_SIZE;
	size = size - offset;
	int nEntries = size / PAGE_SIZE;
	free_pages = nEntries;
	total_pages = nEntries;

    swap_bits = bitmap_create(nEntries);
	if (swap_bits == NULL) {
		panic("Cannot create swap record bit array\n");
		//return NULL;
	}
	error = array_preallocate(pid_array,nEntries+1);
	if (error) {
		panic("Cannot preallocate swap record bit array\n");
		//return NULL;
	}
	int i;
	for(i = 0; i <= nEntries; i++){
	   bitmap_alloc(swap_bits,0); //no space used
	}
	//mk_kuio(struct uio *, void *kbuf, size_t len, off_t pos, enum uio_rw rw);

	swap_lock = lock_create("swap_lock");	
 
		
}

/* REMINDER
 * I need to find the first available bit in the bitmap(its bit might be 0 but had pages in - need to notify that present page its swapaddr  
 * will no longer be used!!!!
 * ALSO HOW DO I UPDATE TLB
 */
void swap_in(vaddr_t required_page){
	struct uio* u;
	void *kbuf;
	coremap entry* page_location;
	paddr_t real_page;
   // NEED TO ADD VNODE TO ADDRESSPACE, NEED TO ADD location: PRESENT(0)/SWAPPED(1), page_index in swapfile: swap_index IN PT_ENTRY

	//in piazza discussion about using kernel virtual to translate to physical addr
	real_page = VADDR_TO_PADDR((u_int32_t)required_page);
	page_location = get_cm_entry(real_page); 
	assert(page_location->state == CLEAN);

	lock_acquire(swap_lock);
	u_int32_t index = ((struct pt_entry*)real_page)[0].swap_index; 
	bitmap_unmark(swap_bits,i); //mark as not occupied
	lock_acquire(coremap_lock);

	if(num_freepage == 0){
	    lock_release(swap_lock);
        lock_release(coremap_lock);
		panic("Swap in failed: Ran out of swap!");
	}

	free_pages ++;
	num_freepage --;
	assert(free_pages >= 0);
	assert(num_freepage >= 0);
	lock_release(swap_lock);
    lock_release(coremap_lock);

	//HOW TO CHANGE ITS PHYSICAL ADDRESS??? WHERE TO PUT IN COREMAP - NOT consecutive!
	// MIGHT HAVE DONE START LOCATION OFF BY 1- CHECK!!
	assert(bitmap_isset(swap_bits,index) != 0);
	
	//what is kernel buffer?
	mk_kuio(u,kbuf,PAGE_SIZE,(off_t)index * PAGE_SIZE, UIO_READ);
	VOP_READ(swap_file, u);

 //?? and how to read back for page??
	
	for(int i = 0;i<= PAGE_SIZE; i++){
		 ((struct pt_entry*)real_page)[i].location = PRESENT;
		 ((struct pt_entry*)real_page)[i].reference = 1;
		 ((struct pt_entry*)real_page)[i].paddr =   //change from disk address to physical address in coremap
		 //((struct pt_entry*)real_page)[i].swap_index = -1;
		
	}
	
	 
}

/* Again, we pass in a kernel virtual address, and check its coremap state. If it's state is clean,
 * that means we can just delete wipe out page since it's also saved in the disk. If it's dirty, we  
 * check the page location bit to see if this page is in the disk. If not(-1), we allocate. If yes,
 * we modify its contect back to disk,then wipe it (mark its entries as invalid, and physical addr
 * the disk address.
 */

void swap_out(vaddr_t required_page){
	struct uio* u;
	void *kbuf;
	coremap entry* page_location;
	paddr_t real_page;
	real_page = VADDR_TO_PADDR((u_int32_t)required_page);
	page_location = get_cm_entry(real_page); 
	u_int32_t index = ((struct pt_entry*)real_page)[0].swap_index;

	lock_acquire(swap_lock);
	lock_acquire(coremap_lock);

	if(num_freepage == 0){
		lock_release(swap_lock);
		lock_release(coremap_lock);
		panic("Swap out failed: Ran out of swap!");

	}
	if(page_location->page_state == CLEAN){
		
		//its location in disk not overwritten 
		if(!(bitmap_isset(swap_bits,index))){
			for(i= 0; i<= PAGE_SIZE;i++){
			   ((struct pt_entry*)real_page)[i].location = SWAPPED;
			   ((struct pt_entry*)real_page)[i].paddr =    //disk addr
			   ((struct pt_entry*)real_page)[i].valid = 0;
			}
		}
		else{ //find a new location and update			
			for(i= 0; i<= PAGE_SIZE;i++){
			   ((struct pt_entry*)real_page)[i].location = SWAPPED;
			   ((struct pt_entry*)real_page)[i].paddr =    //disk addr
			   ((struct pt_entry*)real_page)[i].swap_index = ;
			   ((struct pt_entry*)real_page)[i].valid = 0;
			}
		}
	}
	else{
		//its location in disk not overwritten 
		if(!(bitmap_isset(swap_bits,index))){
			for(i= 0; i<= PAGE_SIZE;i++){
			   ((struct pt_entry*)real_page)[i].location = SWAPPED;
			   ((struct pt_entry*)real_page)[i].paddr =    //disk addr
			   ((struct pt_entry*)real_page)[i].valid = 0;
			}
		}
		else{ //find a new location and update			
			for(i= 0; i<= PAGE_SIZE;i++){
			   ((struct pt_entry*)real_page)[i].location = SWAPPED;
			   ((struct pt_entry*)real_page)[i].paddr =    //disk addr
			   ((struct pt_entry*)real_page)[i].swap_index = ;
			   ((struct pt_entry*)real_page)[i].valid = 0;
			}
		}
		//write to disk for DIRTY pages

		
	}
	num_freepage++;
	free_pages --;
	lock_release(swap_lock);
 	lock_release(coremap_lock);

}

/*Implement the LRU Clock algorithm to find the victim, return its physical address*/

struct coremap_entry* choose_victim(){

	lock_acquire(coremap_lock);

	while(){
		if(clock_hand = coremap[num_entry]){
			clock_hand = coremap[0]; //wrap around
			clock_stay = 0;
		}
		if(coremap[clock_stay]-> state == DIRTY)clock_hand = coremap[clock_stay+1]; //don't touch dirty pages 	
		// DO I NEED TO SET THEM BACK TO CLEAN
		clock_stay ++;

		//found victim
		if(clock_hand-> state == CLEAN)	break;
	}
	
 	
	lock_release(coremap_lock);
	return clock_hand;
}


 


