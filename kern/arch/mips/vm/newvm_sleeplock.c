#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <cpu.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>


static paddr_t lastpaddr, firstpaddr;
struct coremap_page * coremap_start;
static int coremap_used_counter;
static bool load_complete = false;
struct lock *coremap_lock = NULL;

void coremap_load() {
	
	lastpaddr = ram_getsize();
	firstpaddr = ram_getfirstfree();

	if(firstpaddr % 4096 != 0) {
		firstpaddr = ((firstpaddr/4096) + 1) * 4096;
	}

	unsigned int pages = lastpaddr/4096;

	coremap_start = (struct coremap_page *)PADDR_TO_KVADDR(firstpaddr);

	unsigned int coremap_size = sizeof(struct coremap_page)*pages/4096;

	if ((sizeof(struct coremap_page)*pages) % 4096 != 0) {
		coremap_size = coremap_size + 1;
	}

	firstpaddr = firstpaddr + coremap_size * 4096;
	
	KASSERT(firstpaddr % 4096 == 0);	

	for (unsigned int i = 0; i < firstpaddr/4096; i++) {
		coremap_start[i].chunk_size = 0;
		coremap_start[i].state = fixed;
	}
	
	for (unsigned int i = firstpaddr/4096; i < lastpaddr/4096; i++){
		coremap_start[i].chunk_size = 0;
		coremap_start[i].state = free;
	}
		
}

void vm_bootstrap(void) {
	coremap_lock = lock_create("coremap_lock");
	load_complete = true;
}

static paddr_t getppages(unsigned long npages) {
	
	paddr_t addr = 0;
	unsigned int free_pages = 0;
	
	if(load_complete) {
		lock_acquire(coremap_lock);
	}

	for(unsigned int i = firstpaddr/4096; i < lastpaddr/4096; i++) {
		
		if(coremap_start[i].state == free) {
			free_pages++;
		}else {
			free_pages = 0;
		}

		if(free_pages == npages) {
			addr = (i - (npages - 1))*4096;
			break;
		}
	}

	if(free_pages != npages) {
		if(load_complete){
			lock_release(coremap_lock);
		}
		return 0;
	}
	
	for(unsigned int i = 0; i < npages; i++) {
		coremap_start[(addr/4096) + i].state = fixed;
		if(i == 0) {
			coremap_start[(addr/4096) + i].chunk_size = npages;
		} 
	}

	if(load_complete) {
		lock_release(coremap_lock);
	}

	return addr;
}

vaddr_t alloc_kpages(unsigned npages) {

	paddr_t pages_paddr = getppages(npages);
	
	if(pages_paddr == 0) {
		return 0;
	}
	coremap_used_counter += npages;
	vaddr_t pages_vaddr = PADDR_TO_KVADDR(pages_paddr);
	
	return pages_vaddr;
}

void free_kpages (vaddr_t addr)
{
	paddr_t pages_paddr = addr - MIPS_KSEG0;

	if(load_complete) {
		lock_acquire(coremap_lock);
	}
	KASSERT(pages_paddr % 4096 == 0);
	unsigned int page_index = pages_paddr/4096;

	unsigned int chunks = coremap_start[page_index].chunk_size;

	for(unsigned int i = 0; i < chunks; i++) {
		coremap_start[page_index + i].state = free;
		coremap_start[page_index + i].chunk_size = 0; 
	}
	coremap_used_counter -= chunks;
	if(load_complete) {
		lock_release(coremap_lock);
	}
}
int vm_fault(int faulttype, vaddr_t faultaddress){
	(void)faulttype;
	(void)faultaddress;
	return 0;
}

unsigned int coremap_used_bytes(void){
	//if(load_complete) {
	//	spinlock_acquire(&coremap_lock);
	//}
	
	return coremap_used_counter*4096;
	
	//if(load_complete){
	//	spinlock_release(&coremap_lock);
	//}
}

void vm_tlbshootdown(const struct tlbshootdown * ts) {
	(void)ts;
}

