// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "timestamp.hpp"

#include <ctime>
#include <cstdio>

#include <sys/time.h>

namespace dbg 
{
    void generate_timestamp(stamp_buff &buff)
    {
        timeval tv;
        std::memset(&tv, 0, sizeof tv);

        (void)gettimeofday(&tv, 0);

        std::tm now_tm;
        ::localtime_r(&tv.tv_sec, &now_tm);
        std::strftime(buff, sizeof buff, "%H:%M:%S", &now_tm);

        std::sprintf(buff + 8, ".%06d", static_cast<int>(tv.tv_usec));
    }

} // dbg
