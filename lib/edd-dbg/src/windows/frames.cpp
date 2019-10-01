// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "dbg/frames.hpp"
#include "memcpy_cast.hpp"
#include "dbghelp.hpp"
#include "dll.hpp"

#include <cstring>

#include <windows.h>

#if !defined(_M_AMD64) && !defined(_M_IX86)
#   error "unsupported architecture :("
#endif

namespace dbg 
{
    bool walk_frames(frame_sink &sink)
    {
        unsigned skip = 1;

        STACKFRAME64 frame;
        std::memset(&frame, 0, sizeof frame);

        CONTEXT context;
        std::memset(&context, 0, sizeof context);
        context.ContextFlags = CONTEXT_FULL;

        // RtlCaptureContext() crashes with heavy optimizations on MinGW 4.7.
#if defined(__MINGW32__) && !defined(_M_AMD64)
        DWORD eip_val = 0;
        DWORD esp_val = 0;
        DWORD ebp_val = 0;

        __asm__ __volatile__ ("call 1f\n\t" "1: pop %0" : "=g"(eip_val));
        __asm__ __volatile__ ("movl %%esp, %0" : "=g"(esp_val));
        __asm__ __volatile__ ("movl %%ebp, %0" : "=g"(ebp_val));

        context.Eip = eip_val;
        context.Esp = esp_val;
        context.Ebp = ebp_val;
        skip = 2;
#else
        const dll kernel32("kernel32.dll");
        if (!kernel32.loaded()) return false;

        void (WINAPI *RtlCaptureContext_)(CONTEXT*) = kernel32.find_function("RtlCaptureContext");
        if (!RtlCaptureContext_) return false;

        RtlCaptureContext_(&context);
#endif

#if defined(_M_AMD64)
        frame.AddrPC.Offset = context.Rip;
        frame.AddrPC.Mode = AddrModeFlat;
        frame.AddrStack.Offset = context.Rsp;
        frame.AddrStack.Mode = AddrModeFlat;
        frame.AddrFrame.Offset = context.Rbp;
        frame.AddrFrame.Mode = AddrModeFlat;

        const DWORD machine = 0x8664; // IMAGE_FILE_MACHINE_AMD64;
#else
        frame.AddrPC.Offset = context.Eip;
        frame.AddrPC.Mode = AddrModeFlat;
        frame.AddrStack.Offset = context.Esp;
        frame.AddrStack.Mode = AddrModeFlat;
        frame.AddrFrame.Offset = context.Ebp;
        frame.AddrFrame.Mode = AddrModeFlat;

        const DWORD machine = 0x014c; // IMAGE_FILE_MACHINE_I386;
#endif

        HANDLE process = GetCurrentProcess();
        HANDLE thread = GetCurrentThread();

        unsigned level = 0;

        while (ms::StackWalk64(machine, 
                               process, thread, 
                               &frame, &context, 
                               0, ms::SymFunctionTableAccess64, ms::SymGetModuleBase64, 0))
        {
            if (skip)
            {
                --skip;
                continue; // don't capture current frame
            }

            const UINT_PTR pc = static_cast<UINT_PTR>(frame.AddrPC.Offset & ~UINT_PTR(0));

            if (!sink.on_frame(level++, memcpy_cast<const void*>(pc))) 
                return false;
        }

        return true;
    }

} // dbg
