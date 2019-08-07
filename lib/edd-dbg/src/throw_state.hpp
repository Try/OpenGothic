// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef THROW_STATE_HPP_1530_23062012
#define THROW_STATE_HPP_1530_23062012

#include "dbg/throw.hpp"
#include "dbg/frames.hpp"

#include <cstddef>

namespace dbg 
{
    struct throw_state
    {
        throw_state() : exception_ref_count(0) { what_buffer[0] = 0; }

        char what_buffer[2048];
        throw_info current_exception;

        call_stack<256> trace;

        std::size_t exception_ref_count;

        // Returns throw_state for the calling thread, null if unavailable.
        static throw_state *get();
    };

} // dbg

#endif // THROW_STATE_HPP_1530_23062012
