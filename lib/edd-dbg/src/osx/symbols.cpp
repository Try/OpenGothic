// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "dbg/symbols.hpp"

#include <cstdlib>
#include <new>

#include <dlfcn.h>
#include <cxxabi.h>

namespace dbg 
{
    void symsink::on_function(const void *program_counter, const char *name, const char *module)
    {
        process_function(
            program_counter,
            (name && name[0]) ? name : "[unknown function]",
            (module && module[0]) ? module : "[unknown module]"
        );
    }

    struct symdb::impl
    {
        impl() :
            buff(0),
            buffspace(0)
        {
        }

        ~impl()
        {
            std::free(buff);
        }

        char *buff;
        std::size_t buffspace;
    };

    symdb::symdb() :
        p(new (std::nothrow) impl)
    {
    }

    symdb::~symdb()
    {
        delete p;
    }

    bool symdb::lookup_function(const void *program_counter, symsink &sink) const
    {
        Dl_info info;

        if (dladdr(program_counter, &info))
        {
            if (p)
            {
                int status = 0;
                std::size_t len = 0;

                char *demangled = abi::__cxa_demangle(info.dli_sname, p->buff, &len, &status);

                sink.on_function(program_counter, demangled ? demangled : info.dli_sname, info.dli_fname);

                if (demangled)
                {
                    p->buff = demangled; // reuse the space allocated by __cxa_demangle
                    p->buffspace = len;
                }
            }
            else
            {
                sink.on_function(program_counter, info.dli_sname, info.dli_fname);
            }

            return true;
        }
        else
        {
            sink.on_function(program_counter, 0, 0);
            return false;
        }
    }

} // dbg
