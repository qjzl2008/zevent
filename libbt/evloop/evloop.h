#ifndef BTPD_EVLOOP_H
#define BTPD_EVLOOP_H

#include <WinSock2.h>
#include "pstdint.h"

#include "timeheap.h"

#define EV_READ    1
#define EV_WRITE   2
#define EV_TIMEOUT 3

#define MAX_FDS (1024)

typedef void (*evloop_cb_t)(SOCKET fd, short type, void *arg);

struct fdev {
    evloop_cb_t cb;
    void *arg;
    SOCKET fd;
    uint16_t flags;
    int16_t index;
};

struct timeout {
    evloop_cb_t cb;
    void *arg;
    struct th_handle th;
};

int evloop_init(void);
int evloop(void);

int fdev_new(struct fdev *ev, SOCKET fd, uint16_t flags, evloop_cb_t cb,
    void *arg);
int fdev_del(struct fdev *ev);
int fdev_enable(struct fdev *ev, uint16_t flags);
int fdev_disable(struct fdev *ev, uint16_t flags);

void evtimer_init(struct timeout *, evloop_cb_t, void *);
int evtimer_add(struct timeout *, struct timespec *);
void evtimer_del(struct timeout *);

void evtimers_run(void);
struct timespec evtimer_delay(void);
int evtimer_gettime(struct timespec *);

#endif
