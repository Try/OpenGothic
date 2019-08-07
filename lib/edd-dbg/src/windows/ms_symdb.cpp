// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "ms_symdb.hpp"
#include "spin_mutex.hpp"
#include "memcpy_cast.hpp"
#include "dbghelp.hpp"
#include "dbg/symbols.hpp"

#include <new>

namespace dbg 
{
    namespace
    {
        // MSDN says that SymInitialize() should not be called again before a call to SymCleanup().
        // So we keep a global reference count of the number of users of the symbol database.
        
        spin_mutex symdb_users_mtx = SPIN_MUTEX_INITIALIZER;
            unsigned symdb_users = 0;

        HANDLE open_process_symbols_ref()
        {
            const HANDLE process = GetCurrentProcess();

            scoped_spin_lock lk(symdb_users_mtx);

            if (symdb_users == 0 && ms::SymInitialize(process, 0, TRUE) == 0)
                return 0;

            ++symdb_users;
            return process;
        }

        void close_process_symbols_ref(HANDLE process)
        {
            if (!process)
                return;

            scoped_spin_lock lk(symdb_users_mtx);

            if (symdb_users && --symdb_users == 0)
                ms::SymCleanup(process);
        }

        bool get_module_name_utf8(HMODULE mod, char (&name)[3 * 32 * 1024])
        {
            wchar_t module_utf16[32 * 1024] = L"";

            DWORD len = GetModuleFileNameW(mod, module_utf16, 32 * 1024);
            if (len == 32 * 1024)
                module_utf16[--len] = L'\0';

            if (len == 0 || WideCharToMultiByte(CP_UTF8, 0, module_utf16, -1, name, 3 * 32 * 1024, 0, 0) == 0)
            {
                name[0] = '\0';
                return false;
            }

            return true;
        }

        struct symbol_buffer
        {
            static const std::size_t max_sym_length = 4096;

            symbol_buffer()
            {
                std::memset(&sym, 0x00, sizeof sym);
                name_extra[0] = '\0';
                sym.SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
                sym.MaxNameLength = max_sym_length;
            }

            IMAGEHLP_SYMBOL64 &get() { return sym; }

            IMAGEHLP_SYMBOL64 sym;
            char name_extra[max_sym_length]; // extension to array of 1 at end of sym.
        };

    } // anonymous

    ms_symdb::ms_symdb() :
        process(open_process_symbols_ref())
    {
    }

    ms_symdb::~ms_symdb()
    {
        close_process_symbols_ref(process);
    }

    bool ms_symdb::try_lookup_function(const void *program_counter, symsink &sink)
    { 
        if (!process)
            return false;

        char module_utf8[3 * 32 * 1024] = "";
        const UINT_PTR pc = memcpy_cast<UINT_PTR>(program_counter);

        symbol_buffer sym_buff;
        IMAGEHLP_SYMBOL64 &symbol = sym_buff.get();
        symbol.Address = pc;

        const char *module_ptr = 0;
        const char *symbol_ptr = 0;

        {
            // MSDN says all calls to SymGetModuleBase64, SymGetSymFromAddr64, etc must be synchronized.
            scoped_spin_lock lk(symdb_users_mtx);

            const UINT_PTR module_base = static_cast<UINT_PTR>(ms::SymGetModuleBase64(process, pc) & ~UINT_PTR(0));

            if (module_base && get_module_name_utf8(memcpy_cast<HINSTANCE>(module_base), module_utf8))
                module_ptr = module_utf8;

            if (ms::SymGetSymFromAddr64(process, pc, 0, &symbol))
                symbol_ptr = symbol.Name;
        }

        if (symbol_ptr)
        {
            sink.on_function(program_counter, symbol_ptr, module_ptr);
            return true;
        }

        return false;
    }

} // dbg
