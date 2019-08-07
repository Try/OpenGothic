// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef MEMCPY_CAST_HPP_1127_27082012
#define MEMCPY_CAST_HPP_1127_27082012

#include "dbg/static_assert.hpp"

#include <cstring>

namespace dbg 
{
    template<typename To, typename From>
    To memcpy_cast(const From &x)
    {
        To ret;
        DBG_STATIC_ASSERT(sizeof ret == sizeof x);
        std::memcpy(&ret, &x, sizeof ret);
        return ret;
    }

} // dbg

#endif // MEMCPY_CAST_HPP_1127_27082012
