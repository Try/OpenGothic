// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "timestamp.hpp"

#include <ostream>

namespace dbg 
{
    std::ostream &operator << (std::ostream &out, const timestamp &stamp)
    {
        return out << stamp.buff;
    }

} // dbg
