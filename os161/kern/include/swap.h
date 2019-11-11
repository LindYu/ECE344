#ifndef _SWAP_H_
#define _SWAP_H_

#include <types.h>
#include <machine/vm.h>
#include <vnode.h>
#include <uio.h>

/*Functions for swap. Reminder to figure out how to r/w to disk.*/

// IMPORTANT: THE VARIABLES IN THIS FILE MAY NOT BEEN ADDED IN THE ACTUAL STRUCTURE YET. PLEASE READ THE 
// COMMENTS TO SEE WHAT NEEDS TO BE UPDATED. ALSO, THERE'S NOT INTERFACE TO TLB YET AND THE FUNCTIONS HAVE
// NOT BEEN CALLED ANYWHERE. THIS IS AN ISOLATED CRUDE COPY.

void swap_bootstrap(void);
void swap_in(vaddr_t required_page);
void swap_out(vaddr_t required_page);
struct coremap_entry* choose_victim();


#endif /* _SWAP_H_ */
