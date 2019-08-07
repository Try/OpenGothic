// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef SPIN_MUTEX_HPP_1437_18072012
#define SPIN_MUTEX_HPP_1437_18072012

#if defined(_MSC_VER)
// Workaround ill-declared prototypes in some Windows/Platform SDKs
#   define _interlockedbittestandset DBG_UNDEFINED_interlockedbittestandset
#   define _interlockedbittestandreset DBG_UNDEFINED_interlockedbittestandreset
#   include <intrin.h>
#   undef _interlockedbittestandset
#   undef _interlockedbittestandreset
#elif defined(__APPLE__)
#include <libkern/OSAtomic.h>
#endif

namespace dbg 
{
    // Quick and dirty spin-lock that can be statically initialized.
#if defined (_MSC_VER)
#define SPIN_MUTEX_INITIALIZER 0
    typedef __declspec(align(8)) long spin_mutex;
    inline void spin_lock(spin_mutex &m) { while (_InterlockedCompareExchange(&m, 1, 0) == 1) { } }
    inline void spin_unlock(spin_mutex &m) { (void)_InterlockedExchange(&m, 0); }
#elif defined(__APPLE__)
#define SPIN_MUTEX_INITIALIZER OS_SPINLOCK_INIT
    typedef OSSpinLock spin_mutex;
    inline void spin_lock(spin_mutex &m) { OSSpinLockLock(&m); }
    inline void spin_unlock(spin_mutex &m) { OSSpinLockUnlock(&m); } // guarantee full barrier
#elif defined(__GNUC__)
#define SPIN_MUTEX_INITIALIZER 0
    typedef unsigned spin_mutex __attribute__((aligned(8)));
    inline void spin_lock(spin_mutex &m) { while (!__sync_bool_compare_and_swap(&m, 0u, 1u)) { } }
    inline void spin_unlock(spin_mutex &m) { __sync_bool_compare_and_swap(&m, 1u, 0u); } // guarantee full barrier
#endif

    struct scoped_spin_lock
    {
        scoped_spin_lock(spin_mutex &m) : m(m) { spin_lock(m); }
        ~scoped_spin_lock() { spin_unlock(m); }

        spin_mutex &m;
    };

} // dbg

#endif // SPIN_MUTEX_HPP_1437_18072012
