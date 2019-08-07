// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef PE_HPP_0129_29062012
#define PE_HPP_0129_29062012

#include <cstddef>

namespace dbg 
{
    class file;

    struct pe_sink
    {
        virtual ~pe_sink() { }

        // Implementations should take a copy of name if needed, as the storage only
        // exists for the duration of the on_symbol() call.
        virtual void on_symbol(const char *name, std::size_t image_offset) = 0;
    };

    void scan_mingw_pe_file(file &pe, pe_sink &sink);

} // dbg

#endif // PE_HPP_0129_29062012
