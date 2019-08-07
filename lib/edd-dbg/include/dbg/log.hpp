// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef LOG_HPP_1456_18072012
#define LOG_HPP_1456_18072012

#include <cstddef>

namespace dbg 
{
    class logsink
    {
        public:
            logsink() { }
            virtual ~logsink() { }

            // Derived implementations should override this to write the specified
            // number of characters from text to the underlying sink.
            // Any synchronization required for multithreaded logging should be
            // performed by the derived implementation.
            virtual void write(const char *text, std::size_t n) = 0;

        private:
            logsink(const logsink &);
            logsink &operator= (const logsink &);
    };

    // Sets the currently active sink. The specified sink must exist for as long as
    // it is active.
    // Returns the old sink.
    logsink &set_logsink(logsink &sink);

    // Returns the active logsink.
    logsink &get_logsink();

    // Writes to the currently active sink.
    // Used by the library for logging assertions, notes, etc.
    void log_write(const char *text, std::size_t n);


} // dbg

#endif // LOG_HPP_1456_18072012
