#include "ikarus.h"

#include <Tempest/Log>

using namespace Tempest;

enum {
  GothicFirstInstructionAddress =  4198400,  // 0x401000
  ContentParserAddress          = 11223232,  // 0xAB40C0
  vfxParserPointerAddress       =  9234156,  // 0x8CE6EC
  menuParserPointerAddress      =  9248360,  // 0x8D1E68
  pfxParserPointerAddress       =  9278004,  // 0x8D9234

  MEMINT_oGame_Pointer_Address        = 11208836, //0xAB0884
  MEMINT_zTimer_Address               = 10073044, //0x99B3D4
  MEMINT_oCInformationManager_Address = 11191384, //0xAAC458
  MEMINT_gameMan_Pointer_address      = 9185624,  //0x8C2958
  };

Ikarus::Ikarus(GameScript& /*owner*/, Daedalus::DaedalusVM& vm)
  /*:owner(owner)*/ {
  Log::i("DMA mod detected: Ikarus");

  // build-in data with assumed address
  versionHint = 504628679; // G2
  allocator.pin(&versionHint,   GothicFirstInstructionAddress, 4, "MEMINT_ReportVersionCheck");
  allocator.pin(&oGame_Pointer, MEMINT_oGame_Pointer_Address,  4, "oGame*");
  allocator.pin(&parserProxy,   ContentParserAddress,          sizeof(parserProxy), "zCParser proxy");

  // build-in data without assumed address
  oGame_Pointer = allocator.pin(&gameProxy, 0, sizeof(gameProxy), "oGame");

  // Note: no inline asm
  vm.registerInternalFunction("ASMINT_Push",           [](Daedalus::DaedalusVM&){});
  vm.registerInternalFunction("ASMINT_Pop",            [](Daedalus::DaedalusVM&){});
  vm.registerInternalFunction("ASMINT_MyExternal",     [](Daedalus::DaedalusVM&){});
  vm.registerInternalFunction("ASMINT_CallMyExternal", [](Daedalus::DaedalusVM&){});
  vm.registerInternalFunction("ASMINT_Init",           [](Daedalus::DaedalusVM&){});
  vm.registerInternalFunction("ASM_Open",              [](Daedalus::DaedalusVM&){});
  vm.registerInternalFunction("ASM_Close",             [](Daedalus::DaedalusVM&){});
  vm.registerInternalFunction("ASM",                   [](Daedalus::DaedalusVM&){});
  vm.registerInternalFunction("ASM_Run",               [](Daedalus::DaedalusVM&){});
  vm.registerInternalFunction("ASM_RunOnce",           [](Daedalus::DaedalusVM&){});

  vm.registerInternalFunction("MEMINT_SetupExceptionHandler", [this](Daedalus::DaedalusVM& vm){ mem_setupexceptionhandler(vm); });
  vm.registerInternalFunction("MEMINT_ReplaceSlowFunctions",  [    ](Daedalus::DaedalusVM& )  { });
  vm.registerInternalFunction("MEM_GetAddress_Init",          [this](Daedalus::DaedalusVM& vm){ mem_getaddress_init(vm); });
  vm.registerInternalFunction("MEM_PrintStackTrace",          [this](Daedalus::DaedalusVM& vm){ mem_printstacktrace_implementation(vm); });
  vm.registerInternalFunction("MEM_GetFuncPtr",               [this](Daedalus::DaedalusVM& vm){ mem_getfuncptr(vm); });
  vm.registerInternalFunction("MEM_ReplaceFunc",              [this](Daedalus::DaedalusVM& vm){ mem_replacefunc(vm); });
  vm.registerInternalFunction("MEM_SearchVobByName",          [this](Daedalus::DaedalusVM& vm){ mem_searchvobbyname(vm); });

  // ## Basic Read Write ##
  vm.registerInternalFunction("MEM_ReadInt",                  [this](Daedalus::DaedalusVM& vm){ mem_readint(vm);        });
  vm.registerInternalFunction("MEM_WriteInt",                 [this](Daedalus::DaedalusVM& vm){ mem_writeint(vm);       });
  vm.registerInternalFunction("MEM_CopyBytes",                [this](Daedalus::DaedalusVM& vm){ mem_copybytes(vm);      });
  vm.registerInternalFunction("MEM_GetCommandLine",           [this](Daedalus::DaedalusVM& vm){ mem_getcommandline(vm); });

  // ## Basic zCParser related functions ##
  vm.registerInternalFunction("MEM_GetIntAddress",   [this](Daedalus::DaedalusVM& vm){ _takeref(vm);      });
  vm.registerInternalFunction("MEM_PtrToInst",       [this](Daedalus::DaedalusVM& vm){ mem_ptrtoinst(vm); });
  vm.registerInternalFunction("_@",                  [this](Daedalus::DaedalusVM& vm){ _takeref(vm);     });
  vm.registerInternalFunction("_@s",                 [this](Daedalus::DaedalusVM& vm){ _takeref_s(vm);   });
  vm.registerInternalFunction("_@f",                 [this](Daedalus::DaedalusVM& vm){ _takeref_f(vm);   });

  // ## Preliminary MEM_Alloc and MEM_Free ##
  vm.registerInternalFunction("MEM_Alloc", [this](Daedalus::DaedalusVM& vm){ mem_alloc(vm); });
  vm.registerInternalFunction("MEM_Free",  [this](Daedalus::DaedalusVM& vm){ mem_free(vm);  });


  vm.registerInternalFunction("CALL__stdcall",       [this](Daedalus::DaedalusVM& vm){ call__stdcall(vm); });

  // vm.disAsm(vm.getDATFile().getSymbolIndexByName("MEMINT_GetAddress_Init"));
  }

bool Ikarus::isRequired(Daedalus::DaedalusVM& vm) {
  auto& dat = vm.getDATFile();
  return
      dat.hasSymbolName("MEM_InitAll") &&
      dat.hasSymbolName("MEM_ReadInt") &&
      dat.hasSymbolName("MEM_WriteInt") &&
      dat.hasSymbolName("_@") &&
      dat.hasSymbolName("_^");
  }

void Ikarus::mem_setupexceptionhandler(Daedalus::DaedalusVM&) {
  // disallow any SEH handlers and similar - that is not a script business!
  }

void Ikarus::mem_getaddress_init(Daedalus::DaedalusVM&) { /* nop */ }

void Ikarus::mem_replacefunc(Daedalus::DaedalusVM& vm) {
  int   func    = vm.popInt();
  int   dest    = vm.popInt();
  auto& sf      = vm.getDATFile().getSymbolByIndex(size_t(func));
  auto& sd      = vm.getDATFile().getSymbolByIndex(size_t(dest));

  if(sf.properties.elemProps.type!=Daedalus::EParType_Func) {
    Log::e("mem_replacefunc: invalid function ptr");
    return;
    }
  if(sd.properties.elemProps.type!=Daedalus::EParType_Func) {
    Log::e("mem_replacefunc: invalid function ptr");
    return;
    }

  Log::d("mem_replacefunc: ",sd.name," -> ",sf.name);
  //auto& bin = vm.getDATFile().rawCode();
  //bin[sd.address]->op      = Daedalus::EParOp_Jump;
  //bin[sd.address]->address = func;
  }

void Ikarus::mem_printstacktrace_implementation(Daedalus::DaedalusVM &vm) {
  Log::e("[start of stacktrace]");
  auto stk = vm.getCallStack();
  for(auto& i:stk)
    Log::e(i);
  Log::e("[end of stacktrace]");
  }

void Ikarus::mem_getfuncptr(Daedalus::DaedalusVM& vm) {
  auto  func = vm.popInt();
  auto& sym  = vm.getDATFile().getSymbolByIndex(size_t(func));
  if(sym.properties.elemProps.type!=Daedalus::EParType_Func) {
    Log::e("mem_getfuncptr: invalid function ptr");
    vm.setReturn(0);
    return;
    }
  vm.setReturn(0);
  }

void Ikarus::mem_readint(Daedalus::DaedalusVM& vm) {
  const auto address = vm.popInt();
  int32_t v = allocator.readInt(ptr32_t(address));
  vm.setReturn(v);
  }

void Ikarus::mem_writeint(Daedalus::DaedalusVM& vm) {
  auto val     = vm.popInt();
  auto address = ptr32_t(vm.popInt());
  allocator.writeInt(address,val);
  }

void Ikarus::mem_copybytes(Daedalus::DaedalusVM& vm) {
  auto size = uint32_t(vm.popInt());
  auto dst  = uint32_t(vm.popInt());
  auto src  = uint32_t(vm.popInt());
  allocator.copyBytes(src,dst,size);
  }

void Ikarus::mem_getcommandline(Daedalus::DaedalusVM &vm) {
  // TODO: return real commandline
  vm.setReturn("");
  }

void Ikarus::mem_searchvobbyname(Daedalus::DaedalusVM& vm) {
  // see ZS_STAND_PEDRO_LOOP in VarusBikerEdition mod
  auto name = vm.popString();
  (void)name;
  vm.setReturn(0); // NULL-like
  }

void Ikarus::call__stdcall(Daedalus::DaedalusVM& vm) {
  auto address = vm.popInt();
  (void)address;
  }

void Ikarus::mem_ptrtoinst(Daedalus::DaedalusVM& vm) {
  auto address = vm.popInt();
  // TODO: return an instance
  vm.setReturn(address);
  }

void Ikarus::_takeref(Daedalus::DaedalusVM& vm) {
  auto val = vm.popInt();
  (void)val;
  vm.setReturn(0x1);
  }

void Ikarus::_takeref_s(Daedalus::DaedalusVM& vm) {
  auto val = vm.popString();
  (void)val;
  vm.setReturn(0x1);
  }

void Ikarus::_takeref_f(Daedalus::DaedalusVM& vm) {
  auto val = vm.popFloat();
  (void)val;
  vm.setReturn(0x1);
  }

void Ikarus::mem_alloc(Daedalus::DaedalusVM& vm) {
  auto amount = vm.popInt();
  auto ptr    = allocator.alloc(uint32_t(amount));
  vm.setReturn(int32_t(ptr));
  }

void Ikarus::mem_free(Daedalus::DaedalusVM& vm) {
  auto ptr = uint32_t(vm.popInt());
  allocator.free(Mem32::ptr32_t(ptr));
  }
