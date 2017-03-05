#ifndef _FILE_SYSCALL_H_
#define _FILE_SYSCALL_H_

struct file_handle {
        struct vnode *vnode;
        off_t offset;
        struct lock *lock;
        int destroy_count; /* To check if pointing file descriptors are 0 */
        int mode_open;
};

int sys_open(const char *filename, int flags, int *retval);
int sys_read(int fd, void *buf, size_t bufflen, int32_t *retval);
int sys_write(int fd, const void *buff, size_t bufflen, int32_t *ret);
int sys_close(int fd);

#endif
