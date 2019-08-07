// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "dbg/debugger.hpp"

#include <cstddef>
#include <cstdlib>
#include <cstring>

#include <unistd.h>

#include <sys/sysctl.h>
#include <sys/types.h>

namespace dbg 
{
    bool debugger_attached()
    {
        int mib_name[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid() };

        kinfo_proc info;
        std::memset(&info, 0, sizeof info);

        std::size_t size = sizeof(info);
        return 
            sysctl(mib_name, sizeof(mib_name) / sizeof(*mib_name), &info, &size, 0, 0) == 0 &&
            (info.kp_proc.p_flag & P_TRACED) != 0;
    }

    void debugger_break()
    {
        //Debugger();
        __builtin_trap();
    }

} // dbg
