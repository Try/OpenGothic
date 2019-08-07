// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "dbg/frames.hpp"
#include "dbg/symbols.hpp"
#include "dbg/symlog.hpp"

#include <algorithm>
#include <ostream>

namespace dbg 
{
    namespace
    {
        struct array_sink : public frame_sink
        {
            array_sink(const void *array[], unsigned length, unsigned ignore) :
                array(array),
                length(length),
                ignore(ignore),
                n(0)
            {
            }

            virtual bool on_frame(unsigned level, const void *program_counter)
            {
                if (level < ignore)
                    return true;

                if (n == length)
                    return false;

                array[n++] = program_counter;
                return true;
            }

            const void **array;
            const unsigned length;
            const unsigned ignore;
            unsigned n;
        };

    } // anonymous

    unsigned store_frames(const void *pcs[], unsigned max_frames, unsigned ignore)
    {
        std::fill(pcs, pcs + max_frames, static_cast<const void *>(0));

        array_sink s(pcs, max_frames, ignore);
        walk_frames(s);

        return s.n;
    }

    void log_frames(const void * const pcs[], unsigned num_pcs, 
                    const symdb &db,
                    std::ostream &out,
                    const char *indent)
    {
        if (!indent)
            indent = "";

        if (num_pcs == 0 || !pcs[0])
        {
            out << indent << "[no call frames available]\n";
        }
        else
        {
            symlog symsink(out, indent, "\n");

            for (unsigned i = 0, n = num_pcs; i != n && pcs[i]; ++i)
                db.lookup_function(pcs[i], symsink);
        }
    }

} // dbg
