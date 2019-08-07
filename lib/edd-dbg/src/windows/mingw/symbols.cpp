// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "dbg/symbols.hpp"

#include "pe_symtab.hpp"
#include "file.hpp"
#include "ms_symdb.hpp"
#include "spin_mutex.hpp"

#include <cassert>
#include <cstring>
#include <new>

#include <windows.h>

namespace dbg 
{
    namespace
    {
        class module_node
        {
            public:
                module_node(HMODULE mod, const module_node *next) :
                    mod(mod),
                    own_ref(false),
                    next(next)
                {
                    wchar_t utf16_name[32 * 1024] = L"";

                    const DWORD buffsize = sizeof utf16_name / sizeof *utf16_name;
                    DWORD len = GetModuleFileNameW(mod, utf16_name, buffsize);

                    if (len == buffsize)
                        utf16_name[--len] = L'\0';

                    // Store it as UTF-8.
                    if (len == 0 || 
                        WideCharToMultiByte(CP_UTF8, 0, utf16_name, -1, utf8_name, sizeof utf8_name, 0, 0) == 0)
                    {
                        utf8_name[0] = '\0';
                    }

                    // Load debug info
                    try
                    {
                        file pe(utf16_name);
                        pe_symtab temp(mod, pe);
                        temp.swap(symtab);
                    }
                    catch (const std::exception &ex)
                    {
                        (void)ex;
                    }

                    // It's possible that a module may be unloaded and a different one loaded at the same
                    // address (especially since each has a preferred load address).
                    // So we bump the reference count to ensure it's not unloaded as long as we're around.

                    HMODULE temp = 0;
                    own_ref = GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, 
                                                 reinterpret_cast<const char *>(mod),
                                                 &temp);

                    assert(own_ref == false || mod == temp);
                }

                ~module_node()
                {
                    if (own_ref)
                        FreeLibrary(mod);
                }

                const char *name() const { return utf8_name; }
                HMODULE module() const { return mod; }

                const module_node *next_node() const { return next; }

                const char *function_spanning(const void *address) const { return symtab.function_spanning(address); }

            private:
                HMODULE mod;
                char utf8_name[3 * 32 * 1024];
                bool own_ref;
                pe_symtab symtab;
                const module_node *next;
        };

        spin_mutex modules_mutex = SPIN_MUTEX_INITIALIZER;
        const module_node *modules_head = 0;

        struct module_list_cleanup
        {
            ~module_list_cleanup()
            {
                while (modules_head)
                {
                    const module_node *next = modules_head->next_node();
                    delete modules_head;
                    modules_head = next;
                }
            }
        };

        module_list_cleanup clean_modules_list_on_exit; // keep leak detection tools happy.


        // Find or create a node associated with the specified module.
        // May return 0 on allocation failure.
        const module_node *get_node(HMODULE module)
        {
            scoped_spin_lock lk(modules_mutex);

            const module_node *node = modules_head;

            for ( ; node; node = node->next_node())
                if (node->module() == module) 
                    return node;
            
            node = new (std::nothrow) module_node(module, modules_head);
            if (node) modules_head = node;

            return node;
        }

    } // anonymous

    struct symdb::impl : ms_symdb { };

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
        if (p)
        {
            // Try using Microsoft's symbol lookup mechanism first, as the module
            // might be have been built with MSVC.

            if (p->try_lookup_function(program_counter, sink))
                return true;

            MEMORY_BASIC_INFORMATION mbinfo;
            std::memset(&mbinfo, 0, sizeof mbinfo);

            // Use VirtualQuery to find the address at which the module containing the program_counter
            // was loaded.

            if (VirtualQuery(program_counter, &mbinfo, static_cast<DWORD>(sizeof mbinfo)) == sizeof mbinfo)
            {
                const module_node *node = get_node(static_cast<HMODULE>(mbinfo.AllocationBase));;

                if (node)
                {
                    sink.on_function(program_counter, node->function_spanning(program_counter), node->name());
                    return true;
                }
            }
        }

        sink.on_function(program_counter, 0, 0);
        return false;
    }


} // dbg
