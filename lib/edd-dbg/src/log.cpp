// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "dbg/log.hpp"

#include <cstdio>
#include <cassert>

namespace dbg 
{
    namespace
    {
        struct default_logsink : logsink
        {
            virtual void write(const char *text, std::size_t n)
            {
                std::fwrite(text, n, 1, stderr);
            }
        };

        default_logsink default_sink;

        logsink *current_sink = &default_sink;

    } // anonymous

    logsink &set_logsink(logsink &sink)
    {
        assert(current_sink);
        logsink *old = current_sink;
        current_sink = &sink;
        return *old;
    }

    logsink &get_logsink()
    {
        assert(current_sink);
        return *current_sink;
    }

    void log_write(const char *text, std::size_t n)
    {
        assert(current_sink);
        current_sink->write(text, n);
    }

} // dbg
