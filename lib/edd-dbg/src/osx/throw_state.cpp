// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "throw_state.hpp"
#include "spin_mutex.hpp"

#include <new>
#include <cstring>

#include <pthread.h>

namespace dbg 
{
    namespace  
    {
        spin_mutex tls_creation_mutex = SPIN_MUTEX_INITIALIZER;
        pthread_key_t key;
        bool key_created = false;


        void delete_throw_state(void *p) { delete static_cast<throw_state *>(p); }

        bool get_key(pthread_key_t &out)
        {
            scoped_spin_lock lk(tls_creation_mutex);

            if (!key_created)
            {
                if (pthread_key_create(&key, &delete_throw_state) != 0) return false;
                key_created = true;
            }

            out = key;

            return true;
        }
    } 

    throw_state *throw_state::get()
    {
        pthread_key_t k;
        std::memset(&k, 0x00, sizeof k);
        if (!get_key(k)) return 0;

        throw_state *value = static_cast<throw_state *>(pthread_getspecific(key));
        if (value == 0)
        {
            value = new (std::nothrow) throw_state();
            if (!value) return 0;
            if (pthread_setspecific(k, value) != 0) { delete value; return 0; }
        }

        return value;
    }

} // dbg

