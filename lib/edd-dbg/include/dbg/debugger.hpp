// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef DEBUGGER_HPP_0330_15072012
#define DEBUGGER_HPP_0330_15072012

namespace dbg 
{
    bool debugger_attached();

    // Break in to the debugger. This should only be called if it has been determined
    // that a debugger is attached.
    void debugger_break();

} // dbg

#endif // DEBUGGER_HPP_0330_15072012
