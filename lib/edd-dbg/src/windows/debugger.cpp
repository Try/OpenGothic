// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "dbg/assert.hpp"

#include <windows.h>

namespace dbg 
{
    bool debugger_attached()
    {
        return IsDebuggerPresent() == TRUE;
    }

    void debugger_break()
    {
#if defined(_MSC_VER)
        __debugbreak();
#else
        // __builtin_trap doesn't behave nicely with MinGW's GDB.
        // This seems to do what's required.
        DebugBreak(); 
#endif
    }

} // dbg
