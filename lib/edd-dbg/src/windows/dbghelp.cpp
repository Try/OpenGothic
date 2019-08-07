// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "dbghelp.hpp"
#include "spin_mutex.hpp"
#include "dll.hpp"

#if !defined(DBG_RECENT_DBGHELP_DLL)
#   define DBG_RECENT_DBGHELP_DLL "dbghelp.dll"
#endif

namespace dbg 
{
    namespace ms 
    {
        namespace
        {
            // We load dbghelp explicitly so that we get the latest version if the software
            // is installed with a dbghelp redistributable, else the system version.
            spin_mutex dbghelp_mtx = SPIN_MUTEX_INITIALIZER;

                dll dbghelp;

                BOOL (WINAPI *SymInitialize_)(HANDLE, PCSTR, BOOL) = 0;
                BOOL (WINAPI *SymCleanup_)(HANDLE) = 0;
                DWORD64 (WINAPI *SymGetModuleBase64_)(HANDLE, DWORD64) = 0;
                BOOL (WINAPI *SymGetSymFromAddr64_)(HANDLE, DWORD64, PDWORD64, PIMAGEHLP_SYMBOL64) = 0;
                PVOID (WINAPI *SymFunctionTableAccess64_)(HANDLE hProcess, DWORD64 AddrBase) = 0;

                BOOL (WINAPI *StackWalk64_)(DWORD, 
                                            HANDLE, 
                                            HANDLE, 
                                            LPSTACKFRAME64, 
                                            PVOID, 
                                            PREAD_PROCESS_MEMORY_ROUTINE64, 
                                            PFUNCTION_TABLE_ACCESS_ROUTINE64, 
                                            PGET_MODULE_BASE_ROUTINE64, 
                                            PTRANSLATE_ADDRESS_ROUTINE64);


            bool ensure_initialized(const scoped_spin_lock &)
            {
                if (dbghelp.loaded())
                    return true;

                dll temp(DBG_RECENT_DBGHELP_DLL);
                if (!temp.loaded())
                    return false;

                SymInitialize_ = temp.find_function("SymInitialize");
                SymCleanup_ = temp.find_function("SymCleanup");
                SymGetModuleBase64_ = temp.find_function("SymGetModuleBase64");
                SymGetSymFromAddr64_ = temp.find_function("SymGetSymFromAddr64");
                SymFunctionTableAccess64_ = temp.find_function("SymFunctionTableAccess64");
                StackWalk64_ = temp.find_function("StackWalk64");

                if (!SymInitialize_ || 
                    !SymCleanup_ || 
                    !SymGetModuleBase64_ || 
                    !SymGetSymFromAddr64_ || 
                    !SymFunctionTableAccess64_ ||
                    !StackWalk64_)
                {
                    return false;
                }

                temp.swap(dbghelp);
                return true;
            }

        } // anonymous

        BOOL WINAPI SymInitialize(HANDLE process, PCSTR search, BOOL invade)
        {
            const scoped_spin_lock lk(dbghelp_mtx);
            return ensure_initialized(lk) ? SymInitialize_(process, search, invade) : FALSE;
        }

        BOOL WINAPI SymCleanup(HANDLE process)
        {
            const scoped_spin_lock lk(dbghelp_mtx);
            return ensure_initialized(lk) ? SymCleanup_(process) : FALSE;
        }

        DWORD64 WINAPI SymGetModuleBase64(HANDLE process, DWORD64 addr)
        {
            const scoped_spin_lock lk(dbghelp_mtx);
            return ensure_initialized(lk) ? SymGetModuleBase64_(process, addr) : 0;
        }

        BOOL WINAPI SymGetSymFromAddr64(HANDLE process, DWORD64 address, PDWORD64 displacement, PIMAGEHLP_SYMBOL64 sym)
        {
            const scoped_spin_lock lk(dbghelp_mtx);
            return ensure_initialized(lk) ? SymGetSymFromAddr64_(process, address, displacement, sym) : FALSE;
        }

        PVOID WINAPI SymFunctionTableAccess64(HANDLE process, DWORD64 addrbase)
        {
            const scoped_spin_lock lk(dbghelp_mtx);
            return ensure_initialized(lk) ? SymFunctionTableAccess64_(process, addrbase) : 0;
        }
    
        BOOL WINAPI StackWalk64(DWORD machine,
                                HANDLE process, 
                                HANDLE thread, 
                                LPSTACKFRAME64 frame, 
                                PVOID ctx, 
                                PREAD_PROCESS_MEMORY_ROUTINE64 read_func,
                                PFUNCTION_TABLE_ACCESS_ROUTINE64 ft_access_func, 
                                PGET_MODULE_BASE_ROUTINE64 modbase_func, 
                                PTRANSLATE_ADDRESS_ROUTINE64 xl8_func)
        {
            {
                const scoped_spin_lock lk(dbghelp_mtx);
                if (!ensure_initialized(lk))
                    return FALSE;
            }

            return StackWalk64_(machine, process, thread, 
                                frame, ctx, 
                                read_func, ft_access_func, modbase_func, xl8_func);
        }

    } // ms

} // dbg
