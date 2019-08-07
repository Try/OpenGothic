// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef STATIC_ASSERT_HPP_1634_14072012
#define STATIC_ASSERT_HPP_1634_14072012

#include "dbg/config.hpp"

namespace dbg 
{
    template<bool> struct static_assertion_check;
    template<> struct static_assertion_check<true> { static const int value = 1; };

} // dbg

#define DBG_STATIC_ASSERT(condition) \
    enum DBG_TOKEN_JOIN(dbg_static_assertion_, DBG_UNIQUENESS_TOKEN) \
    { \
        DBG_TOKEN_JOIN(dbg_static_assertion_value, DBG_UNIQUENESS_TOKEN) = \
            ::dbg::static_assertion_check<(condition)>::value \
    }

#endif // STATIC_ASSERT_HPP_1634_14072012
