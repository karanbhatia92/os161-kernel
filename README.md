# Operating System OS/161 Kernel Development

## About OS/161
OS/161 is a teaching operating system, that is, a simplified system used for teaching operating systems classes. It is BSD-like in feel and has more "reality" than most other teaching OSes; while it runs on a simulator it has the structure and design of a larger system.

## About System/161
System/161 is a machine simulator that provides a simplified but still realistic environment for OS hacking. It is a 32-bit MIPS system supporting up to 32 processors, with up to 31 hardware slots each holding a single simple device (disk, console, network, etc.) It was designed to support OS/161, with a balance of simplicity and realism chosen to make it maximally useful for teaching. However, it also has proven useful as a platform for rapid development of research kernel projects.

System/161 supports fully transparent debugging, via remote gdb into the simulator. It also provides transparent kernel profiling, statistical monitoring, event tracing (down to the level of individual machine instructions) and one can connect multiple running System/161 instances together into a network using a "hub" program.

## Build Your Own OS
The best way to learn about operating systems is to implement one. So programming assignments are the heart of [ops-class.org](www.ops-class.org).

ops-class.org assignments use the excellent [OS/161](http://os161.eecs.harvard.edu/) instructional operating system. OS/161 was developed at Harvard University by David Holland, Margo Seltzer, and others. It provides some of the realism of large operating systems like Linux. But it remains compact enough to give you a chance to implement large OS subsystems—like virtual memory—yourself. [This paper](https://dl.acm.org/citation.cfm?id=563383) provides a good overview of OS/161.
