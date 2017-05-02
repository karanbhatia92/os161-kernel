#include <proc_syscall.h>
#include <proc.h>
#include <addrspace.h>
#include <syscall.h>
#include <kern/errno.h>
#include <synch.h>
#include <current.h>
#include <thread.h>
#include <copyinout.h>
#include <kern/wait.h>
#include <vfs.h>
#include <copyinout.h>
#include <kern/fcntl.h>
#include <vnode.h>
#include <vm.h>

struct lock *arg_lock;
int sys_fork(pid_t *child_pid, struct trapframe *tf) {
	int i = 0;
	int j = 0;	
	struct trapframe *tf_child;

	if(proc_counter >= PID_MAX){
		return ENPROC;
	}


	/* Process Initialization */
	struct proc *childproc = process_create("child_process");
	if (childproc == NULL) {
		return ENOMEM;
	}

	while(proc_table[j] != NULL){
		if(j == OPEN_MAX - 1){
			return ENPROC;
		}
		j++;
	}	
	proc_table[j] = childproc;

	int err = as_copy(curproc->p_addrspace, &childproc->p_addrspace);
	if(err) {
		return err;	
	}
	proc_counter++;
	childproc->proc_id = proc_counter;
	childproc->parent_id = curproc->proc_id;
	for(i = 0; i < OPEN_MAX; i++) {
		if(curproc->file_table[i] != NULL){
			lock_acquire(curproc->file_table[i]->lock);
			childproc->file_table[i] = curproc->file_table[i];
			curproc->file_table[i]->destroy_count++;
			lock_release(curproc->file_table[i]->lock);
		}
	}

        spinlock_acquire(&curproc->p_lock);
        if (curproc->p_cwd != NULL) {
                VOP_INCREF(curproc->p_cwd);
                childproc->p_cwd = curproc->p_cwd;
        }
        spinlock_release(&curproc->p_lock);

/*	
	tf_child = (struct trapframe *)kmalloc(sizeof(struct trapframe));
	memcpy(tf_child, tf, sizeof(tf));
	tf_child->tf_v0 = 0;
	tf_child->tf_a3 = 0;
	tf_child->tf_epc += 4;
*/
	//kprintf("PID %d forking child with PID : %d \n", childproc->parent_id, childproc->proc_id);
	tf_child = (struct trapframe *)kmalloc(sizeof(struct trapframe));
	if(tf_child == NULL) {
		return ENOMEM;
	}
	//************ Test ***********//
	//kprintf("Address inside tf is : %p , Address inside tf_child is %p , \n", tf, tf_child);
	//if (tf == NULL || tf_child == NULL) {
		//kprintf("parent trapframe is null");
	//}
	//****************************//
	*tf_child = *tf; 
	//kprintf("PID %d stored in proc table at %d \n", childproc->proc_id, j);
	err = thread_fork("child_thread", childproc, enter_forked_process, (struct trapframe *)tf_child, (unsigned long)NULL);
	if (err) {
		return err;
	}
//	kprintf("At %d, proc id is %d \n", j, proc_table[j]->proc_id);
	*child_pid = childproc->proc_id;
	return 0;	
}

int sys_waitpid(pid_t pid, int *status, int options, pid_t* retval) {
	int i = 0;
	int err = 0;
	if(options != 0){
		return EINVAL;
	}
	if(curproc->proc_id == pid){
		return ECHILD;
	}
	
	for(i = 0; i < OPEN_MAX; i++){
		if(proc_table[i] != NULL){
			if(proc_table[i]->proc_id == pid)
			break;
		}
		if(i == OPEN_MAX - 1)
		return ESRCH;
	}
/*
	while(proc_table[i]->proc_id != pid){
		if(i == OPEN_MAX - 1){
			return ESRCH;
		}
		i++;
	}
*/
	if(proc_table[i]->proc_id != 2){
	if(proc_table[i]->parent_id != curproc->proc_id){
		return ECHILD;
	}}
	//kprintf("Parent PID %d calling waitpid on PID %d \n", proc_table[i]->parent_id, proc_table[i]->proc_id);
	KASSERT(proc_table[i] != NULL);
	lock_acquire(proc_table[i]->lock);
	if(proc_table[i]->exit_status){
		if(status != NULL) {
			err = copyout(&proc_table[i]->exit_code, (userptr_t)status, sizeof(proc_table[i]->exit_code));
			if(err){
				lock_release(proc_table[i]->lock);
				proc_destroy(proc_table[i]);
				proc_table[i] = NULL;
				return err;
			}
		}
		lock_release(proc_table[i]->lock);
		*retval = proc_table[i]->proc_id;
		proc_destroy(proc_table[i]);
		proc_table[i] = NULL;
		return 0;
	}
	//kprintf("PID %d going to wait for PID %d \n", proc_table[i]->parent_id, proc_table[i]->proc_id);
	cv_wait(proc_table[i]->cv, proc_table[i]->lock);
	if(status != NULL) {
		err = copyout(&proc_table[i]->exit_code, (userptr_t)status, sizeof(proc_table[i]->exit_code));
		if(err){
			lock_release(proc_table[i]->lock);
			proc_destroy(proc_table[i]);
			proc_table[i] = NULL;
			return err;
		}
	}
	lock_release(proc_table[i]->lock);
	*retval = proc_table[i]->proc_id;
	//kprintf("Parent PID %d done waiting for PID : %d, now destroying it \n", proc_table[i]->parent_id, *retval);
	proc_destroy(proc_table[i]);
	proc_table[i] = NULL;
	//kprintf("After proc destroy parent \n");
	return 0;
}

void sys_exit(int exitcode){
	int i = 0;
	/*
	if(proc_table[0]->proc_id == curproc->proc_id){
		kprintf("Called exit for Proc %d which is at 0 : %s \n", curproc->proc_id, curproc->p_name);
		//thread_exit();
		return;
	}
	*/
	for(i = 0; i < OPEN_MAX; i++){
		if(proc_table[i] != NULL){
			if(proc_table[i]->proc_id == curproc->proc_id)
			break;
		}
		if(i == OPEN_MAX - 1)
		panic("Current process not found in process table");
	}
/*
	while(proc_table[i]->proc_id != curproc->proc_id){
                if(i == OPEN_MAX - 1){
                        panic("Current process not found in process table");
                }
                i++;
        }
*/
	//kprintf("Called exit for PID %d in proc table at %d : %s \n", proc_table[i]->proc_id, i, curproc->p_name);
	lock_acquire(curproc->lock);
	curproc->exit_status = true;
	curproc->exit_code = _MKWAIT_EXIT(exitcode);
	KASSERT(curproc->exit_status == proc_table[i]->exit_status);
	KASSERT(curproc->exit_code == proc_table[i]->exit_code);
	cv_signal(curproc->cv, curproc->lock);
	lock_release(curproc->lock);
	thread_exit();
}

int sys_getpid(pid_t *curproc_pid) {
	*curproc_pid = curproc->proc_id;
	return 0;
}

int sys_execv(const char *program, char **args1) {

	int err = 0;
	int i = 0;
	size_t got = 0;
	int extra_vals = 0;
	int padding = 0;
	int arg_counter = 0;
	struct addrspace *as;
	struct addrspace *old_as;
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;
	int current_position = 0;
	int bytes_remaining = ARG_MAX;
	userptr_t argv_ex = NULL;
	char prog_name[PATH_MAX];
	char *args;
		
	err = copyin((const_userptr_t)args1, &args, sizeof(args));
	if(err){
		return err;
	}
	lock_acquire(arg_lock);	
	err = copyinstr((const_userptr_t)program, prog_name, PATH_MAX, &got);
	if (err) {
		lock_release(arg_lock);
		return err;
	}
	while(args1[arg_counter] != NULL){
		err = copyinstr((const_userptr_t)args1[arg_counter], &arguments[current_position], bytes_remaining, &got);
		if(err) {
			lock_release(arg_lock);
			return err;
		}
		arg_pointers[arg_counter] = current_position; 
		current_position += got;
		extra_vals = got%4;
		if(extra_vals != 0) {
			padding = 4 - extra_vals;
			for(i = 0; i < padding; i++) {
				arguments[current_position + i] = '\0';
			}
			current_position += padding;
			bytes_remaining -= (got + padding);
		}else {
			bytes_remaining -= got;
		}
		arg_counter++;
		
	}
			
	result = vfs_open(prog_name, O_RDONLY, 0, &v);
	if (result) {
		lock_release(arg_lock);
		return result;
	}
	
	as = as_create();
	if (as == NULL) {
		vfs_close(v);
		return ENOMEM;
	}

	old_as = proc_setas(as);
	as_destroy(old_as);
	as_activate();
	
	result = load_elf(v, &entrypoint);
	if (result) {
		lock_release(arg_lock);
		as_destroy(as);
		as_activate();
		vfs_close(v);
		return result;
	}
	
	vfs_close(v);

	result = as_define_stack(as, &stackptr);
	if(result) {
		lock_release(arg_lock);
		return result;
	}
	
	KASSERT(current_position % 4 == 0);
	stackptr -= current_position;
	copyout(arguments, (userptr_t)stackptr, current_position);

	stackptr -= 4;
	char *temp = NULL;
	copyout(&temp, (userptr_t)stackptr, sizeof(temp));
	
	for(i = arg_counter-1 ; i >= 0; i--) {
		stackptr -= 4;
		int val = USERSTACK - (current_position - arg_pointers[i]);
		temp = (char *)val;
		copyout(&temp, (userptr_t)stackptr, sizeof(temp));
	}

	argv_ex = (userptr_t)stackptr;
	stackptr = stackptr - 4;
	lock_release(arg_lock);
	enter_new_process(arg_counter, argv_ex, NULL, stackptr, entrypoint);
	return EINVAL;
}

int sys_sbrk(intptr_t amount, vaddr_t *retval) {
	
	long old_break;
	struct addrspace *as;
	int pages;
	long new_break;
	vaddr_t remove_vpage;
	struct page_table_entry *temp_pte;
	struct page_table_entry *prev_pte;

	as = proc_getas();
	KASSERT(as != NULL);
	if(amount < 0) {
		amount = amount * -1; 
		new_break = (long)as->heap_end - amount;
		if(new_break < 0) {
			return EINVAL;
		}
	} else if(amount > 0) {
		new_break = (long)as->heap_end + amount;
		if(new_break < 0) {
			return ENOMEM;
		}
	} else {
		*retval = as->heap_end;
		return 0;
	}
	if (amount % PAGE_SIZE != 0) {
		return EINVAL;	
	} 

	pages = amount/PAGE_SIZE;
	//kprintf("Page size %d,  Pages : %d", PAGE_SIZE, pages);
	old_break = as->heap_end;
	
	temp_pte = as->start_page_table;
	KASSERT(temp_pte != NULL);
	prev_pte = temp_pte;
	KASSERT(old_break != new_break);
	if (old_break < new_break) {
		if(new_break >= (long)(USERSTACK - VM_STACKPAGES * PAGE_SIZE)){
			return ENOMEM;
		}


		/*	
		new_pages_addr = getppageswrapper(pages);
		if(new_pages_addr == 0) {
			return ENOMEM;
		}

		as->heap_end = new_break;
	
		while(temp_pte->next != NULL) {
			temp_pte = temp_pte->next;
		}

		for (int i = 0; i < pages; i++){
			temp_pte->next = kmalloc(sizeof(struct page_table_entry));
			temp_pte = temp_pte->next;
			temp_pte->next = NULL;
			temp_pte->as_vpage = old_break + (i+1)*PAGE_SIZE;
			KASSERT(temp_pte->as_vpage % PAGE_SIZE == 0);
			temp_pte->as_ppage = new_pages_addr + i * PAGE_SIZE;
			KASSERT(temp_pte->as_ppage % PAGE_SIZE == 0);
			temp_pte->vpage_permission = 0;
		}
		*/	
	}else {
		if(new_break < (long)as->heap_start) {
			return EINVAL;
		}
		
		for (int i = 0; i < pages; i++) {
			remove_vpage = old_break - (i+1)*PAGE_SIZE;
			temp_pte = as->start_page_table;
			prev_pte = temp_pte;
			//bool found = false;
			while(temp_pte != NULL) {
				if(temp_pte->as_vpage == remove_vpage) {
					if(temp_pte->is_swapped) {
						bitmap_unmark_wrapper(temp_pte->diskpage_location);	
					} else {
						free_ppages(temp_pte->as_ppage);
						tlb_invalidate_entry(remove_vpage);
					}
					if(temp_pte == as->start_page_table) {
						temp_pte = temp_pte->next;
						kfree(prev_pte);
						as->start_page_table = temp_pte;
						//found = true;
						break;		
					} else if (temp_pte->next == NULL) {
						kfree(temp_pte);
						prev_pte->next = NULL;
						//found = true;
						break;
					} else {
						temp_pte = temp_pte->next;
						kfree(prev_pte->next);
						prev_pte->next = temp_pte;
						//found = true;
						break;
					}
				} else {
					prev_pte = temp_pte;
					temp_pte = temp_pte->next;
				}
			}
			/*
			if (found == false) {
				return EINVAL;
			}
			*/
		}
	}
	as->heap_end = (vaddr_t)new_break;
	*retval = (vaddr_t)old_break;
	return 0;
}
