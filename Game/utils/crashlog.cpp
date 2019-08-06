#include "crashlog.h"

#include <windows.h>
#include <dbghelp.h>
#include <iostream>

CrashLog::CrashLog() {
  }

void CrashLog::dumpStack() {
  HANDLE process = GetCurrentProcess();
  HANDLE thread  = GetCurrentThread();

  CONTEXT context={};
  context.ContextFlags = CONTEXT_FULL;
  RtlCaptureContext(&context);

  SymInitialize(process, nullptr, TRUE);

  DWORD image;
  STACKFRAME64 stackframe;
  ZeroMemory(&stackframe, sizeof(STACKFRAME64));

#ifdef _M_IX86
  image = IMAGE_FILE_MACHINE_I386;
  stackframe.AddrPC.Offset = context.Eip;
  stackframe.AddrPC.Mode = AddrModeFlat;
  stackframe.AddrFrame.Offset = context.Ebp;
  stackframe.AddrFrame.Mode = AddrModeFlat;
  stackframe.AddrStack.Offset = context.Esp;
  stackframe.AddrStack.Mode = AddrModeFlat;
#elif _M_X64
  image = IMAGE_FILE_MACHINE_AMD64;
  stackframe.AddrPC.Offset = context.Rip;
  stackframe.AddrPC.Mode = AddrModeFlat;
  stackframe.AddrFrame.Offset = context.Rsp;
  stackframe.AddrFrame.Mode = AddrModeFlat;
  stackframe.AddrStack.Offset = context.Rsp;
  stackframe.AddrStack.Mode = AddrModeFlat;
#elif _M_IA64
  image = IMAGE_FILE_MACHINE_IA64;
  stackframe.AddrPC.Offset = context.StIIP;
  stackframe.AddrPC.Mode = AddrModeFlat;
  stackframe.AddrFrame.Offset = context.IntSp;
  stackframe.AddrFrame.Mode = AddrModeFlat;
  stackframe.AddrBStore.Offset = context.RsBSP;
  stackframe.AddrBStore.Mode = AddrModeFlat;
  stackframe.AddrStack.Offset = context.IntSp;
  stackframe.AddrStack.Mode = AddrModeFlat;
#endif

  for(size_t i=0; i<30; i++) {
    BOOL result = StackWalk64(
          image, process, thread,
          &stackframe, &context, nullptr,
          SymFunctionTableAccess64, SymGetModuleBase64, nullptr);

    if(!result)
      break;

    char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)]={};
    PSYMBOL_INFO symbol  = reinterpret_cast<PSYMBOL_INFO>(buffer);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen   = MAX_SYM_NAME;

    DWORD64 displacement = 0;
    if(SymFromAddr(process, stackframe.AddrPC.Offset, &displacement, symbol)) {
      std::cout << "[" << i << "] " << symbol->Name << std::endl;
      } else {
      std::cout << "[" << i << "] ???" << std::endl;
      }

    }

  SymCleanup(process);
  }
