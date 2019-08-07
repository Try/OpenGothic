// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef THROW_HPP_1250_23062012
#define THROW_HPP_1250_23062012

#include "dbg/config.hpp"
#include "dbg/impl/raise.hpp"

#include <iosfwd>

// Use "DBG_THROW(an_exception)" rather than "throw an_exception" to enable logging of
// exceptions and to provide the means of recovering a call stack of the throw site.
// Internally, fungo::raise() is also used to provide the means to catch, store and
// rethrow the exception (across a thread-join boundary, for example) unless 
// DBG_DONT_USE_FUNGO is defined.
#define DBG_THROW(exception) \
    DBG_RAISE(exception, #exception, __FILE__, __LINE__, DBG_FUNCTION)

namespace dbg 
{
    template<unsigned N> class call_stack;
    class symdb;

    // A structure representing the point in the source at which an exception is thrown,
    // and some additional details about the exception.
    struct throw_info
    {
        throw_info();
        throw_info(const char *file, 
                   const char *function,
                   unsigned line,
                   const call_stack<256> &trace,
                   const char *expression,
                   const char *what);

        bool empty() const;

        const char * file;
        const char * function;
        unsigned line;

        const call_stack<256> *trace;

        const char * expression;
        const char * what;
    };

    // The returned throw_info will either be empty(), or will return a log for the exception that is
    // currently in transit.
    // An empty log implies one of the following conditions:
    // - there is no exception in transit 
    // - the exception was not thrown with DBG_THROW
    // - DBG_ENABLE_EXCEPTION_LOG is not defined at the point where DBG_THROW was called.
    // - the implementation failed to acquire a thread-local-storage slot for its book-keeping.
    throw_info current_exception();

    // Logs details of the exception to the given stream. The details will be preceded by a timestamp
    // and the header text if header is not null.
    // The specified symbol database object is used when attempting to find symbols for the functions
    // in the call stack.
    void log(const throw_info &info, const char *header, const symdb &db, std::ostream &out);

    // Equivalent to calling log(info, header, db, out) where db is a temporary symbol database.
    // symbol database objects are very expensive to create, so if performance is a concern aim
    // to reuse a single database object across multiple calls to the other overload.
    void log(const throw_info &info, const char *header, std::ostream &out);

} // dbg

#endif // THROW_HPP_1250_23062012
