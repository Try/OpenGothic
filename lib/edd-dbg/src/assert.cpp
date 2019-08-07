// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#define DBG_ENABLE_ASSERT 1
#include "dbg/assert.hpp"

namespace dbg 
{
    namespace impl 
    {
        assertion::assertion(bool pass, const char *file, unsigned line, const char *function, unsigned skip_frames) :
            quality_infraction(pass, true, "An assertion has been triggered:", file, line, function, skip_frames)
        {
        }

        assertion::~assertion() 
        {
            finish();
        }

        unreachable::unreachable(const char *file, unsigned line, const char *function) :
            quality_infraction(false, true, "Unreachable code path encountered:", file, line, function, 0)
        {
        }

        unreachable::~unreachable() 
        {
            finish();
        }
    
    } // impl

} // dbg
