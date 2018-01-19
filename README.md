# Operating System OS/161 Kernel Development

## About OS/161
OS/161 is a teaching operating system, that is, a simplified system used for teaching operating systems classes. It is BSD-like in feel and has more "reality" than most other teaching OSes; while it runs on a simulator it has the structure and design of a larger system.

## About System/161
System/161 is a machine simulator that provides a simplified but still realistic environment for OS hacking. It is a 32-bit MIPS system supporting up to 32 processors, with up to 31 hardware slots each holding a single simple device (disk, console, network, etc.) It was designed to support OS/161, with a balance of simplicity and realism chosen to make it maximally useful for teaching. However, it also has proven useful as a platform for rapid development of research kernel projects.

System/161 supports fully transparent debugging, via remote gdb into the simulator. It also provides transparent kernel profiling, statistical monitoring, event tracing (down to the level of individual machine instructions) and one can connect multiple running System/161 instances together into a network using a "hub" program.

## Build Your Own OS
The best way to learn about operating systems is to implement one. So programming assignments are the heart of [ops-class.org](www.ops-class.org).

ops-class.org assignments use the excellent [OS/161](http://os161.eecs.harvard.edu/) instructional operating system. OS/161 was developed at Harvard University by David Holland, Margo Seltzer, and others. It provides some of the realism of large operating systems like Linux. But it remains compact enough to give you a chance to implement large OS subsystems—like virtual memory—yourself. [This paper](https://dl.acm.org/citation.cfm?id=563383) provides a good overview of OS/161.

The assignments also provide a variety of different programming challenges. ASST1 is just a warmup. ASST2 challenges you to connect two existing interfaces. ASST3 provides the opportunity to implement a large piece of functionality and design several internal interfaces. These challenges are similar to those that you will face in industry or when building large software systems. And all the assignments require understanding a large and complex code base.

If that wasn’t exciting enough, these assignments are the same ones that kept Mark Zuckerberg [busy in his last semester at Harvard](https://www.youtube.com/watch?v=-3Rt2_9d7Jg)—while he was starting Facebook. Many students at multiple institutions have benefited from their struggle with OS/161. 

A bit more detail about each assignment below.

### [ASST0](https://www.ops-class.org/asst/0/)
This first assignment introduces you to the programming environment you will be working in this semester: the OS/161 operating system, the sys161	simulator, the GNU debugger (GDB), and the Git revision control system.

### [ASST1](https://www.ops-class.org/asst/1/)
Your first real taste of kernel programming. Implement critical kernel synchronization primitives—locks, condition variables and reader-writer locks. Next, use them to solve a few simple toy synchronization problems.

### [ASST2](https://www.ops-class.org/asst/2/)
The first big and complex assignment. Implement the system call interface. When you are finished, your kernel can run user programs.

### [ASST3](https://www.ops-class.org/asst/3/)
The mountain top. A large amount of code to implement and several internal interfaces to design. Implement virtual memory, including address translation, TLB management, page replacement and swapping. When you are done, your kernel can run forever without running out of memory.

## [ASST1: Synchronization](https://www.ops-class.org/asst/1/)
In this assignment you will implement new **synchronization primitives** for OS/161 and use them to solve several **synchronization problems.**
To complete this assignment you will need to be familiar with the OS/161 thread code. The thread system provides interrupts, control functions, spinlocks, and semaphores.   
You will implement the following synchronization primitives:  
**locks, condition variables and reader/writer locks.**  

## [ASST2: System Calls](https://www.ops-class.org/asst/2/)
In this assignment you will add **process and system call support to your OS/161 kernel.**
Currently no support exists for running user processes—​the tests you have run up to this point have run in the kernel as kernel threads. By the time you finish ASST2 you will have the ability to launch a simple shell and enter a somewhat-familiar UNIX environment. Indeed, future tests will be run as user processes, not from the kernel menu.

**Implementing system calls and exception handling.**
The full range of system calls that we think you might want over the course of the semester is listed in kern/include/kern/syscall.h. For this assignment you should implement:  
**File system support: open, read, write, lseek, close, dup2, chdir, and \_\_getcwd.  
Process support: getpid, fork, execv, waitpid, and _exit.**  

## [ASST3: Virtual Memory](https://www.ops-class.org/asst/3/)
In this assignment you will add support for virtual memory to your OS/161	kernel.

In ASST2 you improved OS/161 to the point that you could run user processes. However, there are a number of shortcomings in the current system. A process’s size is limited to 64 pages by the number of Translation Lookaside Buffer (TLB) entries. In addition, while your kernel allocator kmalloc correctly manages sub-page allocations—​memory requests for under 4 KB—​single and multiple-page requests are not properly returned to the system, meaning that pages are discarded after use. This severely limits the lifetime of your system, as you may have observed. Your current kernel cannot run forever.

In this assignment we will adapt OS/161 to take full advantage of the simulated hardware by implementing management of the MIPS software-managed TLB. You will write the code to manage the TLB. You will also write the code to implement paging—​the mechanism by which memory pages of an active process can be sent to disk when memory is needed, and restored to memory when required by the program. This permits many processes to share limited physical memory while providing each process with the abstraction of a very large amount of virtual memory. Finally, you will correctly handle page reclamation, allowing your system to reclaim memory when processes exit and run forever.  
To implement virtual memory and swapping, you must do the following:
1. Implement the code that services TLB faults.
2. Add paging to your operating system.
3. Add the sbrk system call, so that the malloc library we provide works.


### Implementation  
ASST3 is divided into three incremental subtargets:  
1. **Implement the Coremap**  
One way to manage physical memory is to maintain a coremap data structure, a sort of reverse page table. Instead of being indexed by virtual addresses, a coremap is indexed by its physical page number and contains the virtual address and address space identifier for the virtual page currently backed by the page in physical memory.  


2. **Build the Virtual Address Spaces (Page Tables)**  
   1. **TLB Handling:** In this part of the assignment, you will modify OS/161 to handle TLB faults. Additionally, you need to guarantee that the TLB state is initialized properly on a context switch.  
   2. **Paging:** In this part of the assignment, you will modify OS/161 to handle page faults. When you have completed this task your system will generate an exception when a process tries to access an address that is not memory-resident and then handle that exception and continue running the user process.  
   The VM should be able to properly handle page faults from a single user program and concurrent user programs, which in turn tests both the page fault logic and TLB handling.  
   3. **Userspace Heap:** One of the limitations of dumbvm is that it has no support for userspace heaps, meaning that all programs running on OS/161 must statically declare all memory. Your VM system will change this by implementing the sbrk system call, which allows user programs to dynamically allocate and free memory by changing the heap breakpoint.  
   
   
3. **Swapping**  
The third and final part of ASST3 involves building the VM subsystem’s ability to operate with less physical memory than needed by the running processes.  
You will need routines to move a page from disk to memory and from memory to disk. You will also need to decide how to implement backing store—​the place on disk where you store virtual pages not currently stored in physical memory. You will need to store evicted pages and find them when you need them. You should maintain a bitmap that describes the space in your swap area. Think of the swap area as a collection of chunks, where each chunk holds a page. Use the bitmap to keep track of which chunks are full and which are empty. The empty chunks can be evicted into. You also need to keep track, for each page of a given address space, of which chunk in the swap area it maps onto. When there are too many pages to fit in physical memory, you can write (modified) pages out to swap.  
When you need to evict a page, you first need to determine what page to evict. Please implement one **page replacement policy** for ASST3, although you want to experiment with several. Once you have chosen a page, you look up the physical address in the coremap, locate the address space whose page you are evicting and modify the corresponding state information to indicate that the page will no longer be in memory. Then you can evict the page. If the page is dirty, it must first be written to the backing store.



