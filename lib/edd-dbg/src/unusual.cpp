// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#define DBG_ENABLE_UNUSUAL 1
#include "dbg/unusual.hpp"

namespace dbg 
{
    namespace impl 
    {
        unusual::unusual(bool predicate, const char *filename, unsigned line, const char *function, unsigned skip_frames) :
            quality_infraction(!predicate, 
                               false,
                               "An unusual condition has been encountered:",
                               filename, line, function,
                               skip_frames)
        {
        }
            
        unusual::~unusual()
        {
            finish();
        }
    
    } // impl

} // dbg
