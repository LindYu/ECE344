/*coremap.h*/

#ifndef _MIPS_COREMAP_H_
#define _MIPS_COREMAP_H_

#include <types.h>
#include <machine/vm.h>

typedef enum {
	FREE,
    FIXED,
	DIRTY,
	CLEAN
}page_state;

struct coremap_entry{
	int state;
	//paddr_t p_page;
	//vaddr_t v_page;
	int npage;
	int nproc;
	//pid_t pid;

};

extern struct coremap_entry *coremap;
extern struct lock* coremap_lock;
extern int num_freepage;

void coremap_bootstrap();
//vaddr_t get_process(); //pid?

struct coremap_entry* get_cm_entry(u_int32_t paddr);
int get_cm_nproc(u_int32_t paddr);
void set_cm_vaddr(vaddr_t vaddr, paddr_t paddr);
void free_cm_entry(paddr_t paddr);
void inc_cm_nproc(paddr_t paddr);
void print_cm();

int load_segment(struct vnode *v, off_t offset, vaddr_t vaddr, 
	     size_t memsize, size_t filesize,
	     int is_executable);

#endif /*_MIPS_COREMAP_H_*/
