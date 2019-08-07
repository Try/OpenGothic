// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef DBGHELP_HPP_0123_28082012
#define DBGHELP_HPP_0123_28082012

#include <windows.h>

extern "C"
{
    // Some Windows types and structures not defined in the MinGW windows headers:

    // http://msdn.microsoft.com/en-us/library/windows/desktop/ms680203%28v=vs.85%29.aspx
    typedef struct _IMAGEHLP_SYMBOL64 
    {
        DWORD   SizeOfStruct;
        DWORD64 Address;
        DWORD   Size;
        DWORD   Flags;
        DWORD   MaxNameLength;
        CHAR    Name[1]; // MSDN says this is a TCHAR[1]. It lies, even with ANSI/Unicode fudgery in effect.
    } 
    IMAGEHLP_SYMBOL64, *PIMAGEHLP_SYMBOL64;

    // http://msdn.microsoft.com/en-us/library/windows/desktop/ms679272%28v=vs.85%29.aspx
    enum ADDRESS_MODE
    {
        AddrMode1616,
        AddrMode1632,
        AddrModeReal,
        AddrModeFlat
    };

    // http://msdn.microsoft.com/en-us/library/windows/desktop/ms679272%28v=vs.85%29.aspx
    typedef struct _tagADDRESS64 
    {
        DWORD64      Offset;
        WORD         Segment;
        ADDRESS_MODE Mode;
    } 
    ADDRESS64, *LPADDRESS64;

    // http://msdn.microsoft.com/en-us/library/windows/desktop/ms680346%28v=vs.85%29.aspx
    typedef struct _KDHELP64 
    {
        DWORD64 Thread;
        DWORD   ThCallbackStack;
        DWORD   ThCallbackBStore;
        DWORD   NextCallback;
        DWORD   FramePointer;
        DWORD64 KiCallUserMode;
        DWORD64 KeUserCallbackDispatcher;
        DWORD64 SystemRangeStart;
        DWORD64 KiUserExceptionDispatcher;
        DWORD64 StackBase;
        DWORD64 StackLimit;
        DWORD64 Reserved[5];
    }
    KDHELP64, *PKDHELP64;

    // http://msdn.microsoft.com/en-us/library/windows/desktop/ms680559%28v=vs.85%29.aspx
    typedef BOOL (CALLBACK *PREAD_PROCESS_MEMORY_ROUTINE64)(HANDLE, DWORD64, PVOID, DWORD, LPDWORD);

    // http://msdn.microsoft.com/en-us/library/windows/desktop/ms679354%28v=vs.85%29.aspx
    typedef PVOID (CALLBACK *PFUNCTION_TABLE_ACCESS_ROUTINE64)(HANDLE, DWORD64);

    // http://msdn.microsoft.com/en-us/library/windows/desktop/ms679361%28v=vs.85%29.aspx
    typedef DWORD64 (CALLBACK *PGET_MODULE_BASE_ROUTINE64)(HANDLE, DWORD64);

    // http://msdn.microsoft.com/en-us/library/windows/desktop/ms681399%28v=vs.85%29.aspx
    typedef DWORD64 (CALLBACK *PTRANSLATE_ADDRESS_ROUTINE64)(HANDLE, HANDLE, LPADDRESS64);

    // http://msdn.microsoft.com/en-us/library/windows/desktop/ms680646%28v=vs.85%29.aspx
    typedef struct _tagSTACKFRAME64 
    {
        ADDRESS64 AddrPC;
        ADDRESS64 AddrReturn;
        ADDRESS64 AddrFrame;
        ADDRESS64 AddrStack;
        ADDRESS64 AddrBStore;
        PVOID     FuncTableEntry;
        DWORD64   Params[4];
        BOOL      Far;
        BOOL      Virtual;
        DWORD64   Reserved[3];
        KDHELP64  KdHelp;
    } 
    STACKFRAME64, *LPSTACKFRAME64;
}

namespace dbg 
{
    namespace ms 
    {
        // Wrappers around functions from dbghelp.dll.
        // They load the dll dynamically as required and take a lock
        // internally to ensure proper threaded access as demanded by 
        // MSDN.
        //
        // Of course, some other code might be using dbghelp functions
        // independently, but there's not a lot we can do synchronize
        // correctly with that.

        BOOL WINAPI SymInitialize(HANDLE, PCSTR, BOOL);
        BOOL WINAPI SymCleanup(HANDLE) ;
        DWORD64 WINAPI SymGetModuleBase64(HANDLE, DWORD64);
        BOOL WINAPI SymGetSymFromAddr64(HANDLE, DWORD64, PDWORD64, PIMAGEHLP_SYMBOL64);
        PVOID WINAPI SymFunctionTableAccess64(HANDLE, DWORD64);
    
        BOOL WINAPI StackWalk64(DWORD, 
                                HANDLE, 
                                HANDLE, 
                                LPSTACKFRAME64, 
                                PVOID, 
                                PREAD_PROCESS_MEMORY_ROUTINE64, 
                                PFUNCTION_TABLE_ACCESS_ROUTINE64, 
                                PGET_MODULE_BASE_ROUTINE64, 
                                PTRANSLATE_ADDRESS_ROUTINE64);
    } // ms

} // dbg

#endif // DBGHELP_HPP_0123_28082012
