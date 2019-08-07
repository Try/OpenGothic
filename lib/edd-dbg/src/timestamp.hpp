// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef TIMESTAMP_HPP_1727_16072012
#define TIMESTAMP_HPP_1727_16072012

#include <iosfwd>

namespace dbg 
{
    typedef char stamp_buff[16];

    // Fills buff with null terminated string containing the current time.
    void generate_timestamp(stamp_buff &buff);

    struct timestamp
    {
        timestamp() { generate_timestamp(buff); }
        stamp_buff buff;
    };

    std::ostream &operator << (std::ostream &out, const timestamp &stamp);

} // dbg

#endif // TIMESTAMP_HPP_1727_16072012
