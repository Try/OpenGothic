// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "throw_state.hpp"
#include "spin_mutex.hpp"

#include <new>

#include <windows.h>

namespace dbg 
{
    namespace  
    {
        spin_mutex tls_creation_mutex = SPIN_MUTEX_INITIALIZER;
        DWORD key = 0;
        bool key_created = false;

        void NTAPI tls_callback(PVOID, DWORD reason, PVOID)
        {
            if (reason == DLL_THREAD_DETACH || reason == DLL_PROCESS_DETACH)
            {
                scoped_spin_lock lk(tls_creation_mutex);

                if (key_created)
                    delete static_cast<throw_state *>(TlsGetValue(key));
            }

            if (reason == DLL_PROCESS_DETACH)
            {
                if (key_created)
                    TlsFree(key);

                key_created = false;
                key = 0;
            }
        }

        bool get_key(DWORD &out)
        {
            scoped_spin_lock lk(tls_creation_mutex);

            if (!key_created)
            {
                if ((key = TlsAlloc()) == TLS_OUT_OF_INDEXES) 
                    return false;

                key_created = true;
            }

            out = key;

            return true;
        }

        struct static_tls_release
        {
            ~static_tls_release()
            {
                if (key_created)
                    TlsFree(key);

                key_created = false;
                key = 0;
            }
        };
 
        // This is needed if we're linked in to a dynamically loaded dll, where TLS 
        // callbacks aren't invoked by the Windows loader.
        static_tls_release str;

    } // anonymous

    throw_state *throw_state::get()
    {
        DWORD k = 0;
        if (!get_key(k)) return 0;

        throw_state *value = static_cast<throw_state *>(TlsGetValue(k));
        if (value == 0)
        {
            value = new (std::nothrow) throw_state();
            if (!value) return 0;
            if (!TlsSetValue(k, value)) { delete value; return 0; }
        }

        return value;
    }

} // dbg

extern "C"
{

// Use the Windows PE tls callback mechanism to ensure the throw_state* associated with
// each thread is deleted when the thread ends.
// We use .CRT$XLB to ensure that our destructor is called before the CRT (.CRT$XLC)
// is torn down.


#if defined(_MSC_VER)

#   if defined (_M_IX86)
#       pragma comment(linker, "/INCLUDE:__tls_used")
#   else
#       pragma comment(linker, "/INCLUDE:_tls_used")
#   endif

#   pragma data_seg(".CRT$XLB")
    PIMAGE_TLS_CALLBACK dbg_tls_callback_ = dbg::tls_callback;
#   pragma data_seg()

#elif defined(__GNUC__)

    PIMAGE_TLS_CALLBACK dbg_tls_callback_ __attribute__ ((section(".CRT$XLB"))) = dbg::tls_callback;

#else

#   error "Toolchain not supported :("

#endif

} // extern "C"
