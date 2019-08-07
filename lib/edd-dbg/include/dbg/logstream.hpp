// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef LOGSTREAM_HPP_1309_18072012
#define LOGSTREAM_HPP_1309_18072012

#include <ostream>
#include <streambuf>

namespace dbg 
{
    // Used in logstream. Ultimately writes data to the currently active log_sink
    // through dbg::log_write().
    class logbuf : public std::streambuf
    {
        public:
            logbuf();
            ~logbuf() { flush(); }

            void flush();

        private:
            virtual int_type overflow(int_type ch);
            virtual int sync() { flush(); return 0; }

        private:
            // Stack traces can get pretty large (template functions, long module paths). 
            // It would be nice to be able to write them all in one go (under the same 
            // lock). So the internal buffer is relatively large.
            char buffer[4096];
    };

    // Ultimately writes data to the currently active log_sink through dbg::log_write().
    class logstream : public std::ostream
    {
        public:
            logstream() : std::ostream(0) { rdbuf(&buff); }

        private:
            logbuf buff;
    };

} // dbg

#endif // LOGSTREAM_HPP_1309_18072012
