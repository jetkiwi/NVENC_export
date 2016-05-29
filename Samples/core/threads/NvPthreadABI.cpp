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
// Deal with ABI compatibility when dlopen()ing pthread instead of linking
// against it.
//---------------------------------------------------------------------------

#include <dlfcn.h>
#include "NvPthreadABI.h"
#include <platform/NvCompiler.h>
#include "xcodeutil.h"

static void * dlopen_handle;

NvPthreadMutexInit _nv_pthread_mutex_init;
NvPthreadMutexattrInit _nv_pthread_mutexattr_init;
NvPthreadMutexattrSettype _nv_pthread_mutexattr_settype;
NvPthreadMutexLock _nv_pthread_mutex_lock;
NvPthreadMutexUnlock _nv_pthread_mutex_unlock;
NvPthreadMutexDestroy _nv_pthread_mutex_destroy;
NvPthreadMutexattrDestroy _nv_pthread_mutexattr_destroy;
NvPthreadCreate _nv_pthread_create;
NvPthreadJoin _nv_pthread_join;
NvPthreadCondTimedwait _nv_pthread_cond_timedwait;
NvPthreadMutexTrylock _nv_pthread_mutex_trylock;
NvPthreadAttrInit _nv_pthread_attr_init;
NvPthreadAttrDestroy _nv_pthread_attr_destroy;
NvPthreadSetschedparam _nv_pthread_setschedparam;
NvPthreadGetschedparam _nv_pthread_getschedparam;
NvPthreadAttrSetinheritsched _nv_pthread_attr_setinheritsched;
NvPthreadCondInit _nv_pthread_cond_init;
NvPthreadCondDestroy _nv_pthread_cond_destroy;
NvPthreadCondSignal _nv_pthread_cond_signal;
NvPthreadCondBroadcast _nv_pthread_cond_broadcast;
NvPthreadCondWait _nv_pthread_cond_wait;
NvPthreadSelf _nv_pthread_self;
NvPthreadEqual _nv_pthread_equal;

static int _nv_pthread_mutex_init_dummy(
    pthread_mutex_t *           mutex,
    pthread_mutexattr_t const * mattr
)
{
    return 0;
}

static int _nv_pthread_mutexattr_init_dummy(
    pthread_mutexattr_t const * mattr
)
{
    return 0;
}

static int _nv_pthread_mutexattr_settype_dummy(
    pthread_mutexattr_t const * mattr,
    int kind
)
{
    return 0;
}

static int _nv_pthread_mutex_lock_dummy(pthread_mutex_t * mutex)
{
    return 0;
}

static int _nv_pthread_mutex_unlock_dummy(pthread_mutex_t * mutex)
{
    return 0;
}

static int _nv_pthread_mutex_destroy_dummy(pthread_mutex_t * mutex)
{
    return 0;
}

static int _nv_pthread_mutexattr_destroy_dummy(
    pthread_mutexattr_t const * mattr
)
{
    return 0;
}

/* Dummy implementation of pthread_create, pthread_join etc needs to
 * return ERROR. Dummy implemetation of other cases like pthread_mutex_lock/unlock
 * etc returns SUCCESS as some NVCUVID-applications don't rely on threads and
 * it safe to safe to skip any lock/unlock calls. This allow apps that use simple
 * nvcuvid API to work withput -lpthread.
 * But, the app which uses advanced nvcuvid API requires to link against -lpthread,
 * the code now create thread and it will fail correctly if there is no way to
 * create thread.
 */
static int _nv_pthread_create_dummy(
    pthread_t *thread,
    const pthread_attr_t *attr,
    void *(*start_routine) (void *),
    void *arg
)
{
    return 1; // return ERROR, e.g EPERM = 1, Operation not permitted
}

static int _nv_pthread_join_dummy(
    pthread_t thread,
    void **retval
)
{
    return 1; // return ERROR
}

static int _nv_pthread_cond_timedwait_dummy(
    pthread_cond_t *cond,
    pthread_mutex_t *mutex,
    const struct timespec *abstime
)
{
    return 0;
}

static int _nv_pthread_mutex_trylock_dummy(pthread_mutex_t *mutex)
{
    return 0;
}

static int _nv_pthread_attr_init_dummy(pthread_attr_t *attr)
{
    return 0;
}

static int _nv_pthread_attr_destroy_dummy(pthread_attr_t *attr)
{
    return 0;
}

static int _nv_pthread_attr_setinheritsched_dummy(
    pthread_attr_t *attr,
    int inheritsched
)
{
    return 0;
}

static int _nv_pthread_setschedparam_dummy(
    pthread_t thread,
    int policy,
    const struct sched_param *param
)
{
    return 0;
}

static int _nv_pthread_getschedparam_dummy(
    pthread_t thread,
    int *policy,
    struct sched_param *param
)
{
    return 1; // return ERROR
}

static int _nv_pthread_cond_init_dummy(
    pthread_cond_t *cond,
    const pthread_condattr_t *attr
)
{
    return 0;
}

static int _nv_pthread_cond_destroy_dummy(pthread_cond_t *cond)
{
    return 0;
}

static int _nv_pthread_cond_signal_dummy(pthread_cond_t *cond)
{
    return 0;
}

static int _nv_pthread_cond_broadcast_dummy(pthread_cond_t *cond)
{
    return 0;
}

static int _nv_pthread_cond_wait_dummy(
    pthread_cond_t *cond,
    pthread_mutex_t *mutex
)
{
    return 0;
}

static pthread_t _nv_pthread_self_dummy(void)
{
    return 0;
}

static int _nv_pthread_equal_dummy(pthread_t t1, pthread_t t2)
{
    return 1; // threads are equal, assuming there's only one thread.
}

#if defined(NV_LINUX)
#define DLSYM dlvsym
#if defined(__i386__)
#define SYMVER , "GLIBC_" SYMVER_LINUX_X86
#else
#define SYMVER , "GLIBC_" SYMVER_LINUX_X86_64
#endif
#else // !linux
#define DLSYM dlsym
#define SYMVER
#endif

#define GET_PROC(proc) {                                                 \
    _nv_##proc = (typeof(_nv_##proc))DLSYM(dlopen_handle, #proc SYMVER); \
    if (_nv_##proc == 0) {                                               \
        TPRINTF(("%s: Error detecteted \n", __NVFUNCTION__));            \
        dlclose(dlopen_handle);                                          \
        dlopen_handle = 0;                                               \
        goto dlopen_failure;                                             \
    }                                                                    \
}

void NvPthreadABIInit(void)
{
    dlopen_handle = 0;
    dlopen_handle = dlopen(0, RTLD_GLOBAL | RTLD_LAZY);
    if (!dlopen_handle) {
        TPRINTF(("%s: Error detecteted \n", __NVFUNCTION__));
        goto dlopen_failure;
    }

#define SYMVER_LINUX_X86    "2.0"
#define SYMVER_LINUX_X86_64 "2.2.5"
    GET_PROC(pthread_mutex_init);
    GET_PROC(pthread_mutexattr_init);
    GET_PROC(pthread_mutex_lock);
    GET_PROC(pthread_mutex_unlock);
    GET_PROC(pthread_mutex_destroy);
    GET_PROC(pthread_mutexattr_destroy);
    GET_PROC(pthread_join);
    GET_PROC(pthread_cond_timedwait);
    GET_PROC(pthread_mutex_trylock);
    GET_PROC(pthread_attr_destroy);
    GET_PROC(pthread_attr_setinheritsched);
    GET_PROC(pthread_setschedparam);
    GET_PROC(pthread_getschedparam);
    GET_PROC(pthread_cond_init);
    GET_PROC(pthread_cond_destroy);
    GET_PROC(pthread_cond_signal);
    GET_PROC(pthread_cond_broadcast);
    GET_PROC(pthread_cond_wait);
    GET_PROC(pthread_self);
    GET_PROC(pthread_equal);
#undef SYMVER_LINUX_X86
#undef SYMVER_LINUX_X86_64
#define SYMVER_LINUX_X86    "2.1"
#define SYMVER_LINUX_X86_64 "2.2.5"
    GET_PROC(pthread_mutexattr_settype);
    GET_PROC(pthread_create);
    GET_PROC(pthread_attr_init);
#undef SYMVER_LINUX_X86
#undef SYMVER_LINUX_X86_64

dlopen_failure:
    if (!dlopen_handle) {
        _nv_pthread_mutex_init = _nv_pthread_mutex_init_dummy;
        _nv_pthread_mutexattr_init = _nv_pthread_mutexattr_init_dummy;
        _nv_pthread_mutexattr_settype = _nv_pthread_mutexattr_settype_dummy;
        _nv_pthread_mutex_lock = _nv_pthread_mutex_lock_dummy;
        _nv_pthread_mutex_unlock = _nv_pthread_mutex_unlock_dummy;
        _nv_pthread_mutex_destroy = _nv_pthread_mutex_destroy_dummy;
        _nv_pthread_mutexattr_destroy = _nv_pthread_mutexattr_destroy_dummy;
        _nv_pthread_create = _nv_pthread_create_dummy;
        _nv_pthread_join = _nv_pthread_join_dummy;
        _nv_pthread_cond_timedwait = _nv_pthread_cond_timedwait_dummy;
        _nv_pthread_mutex_trylock = _nv_pthread_mutex_trylock_dummy;
        _nv_pthread_attr_init = _nv_pthread_attr_init_dummy;
        _nv_pthread_attr_destroy = _nv_pthread_attr_destroy_dummy;
        _nv_pthread_attr_setinheritsched = _nv_pthread_attr_setinheritsched_dummy;
        _nv_pthread_setschedparam = _nv_pthread_setschedparam_dummy;
        _nv_pthread_getschedparam = _nv_pthread_getschedparam_dummy;
        _nv_pthread_cond_init = _nv_pthread_cond_init_dummy;
        _nv_pthread_cond_destroy = _nv_pthread_cond_destroy_dummy;
        _nv_pthread_cond_signal = _nv_pthread_cond_signal_dummy;
        _nv_pthread_cond_broadcast = _nv_pthread_cond_broadcast_dummy;
        _nv_pthread_cond_wait = _nv_pthread_cond_wait_dummy;
        _nv_pthread_self = _nv_pthread_self_dummy;
        _nv_pthread_equal = _nv_pthread_equal_dummy;
    }
}

void NvPthreadABIFini(void)
{
    if (dlopen_handle) {
        dlclose(dlopen_handle);
        dlopen_handle = 0;
    }
}
