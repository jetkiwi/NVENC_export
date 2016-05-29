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
// Platform independent critical section and related function decorations
//---------------------------------------------------------------------------

#ifndef _NV_CRITICAL_SECTION_H
#define _NV_CRITICAL_SECTION_H

#if defined __unix || defined __linux
#define NV_UNIX
#endif

#if defined(__x86_64) || defined(AMD64) || defined(_M_AMD64)
#define NVCPU_X86_64
#else
#define NVCPU_X86
#endif

#if defined WIN32 || defined _WIN32

#include <windows.h>
typedef CRITICAL_SECTION NVCriticalSection;

__inline void NVCreateCriticalSection(NVCriticalSection *cs)    { InitializeCriticalSection(cs); }
__inline void NVDestroyCriticalSection(NVCriticalSection *cs)   { DeleteCriticalSection(cs); }
__inline void NVLockCriticalSection(NVCriticalSection *cs)      { EnterCriticalSection(cs); }
__inline void NVUnlockCriticalSection(NVCriticalSection * cs)   { LeaveCriticalSection(cs); }

#elif defined __APPLE__ || defined __MACOSX

#include <pthread.h>

typedef struct {
    pthread_mutex_t     m_mutex;
    pthread_mutexattr_t m_attr;
} NVCriticalSection;

__inline void NVCreateCriticalSection(NVCriticalSection *cs)    { pthread_mutexattr_init(&cs->m_attr); pthread_mutexattr_settype(&cs->m_attr, PTHREAD_MUTEX_RECURSIVE); pthread_mutex_init(&cs->m_mutex, &cs->m_attr); }
__inline void NVDestroyCriticalSection(NVCriticalSection *cs)   { pthread_mutex_destroy(&cs->m_mutex); pthread_mutexattr_destroy(&cs->m_attr); }
__inline void NVLockCriticalSection(NVCriticalSection *cs)      { pthread_mutex_lock(&cs->m_mutex); }
__inline void NVUnlockCriticalSection(NVCriticalSection * cs)   { pthread_mutex_unlock(&cs->m_mutex); }

#elif defined NV_UNIX

#include "threads/NvPthreadABI.h"

typedef struct {
    pthread_mutex_t     m_mutex;
    pthread_mutexattr_t m_attr;
} NVCriticalSection;

__inline void NVCreateCriticalSection(NVCriticalSection *cs)    { pthread_mutexattr_init(&cs->m_attr); pthread_mutexattr_settype(&cs->m_attr, PTHREAD_MUTEX_RECURSIVE); pthread_mutex_init(&cs->m_mutex, &cs->m_attr); }
__inline void NVDestroyCriticalSection(NVCriticalSection *cs)   { pthread_mutex_destroy(&cs->m_mutex); pthread_mutexattr_destroy(&cs->m_attr); }
__inline void NVLockCriticalSection(NVCriticalSection *cs)      { pthread_mutex_lock(&cs->m_mutex); }
__inline void NVUnlockCriticalSection(NVCriticalSection * cs)   { pthread_mutex_unlock(&cs->m_mutex); }

#else

#error Critical section functions unknown for this platform.

#endif


#if defined WIN32 || defined _WIN32

#include <windows.h>

__inline U32 NVInterlockedIncrement(volatile U32 *p) { return InterlockedIncrement((volatile LONG *)p); }
__inline U32 NVInterlockedDecrement(volatile U32 *p) { return InterlockedDecrement((volatile LONG *)p); }

__inline void NVInterlockedAdd(volatile long *pDestination, long value)
{
    long cur;
    do
    {
       cur = *pDestination;
    }
    while (InterlockedCompareExchange(pDestination, cur + value, cur) != cur);
}

#elif defined __APPLE__ || defined __MACOSX

#include <CoreServices/CoreServices.h>
#include <libkern/OSAtomic.h>

__inline U32 NVInterlockedIncrement(volatile U32 *p) { return OSAtomicIncrement32((volatile int32_t *)p); }
__inline U32 NVInterlockedDecrement(volatile U32 *p) { return OSAtomicDecrement32((volatile int32_t *)p); }

#elif defined NV_UNIX

#if defined(NVCPU_X86) || defined(NVCPU_X86_64)

/* replace _count with _count+1 and set _acquire=_count */
#define NV_ATOMIC_INCREMENT(_count, _acquire)            \
{                                                        \
    U32 _release;                                        \
    char _fail;                                          \
    do {                                                 \
        U32 _dummy;                                      \
        _acquire = _count;                               \
        _release = _acquire + 1;                         \
        __asm__ __volatile__(                            \
               "lock ; cmpxchgl %4,%1\n\t"               \
               "setnz %0"                                \
               : "=d" (_fail),                           \
               "=m" (_count),                            \
               "=a" (_dummy)                             \
               : "2" (_acquire),                         \
               "r" (_release));                          \
    } while (_fail);                                     \
}

/* replace _count with _count-1 and set _acquire=_count */
#define NV_ATOMIC_DECREMENT(_count, _acquire)            \
{                                                        \
    U32 _release;                                        \
    char _fail;                                          \
    do {                                                 \
        U32 _dummy;                                      \
        _acquire = _count;                               \
        _release = _acquire - 1;                         \
        __asm__ __volatile__(                            \
               "lock ; cmpxchgl %4,%1\n\t"               \
               "setnz %0"                                \
               : "=d" (_fail),                           \
               "=m" (_count),                            \
               "=a" (_dummy)                             \
               : "2" (_acquire),                         \
               "r" (_release));                          \
    } while (_fail);                                     \
}

#elif defined(NVCPU_ARM)

#define NV_ATOMIC_OPERATION(_count, _acquire, _op)              \
{                                                               \
    U32 newval;                                                 \
    char fail = 0;                                              \
                                                                \
    do {                                                        \
        _acquire = _count;                                      \
        newval = (_op);                                         \
                                                                \
        __sync_synchronize();                                   \
        __asm__ __volatile(                                     \
            "ldrex r0, [%[counter]]\n\t"                        \
            "cmp r0, %[oldval]\n\t"                             \
            "it eq\n\t"                                         \
            "strexeq %[ret], %[newval], [%[counter]]\n\t"       \
            "it ne\n\t"                                         \
            "movne %[ret], #1\n\t"                              \
            : [ret] "=&r" (fail)                                \
            : [counter] "r" (&_count),                          \
              [oldval] "r" (_acquire),                          \
              [newval] "r" (newval)                             \
            : "r0");                                            \
        __sync_synchronize();                                   \
    } while (fail);                                             \
}

#define NV_ATOMIC_INCREMENT(_count, _acquire)   \
    NV_ATOMIC_OPERATION(_count, _acquire, _count + 1)

#define NV_ATOMIC_DECREMENT(_count, _acquire)   \
    NV_ATOMIC_OPERATION(_count, _acquire, _count - 1)

#else
#error "NV_ATOMIC_{INCREMENT,DECREMENT} undefined for this architecture."
#endif

// NVInterlockedIncrement wants to return the value that *p was
// incremented to.
__inline U32 NVInterlockedIncrement(volatile U32 *p)
{
    U32 val;
    NV_ATOMIC_INCREMENT(*p, val);
    return val + 1;
}
__inline U32 NVInterlockedDecrement(volatile U32 *p)
{
    U32 val;
    NV_ATOMIC_DECREMENT(*p, val);
    return val - 1;
}

#else

#error NVInterlockedIncrement functions unknown for this platform.

#endif

#endif // _NV_CRITICAL_SECTION_H
