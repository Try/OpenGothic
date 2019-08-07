// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "envvar.hpp"

#include <cassert>

#include <windows.h>

namespace dbg 
{
    bool envvar_is_set(const char *name)
    {
        assert(name);
        char buffer[2] = "";

        if (GetEnvironmentVariableA(name, buffer, sizeof buffer) == 0)
            return false;

        return *buffer != '\0' && *buffer != '0';
    }

} // dbg
