// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef QUALITY_INFRACTION_HPP_2020_16072012
#define QUALITY_INFRACTION_HPP_2020_16072012

#include "dbg/impl/format.hpp"

namespace dbg 
{
    namespace impl 
    {
        // Used in a number of DBG_XXX macros as a means to record events that
        // may indicate a problem with the software.
        // Provides an object against wich we can overload operators in order
        // to attach notes ('tags').
        struct quality_infraction
        {
            // Contains a name and a value and stores the means to print
            // that value to an ostream.
            // Where one or more DBG_TAG() macros are used with an DBG_XXX
            // macro (e.g. DBG_ASSERT, DBG_UNUSUAL, ...), a temporary linked
            // list of these is built on the stack.
            struct tag
            {
                template<typename T>
                tag(const char *name, const T &value) : 
                    name(name),
                    value(&value),
                    fmt(&::dbg::impl::formatter<T>),
                    next(0),
                    head(0)
                { 
                }

                // If we're the first tag in the linked list that is being
                // destroyed, the destructor signals the quality_infraction 
                // object that it's time to check for failure and write debug
                // output if required.
                DBG_NEVER_INLINE ~tag();

                const char * const name;
                const void * const value;
                void (*fmt)(std::ostream &, const void *);
                mutable const tag *next;
                mutable const quality_infraction *head;

                tag(const tag &); // no implementation, shouldn't be copied
                tag &operator=(const tag &); // ditto
            };

            quality_infraction(bool pass, 
                               bool fatal,
                               const char *header,
                               const char *file, unsigned line, const char *function,
                               unsigned skip_frames);

            DBG_NEVER_INLINE void finish() const;

            mutable bool pass;
            const bool fatal;

            const char * const header;
            const char * const file;
            const unsigned line;
            const char * const function;

            const unsigned skip_frames;

            mutable const tag *tags;

            quality_infraction(const quality_infraction &); // no implementation, shouldn't be copied
            quality_infraction &operator= (const quality_infraction &); // ditto
        };

        const quality_infraction::tag &operator, (const quality_infraction &lhs,
                                                  const quality_infraction::tag &rhs);

        const quality_infraction::tag &operator, (const quality_infraction::tag &lhs,
                                                  const quality_infraction::tag &rhs);
    
    } // impl

} // dbg

#endif // QUALITY_INFRACTION_HPP_2020_16072012
