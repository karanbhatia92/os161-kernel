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
		return -1;
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
/*	
	tf_child = (struct trapframe *)kmalloc(sizeof(struct trapframe));
	memcpy(tf_child, tf, sizeof(tf));
	tf_child->tf_v0 = 0;
	tf_child->tf_a3 = 0;
	tf_child->tf_epc += 4;
*/
	kprintf("PID %d forking child with PID : %d \n", childproc->parent_id, childproc->proc_id);
	tf_child = (struct trapframe *)kmalloc(sizeof(struct trapframe));
	*tf_child = *tf;
	kprintf("PID %d stored in proc table at %d \n", childproc->proc_id, j);
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
	
	for(i = 0; i < OPEN_MAX; i++){
		if(proc_table[i] != NULL){
			if(proc_table[i]->proc_id == pid)
			break;
		}
		if(i == OPEN_MAX - 1)
		return ESRCH;
	}
	proc_table[i]->parent_waiting = true;
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
	kprintf("Parent PID %d calling waitpid on PID %d \n", proc_table[i]->parent_id, proc_table[i]->proc_id);
	KASSERT(proc_table[i] != NULL);
	lock_acquire(proc_table[i]->lock);
	if(proc_table[i]->exit_status){
		err = copyout(&proc_table[i]->exit_code, (userptr_t)status, sizeof(proc_table[i]->exit_code));
		if(err){
			lock_release(proc_table[i]->lock);
			proc_destroy(proc_table[i]);
			proc_table[i] = NULL;
			return err;
		}
		lock_release(proc_table[i]->lock);
		*retval = proc_table[i]->proc_id;
		proc_destroy(proc_table[i]);
		proc_table[i] = NULL;
		return 0;
	}
	kprintf("PID %d going to wait for PID %d \n", proc_table[i]->parent_id, proc_table[i]->proc_id);
	cv_wait(proc_table[i]->cv, proc_table[i]->lock);
	err = copyout(&proc_table[i]->exit_code, (userptr_t)status, sizeof(proc_table[i]->exit_code));
	if(err){
		lock_release(proc_table[i]->lock);
		proc_destroy(proc_table[i]);
		proc_table[i] = NULL;
		return err;
	}
	lock_release(proc_table[i]->lock);
	*retval = proc_table[i]->proc_id;
	kprintf("Parent PID %d done waiting for PID : %d, now destroying it \n", proc_table[i]->parent_id, *retval);
	proc_destroy(proc_table[i]);
	proc_table[i] = NULL;
	kprintf("After proc destroy parent \n");
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
	kprintf("Called exit for PID %d in proc table at %d : %s \n", proc_table[i]->proc_id, i, curproc->p_name);
	lock_acquire(curproc->lock);
	curproc->exit_status = true;
	curproc->exit_code = _MKWAIT_EXIT(exitcode);
	KASSERT(curproc->exit_status == proc_table[i]->exit_status);
	KASSERT(curproc->exit_code == proc_table[i]->exit_code);
	if(curproc->parent_waiting){
	kprintf("PID %d Signaling Parent PID %d to wake up \n", curproc->proc_id, curproc->parent_id);
	cv_signal(curproc->cv, curproc->lock);
	lock_release(curproc->lock);
	thread_exit();
	} else {
		kprintf("No Parent waiting for PID %d, now destroying it \n", curproc->proc_id);
		lock_release(curproc->lock);
		//proc_destroy(curproc);
		//proc_table[i] = NULL;
		kprintf("After proc destroy \n");
		thread_exit();
	}
}

int sys_getpid(pid_t *curproc_pid){
	*curproc_pid = curproc->proc_id;
	return 0;
}

int sys_execv(const char *program){
	(void)program;
	return 0;
}
