/*
 * Copyright 1993-2012 NVIDIA Corporation.  All rights reserved.
 *
 * Please refer to the NVIDIA end user license agreement (EULA) associated
 * with this source code for terms and conditions that govern your use of
 * this software. Any use, reproduction, disclosure, or distribution of
 * this software and related documentation outside the terms of the EULA
 * is strictly prohibited.
 *
 */

//---------------------------------------------------------------------------
// To deal with ABI compatibility when dlopen()ing pthread instead of linking
// against it, this file contains extern defination of all function pointers
// as pthread-ABI stuff is shared accros diffrent modules like 
// platform/NvThreading/NvCriticalSection,
// platform/NvThreading/Linux/NvThreadingLinux.
//---------------------------------------------------------------------------

#ifndef _NV_PTHREAD_ABI_H
#define _NV_PTHREAD_ABI_H

#include <pthread.h>

typedef int (*NvPthreadMutexInit)(
    pthread_mutex_t *           mutex,
    pthread_mutexattr_t const * mattr
);
typedef int (*NvPthreadMutexattrInit)(
    pthread_mutexattr_t const * mattr
);
typedef int (*NvPthreadMutexattrSettype)(
    pthread_mutexattr_t const *mattr,
    int kind
);
typedef int (*NvPthreadMutexLock)(pthread_mutex_t * mutex);
typedef int (*NvPthreadMutexUnlock)(pthread_mutex_t * mutex);
typedef int (*NvPthreadMutexDestroy)(pthread_mutex_t * mutex);
typedef int (*NvPthreadMutexattrDestroy)(
    pthread_mutexattr_t const *mattr
);
typedef int (*NvPthreadCreate)(
    pthread_t *thread,
    const pthread_attr_t *attr,
    void *(*start_routine) (void *),
    void *arg
);
typedef int (*NvPthreadJoin)(
    pthread_t thread,
    void **retval
);
typedef int (*NvPthreadCondTimedwait)(
    pthread_cond_t *cond,
    pthread_mutex_t *mutex,
    const struct timespec *abstime
);
typedef int (*NvPthreadMutexTrylock)(pthread_mutex_t *mutex);
typedef int (*NvPthreadAttrInit)(pthread_attr_t *attr);
typedef int (*NvPthreadAttrDestroy)(pthread_attr_t *attr);
typedef int (*NvPthreadAttrSetinheritsched)(
    pthread_attr_t *attr,
    int inheritsched
);
typedef int (*NvPthreadSetschedparam)(
    pthread_t thread,
    int policy,
    const struct sched_param *param
);
typedef int (*NvPthreadGetschedparam)(
    pthread_t thread,
    int *policy,
    struct sched_param *param
);
typedef int (*NvPthreadCondInit)(
    pthread_cond_t *cond,
    const pthread_condattr_t *attr
);
typedef int (*NvPthreadCondDestroy)(pthread_cond_t *cond);
typedef int (*NvPthreadCondSignal)(pthread_cond_t *cond);
typedef int (*NvPthreadCondBroadcast)(pthread_cond_t *cond);
typedef int (*NvPthreadCondWait)(
    pthread_cond_t *cond,
    pthread_mutex_t *mutex
);
typedef pthread_t (*NvPthreadSelf)(void);
typedef int (*NvPthreadEqual)(pthread_t t1, pthread_t t2);

extern NvPthreadMutexInit _nv_pthread_mutex_init;
extern NvPthreadMutexattrInit _nv_pthread_mutexattr_init;
extern NvPthreadMutexattrSettype _nv_pthread_mutexattr_settype;
extern NvPthreadMutexLock _nv_pthread_mutex_lock;
extern NvPthreadMutexUnlock _nv_pthread_mutex_unlock;
extern NvPthreadMutexDestroy _nv_pthread_mutex_destroy;
extern NvPthreadMutexattrDestroy _nv_pthread_mutexattr_destroy;
extern NvPthreadCreate _nv_pthread_create;
extern NvPthreadJoin _nv_pthread_join;
extern NvPthreadCondTimedwait _nv_pthread_cond_timedwait;
extern NvPthreadMutexTrylock _nv_pthread_mutex_trylock;
extern NvPthreadAttrInit _nv_pthread_attr_init;
extern NvPthreadAttrDestroy _nv_pthread_attr_destroy;
extern NvPthreadAttrSetinheritsched _nv_pthread_attr_setinheritsched;
extern NvPthreadSetschedparam _nv_pthread_setschedparam;
extern NvPthreadGetschedparam _nv_pthread_getschedparam;
extern NvPthreadCondInit _nv_pthread_cond_init;
extern NvPthreadCondDestroy _nv_pthread_cond_destroy;
extern NvPthreadCondSignal _nv_pthread_cond_signal;
extern NvPthreadCondBroadcast _nv_pthread_cond_broadcast;
extern NvPthreadCondWait _nv_pthread_cond_wait;
extern NvPthreadSelf _nv_pthread_self;
extern NvPthreadEqual _nv_pthread_equal;

#define pthread_mutexattr_init  _nv_pthread_mutexattr_init
#define pthread_mutexattr_settype _nv_pthread_mutexattr_settype
#define pthread_mutex_init _nv_pthread_mutex_init
#define pthread_mutex_lock _nv_pthread_mutex_lock
#define pthread_mutex_unlock _nv_pthread_mutex_unlock
#define pthread_mutex_destroy _nv_pthread_mutex_destroy
#define pthread_mutexattr_destroy _nv_pthread_mutexattr_destroy
#define pthread_create _nv_pthread_create
#define pthread_join _nv_pthread_join
#define pthread_cond_timedwait _nv_pthread_cond_timedwait
#define pthread_mutex_trylock _nv_pthread_mutex_trylock
#define pthread_attr_init _nv_pthread_attr_init
#define pthread_attr_destroy _nv_pthread_attr_destroy
#define pthread_attr_setinheritsched _nv_pthread_attr_setinheritsched
#define pthread_setschedparam _nv_pthread_setschedparam
#define pthread_getschedparam _nv_pthread_getschedparam
#define pthread_cond_init _nv_pthread_cond_init
#define pthread_cond_destroy _nv_pthread_cond_destroy
#define pthread_cond_signal _nv_pthread_cond_signal
#define pthread_cond_broadcast _nv_pthread_cond_broadcast
#define pthread_cond_wait _nv_pthread_cond_wait
#define pthread_self _nv_pthread_self
#define pthread_equal _nv_pthread_equal

// platform/NvThreading/NvPthreadABI/NvPthreadABI.cpp
void NvPthreadABIInit(void);
void NvPthreadABIFini(void);

#endif // _NV_PTHREAD_ABI_H
