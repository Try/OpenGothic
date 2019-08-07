// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "dbg/logstream.hpp"
#include "dbg/log.hpp"

namespace dbg 
{
    logbuf::logbuf()
    {
        setp(buffer, buffer + (sizeof buffer) - 1); // -1 to make overflow() simpler
    }

    void logbuf::flush()
    {
        const std::ptrdiff_t n = pptr() - pbase();
        if (n)
        {
            log_write(pbase(), n);
            pbump(-n);
        }
    }

    logbuf::int_type logbuf::overflow(logbuf::int_type ch)
    {
        if (ch != traits_type::eof())
        {
            // We always have space for one more character at the back due to our
            // having specified a pointer one position before 'end' in the setp()
            // call in the constructor.
            *pptr() = traits_type::to_char_type(ch);

            pbump(1);
            flush();
        }

        return ch;
    }

} // dbg
