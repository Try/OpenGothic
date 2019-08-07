// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "dbg/symbols.hpp"

namespace dbg 
{
    void symsink::on_function(const void *program_counter, const char *name, const char *module)
    {
        process_function(
            program_counter,
            (name && name[0]) ? name : "[unknown function]",
            (module && module[0]) ? module : "[unknown module]"
        );
    }

} // dbg
