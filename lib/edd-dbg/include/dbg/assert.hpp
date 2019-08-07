// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ASSERT_HPP_1655_14072012
#define ASSERT_HPP_1655_14072012

#include "dbg/config.hpp"

// Save some preprocessing/compilation time if assertions are disabled
#if defined(DBG_ENABLE_ASSERT) || defined(DBG_ENABLE_HOT_ASSERT)
#include "dbg/impl/quality_infraction.hpp"

namespace dbg 
{
    namespace impl 
    {
        struct assertion : public quality_infraction
        {
            assertion(bool pass, const char *file, unsigned line, const char *function, unsigned skip_frames = 0);
            DBG_NEVER_INLINE ~assertion();
        };

        struct unreachable : public quality_infraction
        {
            unreachable(const char *file, unsigned line, const char *function);
            DBG_NEVER_INLINE ~unreachable();
        };

        // For use in impure assertions.
        template<typename T>
        T &assert_and_forward(T &arg, const char *condition, 
                              const char *file, unsigned line, const char *function)
        {
            assertion(!!arg, file, line, function, 1), assertion::tag("condition", condition);
            return arg;
        }

        // For use in impure assertions.
        template<typename T>
        const T &assert_and_forward(const T &arg, const char *condition, 
                                    const char *file, unsigned line, const char *function)
        {
            assertion(!!arg, file, line, function, 1), assertion::tag("condition", condition);
            return arg;
        }

    } // impl

} // dbg

#endif // DBG_ENABLE_(HOT_)ASSERTIONS


#if defined(DBG_ENABLE_ASSERT)
#   define DBG_TAG(var) ::dbg::impl::assertion::tag("'" #var "'", var)
#else
#   define DBG_TAG(var) (void)(sizeof(var))
#endif

#if defined(DBG_ENABLE_HOT_ASSERT)
#   define DBG_HOT_TAG(var) ::dbg::impl::assertion::tag("'" #var "'", var)
#else
#   define DBG_HOT_TAG(var) (void)(sizeof(var))
#endif


// DBG_(HOT_)ASSERT
// Basic assertion macro. Displays location of assertion and a backtrace and will
// attempt to break in to the debugger if one is detected.
// Local values can be attached for display on failure with DBG_(HOT_)TAG, e.g.
//
//   DBG_ASSERT(x < 5 || (x & flag) != 0), DBG_TAG(x), DBG_TAG(flag);
//
#define DBG_ASSERT_IMPL(condition) \
    ::dbg::impl::assertion(!!(condition), __FILE__, __LINE__, DBG_FUNCTION), \
    ::dbg::impl::assertion::tag("condition", #condition)

#if defined(DBG_ENABLE_ASSERT)
#   define DBG_ASSERT(condition) DBG_ASSERT_IMPL(condition)
#else
#   define DBG_ASSERT(condition) (void)(sizeof(condition))
#endif

#if defined(DBG_ENABLE_HOT_ASSERT)
#   define DBG_HOT_ASSERT(condition) DBG_ASSERT_IMPL(condition)
#else
#   define DBG_HOT_ASSERT(condition) (void)(sizeof(condition))
#endif



// DBG_(HOT_)IMPURE_ASSERT
// Like DBG_(HOT_)ASSERT, except that it may be used to check expressions that have side-effects.
// The value of the macro invocation is the value of the expression being checked.
#define DBG_IMPURE_ASSERT_IMPL(expression) \
    (::dbg::impl::assert_and_forward(expression, #expression, __FILE__, __LINE__, DBG_FUNCTION))

#if defined(DBG_ENABLE_ASSERT)
#   define DBG_IMPURE_ASSERT(expression) DBG_IMPURE_ASSERT_IMPL(expression)
#else
#   define DBG_IMPURE_ASSERT(expression) (expression)
#endif

#if defined(DBG_ENABLE_HOT_ASSERT)
#   define DBG_HOT_IMPURE_ASSERT(expression) DBG_IMPURE_ASSERT_IMPL(expression)
#else
#   define DBG_HOT_IMPURE_ASSERT(expression) (expression)
#endif



// DBG_(HOT_)ASSERT_UNREACHABLE
// Used to label supposedly-unreachable code paths.
// Local values can be attached for display on failure with DBG_(HOT_)TAG, e.g.
//
//   DBG_ASSERT_UNREACHABLE, DBG_TAG(x);
//
#define DBG_ASSERT_UNREACHABLE_IMPL ::dbg::impl::unreachable(__FILE__, __LINE__, DBG_FUNCTION)

#if defined(DBG_ENABLE_ASSERT)
#   define DBG_ASSERT_UNREACHABLE DBG_ASSERT_UNREACHABLE_IMPL
#else
#   define DBG_ASSERT_UNREACHABLE (void)0
#endif

#if defined(DBG_ENABLE_HOT_ASSERT)
#   define DBG_HOT_ASSERT_UNREACHABLE DBG_ASSERT_UNREACHABLE_IMPL
#else
#   define DBG_HOT_ASSERT_UNREACHABLE (void)0
#endif



// "DBG_(HOT_)ASSERT_RELATION(a, op, b)" 
//
// is the same as:
//
// "DBG_(HOT_)ASSERT(a op b), DBG_(HOT_)TAG(a), DBG_(HOT_)TAG(b)"
//
// where op is a binary operator such as "<", "==", etc.
#define DBG_ASSERT_RELATION_IMPL(a, op, b) \
    ::dbg::impl::assertion(!!((a) op (b)), __FILE__, __LINE__, DBG_FUNCTION), \
    ::dbg::impl::assertion::tag("condition", #a " " #op " " #b), \
    ::dbg::impl::assertion::tag("'" #a "'", a), \
    ::dbg::impl::assertion::tag("'" #b "'", b)

#if defined(DBG_ENABLE_ASSERT)
#define DBG_ASSERT_RELATION(a, op, b) DBG_ASSERT_RELATION_IMPL(a, op, b)
#else
#   define DBG_ASSERT_RELATION(a, op, b) (void)(sizeof((a) op (b)))
#endif

#if defined(DBG_ENABLE_HOT_ASSERT)
#   define DBG_HOT_ASSERT_RELATION(a, op, b) DBG_ASSERT_RELATION_IMPL(a, op, b)
#else
#   define DBG_HOT_ASSERT_RELATION(a, op, b) (void)(sizeof((a) op (b)))
#endif

#endif // ASSERT_HPP_1655_14072012
