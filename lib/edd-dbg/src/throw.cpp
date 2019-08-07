// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "dbg/throw.hpp"
#include "dbg/symbols.hpp"
#include "dbg/frames.hpp"
#include "dbg/logstream.hpp"

#include "throw_state.hpp"
#include "timestamp.hpp"
#include "envvar.hpp"

#include <cassert>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <exception>

namespace dbg 
{
    namespace
    {
        void dbg_terminate_handler();

        std::terminate_handler old_terminate = std::set_terminate(dbg_terminate_handler);

        void dbg_terminate_handler()
        {
            throw_info info = current_exception();
            
            if (!info.empty())
            {
                logstream out;
                log(info, "An uncaught exception has caused process termination:", out);
            }

            if (old_terminate)
                old_terminate();
        }

        DBG_NEVER_INLINE 
        void record_exception(const char *what,
                              const char *expr,
                              const char *file,
                              unsigned line,
                              const char *function)
        {
            throw_state *s = throw_state::get();
            if (!s)
                return;

            call_stack<256> trace;
            trace.collect(3); // omit this function, dbg::log_exception() and dbg::impl::raise() from the trace.

            // We're going to store a throw_info in our thread-local throw_state. So we must take
            // a copy of the exception's what() text to avoid a dangling pointer in to the 
            // internals of the current exception, as it may get destroyed in favour of another
            // copy created by the C++ runtime machinery.
            const std::size_t whatlen = std::strlen(what);

            if (whatlen < (sizeof s->what_buffer))
            {
                std::strcpy(s->what_buffer, what);
            }
            else
            {
                char *end = std::copy(what, what + (sizeof s->what_buffer) - 4, s->what_buffer);
                std::strcpy(end, "...");
            }

            s->trace = trace; // so s->current_exception can refer to it.
            s->current_exception = throw_info(file, function, line, s->trace, expr, s->what_buffer);

            if (envvar_is_set("DBG_LOG_ON_THROW"))
            {
                logstream out;
                log(s->current_exception, "An exception is being thrown:", out);
            }
        }

    } // anonymous
    

    throw_info::throw_info() :
        file(""),
        function(""),
        line(0),
        trace(0),
        expression(""),
        what("-")
    {
    }

    throw_info::throw_info(const char *file,
                           const char *function,
                           unsigned line,
                           const call_stack<256> &trace,
                           const char *expression,
                           const char *what) :
        file(file ? file : ""),
        function(function ?  function : ""),
        line(line),
        trace(&trace),
        expression(expression ? expression : ""),
        what(what ? what : "-")
    {
    }

    bool throw_info::empty() const
    {
        return line == 0;
    }

    namespace impl 
    {
        // tag maintains a thread-local reference count in order to keep track
        // of whether the current exception was thrown by DBG_THROW (all such
        // exceptions are adapted so that their type contains a tag).

        tag::tag()
        {
            throw_state *s = throw_state::get();
            if (s)
                s->exception_ref_count = 1;
        }

        tag::~tag() throw()
        {
            throw_state *s = throw_state::get();
            if (s)
            {
                // FIXME: I've observed exception_ref_count to be 0 here on some occasions. 
                // Why/how? Possibly something to do with copy elision of exceptions?
                if (s->exception_ref_count) 
                    s->exception_ref_count--;
            }
        }

        tag::tag(const tag &)
        {
            throw_state *s = throw_state::get();
            if (s)
                s->exception_ref_count++;
        }

        tag &tag::operator= (const tag &)
        {
            return *this;
        }


        void log_exception(const std::exception *ex, 
                           const char *expr,
                           const char *file,
                           unsigned line,
                           const char *function)
        {
            record_exception(ex->what(), expr, file, line, function);
        }

        void log_exception(const void *,
                           const char *expr,
                           const char *file,
                           unsigned line,
                           const char *function)
        {
            record_exception("", expr, file, line, function);
        }

    } // impl

    throw_info current_exception()
    {
        throw_state *s = throw_state::get();
        if (!s) return throw_info();

        if (s->exception_ref_count == 0)
            s->current_exception = throw_info();

        return s->current_exception;
    }

    void log(const throw_info &info, const char *header, const symdb &db, std::ostream &out)
    {
        assert(info.file);
        assert(info.function);
        assert(info.expression);
        assert(info.what);

        const char *small_indent = "";
        const char *big_indent = "  ";
        if (header && header[0] != '\0')
        {
            out << '[' << timestamp() << "] " << header << "\n\n";
            small_indent = "  ";
            big_indent = "    ";
        }

        if (info.file[0] != '\0') 
            out << small_indent << "  location: " << info.file << ':' << info.line << '\n';
        else
            out << small_indent << "  location: \n";

        out << small_indent << "  function: " << info.function << '\n' <<
               small_indent << "expression: " << info.expression << '\n' <<
               small_indent << "   details: " << info.what << "\n\n";

        const call_stack<256> &trace = info.trace ? *info.trace : call_stack<256>();

        trace.log(db, out, big_indent);
        out << std::endl;
    }

    void log(const throw_info &info, const char *header, std::ostream &out)
    {
        symdb db;
        log(info, header, db, out);
    }

} // dbg
