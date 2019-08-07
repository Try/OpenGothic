// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef UNUSUAL_HPP_2105_16072012
#define UNUSUAL_HPP_2105_16072012

#include "dbg/config.hpp"

// Save some preprocessing/compilation time if 'unusuals' are disabled
#if defined(DBG_ENABLE_UNUSUAL) || defined(DBG_ENABLE_HOT_UNUSUAL)
#include "dbg/impl/quality_infraction.hpp"

namespace dbg 
{
    namespace impl 
    {
        struct unusual : public quality_infraction
        {
            unusual(bool condition, const char *filename, unsigned line, const char *function, unsigned skip_frames = 0);
            DBG_NEVER_INLINE ~unusual();
        };

        // For use in impure unusuals.
        template<typename T>
        T &unusual_and_forward(T &arg, const char *condition, 
                               const char *file, unsigned line, const char *function)
        {
            unusual(!!arg, file, line, function, 1), unusual::tag("condition", condition);
            return arg;
        }

        // For use in impure unusuals.
        template<typename T>
        const T &unusual_and_forward(const T &arg, const char *condition, 
                                     const char *file, unsigned line, const char *function)
        {
            unusual(!!arg, file, line, function, 1), unusual::tag("condition", condition);
            return arg;
        }

    } // impl

} // dbg

#endif // DBG_ENABLE_(HOT_)UNUSUAL


#if defined(DBG_ENABLE_UNUSUAL)
#   define DBG_UTAG(var) ::dbg::impl::unusual::tag("'" #var "'", var)
#else
#   define DBG_UTAG(var) (void)(sizeof(var))
#endif

#if defined(DBG_ENABLE_HOT_UNUSUAL)
#   define DBG_HOT_UTAG(var) ::dbg::impl::unusual::tag("'" #var "'", var)
#else
#   define DBG_HOT_UTAG(var) (void)(sizeof(var))
#endif



// DBG_(HOT_)UNUSUAL
// Used to record events that are unusual, but from which we may be able to recover and
// don't necessarily indicate problems with the software. For example, failure to open
// a file that we believe should typically exist.
//
// When an unusual event is encountered, the location and a backtrace are logged and an
// attempt is made to break in to the debugger if one is detected. Local values can be
// attached for display on failure with DBG_(HOT_)UTAG, e.g.
//
//    DBG_UNUSUAL(!file.is_open()), DBG_UTAG(filename);
//
// Note that the condition passed to DBG_(HOT_)UNUSUAL is the opposite to what you might
// pass to DBG_(HOT_)ASSERT i.e. nothing is logged when the condition is false.
//
#define DBG_UNUSUAL_IMPL(condition) \
    (::dbg::impl::unusual(condition, __FILE__, __LINE__, DBG_FUNCTION)), \
    ::dbg::impl::unusual::tag("condition", #condition)

#if defined(DBG_ENABLE_UNUSUAL)
#   define DBG_UNUSUAL(condition) DBG_UNUSUAL_IMPL(condition)
#else
#   define DBG_UNUSUAL(condition) (void)(sizeof(condition))
#endif

#if defined(DBG_ENABLE_HOT_UNUSUAL)
#   define DBG_HOT_UNUSUAL(condition) DBG_UNUSUAL_IMPL(condition)
#else
#   define DBG_HOT_UNUSUAL(condition) (void)(sizeof(condition))
#endif



// DBG_(HOT_)IMPURE_UNUSUAL
// Like DBG_(HOT_)UNUSUAL, except that it may be used to check expressions that have side-effects.
// The value of the macro invocation is the value of the expression being checked.
#define DBG_IMPURE_UNUSUAL_IMPL(expression) \
    (::dbg::impl::unusual_and_forward(expression, #expression, __FILE__, __LINE__, DBG_FUNCTION))

#if defined(DBG_ENABLE_UNUSUAL)
#   define DBG_IMPURE_UNUSUAL(expression) DBG_IMPURE_UNUSUAL_IMPL(expression)
#else
#   define DBG_IMPURE_UNUSUAL(expression) (expression)
#endif

#if defined(DBG_ENABLE_HOT_UNUSUAL)
#   define DBG_HOT_IMPURE_UNUSUAL(expression) DBG_IMPURE_UNUSUAL_IMPL(expression)
#else
#   define DBG_HOT_IMPURE_UNUSUAL(expression) (expression)
#endif

#endif // UNUSUAL_HPP_2105_16072012
