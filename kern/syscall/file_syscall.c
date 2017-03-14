#include <types.h>
#include <proc.h>
#include <current.h>
#include <vnode.h>
#include <vfs.h>
#include <uio.h>
#include <synch.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <copyinout.h>
#include <file_syscall.h>
#include <limits.h>
#include <kern/unistd.h>
#include <endian.h>
#include <stat.h>

int sys_open(const char *filename, int flags, int *retval) {
	int i = 3;
	int err = 0;
	int mode;
	size_t len = PATH_MAX;
	size_t got;
	char *cin_filename = (char *)kmalloc(len);
	int copyinside = copyinstr((const_userptr_t)filename, cin_filename, len, &got);
	if (copyinside) {
		kfree(cin_filename);
		return copyinside;
	}
	while(curproc->file_table[i] != NULL){
		if(i == OPEN_MAX-1){
			kfree(cin_filename);
			return EMFILE;	
		}
		i++;
	}
	curproc->file_table[i] = (struct file_handle *)kmalloc(sizeof(struct file_handle));
	KASSERT(curproc->file_table[i] != NULL);
	err = vfs_open(cin_filename, flags, 0, &curproc->file_table[i]->vnode);
	if (err) {
		kfree(cin_filename);
		kfree(curproc->file_table[i]);
		return err;
	}
	if(flags & O_APPEND){
		struct stat statbuf;
		err = VOP_STAT(curproc->file_table[i]->vnode, &statbuf);
		if (err){
			kfree(cin_filename);
			kfree(curproc->file_table[i]);
			return err;
		}
		curproc->file_table[i]->offset = statbuf.st_size;
	} else {
		curproc->file_table[i]->offset = 0;
	}
	curproc->file_table[i]->destroy_count = 1;
	mode = flags & O_ACCMODE;
	switch(mode){
		case O_RDONLY:
			curproc->file_table[i]->mode_open = O_RDONLY;
			break;
		case O_WRONLY:
			curproc->file_table[i]->mode_open = O_WRONLY;
			break;
		case O_RDWR:
			curproc->file_table[i]->mode_open = O_RDWR;
			break;
		default:
			kfree(cin_filename);
			vfs_close(curproc->file_table[i]->vnode);
			kfree(curproc->file_table[i]);
			return EINVAL;
	}
	curproc->file_table[i]->lock = lock_create("filehandle_lock");
	if(curproc->file_table[i]->lock == NULL) {	
			kfree(cin_filename);
			vfs_close(curproc->file_table[i]->vnode);
			kfree(curproc->file_table[i]);
	}
	*retval = i;
	kfree(cin_filename);
	return 0;
}

int sys_read(int fd, void *buf, size_t bufflen, int32_t *retval){

	if(fd < 0 || fd > OPEN_MAX || curproc->file_table[fd] == NULL
		|| curproc->file_table[fd]->mode_open == O_WRONLY) {
		return EBADF;		
	}
	char *buffer = (char *)kmalloc(sizeof(*buf)*bufflen);
	struct iovec iov;
	struct uio kuio;
	lock_acquire(curproc->file_table[fd]->lock);	
	uio_kinit(&iov, &kuio, buffer, bufflen, curproc->file_table[fd]->offset, UIO_READ);

	int err = VOP_READ(curproc->file_table[fd]->vnode, &kuio);
	if (err){
		lock_release(curproc->file_table[fd]->lock);
		kfree(buffer);
		return err;
	}
	off_t bytes = kuio.uio_offset - curproc->file_table[fd]->offset;
	*retval = (int32_t)bytes;
	curproc->file_table[fd]->offset = kuio.uio_offset;
	if(bytes != 0){
		err = copyout(buffer, (userptr_t)buf, *retval);
		if(err) {
			lock_release(curproc->file_table[fd]->lock);
			kfree(buffer);
			return err;
		}
	}
	lock_release(curproc->file_table[fd]->lock);
	kfree(buffer);
	return 0;
}

int sys_write(int fd, const void *buff, size_t bufflen, int32_t *retval) {
	
	if(fd < 0 || fd > OPEN_MAX || curproc->file_table[fd] == NULL
		|| curproc->file_table[fd]->mode_open == O_RDONLY) {
		return EBADF;		
	}

	char *buffer = (char *)kmalloc(sizeof(*buff)*bufflen);
	int copyinside = copyin((const_userptr_t)buff, buffer, bufflen);
	if(copyinside){
		kfree(buffer);
		return copyinside;
	}

	struct iovec iov;
	struct uio kuio;
	lock_acquire(curproc->file_table[fd]->lock);	
	uio_kinit(&iov, &kuio, buffer, bufflen, curproc->file_table[fd]->offset, UIO_WRITE);

	int vopwriter = VOP_WRITE(curproc->file_table[fd]->vnode, &kuio);
	if (vopwriter){
		kfree(buffer);
		return vopwriter;
	}
	off_t bytes = kuio.uio_offset - curproc->file_table[fd]->offset;
	*retval = (int32_t)bytes;
	curproc->file_table[fd]->offset = kuio.uio_offset;
	lock_release(curproc->file_table[fd]->lock);
	kfree(buffer);
	return 0;
}

int sys_close(int fd) {
	if(fd < 0 || fd > OPEN_MAX || curproc->file_table[fd] == NULL) {
		return EBADF;
	}
	lock_acquire(curproc->file_table[fd]->lock);
	KASSERT(curproc->file_table[fd]->destroy_count > 0);
	curproc->file_table[fd]->destroy_count--;

	if(curproc->file_table[fd]->destroy_count > 0) {
		lock_release(curproc->file_table[fd]->lock);
		curproc->file_table[fd] = NULL;
	}else {
		lock_release(curproc->file_table[fd]->lock);
		KASSERT(curproc->file_table[fd]->destroy_count == 0);
		lock_destroy(curproc->file_table[fd]->lock);
                vfs_close(curproc->file_table[fd]->vnode);
                kfree(curproc->file_table[fd]);
		curproc->file_table[fd] = NULL;
	}
	
	return 0;
}

int sys_lseek(int fd, int32_t pos1, int32_t pos2, uint32_t sp16, uint32_t *ret1, uint32_t *ret2) {
	
	int whence;
	off_t offset_64;
	int err = 0;
	struct stat statbuf;

	if(fd < 3 || fd > OPEN_MAX || curproc->file_table[fd] == NULL) {
		return EBADF;		
	}

	err = copyin((const_userptr_t)sp16, &whence, sizeof(int));
	if(err){
		return err;
	}

	if(whence < 0 || whence > 2) {
		return EINVAL;
	}

	bool sign = ((pos1) & (1<<(31)));
	join32to64((uint32_t)pos1, (uint32_t)pos2, (uint64_t *)&offset_64);
	offset_64 = ((offset_64) | ((off_t)sign<<(63)));
	lock_acquire(curproc->file_table[fd]->lock);
	switch (whence) {
		case 0:
			if(offset_64 < 0){
				lock_release(curproc->file_table[fd]->lock);
				return EINVAL;
			}
			curproc->file_table[fd]->offset = offset_64;
			break;
		case 1:
			curproc->file_table[fd]->offset += offset_64;
			break;

		case 2:
			err = VOP_STAT(curproc->file_table[fd]->vnode, &statbuf);
			if (err){
				lock_release(curproc->file_table[fd]->lock);
				return err;
			}
			curproc->file_table[fd]->offset = statbuf.st_size + offset_64;
			break;

		default:
			lock_release(curproc->file_table[fd]->lock);
			return EINVAL;
			
	}
	split64to32((uint64_t)curproc->file_table[fd]->offset, ret1, ret2);
	lock_release(curproc->file_table[fd]->lock);
	return 0;
}

int sys_dup2(int oldfd, int newfd, int *retval) {
	struct file_handle *temp = NULL;
	bool newfdflag = false;
	if(oldfd < 0 || oldfd > OPEN_MAX || newfd < 0 || newfd > OPEN_MAX || curproc->file_table[oldfd] == NULL) {
		return EBADF;		
	}
	if(oldfd == newfd){
		*retval = newfd;
		return 0;
	}
	if(curproc->file_table[newfd] != NULL){
		temp = curproc->file_table[newfd];
		newfdflag = true;
	}
	
	lock_acquire(curproc->file_table[oldfd]->lock);
	if(curproc->file_table[oldfd]->destroy_count == 0) {
		lock_release(curproc->file_table[oldfd]->lock);
		return EBADF;
	}
	curproc->file_table[newfd] = curproc->file_table[oldfd];
	KASSERT(curproc->file_table[oldfd]->destroy_count > 0);
	curproc->file_table[oldfd]->destroy_count++;
	lock_release(curproc->file_table[oldfd]->lock);

	if(newfdflag) {
		lock_acquire(temp->lock);	
		KASSERT(temp->destroy_count > 0);
		temp->destroy_count--;
		if(temp->destroy_count > 0) {
			lock_release(temp->lock);
			temp = NULL;
		} else {
			lock_release(temp->lock);
			KASSERT(temp->destroy_count == 0);
			lock_destroy(temp->lock);
        	        vfs_close(temp->vnode);
        	        kfree(temp);
			temp = NULL;
		}
	}
	*retval = newfd;
	return 0;
}
