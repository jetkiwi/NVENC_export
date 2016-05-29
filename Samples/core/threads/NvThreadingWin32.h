/*
 * Copyright 1993-2013 NVIDIA Corporation.  All rights reserved.
 *
 * Please refer to the NVIDIA end user license agreement (EULA) associated
 * with this source code for terms and conditions that govern your use of
 * this software. Any use, reproduction, disclosure, or distribution of
 * this software and related documentation outside the terms of the EULA
 * is strictly prohibited.
 *
 */

//---------------------------------------------------------------------------
// NvThreadingWin32.cpp
//
// Win32 implementation of the IThread interface.
//---------------------------------------------------------------------------

#ifndef _NV_OSAL_NV_THREADING_WIN32_H
#define _NV_OSAL_NV_THREADING_WIN32_H

#if defined(_WIN32_WINNT)
#  if _WIN32_WINNT < 0x0403
#    undef _WIN32_WINNT
#  endif
#endif
#if !defined(_WIN32_WINNT)
#  define _WIN32_WINNT 0x0403
#endif

#include <windows.h>
#include <crtdbg.h>
#include <threads/NvThreading.h>

class CNvThreadingWin32 : public INvThreading {
public:
    CNvThreadingWin32();
    virtual ~CNvThreadingWin32();

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
    // Win32 specific.
    //////////////////////////////////////////////////////////////////////

    HANDLE ThreadGetHandle(Handle hThreadHandle) const;

private:
    UINT m_uTimerResolution;

    struct CNvTimerData {
        bool   (*pFunc)(void*);
        void*    pParam;
        MMRESULT hTimer;
        bool     bFirstEvent;
        UINT     uPeriodMs;
        UINT     uTimerResolution;
    };

    static void CALLBACK TimerFunc(UINT uID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2);

    struct CNvThreadData {
        U32 (*pFunc)(void*);
        void*        pParam;
        HANDLE       hThread;
        DWORD        dwThreadId;
    };

    static DWORD WINAPI ThreadFunc(LPVOID lpParameter);
};

#endif
