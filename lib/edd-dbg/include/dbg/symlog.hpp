// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef SYMLOG_HPP_2357_23062012
#define SYMLOG_HPP_2357_23062012

#include "dbg/symbols.hpp"
#include <iosfwd>

namespace dbg 
{
    // Implements the symsink interface to log function and module names.
    class symlog : public symsink
    {
        public:
            symlog(std::ostream &out, const char *prefix = "", const char *suffix = "");

        private:
            // Writes "<prefix><program_counter>: <name> in <module><suffix>".
            virtual void process_function(const void *program_counter, const char *name, const char *module);
        
        private:
            std::ostream &out;
            const char *prefix;
            const char *suffix;
    };

} // dbg

#endif // SYMLOG_HPP_2357_23062012
