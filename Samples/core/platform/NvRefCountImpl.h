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
//! \file NvRefCountImpl.h
//! \brief Common implementation of \e INvRefCount.
//---------------------------------------------------------------------------

#ifndef _INVREFCOUNTIMPL_H
#define _INVREFCOUNTIMPL_H

#include <platform/include/NvAssert.h>
#include <common/include/NvRefCount.h>
#include <platform/include/NvCallingConventions.h>

#ifdef _DEBUG
#include <map>
#include <stdio.h>
#include <typeinfo>

#include <platform/NvThreading/NvThreadingClasses.h>
#endif

//-----------------------------------------------------------------------------
// INvRefCountImpl
//-----------------------------------------------------------------------------

//! \brief Implementation of \e INvRefCount.

//! \e INvRefCount::AddRef() and \e INvRefCount::Release() can be implemented
//! in any way that is suitable for the particular class. The vast majority
//! of classes will choose to use the implementation contained in this module.
//! This implementation of \e INvRefCount is as follows:
//! \li 1) Ref count is initialized to 1 when the object is created.
//! \li 2) Ref count is incremented by 1 on \e AddRef().
//! \li 3) Ref count is decremented by 1 on \e Release().
//! \li 4) When ref count goes to 0, delete is called on the object.
//!
//! The \e AddRef() and \e Release() methods are defined in the
//! \e INVREFCOUNT_IMPL macro.
//!
//! There is also a debugging tool in this implementation that will catch
//! any open references when the application quits. \e DumpRefCounts() can be
//! called on application exit to determine the number of open references
//! that show some \e Release() calls were missed by the application.
//!
//! Typical use of \e INvRefCountImpl would be:
//! \code
//! class foo : public virtual INvRefCountImpl {
//! public:
//!    INVREFCOUNT_IMPL
//! \endcode
class INvRefCountImpl : public virtual INvRefCount
{
public:

    //! Initialize the ref count to 1 on create.
    INvRefCountImpl() :
        __INvRefCountIMPL_m_ulRefCount(1)
    {
        _InternalAddRef(this);
    }

protected:
    // copy constructor:
    //!   This prevents the default compiler-generated "memcpy" for copy-construction.
    //!   This is identical to the default constructor because a "copy" is a new object so
    //!   its reference count is exactly like a newly constructed object, not a copy of the old object.
    INvRefCountImpl(const INvRefCountImpl& src) :
        __INvRefCountIMPL_m_ulRefCount(1)
    {
        _InternalAddRef(this);
    }

    virtual ~INvRefCountImpl()
    {
        NV_ASSERT(__INvRefCountIMPL_m_ulRefCount == 0);
    }

#ifdef _DEBUG

    struct INvRefCountInfo
    {
        INvRefCountInfo()
        {
            ulCount = 1;
        }
        unsigned long ulCount;
    };

    typedef std::map<INvRefCountImpl*, INvRefCountInfo*> INvRefCountMap;

    static CNvMutex& GetRefCountMapMutex()
    {
        static CNvMutex mutexRefCountMap;
        return mutexRefCountMap;
    }

    static INvRefCountMap& GetRefCountMap()
    {
        static INvRefCountMap INvRefCountMap;
        return INvRefCountMap;
    }

    void _InternalAddRef(INvRefCountImpl* pPointer)
    {
        CNvAutoMutex lock(GetRefCountMapMutex());

        INvRefCountMap::const_iterator it = GetRefCountMap().find(pPointer);

        if (it == GetRefCountMap().end())
        {
            INvRefCountInfo *pInfo = new INvRefCountInfo;

            GetRefCountMap().insert(INvRefCountMap::value_type(pPointer, pInfo));
        }
        else
            it->second->ulCount++;
    }

    void _InternalRelease(INvRefCountImpl* pPointer)
    {
        CNvAutoMutex lock(GetRefCountMapMutex());

        INvRefCountMap::iterator it = GetRefCountMap().find(pPointer);

        if (it == GetRefCountMap().end())
        {
            NV_ASSERT(0);
        }
        else
        {
            it->second->ulCount--;
            if (it->second->ulCount == 0)
            {
                delete it->second;
                GetRefCountMap().erase(it);
            }
        }
    }

public:

    static unsigned long DumpRefCounts()
    {
        CNvAutoMutex lock(GetRefCountMapMutex());

        FILE *pFile = fopen("RefCounts.txt", "w");
        if (pFile)
        {
            for(INvRefCountMap::const_iterator it = GetRefCountMap().begin(); it != GetRefCountMap().end(); ++it)
            {
#if defined(UNDER_CE) || defined(NV_TARGET_NO_EXCEPTIONS)
                fprintf(pFile, "Outstanding RefCount %p = %lu\n", it->first, it->second->ulCount);
#else
                try {
                    const std::type_info &info = typeid(*it->first);
                    if (&info)
                        fprintf(pFile, "%s %p RefCount = %lu\n", info.name(), it->first, it->second->ulCount);
                    else
                        // we should never get here, but under GCC 3.2, a null vtable returns a bad pointer
                        fprintf(pFile, "bad ptr %p = %lu\n", it->first, it->second->ulCount);
                }
                catch (...) {
                    fprintf(pFile, "bad ptr %p, RefCount = %lu\n", it->first, it->second->ulCount);
                }
#endif
                fflush(pFile);
            }
            fclose(pFile);
        }
        return static_cast<unsigned long>(GetRefCountMap().size());
    }

#else

    //! Part of the implementation of \e DumpRefCounts.
    void _InternalAddRef(INvRefCountImpl* pPointer)
    {
    }

    //! Part of the implementation of \e DumpRefCounts.
    void _InternalRelease(INvRefCountImpl* pPointer)
    {
    }

public:
    //! Dump all the open references to "RefCounts.txt" in the CWD for debug builds.
    static unsigned long DumpRefCounts()
    {
        return 0;
    }

#endif

protected:
    //! The reference count variable.
    unsigned long __INvRefCountIMPL_m_ulRefCount;
};

//! This macro can be used to define a standard implementation of \e AddRef()
//! and \e Release().
#define INVREFCOUNT_IMPL\
    unsigned long NV_CALL_CONV_COM AddRef()\
    {\
        _InternalAddRef(this);\
        return ++__INvRefCountIMPL_m_ulRefCount;\
    }\
\
    unsigned long NV_CALL_CONV_COM Release()\
    {\
        _InternalRelease(this);\
        unsigned long ulRefCount;\
        ulRefCount = --__INvRefCountIMPL_m_ulRefCount;\
        if (ulRefCount == 0)\
            delete this;\
        return ulRefCount;\
    }

#endif
