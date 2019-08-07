// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "dbg/note.hpp"
#include "dbg/logstream.hpp"
#include "timestamp.hpp"
#include "envvar.hpp"

#include <iomanip>
#include <cstdlib>

namespace dbg
{
    namespace impl
    {
        namespace
        {
            void prefix(std::ostream &out)
            {
                out << '[' << timestamp() << "] ";
            }

            void suffix(std::ostream &out, 
                        const char *filename,
                        unsigned line,
                        const char *) // function tends to make lines too long
            {
                out << " -- " << filename << ':' << line << /*", " << function <<*/ std::endl;
            }

            bool env_var_present()
            {
                return envvar_is_set("DBG_ENABLE_NOTE");
            }

        } // anonymous

        void log_note(const char *name, const void *value, void (*format)(std::ostream &, const void *), 
                      const char *filename, unsigned line, const char *function)
        {
            if (env_var_present())
            {
                logstream out;
                prefix(out);
                out << name << " = ";
                format(out, value);
                suffix(out, filename, line, function);
            }
        }

        void log_note(const char *text, const char *filename, unsigned line, const char *function)
        {
            if (env_var_present())
            {
                logstream out;
                prefix(out);
                out << text;
                suffix(out, filename, line, function);
            }
        }

    } // impl

} // dbg
