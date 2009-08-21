/**
 * @file  event/pod.h
 * @brief pod definitions
 *
 * @{
 */

#ifndef ZEVENT_POD_H
#define ZEVENT_POD_H

#include "apr.h"
#include "apr_strings.h"
#define APR_WANT_STRFUNC
#include "apr_want.h"

#include "zevent_network.h"

#define RESTART_CHAR '$'
#define GRACEFUL_CHAR '!'

#define ZEVENT_RESTART  0
#define ZEVENT_GRACEFUL 1

typedef struct zevent_pod_t zevent_pod_t;

struct zevent_pod_t
{
    apr_file_t *pod_in;
    apr_file_t *pod_out;
    apr_pool_t *p;
};

ZEVENT_DECLARE(apr_status_t) zevent_pod_open(apr_pool_t * p, zevent_pod_t ** pod);
ZEVENT_DECLARE(int) zevent_pod_check(zevent_pod_t * pod);
ZEVENT_DECLARE(apr_status_t) zevent_pod_close(zevent_pod_t * pod);
ZEVENT_DECLARE(apr_status_t) zevent_pod_signal(zevent_pod_t * pod, int graceful);
ZEVENT_DECLARE(void) zevent_pod_killpg(zevent_pod_t * pod, int num, int graceful);

#endif
/** @} */
