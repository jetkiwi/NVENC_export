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
// NvThreadingLinux.h
//
// Linux implementation of the NvThreading::INvThreading interface.
//
// Copyright(c) 2003 NVIDIA Corporation.
//---------------------------------------------------------------------------

#ifndef _NV_THREADING_LINUX_H
#define _NV_THREADING_LINUX_H

#if (NV_PROFILE==1)
#include <sys/time.h>
#endif
#include <sys/resource.h>
#include <sys/types.h>
#include <pthread.h>

#include <threads/NvThreading.h>

class CNvThreadingLinux : public INvThreading {
public:
    CNvThreadingLinux();
    virtual ~CNvThreadingLinux();

    //////////////////////////////////////////////////////////////////////
    // Mutex.
    //////////////////////////////////////////////////////////////////////

    virtual NvResult MutexCreate(Handle* puMutexHandle);
    virtual NvResult MutexAcquire(Handle uMutexHandle);
    virtual NvResult MutexTryAcquire(Handle uMutexHandle);
    virtual NvResult MutexRelease(Handle uMutexHandle);
    virtual NvResult MutexDestroy(Handle* puMutexHandle);

    //////////////////////////////////////////////////////////////////////
    // Events.
    //////////////////////////////////////////////////////////////////////

    virtual NvResult EventCreate(Handle* puEventHandle, bool bManual, bool bSet);
    virtual NvResult EventWait(Handle uEventHandle, U32 uTimeoutMs);
    virtual NvResult EventSet(Handle uEventHandle);
    virtual NvResult EventReset(Handle uEventHandle);
    virtual NvResult EventDestroy(Handle* puEventHandle);

    //////////////////////////////////////////////////////////////////////
    // Semaphores.
    //////////////////////////////////////////////////////////////////////

    virtual NvResult SemaphoreCreate(Handle* puSemaphoreHandle, U32 uInitCount, U32 uMaxCount);
    virtual NvResult SemaphoreIncrement(Handle uSemaphoreHandle);
    virtual NvResult SemaphoreDecrement(Handle uSemaphoreHandle, U32 uTimeoutMs);
    virtual NvResult SemaphoreDestroy(Handle* puSemaphoreHandle);

    //////////////////////////////////////////////////////////////////////
    // Timers.
    //////////////////////////////////////////////////////////////////////

    virtual NvResult TimerCreate(Handle* puTimerHandle, bool (*pFunc)(void* pParam), void* pParam, U32 uTimeMs, U32 uPeriodMs);
    virtual NvResult TimerDestroy(Handle* puTimerHandle);

    //////////////////////////////////////////////////////////////////////
    // Threads.
    //////////////////////////////////////////////////////////////////////

    virtual NvResult ThreadCreate(Handle* puThreadHandle, U32 (*pFunc)(void* pParam), void* pParam, S32 sPriority);
    virtual NvResult ThreadPriorityGet(Handle uThreadHandle, S32& rsPriority);
    virtual NvResult ThreadPrioritySet(Handle uThreadHandle, S32 sPriority);
    virtual NvResult ThreadDestroy(Handle* puThreadHandle);
    virtual bool ThreadIsCurrent(Handle uThreadHandle);

    //////////////////////////////////////////////////////////////////////
    // Misc.
    //////////////////////////////////////////////////////////////////////

    virtual U32 GetTicksMs();
    virtual U32 GetThreadID(Handle hThreadHandle);

    //////////////////////////////////////////////////////////////////////
    // Linux specific.
    //////////////////////////////////////////////////////////////////////

    void ResetTimeBase(U32 uOffsetMs);

private:
    typedef unsigned long long time_ms_t;

    struct CMutexData {
        pthread_mutexattr_t mutexattr;
        pthread_mutex_t     mutex;
    };

    struct CNvTimerData {
        pthread_cond_t     condition;
        pthread_mutex_t    mutex;
        pthread_t          thread;
        pthread_attr_t     thread_attr;
        time_ms_t          nextTime;
        U32                period;
        bool               exit;
        bool               (*pFunc)(void*);
        void*              pParam;
    };

    struct CConditionData {
        pthread_cond_t     condition;
        pthread_mutex_t    mutex;
        bool               signaled;
        bool               manual;
    };

    struct CNvThreadData {
        U32 (*pFunc)(void*);
        pthread_cond_t     condition;
        pthread_mutex_t    mutex;
        void*              pParam;
        pthread_t          thread;
        pthread_attr_t     thread_attr;
        pid_t              pid;
        S32                priority;

#if (NV_PROFILE==1)
        struct itimerval profTimer;
#endif
    };

    struct CNvSemaphoreData {
        pthread_cond_t  condition;
        pthread_mutex_t mutex;
        U32             maxCount;
        U32             count;
    };

    time_t initialTime;

    // Inherited from calling thread
    int m_iSchedPolicy;        // SCHED_OTHER = 0, SCHED_FIFO = 1, SCHED_RR = 2
    S32 m_iSchedPriorityMin;   // Minimum priority for scheduling policy
    S32 m_iSchedPriorityMax;   // Maximum priority for scheduling policy
    S32 m_iSchedPriorityBase;  // Base priority
    
    static void* TimerFunc(void* lpParameter);
    static void* ThreadFunc(void* lpParameter);

    static time_ms_t GetTime();

    virtual NvResult _MutexCreate(Handle* puMutexHandle, bool bIsRecursive);
};

#endif
