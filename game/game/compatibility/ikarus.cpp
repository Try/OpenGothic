#include "ikarus.h"

#include <Tempest/Application>
#include <Tempest/Log>

#include <cstddef>
#include <phoenix/vobs/misc.hh>

#include "game/gamescript.h"

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
  MEMINT_gameMan_Pointer_Address      = 9185624,  //0x8C2958

  MEMINT_SENDTOSPY_IMPLEMENTATION_ZERR_G2 = 9231568,
  };

struct Ikarus::memory_instance : public phoenix::instance {
  explicit memory_instance(ptr32_t address):address(address){}
  ptr32_t address;
  };

Ikarus::Ikarus(GameScript& /*owner*/, phoenix::vm& vm) : vm(vm) {
  Log::i("DMA mod detected: Ikarus");

  // built-in data with assumed address
  versionHint = 504628679; // G2
  allocator.pin(&versionHint,   GothicFirstInstructionAddress,           4, "MEMINT_ReportVersionCheck");
  allocator.pin(&oGame_Pointer, MEMINT_oGame_Pointer_Address,            4, "oGame*");
  allocator.pin(&parserProxy,   ContentParserAddress,                    sizeof(parserProxy), "zCParser proxy");

  // built-in data without assumed address
  oGame_Pointer = allocator.pin(&gameProxy, 0, sizeof(gameProxy), "oGame");

  symbolsPtr = allocator.alloc(uint32_t(vm.symbols().size() * sizeof(phoenix::symbol)));

  // ## Builtin instances
  allocator.alloc(MEMINT_SENDTOSPY_IMPLEMENTATION_ZERR_G2, 64, "ZERROR");

  // Note: no inline asm
  // TODO: Make sure this actually works!
  vm.override_function("ASMINT_Push",           [](int){});
  vm.override_function("ASMINT_Pop",            []() -> int { return 0; });
  vm.override_function("ASMINT_MyExternal",     [](){});
  vm.override_function("ASMINT_CallMyExternal", [](){});
  vm.override_function("ASMINT_Init",           [](){});
  vm.override_function("ASM_Open",              [](int){});
  vm.override_function("ASM_Close",             []() -> int { return 0; });
  vm.override_function("ASM",                   [](int,int){});
  vm.override_function("ASM_Run",               [](int){});
  vm.override_function("ASM_RunOnce",           [](){});

  vm.override_function("MEMINT_SetupExceptionHandler", [this](){ mem_setupexceptionhandler(); });
  vm.override_function("MEMINT_ReplaceSlowFunctions",  [    ](){ });
  vm.override_function("MEM_GetAddress_Init",          [this](){ mem_getaddress_init(); });
  vm.override_function("MEM_PrintStackTrace",          [this](){ mem_printstacktrace_implementation(); });
  vm.override_function("MEM_GetFuncPtr",               [this](int func){ return mem_getfuncptr(func); });
  vm.override_function("MEM_GetFuncId",                [this](int func){ return mem_getfuncid(func); });
  vm.override_function("MEM_ReplaceFunc",              [this](int dest, int func){ mem_replacefunc(dest, func); });
  vm.override_function("MEM_SearchVobByName",          [this](std::string_view name){ return mem_searchvobbyname(name); });
  vm.override_function("MEM_GetSymbolIndex",           [this](std::string_view name){ return mem_getsymbolindex(name); });
  vm.override_function("MEM_GetSymbolByIndex",         [this](int index){ return mem_getsymbolbyindex(index); });
  vm.override_function("MEM_GetSystemTime",            [this](){ return mem_getsystemtime(); });

  // ## Basic Read Write ##
  vm.override_function("MEM_ReadInt",                  [this](int address){ return mem_readint(address);                  });
  vm.override_function("MEM_WriteInt",                 [this](int address, int val){ mem_writeint(address, val);          });
  vm.override_function("MEM_CopyBytes",                [this](int src, int dst, int size){ mem_copybytes(src, dst, size); });
  vm.override_function("MEM_ReadString",               [this](int sym){ return mem_readstring(sym);           });

  vm.override_function("MEM_GetCommandLine",           [this](){ return mem_getcommandline(); });

  // ## Basic zCParser related functions ##
  vm.override_function("MEM_GetIntAddress",   [this](int val)              { return _takeref(val);      });
  vm.override_function("MEM_PtrToInst",       [this](int address)          { return mem_ptrtoinst(ptr32_t(address)); });
  vm.override_function("_@",                  [this](int val)              { return _takeref(val);     });
  vm.override_function("_@s",                 [this](std::string_view val) { return _takeref_s(val);   });
  vm.override_function("_@f",                 [this](float val)            { return _takeref_f(val);   });
  vm.override_function("_^",                  [this](int address)          { return mem_ptrtoinst(ptr32_t(address)); });

  // ## Preliminary MEM_Alloc and MEM_Free ##
  vm.override_function("MEM_Alloc", [this](int amount ) -> int { return mem_alloc(amount);  });
  vm.override_function("MEM_Free",  [this](int address)        { mem_free(address);  });
  vm.register_memory_trap([this](int32_t val, size_t i, const std::shared_ptr<phoenix::instance>& inst, phoenix::symbol& sym) { mem_trap_i32( val, i, inst, sym); });
  vm.register_memory_trap([this](size_t i, const std::shared_ptr<phoenix::instance>& inst, phoenix::symbol& sym) { return mem_trap_i32(i, inst, sym); });

  // ## Control-flow ##
  vm.override_function("repeat", [this](phoenix::vm& vm) { return repeat(vm);    });
  vm.register_loop_trap([this](phoenix::symbol& i)       { return loop_trap(&i); });

  // ## Strings
  vm.override_function("STR_SubStr", [this](std::string_view str, int start, int count){ return str_substr(str,start,count); });
  vm.override_function("STR_Len",    [this](std::string_view str){ return str_len(str); });

  vm.override_function("CALL__stdcall",  [this](int address){ call__stdcall(address); });
  vm.override_function("HASH",           [this](int v){ return hash(v); });

  vm.override_function("LoadLibrary", [](std::string_view name){
    Log::d("suppress LoadLibrary: ",name);
    return 0x1;
    });
  vm.override_function("GetProcAddress", [    ](int hmodule, std::string_view name){
    Log::d("suppress GetProcAddress: ",name);
    return 0x1;
    });

  // ## ZSpy ##
  vm.override_function("MEM_SendToSpy", [this](int cat, std::string_view msg){ return mem_sendtospy(cat,msg); });

  // ## Constants
  if(auto v = vm.find_symbol_by_name("CurrSymbolTableLength")) {
    v->set_int(int(vm.symbols().size()));
    }
  }

bool Ikarus::isRequired(phoenix::script& vm) {
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

std::string Ikarus::mem_getcommandline() {
  // TODO: return real commandline
  return "";
  }

void Ikarus::mem_sendtospy(int cat, std::string_view msg) {
  Log::d("[zpy]: ", msg);
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

int Ikarus::mem_getfuncid(int func) {
  auto* sym = vm.find_symbol_by_index(uint32_t(func));
  Log::d("mem_getfuncid: ", (sym!=nullptr) ? sym->name() : "");
  return func;
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

std::string Ikarus::mem_readstring(int address) {
  const size_t symIndex = (size_t(address) - symbolsPtr)/sizeof(phoenix::symbol);
  const size_t sub      = (size_t(address) - symbolsPtr)%sizeof(phoenix::symbol);
  if(symIndex>=vm.symbols().size() || sub!=0)
    return "";
  auto& s = vm.symbols()[symIndex];
  return s.name();
  }

int Ikarus::mem_searchvobbyname(std::string_view name) {
  // see ZS_STAND_PEDRO_LOOP in VarusBikerEdition mod
  (void)name;
  return 0; // NULL-like
  }

int Ikarus::mem_getsymbolindex(std::string_view name) {
  auto sym = vm.find_symbol_by_name(name);
  if(sym==nullptr)
    return 0;// NULL-like
  return int32_t(sym->index());
  }

int Ikarus::mem_getsymbolbyindex(int index) {
  return int(ptr32_t(symbolsPtr + size_t(index)*sizeof(phoenix::symbol)));
  }

int Ikarus::mem_getsystemtime() {
  return int(Application::tickCount());
  }

std::string Ikarus::str_substr(std::string_view str, int start, int count) {
  if(start < 0 || (count < 0)) {
    Log::e("STR_SubStr: start and count may not be negative.");
    return "";
    };

  if(size_t(start)>=str.size()) {
    Log::e("STR_SubStr: The desired start of the substring lies beyond the end of the string.");
    return "";
    };

  return std::string(str.substr(size_t(start), size_t(count)));
  }

int Ikarus::str_len(std::string_view str) {
  return int(str.length());
  }

void Ikarus::call__stdcall(int address) {
  (void)address;
  }

int Ikarus::hash(int x) {
  // original ikrus has uses pointe to stack, so it get's dicy
  // TODO: CRC32 to match behaviour
  return int(std::hash<int>()(x));
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
  auto ptr = allocator.alloc(uint32_t(amount));
  return int32_t(ptr);
  }

void Ikarus::mem_free(int ptr) {
  allocator.free(Mem32::ptr32_t(ptr));
  }

std::shared_ptr<phoenix::instance> Ikarus::mem_ptrtoinst(ptr32_t address) {
  return std::make_shared<memory_instance>(address);
  }

void Ikarus::mem_trap_i32(int32_t v, size_t i, const std::shared_ptr<phoenix::instance>& inst, phoenix::symbol& sym) {
  assert(i==0); // TODO: arrays
  memory_instance& m = dynamic_cast<memory_instance&>(*inst);
  ptr32_t addr = m.address + ptr32_t(sym.offset_as_member()) + ptr32_t(i*4u);
  allocator.writeInt(addr, v);
  }

int32_t Ikarus::mem_trap_i32(size_t i, const std::shared_ptr<phoenix::instance>& inst, phoenix::symbol& sym) {
  assert(i==0); // TODO: arrays
  memory_instance& m = dynamic_cast<memory_instance&>(*inst);
  ptr32_t addr = m.address + ptr32_t(sym.offset_as_member()) + ptr32_t(i*4u);
  return allocator.readInt(addr);
  }

phoenix::naked_call Ikarus::repeat(phoenix::vm& vm) {
  const int        len = vm.pop_int();
  phoenix::symbol* i   = std::get<phoenix::symbol*>(vm.pop_reference());
  // Log::i("repeat: ", i->get_int(), " < ", len);
  if(i->get_int() < len) {
    loop_start.push_back({vm.pc() - 5*2, i});
    return phoenix::naked_call();
    }

  auto end = vm.find_symbol_by_name("END");
  auto rep = vm.find_symbol_by_name("REPEAT");

  int depth = 1;
  for(uint32_t i=vm.pc(); i<vm.size(); ) {
    const auto inst = vm.instruction_at(i);
    if(inst.op==phoenix::opcode::pushv && vm.find_symbol_by_index(inst.symbol)==end) {
      depth--;
      }
    else if(inst.op==phoenix::opcode::bl && vm.find_symbol_by_index(inst.symbol)==rep) {
      depth++;
      }
    i += inst.size;
    if(depth==0) {
      auto trap = vm.instruction_at(vm.pc());
      vm.unsafe_jump(i-trap.size);
      break;
      }
    }
  return phoenix::naked_call();
  }

void Ikarus::loop_trap(phoenix::symbol* i) {
  // Log::i("end");
  const auto ret = loop_start.back();
  loop_start.pop_back();
  ret.i->set_int(ret.i->get_int()+1);

  auto trap = vm.instruction_at(vm.pc());
  vm.unsafe_jump(ret.pc-trap.size);
  }
