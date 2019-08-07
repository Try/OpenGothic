// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef NOTE_HPP_1646_16072012
#define NOTE_HPP_1646_16072012

#include "dbg/config.hpp"

// Save some preprocessing/compilation time if notes are disabled
#if defined(DBG_ENABLE_NOTE) || defined(DBG_ENABLE_HOT_NOTE)

#include "dbg/impl/format.hpp"

namespace dbg 
{
    namespace impl
    {
        void log_note(const char *name, const void *value, void (*format)(std::ostream &, const void *), 
                      const char *filename, unsigned line, const char *function);

        void log_note(const char *text, const char *filename, unsigned line, const char *function);

        template<typename T> struct enable_if_const_char { };
        template<> struct enable_if_const_char<const char> { typedef void type; };

        // For string literals.
        template<typename ConstChar, unsigned N>
        typename enable_if_const_char<ConstChar>::type
        note(const char *, ConstChar (&text)[N], const char *filename, unsigned line, const char *function)
        {
            log_note(text, filename, line, function);
        }

        // For everything else.
        template<typename T>
        void note(const char *name, const T &value, const char *filename, unsigned line, const char *function)
        {
            log_note(name, &value, &dbg::impl::formatter<T>, filename, line, function);
        }

    } // impl

} // dbg

#endif // DBG_ENABLE_(HOT_)NOTES


// DBG_(HOT_)NOTE() can be used to log strings or the values of variables (depending on the type of the argument).
// For notes to appear in the log, DBG_ENABLE_(HOT_)NOTE must be defined as a preprocessor symbol and DBG_ENABLE_NOTE
// must exist as an environment variable with a non-empty value other than "0".
//
// Example output (excluding timestamp and location info):
//
//   DBG_NOTE("hello"); // output: hello
//
//   const char *s = "hello";
//   DBG_NOTE(s); // output: s = hello
//
//   int x = 40;
//   int y = 2;
//   DBG_NOTE(x + y); // outut: x + y = 42
//
#define DBG_NOTE_IMPL(var) (::dbg::impl::note(#var, (var), __FILE__, __LINE__, DBG_FUNCTION))

#if defined(DBG_ENABLE_NOTE)
#   define DBG_NOTE(var) DBG_NOTE_IMPL(var)
#else
#   define DBG_NOTE(var) ((void)(sizeof(var)))
#endif

#if defined(DBG_ENABLE_HOT_NOTE)
#   define DBG_HOT_NOTE(var) DBG_NOTE_IMPL(var)
#else
#   define DBG_HOT_NOTE(var) ((void)(sizeof(var)))
#endif

#endif // NOTE_HPP_1646_16072012
