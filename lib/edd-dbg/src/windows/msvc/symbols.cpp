// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "dbg/symbols.hpp"
#include "ms_symdb.hpp"

#include <new>

namespace dbg 
{
    struct symdb::impl
    {
        ms_symdb msdb;
    };
    
    symdb::symdb() :
        p(new (std::nothrow) impl)
    {
    }

    symdb::~symdb()
    {
        delete p;
    }

    bool symdb::lookup_function(const void *program_counter, symsink &sink) const
    {
        if (p && p->msdb.try_lookup_function(program_counter, sink))
            return true;

        sink.on_function(program_counter, 0, 0);
        return false;
    }

} // dbg
