#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include "shmop.h"

int shm_create(shm_t *new_m, 
		uint32_t reqsize, 
		const char *filename)
{
    int status;

    struct shmid_ds shmbuf;
    uid_t uid;
    gid_t gid;
    uint32_t nbytes;
    key_t shmkey;

    new_m->reqsize = reqsize;
    new_m->filename = strdup(filename);

    int fd = open(filename, 
	    O_RDWR | O_CREAT /*| O_EXCL*/,0666);
    if (fd < 0) {
	return errno;
    }

    shmkey = ftok(filename, 1);
    if (shmkey == (key_t)-1) {
	return errno;
    }

    if ((new_m->shmid = shmget(shmkey, new_m->reqsize,
		    SHM_R | SHM_W | IPC_CREAT | IPC_EXCL)) < 0) {
	return errno;
    }

    if ((new_m->base = shmat(new_m->shmid, NULL, 0)) == (void *)-1) {
	return errno;
    }
    new_m->usable = new_m->base;

    if (shmctl(new_m->shmid, IPC_STAT, &shmbuf) == -1) {
	return errno;
    }
    uid = getuid();
    gid = getgid();
    shmbuf.shm_perm.uid = uid;
    shmbuf.shm_perm.gid = gid;
    if (shmctl(new_m->shmid, IPC_SET, &shmbuf) == -1) {
	return errno;
    }
    nbytes = sizeof(reqsize); 
    status = write(fd, (const void *)&reqsize,nbytes);
    if(status < 0)
	return errno;

    close(fd);
    return 0;
}

int shm_attach(shm_t *new_m,const char *filename)
{
    int status;

    uint32_t nbytes;
    key_t shmkey;

    new_m->filename = strdup(filename);

    int fd = open(filename, 
	    O_RDONLY);
    if (fd < 0) {
	return errno;
    }
    nbytes = sizeof(new_m->reqsize);
    status = read(fd, (void *)&(new_m->reqsize),nbytes); 
    close(fd);

    shmkey = ftok(filename, 1);
    if (shmkey == (key_t)-1) {
	return errno;
    }

    if ((new_m->shmid = shmget(shmkey, new_m->reqsize,
		    SHM_R | SHM_W)) < 0) {
	printf("%d.%s\n",errno,strerror(errno));
	return errno;
    }

    if ((new_m->base = shmat(new_m->shmid, NULL, 0)) == (void *)-1) {
	return errno;
    }
    new_m->usable = new_m->base;
    new_m->reqsize = new_m->reqsize;

    return 0;
}

int shm_detach(shm_t *m)
{
    int rv = shmdt(m->base);
    return rv;
}

int shm_remove(const char *filename)
{
    int status;
    key_t shmkey;
    int shmid;

    int fd = open(filename, O_WRONLY);
    if (fd < 0) {
	return -1;
    }

    shmkey = ftok(filename, 1);
    if (shmkey == (key_t)-1) {
	goto shm_remove_failed;
    }

    close(fd);
    if ((shmid = shmget(shmkey, 0, SHM_R | SHM_W)) < 0) {
	goto shm_remove_failed;
    }

    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
	goto shm_remove_failed;
    }
    unlink(filename);
    return 0;
shm_remove_failed:
    status = errno;
    unlink(filename);
    return -1;
}

