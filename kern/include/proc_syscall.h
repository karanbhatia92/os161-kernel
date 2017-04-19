#ifndef _PROC_SYSCALL_H_
#define _PROC_SYSCALL_H_
#include <types.h>
#include <proc.h>
#include <mips/trapframe.h>
#include <synch.h>
#include <limits.h>
/* Process Table that is used to map between process structures and PIDs */
#define MAX_NO_ARGS 3851
char arguments[ARG_MAX];
int arg_pointers[MAX_NO_ARGS];
struct lock *arg_lock;
struct proc *proc_table[OPEN_MAX];
extern int proc_counter;
int sys_fork(pid_t *child_pid, struct trapframe *tf);
int sys_waitpid(pid_t pid, int *status, int options, pid_t* retval);
void sys_exit(int exitcode);
int sys_getpid(pid_t *curproc_pid);
int sys_execv(const char *program, char **args);
void strcopy(char **destination, char **source, int* bytes_remaining);
#endif
