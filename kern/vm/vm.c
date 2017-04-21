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
#include <mips/tlb.h>

static paddr_t lastpaddr, firstpaddr;
struct coremap_page * coremap_start;
static int coremap_used_counter;
static bool load_complete = true;
static struct spinlock coremap_lock = SPINLOCK_INITIALIZER;

static
void
as_zero_region(paddr_t paddr, unsigned npages)
{
        bzero((void *)PADDR_TO_KVADDR(paddr), npages * PAGE_SIZE);
}

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
	
}


static paddr_t getppages(unsigned long npages) {
	
	paddr_t addr = 0;
	unsigned int free_pages = 0;
	
	if(load_complete) {
		spinlock_acquire(&coremap_lock);
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
			spinlock_release(&coremap_lock);
		}
		return 0;
	}
	
	for(unsigned int i = 0; i < npages; i++) {
		coremap_start[(addr/4096) + i].state = fixed;
		if(i == 0) {
			coremap_start[(addr/4096) + i].chunk_size = npages;
		} 
	}
	as_zero_region(addr, npages);
	coremap_used_counter += npages;
	if(load_complete) {
		spinlock_release(&coremap_lock);
	}

	return addr;
}

paddr_t getppageswrapper(unsigned long pages){
        paddr_t ppages_addr = getppages(pages);
        return ppages_addr;
}


vaddr_t alloc_kpages(unsigned npages) {

	paddr_t pages_paddr = getppages(npages);
	
	if(pages_paddr == 0) {
		return 0;
	}
	vaddr_t pages_vaddr = PADDR_TO_KVADDR(pages_paddr);
	
	return pages_vaddr;
}

void free_kpages (vaddr_t addr)
{
	paddr_t pages_paddr = addr - MIPS_KSEG0;

	if(load_complete) {
		spinlock_acquire(&coremap_lock);
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
		spinlock_release(&coremap_lock);
	}
}

void free_ppages(paddr_t page_paddr) {
	spinlock_acquire(&coremap_lock);
	
	KASSERT(page_paddr % 4096 == 0);

	unsigned int page_index = page_paddr/4096;
	unsigned int chunks = coremap_start[page_index].chunk_size;
	
	KASSERT(chunks == 1);
	coremap_start[page_index].state = free;
	coremap_start[page_index].chunk_size = 0;
	coremap_used_counter -= chunks;
	spinlock_release(&coremap_lock);
	
}

void tlb_invalidate_entry(vaddr_t remove_vaddr) {

	uint32_t ehi, elo;	
	int spl = splhigh();
        for (int i=0; i<NUM_TLB; i++) {
                tlb_read(&ehi, &elo, i);
                if (ehi == remove_vaddr) {
			tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
			break;
		}
        }
        splx(spl);	
}

int vm_fault(int faulttype, vaddr_t faultaddress)
{
	struct addrspace *as;
	vaddr_t stackbase, stacktop;
	paddr_t ppage = 0;
	bool valid_addr = false;
	int i, spl;
	struct region *rg;
	struct page_table_entry *pte;
	struct page_table_entry *pte_prev = NULL;
	uint32_t ehi, elo;	
	(void)faulttype;
	faultaddress &= PAGE_FRAME;
	as = proc_getas();
	rg = as->start_region;

	if (curproc == NULL) {
                /*
                 * No process. This is probably a kernel fault early
                 * in boot. Return EFAULT so as to panic instead of
                 * getting into an infinite faulting loop.
                 */
                return EFAULT;
        }
	
	if (as == NULL) {
		return EFAULT;
	}
	
	KASSERT(as->start_region != NULL);

	stacktop = USERSTACK;
	stackbase = USERSTACK - VM_STACKPAGES * PAGE_SIZE;
	
	if(faultaddress >= stackbase && faultaddress < stacktop) {
		valid_addr = true;
	} else if (faultaddress >= as->heap_start && faultaddress < as->heap_end) {
		valid_addr = true;
	} else {
		while(rg != NULL) {
			if(faultaddress >= rg->start && faultaddress < (rg->start + rg->size)){
			//	if(faulttype == VM_FAULT_READ && rg->read == true) {
					valid_addr = true;
					break;
			/*	} else if (faulttype == VM_FAULT_WRITE && rg->write == true) {
					valid_addr = true;
					break;	
				} else if (faulttype == VM_FAULT_READONLY) {
					
				}
				valid_addr = false;
				break;
			*/	
			}
			rg = rg->next;
		}
	}
	if(valid_addr == false) {
		return EFAULT;
	}
	if(as->start_page_table == NULL) {
		ppage  = getppages(1);
		if(ppage == 0) {
			return ENOMEM;
		}
		as->start_page_table = kmalloc(sizeof(struct page_table_entry));
		pte = as->start_page_table;
		pte->as_vpage = faultaddress;
		pte->as_ppage = ppage;
		pte->vpage_permission = 0;
		pte->next = NULL; 	
	} else {
		pte = as->start_page_table;
		while(pte != NULL) {
			if(pte->as_vpage == faultaddress) {
				ppage = pte->as_ppage;
				break;
			}
			pte_prev = pte;
			pte = pte->next;
		}
		if (pte == NULL) {
			ppage  = getppages(1);
			if(ppage == 0) {
				return ENOMEM;
			}
                	pte_prev->next = kmalloc(sizeof(struct page_table_entry));
			pte = pte_prev->next;
                	pte->as_vpage = faultaddress;
                	pte->as_ppage = ppage;
                	pte->vpage_permission = 0;
                	pte->next = NULL;
		}
	}
	
	KASSERT((ppage & PAGE_FRAME) == ppage);
	spl = splhigh();

        for (i=0; i<NUM_TLB; i++) {
                tlb_read(&ehi, &elo, i);
                if (elo & TLBLO_VALID) {
                        continue;
                }
                ehi = faultaddress;
                elo = ppage | TLBLO_DIRTY | TLBLO_VALID;
                tlb_write(ehi, elo, i);
                splx(spl);
                return 0;
        }
	ehi = faultaddress;
        elo = ppage | TLBLO_DIRTY | TLBLO_VALID;
	tlb_random(ehi, elo);
        splx(spl);	 
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

