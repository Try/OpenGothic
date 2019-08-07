// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "dbg/impl/format.hpp"

#include <ostream>

namespace dbg 
{
    namespace impl 
    {
        void format(std::ostream &out, char x) { out << x; }
        void format(std::ostream &out, signed char x) { out << x; }
        void format(std::ostream &out, unsigned char x) { out << x; }
        void format(std::ostream &out, wchar_t x) { out << x; }
        void format(std::ostream &out, short x) { out << x; }
        void format(std::ostream &out, unsigned short x) { out << x; }
        void format(std::ostream &out, int x) { out << x; }
        void format(std::ostream &out, unsigned int x) { out << x; }
        void format(std::ostream &out, long x) { out << x; }
        void format(std::ostream &out, unsigned long x) { out << x; }
        void format(std::ostream &out, bool x) { out << x; }
        void format(std::ostream &out, float x) { out << x; }
        void format(std::ostream &out, double x) { out << x; }
        void format(std::ostream &out, long double x) { out << x; }
        void format(std::ostream &out, const void *x) { out << x; }
        void format(std::ostream &out, const char *x) { out << x; }
        void format(std::ostream &out, const signed char *x) { out << x; }
        void format(std::ostream &out, const unsigned char *x) { out << x; }
    
    } // impl

} // dbg
