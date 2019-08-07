// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "dbg/symlog.hpp"
#include "memcpy_cast.hpp"

#include <ostream>
#include <cstddef>
#include <cstring>

namespace dbg 
{
    symlog::symlog(std::ostream &out, const char *prefix, const char *suffix) :
        out(out),
        prefix(prefix ? prefix : ""),
        suffix(suffix ? suffix : "")
    {
    }

    void symlog::process_function(const void *program_counter, const char *name, const char *module)
    {
        const std::size_t pc = memcpy_cast<std::size_t>(program_counter);

        out << prefix << "0x";

        for (unsigned n = 0; n != 2 * sizeof(pc); ++n)
        {
            const std::size_t shift = (sizeof(pc) * 8) - 4 * (n + 1);
            const std::size_t mask = std::size_t(0xF) << shift;
            const std::size_t nibble = (pc & mask) >> shift;

            out << "0123456789abcdef"[nibble];
        }

        out << ": " << name << " in " << module << suffix;
    }
        
} // dbg
