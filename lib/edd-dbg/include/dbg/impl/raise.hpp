// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef RAISE_HPP_1337_23062012
#define RAISE_HPP_1337_23062012

#include "dbg/config.hpp"
#include "dbg/debugger.hpp"

#if !defined(DBG_DONT_USE_FUNGO)
#   include "fungo/raise.hpp"
#endif

#include <exception>

#if defined(DBG_BREAK_ON_THROW)
#   define DBG_OPTIONAL_DEBUGGER_BREAK_IN_RAISE \
        if (::dbg::debugger_attached()) \
            ::dbg::debugger_break();
#else
#   define DBG_OPTIONAL_DEBUGGER_BREAK_IN_RAISE
#endif

#if defined(DBG_DONT_USE_FUNGO)
#   define DBG_LOGGING_RAISE_FUNCTION ::dbg::impl::raise
#   define DBG_SILENTLY_RAISE(ex) throw (ex)
#else
#   define DBG_LOGGING_RAISE_FUNCTION ::dbg::impl::raise_with_fungo
#   define DBG_SILENTLY_RAISE(ex) ::fungo::raise(ex)
#endif


#if defined(DBG_ENABLE_EXCEPTION_LOG)
#   define DBG_RAISE(exception, expr, file, line, function) \
        do \
        { \
            DBG_OPTIONAL_DEBUGGER_BREAK_IN_RAISE \
            DBG_LOGGING_RAISE_FUNCTION(exception, expr, file, line, function); \
        } \
        while (false)
#else
#   define DBG_RAISE(exception, expr, file, line, function) \
        do \
        { \
            DBG_OPTIONAL_DEBUGGER_BREAK_IN_RAISE \
            DBG_SILENTLY_RAISE(exception); \
        } \
        while (false)
#endif

namespace dbg 
{
    namespace impl 
    {
        DBG_NEVER_INLINE
        void log_exception(const std::exception *ex,
                           const char *ex_str,
                           const char *file,
                           unsigned line,
                           const char *function);

        DBG_NEVER_INLINE
        void log_exception(const void *, // accepts anything that isn't an std::exception
                           const char *ex_str,
                           const char *file,
                           unsigned line,
                           const char *function);

        struct tag 
        { 
            tag();
            ~tag() throw();

            tag(const tag &);
            tag &operator= (const tag &);
        };

        // A tagged<Exception> contains a tag, which provides a way of identifying it
        // at catch sites. We use this to determine if an exception was thrown with
        // dbg::raise(), which ensures all exceptions it raises are tagged.
        template<typename Exception>
        class tagged : public Exception
        { 
            public:
                explicit tagged(const Exception &ex) : Exception(ex) { } 

            private:
                tag internal;
        };
    
        // Used by DBG_THROW to log the exception being thrown in a manner corresponding to
        // the current log immediacy value.
        template<typename Exception>
        DBG_NEVER_INLINE
        DBG_DOESNT_RETURN
        void raise(const Exception &ex,
                   const char *expr,
                   const char *file,
                   unsigned line,
                   const char *function)
        {
            log_exception(&ex, expr, file, line, function);
            throw ::dbg::impl::tagged<Exception>(ex);
        }
    
#if !defined(DBG_DONT_USE_FUNGO)
        template<typename Exception>
        DBG_NEVER_INLINE
        DBG_DOESNT_RETURN
        void raise_with_fungo(const Exception &ex,
                              const char *expr,
                              const char *file,
                              unsigned line,
                              const char *function)
        {
            log_exception(&ex, expr, file, line, function);
            ::fungo::raise( ::dbg::impl::tagged<Exception>(ex) );
        }
#endif

    } // impl

} // dbg

#endif // RAISE_HPP_1337_23062012
