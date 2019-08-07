// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef FORMAT_HPP_1654_16072012
#define FORMAT_HPP_1654_16072012

#include "dbg/config.hpp"
#include <iosfwd>

namespace dbg 
{
    namespace impl 
    {
        // These overloads avoid our having to include <ostream>, making the 
        // assert.hpp header (and others) friendlier in terms of compile times.
        void format(std::ostream &out, char x);
        void format(std::ostream &out, signed char x);
        void format(std::ostream &out, unsigned char x);
        void format(std::ostream &out, wchar_t x);
        void format(std::ostream &out, short x);
        void format(std::ostream &out, unsigned short x);
        void format(std::ostream &out, int x);
        void format(std::ostream &out, unsigned int x);
        void format(std::ostream &out, long x);
        void format(std::ostream &out, unsigned long x);
        // TODO: long long?
        void format(std::ostream &out, bool x);
        void format(std::ostream &out, float x);
        void format(std::ostream &out, double x);
        void format(std::ostream &out, long double x);
        void format(std::ostream &out, const void *x);
        void format(std::ostream &out, const char *x);
        void format(std::ostream &out, const signed char *x);
        void format(std::ostream &out, const unsigned char *x);

        template<typename T> 
        void format(std::ostream &out, T *x) 
        { 
            format(out, static_cast<const void *>(x)); 
        }

        // For any other type, <ostream> will already be included or it
        // too will only require <iosfwd>.
        template<typename T>
        void format(std::ostream &out, const T &x) 
        { 
#if defined(_MSC_VER)
            // This is a workaround to prevent error C2027: use of undefined type 'std::basic_ostream<Elem,_Traits>'
            // when std::ostream is declared but not defined at the point where asserts etc are used.
            // http://www.gamedev.net/topic/629420-compiler-discrepancy-use-of-undefined-type/
            std::ostream &lhs = 
#endif
            out << x; 
        }
    
        // formatter<T>(out, p) will delegate to the appropriate format() overload
        // or the primary template.
        template<typename T>
        void formatter(std::ostream &out, const void *p)
        {
            ::dbg::impl::format(out, *static_cast<const T *>(p));
        }

    } // impl

} // dbg

#endif // FORMAT_HPP_1654_16072012
