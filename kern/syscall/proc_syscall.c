#include<proc_syscall.h>
#include<proc.h>
#include<addrspace.h>
#include<syscall.h>
#include<kern/errno.h>
#include<synch.h>
#include<current.h>
#include<thread.h>

static struct proc * process_create(const char *name) {
	struct proc *proc;

        proc = kmalloc(sizeof(*proc));
        if (proc == NULL) {
                return NULL;
        }
        proc->p_name = kstrdup(name);
        if (proc->p_name == NULL) {
                kfree(proc);
                return NULL;
        }

        proc->p_numthreads = 0;
        spinlock_init(&proc->p_lock);

        /* VM fields */
        proc->p_addrspace = NULL;

        /* VFS fields */
        proc->p_cwd = NULL;

        /* Setting file table to NULL*/
        for (int i = 0; i < OPEN_MAX; i++)
                proc->file_table[i] = NULL;

        /* Process call related changes start here */
        proc->proc_id = -1;
        proc->parent_id = -1;
        proc->exit_status = false;
        proc->exit_code = -1;
        /* Process Call related changes end here  */

        return proc;
}

int sys_fork(pid_t *child_pid, struct trapframe *tf) {

	struct trapframe *tf_child;
	if(proc_counter >= PID_MAX){
		return ENPROC;
	}
	int i = 0;
	
	/* Process Initialization */
	struct proc *childproc = process_create("child_process");
	if (childproc == NULL) {
		return -1;
	}

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
//	tf_child = (struct trapframe *)kmalloc(sizeof(struct trapframe));
	tf_child = tf;
	err = thread_fork("child_thread", childproc, enter_forked_process, (struct trapframe *)tf_child, (unsigned long)NULL);
	if (err) {
		return err;
	}
	last->next = (struct proc_node *)kmalloc(sizeof(struct proc_node));
	last = last->next;
	last->proc = childproc;
	last->next = NULL;
	*child_pid = childproc->proc_id;
	return 0;	
}

int sys_getpid(pid_t *curproc_pid){
	*curproc_pid = curproc->proc_id;
	return 0;
}
