// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "timestamp.hpp"

#include <cstring>
#include <cstdio>

#include <windows.h>

namespace dbg 
{
    void generate_timestamp(stamp_buff &buff)
    {
        SYSTEMTIME st;
        std::memset(&st, 0, sizeof st);

        GetLocalTime(&st);

        std::sprintf(
            buff, 
            "%02d:%02d:%02d.%03d",
            static_cast<int>(st.wHour),
            static_cast<int>(st.wMinute),
            static_cast<int>(st.wSecond),
            static_cast<int>(st.wMilliseconds)
        );
    }

} // dbg
