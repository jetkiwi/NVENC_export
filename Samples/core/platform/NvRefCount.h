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
//! \file NvRefCount.h
//! \brief Base abstract interface for all reference counted classes.
//---------------------------------------------------------------------------

#ifndef _NV_IREFCOUNT_ITF_H
#define _NV_IREFCOUNT_ITF_H

#include "NvCallingConventions.h"

//! \brief Base abstract interface for all reference counted classes.

//! \e INvRefCount is the base abstract interface for all reference counted classes.
//! Each class that inherits from \e INvRefCount must implement \e AddRef()
//! and \e Release(). When a reference counted class is created, the initial
//! reference count will be 1. Whenever a new variable points to that class
//! the application should call \e AddRef() after assignment. Each time the
//! variable is destroyed, \e Release() should be called. On each \e AddRef()
//! the reference count is incremented. On each \e Release() the reference
//! count is decremented. When the reference count goes to 0 the object is
//! automatically destroyed.
//!
//! An implementation of INvRefCount is provided by \e INvRefCountImpl.
//! Application developers will typically use \e INvRefCountImpl instead of
//! deriving new objects from \e INvRefCount.
class INvRefCount
{
public:

    //!
    //! Increment the reference count by 1.
    virtual unsigned long NV_CALL_CONV_COM AddRef() = 0;

    //! Decrement the reference count by 1. When the reference count
    //! goes to 0 the object is automatically destroyed.
    virtual unsigned long NV_CALL_CONV_COM Release() = 0;

protected:

    virtual ~INvRefCount() { }
};

//! \name Disable copy and assign macros
//! The following macros disable some default standard C++ behaviour that
//! can cause trouble for reference counted classes. When the standard features
//! are used by mistake the reference count can become inaccurate or meaningless.
//@{

//! Disable the default copy constructor.
#define DISABLE_COPY_CONSTRUCTOR(type) protected: type(const type &inst) { }

//! Disable the default copy constructor for a derived base.
#define DISABLE_COPY_CONSTRUCTOR_WITH_BASE(type, base) protected: type(const type &inst) : base { }

//! Disable the default assignment operator.
#define DISABLE_ASSIGNMENT(type) protected: type & operator=(const type &inst) { return *this; }

//! Disable the default copy constructor and assignment operator.
#define DISABLE_COPY_AND_ASSIGN(type) DISABLE_COPY_CONSTRUCTOR(type) DISABLE_ASSIGNMENT(type)

//! Disable the default copy constructor and assignment operator for a derived base.
#define DISABLE_COPY_AND_ASSIGN_WITH_BASE(type, base) DISABLE_COPY_CONSTRUCTOR_WITH_BASE(type, base) DISABLE_ASSIGNMENT(type)

//@}
#endif
