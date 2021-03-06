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
	//************ Test ***********//
	kprintf("Address inside tf is : %p , Address inside tf_child is %p , \n", tf, tf_child);
	if (tf == NULL || tf_child == NULL) {
		kprintf("parent trapframe is null");
	}
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
	if(curproc->parent_waiting){
	//kprintf("PID %d Signaling Parent PID %d to wake up \n", curproc->proc_id, curproc->parent_id);
	cv_signal(curproc->cv, curproc->lock);
	lock_release(curproc->lock);
	thread_exit();
	} else {
		//kprintf("No Parent waiting for PID %d, now destroying it \n", curproc->proc_id);
		lock_release(curproc->lock);
		//proc_destroy(curproc);
		//proc_table[i] = NULL;
		//kprintf("After proc destroy \n");
		thread_exit();
	}
}

int sys_getpid(pid_t *curproc_pid) {
	*curproc_pid = curproc->proc_id;
	return 0;
}

int sys_execv(const char *program, char **args) {
	//(void)args;
	int err = 0;
	int i = 0;
	size_t got = 0;
	int arg_counter = 0;
	struct addrspace *as;
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;
	int arg_length = 0;
	//char **args_ex = NULL;	
	//char **args_ex1 = NULL;
	char **args_stack_ptr = NULL;
	char *program_ex = NULL;
	//char *program_cp = program_ex;
	//char *program_ex1;
	int bytes_remaining = ARG_MAX;
	userptr_t argv_ex = NULL;
	//(void)program;
	//arg_length = strlen(program);
	//program_ex = kmalloc(arg_length+1);
	lock_acquire(arg_lock);
	//err = copyinstr((const_userptr_t)program, arguments, ARG_MAX, &got);
	//if(err){
		//kfree(program_ex);
		//lock_release(arg_lock);
		//return err;
	//}
	//bytes_remaining -= strlen(arguments)+1;
	//if(bytes_remaining < 0) {
		//lock_release(arg_lock);
		//return E2BIG;
	//}
	//arg_length += strlen(arguments)+1;
	//strcopy(&program_ex1, &program_ex, &bytes_remaining);

	while(args[arg_counter] != NULL){
		arg_counter++;
	}
	kprintf("print arg_counter: %d", arg_counter);
	//args_ex = kmalloc(sizeof(char *)*arg_counter);
	//args_ex1 = kmalloc(sizeof(char *)*arg_counter);
	for (i = 0; i < arg_counter ; i++) {
		/*if(bytes_remaining == 64104){
			kprintf("Caught ast \n");
		}*/
		//arg_length = strlen(args[i]);
		//args_ex1[i] = kmalloc(arg_length+1);	
		err = copyinstr((const_userptr_t)args[i], &arguments[arg_length], ARG_MAX, &got);
		if (err) {
			if(err == 6){
				kprintf("Caught \n");
			}
			//kfree(program_ex);
			//kfree(args_ex);
			//kfree(args_ex1);
			lock_release(arg_lock);
			return err;
		}
		
		//args_ex1[i] = kmalloc((sizeof(char) * strlen(args_ex[i]))+1);
		//strcpy(args_ex1[i], args_ex[i]);
		//kfree(args_ex[i]);
		bytes_remaining -= strlen(&arguments[arg_length])+1;
		kprintf("Bytes remaining : %d", bytes_remaining);
		if(bytes_remaining < 0) {
			lock_release(arg_lock);
			return E2BIG;
		}
		//kprintf("Arguments: %s", &arguments[arg_length]);
		arg_length += strlen(&arguments[arg_length])+1;
		//strcopy(&args_ex1[i], &args_ex[i], &bytes_remaining);	
		//kprintf("Printing bytes remaining to check %d \n", bytes_remaining);
		//kprintf("Printing args_ex1 %d string %s and bytes remaining %d \n", i, args_ex1[i], bytes_remaining);
	}
	//kfree(args_ex);
	arg_length = 0;
	
	program_ex = kmalloc(strlen(arguments)+1);
	arg_length += strlen(arguments)+1;
	strcpy(program_ex, arguments);
	result = vfs_open(program_ex, O_RDONLY, 0, &v);
	if (result) {
		kfree(program_ex);
		lock_release(arg_lock);
		return result;
	}
	
	as = as_create();
	if (as == NULL) {
		vfs_close(v);
		kfree(program_ex);
		lock_release(arg_lock);
		return ENOMEM;
	}

	proc_setas(as);
	as_activate();
	
	result = load_elf(v, &entrypoint);
	if (result) {
		vfs_close(v);
		kfree(program_ex);
		lock_release(arg_lock);
		return result;
	}
	
	vfs_close(v);

	result = as_define_stack(as, &stackptr);
	if(result) {
		kfree(program_ex);
		lock_release(arg_lock);
		return result;
	}

	args_stack_ptr = kmalloc(sizeof(char *)*arg_counter);
	for(i = 0; i < arg_counter; i++) {
		int len = strlen(&arguments[arg_length]);
		int no_null_terminator = 4 - len%4;
		int blocks = len/4;
		int copy_len = 0;
		if (no_null_terminator == 4) {
			copy_len = len + 4;
		}else{
			copy_len = len + no_null_terminator;	
		}
		args_stack_ptr[i] = kmalloc(copy_len);
		strcpy(args_stack_ptr[i],&arguments[arg_length]);
		arg_length += strlen(&arguments[arg_length])+1;
		for (int j = 0; j < no_null_terminator; j++){
			args_stack_ptr[i][len + j] = '\0';
		}
		stackptr -= 4 * (blocks + 1);
		copyout(args_stack_ptr[i], (userptr_t)stackptr, copy_len);
		args_stack_ptr[i] = (char *)stackptr;
	}

	for(i = arg_counter ; i >= 0; i--) {
		char *temp = NULL;
		if (i == arg_counter) {
			temp = NULL;
		}else{
			temp = args_stack_ptr[i];
		}
		stackptr -= 4;
		copyout(&temp, (userptr_t)stackptr, sizeof(temp)); 
	}

	argv_ex = (userptr_t)stackptr;
	stackptr = stackptr - 4;
	lock_release(arg_lock);
	//kprintf("Arg counter %d \n", arg_counter);	
	enter_new_process(arg_counter, argv_ex, NULL, stackptr, entrypoint);
	return EINVAL;
}

void strcopy(char **destination, char **source, int* bytes_remaining) {

	*destination = kmalloc((sizeof(char) * strlen(*source))+1);
	strcpy(*destination, *source);
	kfree(*source);
	*bytes_remaining -= strlen(*destination);

}

