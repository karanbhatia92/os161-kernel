/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <kern/errno.h>
#include <spl.h>
#include <lib.h>
#include <addrspace.h>
#include <vm.h>
#include <proc.h>
#include <mips/tlb.h>
#include <synch.h>
/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */

struct addrspace *
as_create(void)
{
	struct addrspace *as;

	as = kmalloc(sizeof(struct addrspace));
	if (as == NULL) {
		return NULL;
	}

	/*
	 * Initialize as needed.
	 */
	as->start_region = NULL;
	as->start_page_table = NULL;
	as->heap_start = 0;
	as->heap_end = 0;
	return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *newas;
	struct page_table_entry *temp_pte = NULL;
	struct page_table_entry *old_pte = NULL;
	struct region *old_region = NULL;
	struct region *temp_region = NULL;
	int err = -1;
	paddr_t ppage = 0;

	newas = as_create();
	if (newas==NULL) {
		return ENOMEM;
	}
	
	old_pte = old->start_page_table;
	old_region = old->start_region;

	while(old_pte != NULL){
		//if(newas->start_page_table == NULL) {
		if(newas->start_page_table == NULL) {
			temp_pte = kmalloc(sizeof(struct page_table_entry));
			if(temp_pte == NULL) {
				return ENOMEM;
			}
			newas->start_page_table = temp_pte;
		}else {
			KASSERT(temp_pte->next == NULL);
			temp_pte->next = kmalloc(sizeof(struct page_table_entry));
			if(temp_pte->next == NULL) {
				return ENOMEM;
			}
			temp_pte = temp_pte->next;
		}
		temp_pte->lock = lock_create("PTE_Lock");
		if(temp_pte->lock == NULL) {
			kfree(temp_pte);
			return ENOMEM;
		}
		lock_acquire(temp_pte->lock);
		temp_pte->as_vpage = old_pte->as_vpage;
		temp_pte->vpage_permission = old_pte->vpage_permission;
		temp_pte->state = UNMAPPED;
		temp_pte->next = NULL;
		ppage = getuserpage(1, newas, temp_pte->as_vpage);
		if(ppage == 0) {
			return ENOMEM;
		}
		lock_acquire(old_pte->lock);
		if(old_pte->state == SWAPPED) {
			bool unmark = false;
			err = diskblock_read(ppage, old_pte->diskpage_location, unmark);
			if (err) {
				panic("Cannot read from disk to memory");
			}	
		} else {
			memmove((void *)PADDR_TO_KVADDR(ppage),(const void *)PADDR_TO_KVADDR(old_pte->as_ppage),PAGE_SIZE);
		}
		lock_release(old_pte->lock);
		temp_pte->state = MAPPED;
		temp_pte->as_ppage = ppage;
		lock_release(temp_pte->lock);
		old_pte = old_pte->next;	
		/*}else {
			temp_pte->next = kmalloc(sizeof(struct page_table_entry));
			temp_pte = temp_pte->next;
			temp_pte->as_vpage = old_pte->as_vpage;
			temp_pte->vpage_permission = old_pte->vpage_permission;
			temp_pte->next = NULL:
			ppage = getppages(1);
			memmove((void *)PADDR_TO_KVADDR(ppage),(const void *)PADDR_TO_KVADDR(old_pte->as_ppage),PAGE_SIZE);
			temp_pte->as_ppage = ppage;
			old_pte = old_pte->next;
		}*/	
	}

	while(old_region != NULL) {
		if(newas->start_region == NULL) {
			temp_region = kmalloc(sizeof(struct region));
			if(temp_region == NULL) {
				return ENOMEM;
			}
			newas->start_region = temp_region;
		}else {
			KASSERT(temp_region->next == NULL);
			temp_region->next = kmalloc(sizeof(struct region));
			if(temp_region->next == NULL) {
				return ENOMEM;
			}
			temp_region = temp_region->next;
		}
		temp_region->start = old_region->start;
		temp_region->size = old_region->size;
		temp_region->npages = old_region->npages;
		temp_region->read = old_region->read;
		temp_region->write = old_region->write;
		temp_region->execute = old_region->execute;
		temp_region->next = NULL;
		old_region = old_region->next;
	}

	newas->heap_start = old->heap_start;
	newas->heap_end = old->heap_end;
	
	/*
	 * Write this.
	 */

	//(void)old;

	*ret = newas;
	return 0;
}

void
as_destroy(struct addrspace *as)
{
	/*
	 * Clean up as needed.
	 */
	KASSERT(as != NULL);
	struct page_table_entry *pte = as->start_page_table;
	struct region *rg = as->start_region;
	int err;
	struct page_table_entry *prev_pte = NULL;
	struct region *prev_rg = NULL;		
	while(pte != NULL) {
		lock_acquire(pte->lock);
		KASSERT(pte->state != UNMAPPED);
		if (pte->state == SWAPPED) {
			bitmap_unmark_wrapper(pte->diskpage_location);
		} else {
			err = free_ppages(pte->as_ppage); 
			if (err) {
				lock_release(pte->lock);
				continue;
			}			
		}
		lock_release(pte->lock);
		lock_destroy(pte->lock);
		prev_pte = pte;
		pte = pte->next;
		as->start_page_table = pte;
		kfree(prev_pte);
	}
	while(rg != NULL) {
		prev_rg = rg;
		rg = rg->next;
		kfree(prev_rg);
	}
	kfree(as);
}

void
as_activate(void)
{
        int i, spl;
	struct addrspace *as;

	as = proc_getas();
	if (as == NULL) {
		/*
		 * Kernel thread without an address space; leave the
		 * prior address space in place.
		 */
		return;
	}

	/*
	 * Write this.
	 */
        /* Disable interrupts on this CPU while frobbing the TLB. */
        spl = splhigh();

        for (i=0; i<NUM_TLB; i++) {
                tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
        }

        splx(spl);

}

void
as_deactivate(void)
{
	/*
	 * Write this. For many designs it won't need to actually do
	 * anything. See proc.c for an explanation of why it (might)
	 * be needed.
	 */
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
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t memsize,
		 int readable, int writeable, int executable)
{
	KASSERT(as != NULL);
	size_t npages;
	struct region *current_region;
	/*
	 * Write this.
	 */
        /* Align the region. First, the base... */
        memsize += vaddr & ~(vaddr_t)PAGE_FRAME;
        vaddr &= PAGE_FRAME;

        /* ...and now the length. */
        memsize = (memsize + PAGE_SIZE - 1) & PAGE_FRAME;

        npages = memsize / PAGE_SIZE;
	if (as->start_region == NULL) {
		current_region = kmalloc(sizeof(struct region));
		if(current_region == NULL) {
			return ENOMEM;
		}
		as->start_region = current_region;
	} else {
		current_region = as->start_region;
		while(current_region->next != NULL) {
			current_region = current_region->next;
		}
		current_region->next = kmalloc(sizeof(struct region));
		if(current_region->next == NULL) {
			return ENOMEM;
		}
		current_region = current_region->next;
	}
	current_region->start = vaddr;
	current_region->size = memsize;
	current_region->npages = npages;
	current_region->next = NULL;
	if (readable == 4) {
		current_region->read = true;
	} else {
		current_region->read = false;
	}
	if (writeable == 2) {
		current_region->write = true;
	} else {
		current_region->write = false;
	}
	if (executable == 1) {
		current_region->execute = true;
	} else {
		current_region->execute = false;
	}
	return 0;
}

int
as_prepare_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */
	vaddr_t heap_addr = 0;
	vaddr_t largest_vaddr = 0;
	size_t largest_vaddr_size = 0;
	struct region *rg = as->start_region;
	largest_vaddr = rg->start;
	while(rg != NULL) {
		if (rg->start > largest_vaddr) {
			largest_vaddr = rg->start;
			largest_vaddr_size = rg->size;
		}
		rg = rg->next;
	}
	heap_addr = largest_vaddr + largest_vaddr_size;
	KASSERT(heap_addr%4 == 0);
	as->heap_start = heap_addr;
	as->heap_end = heap_addr;
	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */

	(void)as;
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	/*
	 * Write this.
	 */

	/* Initial user-level stack pointer */
	(void)as;
	*stackptr = USERSTACK;

	return 0;
}

