// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "envvar.hpp"

#include <cassert>
#include <cstdlib>

namespace dbg 
{
    bool envvar_is_set(const char *name)
    {
        assert(name);
        const char *var = std::getenv(name);
        return var && *var != '\0' && *var != '0';
    }

} // dbg
