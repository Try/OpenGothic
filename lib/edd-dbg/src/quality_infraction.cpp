// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "dbg/impl/quality_infraction.hpp"
#include "dbg/fatality.hpp"
#include "dbg/symbols.hpp"
#include "dbg/frames.hpp"
#include "dbg/debugger.hpp"
#include "dbg/logstream.hpp"
#include "timestamp.hpp"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iomanip>

namespace dbg 
{
    namespace
    {
        fatality_handler on_fatality = std::abort;

    } // anonymous

    fatality_handler set_fatality_handler(fatality_handler handler)
    {
        if (!handler)
            handler = std::abort;

        fatality_handler old = on_fatality;
        on_fatality = handler;

        return old;
    }

    fatality_handler get_fatality_handler()
    {
        return on_fatality;
    }

    namespace impl
    {
        namespace
        {
            std::ostream &name_colon(std::ostream &out, const char *name, std::size_t name_width)
            {
                return (out << "  " << std::setw(name_width) << name << ": ");
            }

        } // anonymous

        quality_infraction::tag::~tag()
        {
            if (next == 0) 
            {
                // This is the last tag, so it will be destructed first. At this
                // point the quality_infraction object and all tags will still exist due
                // to C++ destruction order rules.
                // So this is the point to print the quality_infraction details.
                assert(head);
                head->finish();
            }
        }

        quality_infraction::quality_infraction(bool pass, 
                                               bool fatal,
                                               const char *header,
                                               const char *file, unsigned line, const char *function,
                                               unsigned skip_frames) :
            pass(pass),
            fatal(fatal),
            header(header),
            file(file),
            line(line),
            function(function),
            skip_frames(2 + skip_frames),
            tags(0)
        {
        }

        void quality_infraction::finish() const
        {
            if (!pass)
            {
                logstream out;
                out << "\n[" << timestamp() << "] " << header << "\n\n";

                // Find the longest tag name so that we can align the output
                std::size_t name_width = 8;

                const tag *link = tags;
                while (link)
                {
                    name_width = std::max(name_width, std::strlen(link->name));
                    link = link->next;
                }

                name_colon(out, "location", name_width) << file << ':' << line << '\n';
                name_colon(out, "function", name_width) << function << '\n';

                // Print the tag names and values
                link = tags;
                while (link)
                {
                    name_colon(out, link->name, name_width);
                    link->fmt(out, link->value);
                    out << "\n";

                    link = link->next;
                }

                // Print a stack trace
                symdb db;
                call_stack<256> trace;
                trace.collect(skip_frames);

                out << '\n';
                trace.log(db, out, "    ");
                out << '\n';

                out.flush();

                if (debugger_attached())
                {
                    debugger_break();
                }
                else if (fatal)
                {
                    assert(on_fatality);
                    on_fatality();
                }

                pass = true;
            }
        }

        const quality_infraction::tag &operator, (const quality_infraction &lhs, 
                                                  const quality_infraction::tag &rhs)
        {
            lhs.tags = &rhs;
            rhs.head = &lhs;
            return rhs;
        }

        const quality_infraction::tag &operator, (const quality_infraction::tag &lhs, 
                                                  const quality_infraction::tag &rhs)
        {
            lhs.next = &rhs;
            rhs.head = lhs.head;
            return rhs;
        }

    } // impl

} // dbg
