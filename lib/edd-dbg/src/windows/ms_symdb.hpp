// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef MS_SYMBOL_DATABASE_HPP_0056_10072012
#define MS_SYMBOL_DATABASE_HPP_0056_10072012

#include <cstddef>

#include <windows.h>

namespace dbg 
{
    class symsink;

    // Used to access symbols from Microsoft's own debug-information formats (typically
    // PDB files).
    // This is used by the MinGW and MSVC implementations if dbg::symdb.
    class ms_symdb
    {
        public:
            ms_symdb();
            ~ms_symdb();

            // If a function can be found spanning the specified program_counter, its
            // details are emitted through sink and true is returned.
            // Nothing is emitted if such a function cannot be found.
            bool try_lookup_function(const void *program_counter, symsink &sink);

        private:
            ms_symdb(const ms_symdb &);
            ms_symdb &operator= (const ms_symdb &);

        private:
            const HANDLE process;
    };

} // dbg

#endif // MS_SYMBOL_DATABASE_HPP_0056_10072012
