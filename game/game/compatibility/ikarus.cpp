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

Ikarus::Ikarus(GameScript& /*owner*/, phoenix::vm& vm) : vm(vm)
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
  // TODO: Make sure this actually works!
  vm.override_function("ASMINT_Push",           [](){});
  vm.override_function("ASMINT_Pop",            [](){});
  vm.override_function("ASMINT_MyExternal",     [](){});
  vm.override_function("ASMINT_CallMyExternal", [](){});
  vm.override_function("ASMINT_Init",           [](){});
  vm.override_function("ASM_Open",              [](){});
  vm.override_function("ASM_Close",             [](){});
  vm.override_function("ASM",                   [](){});
  vm.override_function("ASM_Run",               [](){});
  vm.override_function("ASM_RunOnce",           [](){});

  vm.override_function("MEMINT_SetupExceptionHandler", [this](){ mem_setupexceptionhandler(); });
  vm.override_function("MEMINT_ReplaceSlowFunctions",  [    ](){ });
  vm.override_function("MEM_GetAddress_Init",          [this](){ mem_getaddress_init(); });
  vm.override_function("MEM_PrintStackTrace",          [this](){ mem_printstacktrace_implementation(); });
  vm.override_function("MEM_GetFuncPtr",               [this](int func){ return mem_getfuncptr(func); });
  vm.override_function("MEM_ReplaceFunc",              [this](int dest, int func){ mem_replacefunc(dest, func); });
  vm.override_function("MEM_SearchVobByName",          [this](std::string_view name){ return mem_searchvobbyname(name); });

  // ## Basic Read Write ##
  vm.override_function("MEM_ReadInt",                  [this](int address){ return mem_readint(address);        });
  vm.override_function("MEM_WriteInt",                 [this](int address, int val){ mem_writeint(address, val);       });
  vm.override_function("MEM_CopyBytes",                [this](int src, int dst, int size){ mem_copybytes(src, dst, size);      });
  vm.override_function("MEM_GetCommandLine",           [this](){ return mem_getcommandline(); });

  // ## Basic zCParser related functions ##
  vm.override_function("MEM_GetIntAddress",   [this](int val){ return _takeref(val);      });
  vm.override_function("MEM_PtrToInst",       [this](int address){ return mem_ptrtoinst(address); });
  vm.override_function("_@",                  [this](int val){ return _takeref(val);     });
  vm.override_function("_@s",                 [this](std::string_view val){ return _takeref_s(val);   });
  vm.override_function("_@f",                 [this](float val){ return _takeref_f(val);   });

  // ## Preliminary MEM_Alloc and MEM_Free ##
  vm.override_function("MEM_Alloc", [this](int amount){ mem_alloc(amount); });
  vm.override_function("MEM_Free",  [this](int address){ mem_free(address);  });


  vm.override_function("CALL__stdcall", [this](int address){ call__stdcall(address); });

  // vm.disAsm(vm.getDATFile().getSymbolIndexByName("MEMINT_GetAddress_Init"));
  }

bool Ikarus::isRequired(phoenix::vm& vm) {
  return
      vm.find_symbol_by_name("MEM_InitAll") != nullptr &&
      vm.find_symbol_by_name("MEM_ReadInt") != nullptr &&
      vm.find_symbol_by_name("MEM_WriteInt") != nullptr &&
      vm.find_symbol_by_name("_@") != nullptr &&
      vm.find_symbol_by_name("_^") != nullptr;
  }

void Ikarus::mem_setupexceptionhandler() {
  // disallow any SEH handlers and similar - that is not a script business!
  }

void Ikarus::mem_getaddress_init() { /* nop */ }

void Ikarus::mem_replacefunc(int dest, int func) {
  auto* sf      = vm.find_symbol_by_index(uint32_t(func));
  auto* sd      = vm.find_symbol_by_index(uint32_t(dest));

  if(sf == nullptr || sf->type() != phoenix::datatype::function) {
    Log::e("mem_replacefunc: invalid function ptr");
    return;
    }
  if(sd == nullptr || sd->type() != phoenix::datatype::function) {
    Log::e("mem_replacefunc: invalid function ptr");
    return;
    }

  Log::d("mem_replacefunc: ",sd->name()," -> ",sf->name());
  //auto& bin = vm.getDATFile().rawCode();
  //bin[sd.address]->op      = EParOp_Jump;
  //bin[sd.address]->address = func;
  }

void Ikarus::mem_printstacktrace_implementation() {
  Log::e("[start of stacktrace]");
  vm.print_stack_trace();
  Log::e("[end of stacktrace]");
  }

int Ikarus::mem_getfuncptr(int func) {
  auto* sym  = vm.find_symbol_by_index(uint32_t(func));
  if(sym == nullptr || sym->type() != phoenix::datatype::function) {
    Log::e("mem_getfuncptr: invalid function ptr");
    return 0;
    }
    return 0;
  }

int Ikarus::mem_readint(int address) {
  return allocator.readInt(ptr32_t(address));
  }

void Ikarus::mem_writeint(int address, int val) {
  allocator.writeInt(uint32_t(address),val);
  }

void Ikarus::mem_copybytes(int src, int dst, int size) {
  allocator.copyBytes(ptr32_t(src),ptr32_t(dst),ptr32_t(size));
  }

std::string Ikarus::mem_getcommandline() {
  // TODO: return real commandline
  return "";
  }

int Ikarus::mem_searchvobbyname(std::string_view name) {
  // see ZS_STAND_PEDRO_LOOP in VarusBikerEdition mod
  (void)name;
  return 0; // NULL-like
  }

void Ikarus::call__stdcall(int address) {
  (void)address;
  }

int Ikarus::mem_ptrtoinst(int address) {
  return address;
  }

int Ikarus::_takeref(int val) {
  (void)val;
  return 0x1;
  }

int Ikarus::_takeref_s(std::string_view val) {
  (void)val;
  return 0x1;
  }

int Ikarus::_takeref_f(float val) {
  (void)val;
  return 0x1;
  }

int Ikarus::mem_alloc(int amount) {
  auto ptr    = allocator.alloc(uint32_t(amount));
  return int32_t(ptr);
  }

void Ikarus::mem_free(int ptr) {
  allocator.free(Mem32::ptr32_t(ptr));
  }
