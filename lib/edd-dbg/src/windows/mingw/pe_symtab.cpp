// Copyright Edd Dawson 2013
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "pe_symtab.hpp"
#include "pe.hpp"

#include "dbg/static_assert.hpp"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include <cxxabi.h>

namespace dbg 
{
    namespace
    {
        class gnu_demangler
        {
            public:
                static const std::size_t initial_buff_size = 128;

                gnu_demangler() : 
                    buff(static_cast<char *>(std::malloc(initial_buff_size))),
                    allocated(initial_buff_size) 
                {
                    if (!buff)
                        throw std::bad_alloc();
                }

                ~gnu_demangler()
                {
                    std::free(buff);
                }

                // Returns either symname, or a pointer to an internally maintained
                // buffer containing the demangled name.
                // The buffer is reused on the next call to demangle().
                const char *demangle(const char *symname)
                {
                    assert(symname);

                    const char *demangled = try_demangle(symname);

                    if (!demangled && symname[0] == '_')
                        demangled = try_demangle(symname + 1);

                    return demangled ? demangled : symname;
                }

            private:
                const char *try_demangle(const char *symname)
                {
                    int status = 0;
                    char *demangled = abi::__cxa_demangle(symname, buff, &allocated, &status);

                    assert(status != -3); // indicates invalid arguments.
                    if (status == -1)
                        throw std::bad_alloc();

                    if (status == -2)
                        return 0;

                    buff = demangled;
                    return buff;
                }

                gnu_demangler(const gnu_demangler &);
                gnu_demangler &operator= (const gnu_demangler &);

            private:
                char *buff;
                std::size_t allocated;
        };

        struct symbol_collector : pe_sink
        {
            symbol_collector()
            {
            }

            void on_symbol(const char *name, std::size_t image_offset)
            {
                name = demangler.demangle(name);

                const std::size_t name_offset = strtab.size();
                strtab.insert(strtab.end(), name, name + std::strlen(name) + 1);

                pe_symbol sym;
                sym.address = image_offset;
                sym.name_offset = name_offset;

                symbols.push_back(sym);
            }

            std::vector<char> strtab;
            std::vector<pe_symbol> symbols;
            gnu_demangler demangler;
        };

        template<typename Target, typename Source>
        Target memcpy_cast(Source source)
        {
            Target ret;

            DBG_STATIC_ASSERT(sizeof(Target) == sizeof(Source));
            std::memcpy(&ret, &source, sizeof(Source));

            return ret;
        }

    } // anonymous

    pe_symtab::pe_symtab()
    {
    }

    pe_symtab::pe_symtab(const void *module, file &pe)
    {
        const std::size_t load_address = memcpy_cast<std::size_t>(module);

        symbol_collector collector;
        scan_mingw_pe_file(pe, collector);

        strtab.swap(collector.strtab);
        symbols.swap(collector.symbols);

        for (std::size_t s = 0, ns = symbols.size(); s != ns; ++s)
            symbols[s].address += load_address;

        std::sort(symbols.begin(), symbols.end());
    }
        
    const char *pe_symtab::function_spanning(const void *address) const
    {
        pe_symbol sym;
        sym.address = memcpy_cast<std::size_t>(address);

        std::vector<pe_symbol>::const_iterator it = std::upper_bound(symbols.begin(), symbols.end(), sym);

        if (it != symbols.begin())
        {
            --it;
            return &strtab[it->name_offset];
        }

        return 0;
    }

    void pe_symtab::swap(pe_symtab &other)
    {
        strtab.swap(other.strtab);
        symbols.swap(other.symbols);
    }

} // dbg
