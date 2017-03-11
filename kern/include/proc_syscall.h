#ifndef _PROC_SYSCALL_H_
#define _PROC_SYSCALL_H_
#include<types.h>
#include<proc.h>
#include<mips/trapframe.h>
/* Process Table that is used to map between process structures and PIDs */
struct proc_node{
	struct proc *proc;
	struct proc_node *next;
};

struct proc_node* first;
struct proc_node* last;
int proc_counter;

int sys_fork(pid_t *child_pid, struct trapframe *tf);
int sys_getpid(pid_t *curproc_pid);
#endif
