#include "ikarus.h"

#include <Tempest/Application>
#include <Tempest/Log>

#include <charconv>
#include <cstddef>
#include <phoenix/vobs/misc.hh>

#include "game/gamescript.h"
#include "gothic.h"

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

  OCNPC__ENABLE_EQUIPBESTWEAPONS = 7626662, //0x745FA6
  };

void Ikarus::memory_instance::set_int(const phoenix::symbol &sym, uint16_t index, int32_t value) {
  ptr32_t addr = address + ptr32_t(sym.offset_as_member()) + ptr32_t(index*sym.class_size());
  owner.allocator.writeInt(addr, value);
  }

int32_t Ikarus::memory_instance::get_int(const phoenix::symbol& sym, uint16_t index) const {
  ptr32_t addr = address + ptr32_t(sym.offset_as_member()) + ptr32_t(index * sym.class_size());
  int32_t v = owner.allocator.readInt(addr);
  return v;
  }

void Ikarus::memory_instance::set_float(const phoenix::symbol& sym, uint16_t index, float value) {
  ptr32_t addr = address + ptr32_t(sym.offset_as_member()) + ptr32_t(index*sym.class_size());
  owner.allocator.writeInt(addr, *reinterpret_cast<const int32_t*>(&value));
  }

float Ikarus::memory_instance::get_float(const phoenix::symbol& sym, uint16_t index) const {
  ptr32_t addr = address + ptr32_t(sym.offset_as_member()) + ptr32_t(index * sym.class_size());
  int32_t v = owner.allocator.readInt(addr);

  float f = 0;
  std::memcpy(&f, &v, 4);
  return f;
  }

void Ikarus::memory_instance::set_string(const phoenix::symbol& sym, uint16_t index, std::string_view value) {
  ptr32_t addr = address + ptr32_t(sym.offset_as_member()) + ptr32_t(index*sym.class_size());
  (void)addr;
  Log::d("memory_instance: ", sym.name());
  // allocator.writeInt(addr, 0);
  }

const std::string& Ikarus::memory_instance::get_string(const phoenix::symbol& sym, uint16_t index) const {
  ptr32_t addr = address + ptr32_t(sym.offset_as_member()) + ptr32_t(index*sym.class_size());

  Log::d("memory_instance: ", sym.name());
  (void)addr;
  static std::string empty;
  return empty;
  }


Ikarus::Ikarus(GameScript& /*owner*/, phoenix::vm& vm) : vm(vm) {
  Log::i("DMA mod detected: Ikarus");

  // built-in data with assumed address
  versionHint = 504628679; // G2
  allocator.pin(&versionHint,   GothicFirstInstructionAddress, 4, "MEMINT_ReportVersionCheck");
  allocator.pin(&oGame_Pointer, MEMINT_oGame_Pointer_Address,  4, "oGame*");
  allocator.pin(&parserProxy,   ContentParserAddress,          sizeof(parserProxy), "zCParser proxy");

  // built-in data without assumed address
  oGame_Pointer = allocator.pin(&gameProxy, sizeof(gameProxy), "oGame");
  gameProxy._ZCSESSION_WORLD = allocator.alloc(148);

  symbolsPtr    = allocator.alloc(uint32_t(vm.symbols().size() * sizeof(phoenix::symbol)));

  parserProxy.symtab_table.numInArray = int32_t(vm.symbols().size());
  parserProxy.symtab_table.numAlloc   = 0;//parserProxy.symtab_table.numInArray;

  // ## Builtin instances
  allocator.alloc(MEMINT_SENDTOSPY_IMPLEMENTATION_ZERR_G2, 64, "ZERROR");

  allocator.alloc(0x723ee2, 6,  "unknown, pick-lock related"); // CoM: INIT_RANDOMIZEPICKLOCKS_GAMESTART
  allocator.alloc(0x70b6bc, 4,  "unknown"); // CoM: INIT_ARMORUNLOCKINNPCOVERRIDE
  allocator.alloc(0x70af4b, 20, "unknown");
  allocator.alloc(OCNPC__ENABLE_EQUIPBESTWEAPONS, 18, "OCNPC__ENABLE_EQUIPBESTWEAPONS");

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
  vm.override_function("MEM_PrintStackTrace",          [this](){ mem_printstacktrace_implementation();   });
  vm.override_function("MEM_GetFuncOffset",            [this](phoenix::func func){ return mem_getfuncoffset(func); });
  vm.override_function("MEM_GetFuncID",                [this](phoenix::func sym) { return mem_getfuncid(sym);      });
  vm.override_function("MEM_CallByID",                 [this](int sym) { return mem_callbyid(sym);       });
  vm.override_function("MEM_GetFuncPtr",               [this](int sym) { return mem_getfuncptr(sym);     });
  vm.override_function("MEM_ReplaceFunc",              [this](phoenix::func dest, phoenix::func func){ mem_replacefunc(dest, func); });
  vm.override_function("MEM_GetFuncIdByOffset",        [this](int off) { return mem_getfuncidbyoffset(off); });
  vm.override_function("MEM_AssignInst",               [this](int sym, int ptr) { mem_assigninst(sym, ptr); });

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

  // ## MEM_Alloc and MEM_Free ##
  vm.override_function("MEM_Alloc",   [this](int amount )                      { return mem_alloc(amount);               });
  vm.override_function("MEM_Free",    [this](int address)                      { mem_free(address);                      });
  vm.override_function("MEM_Realloc", [this](int address, int oldsz, int size) { return mem_realloc(address,oldsz,size); });

  // ## Control-flow ##
  vm.override_function("repeat", [this](phoenix::vm& vm) { return repeat(vm);    });
  vm.override_function("while",  [this](phoenix::vm& vm) { return while_(vm);    });
  vm.register_access_trap([this](phoenix::symbol& i)     { return loop_trap(&i); });

  // ## Strings
  vm.override_function("STR_SubStr", [this](std::string_view str, int start, int count){ return str_substr(str,start,count); });
  vm.override_function("STR_Len",    [this](std::string_view str){ return str_len(str); });
  vm.override_function("STR_ToInt",  [this](std::string_view str){ return str_toint(str); });

  // ## Ini-file
  vm.override_function("MEM_GetGothOpt",           [this](std::string_view sec, std::string_view opt) { return mem_getgothopt(sec,opt);       });
  vm.override_function("MEM_GetModOpt",            [this](std::string_view sec, std::string_view opt) { return mem_getmodopt(sec,opt);        });
  vm.override_function("MEM_GothOptSectionExists", [this](std::string_view sec)                       { return mem_gothoptaectionexists(sec); });
  vm.override_function("MEM_GothOptExists",        [this](std::string_view sec, std::string_view opt) { return mem_gothoptexists(sec,opt);    });
  vm.override_function("MEM_ModOptSectionExists",  [this](std::string_view sec)                       { return mem_modoptsectionexists(sec);  });
  vm.override_function("MEM_ModOptExists",         [this](std::string_view sec, std::string_view opt) { return mem_modoptexists(sec,opt);     });
  vm.override_function("MEM_SetGothOpt",           [this](std::string_view sec, std::string_view opt, std::string_view v) { return mem_setgothopt(sec,opt,v); });

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

  // ## Traps
  if(auto end = vm.find_symbol_by_name("END")) {
    end->set_access_trap_enable(true);
    }
  if(auto break_ = vm.find_symbol_by_name("BREAK")) {
    break_->set_access_trap_enable(true);
    }
  if(auto continue_ = vm.find_symbol_by_name("CONTINUE")) {
    continue_->set_access_trap_enable(true);
    }

  // if(auto fn = vm.find_symbol_by_name("SETINITIALTIMEINWORLD")) {
  //   fn->set_access_trap_enable(true);
  //   }
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

void Ikarus::mem_replacefunc(phoenix::func dest, phoenix::func func) {
  auto* sf  = func.value;
  auto* sd  = dest.value;
  Log::d("mem_replacefunc: ",sd->name()," -> ",sf->name());

  //auto& bin = vm.getDATFile().rawCode();
  //bin[sd.address]->op      = EParOp_Jump;
  //bin[sd.address]->address = func;
  }

int Ikarus::mem_getfuncidbyoffset(int off) {
  Log::e("TODO: mem_getfuncidbyoffset ", off);
  return 0;
  }

void Ikarus::mem_assigninst(int index, int ptr) {
  auto* sym  = vm.find_symbol_by_index(uint32_t(index));
  if(sym==nullptr) {
    Log::e("MEM_AssignInst: Invalid instance: ",index);
    return;
  }
  sym->set_instance(std::make_shared<memory_instance>(*this, ptr32_t(ptr)));
  }

void Ikarus::mem_printstacktrace_implementation() {
  Log::e("[start of stacktrace]");
  vm.print_stack_trace();
  Log::e("[end of stacktrace]");
  }

int Ikarus::mem_getfuncoffset(phoenix::func func) {
  auto* sym = func.value;
  if(sym == nullptr) {
    Log::e("mem_getfuncptr: invalid function ptr");
    return 0;
    }
  return int(sym->address());
  }

int Ikarus::mem_getfuncid(phoenix::func func) {
  return int(func.value->index());
  }

void Ikarus::mem_callbyid(int symbId) {
  auto* sym = vm.find_symbol_by_index(uint32_t(symbId));
  if(sym==nullptr) {
    Log::e("MEM_CallByID: Provided symbol is not callable (not function, prototype or instance): ", sym->name());
    return;
    }
  vm.call_function(sym);
  }

int Ikarus::mem_getfuncptr(int func) {
  // NOTE: need to put .text section into 32-bit space, to get proper pointers
  auto* sym = vm.find_symbol_by_index(uint32_t(func));
  while(sym!=nullptr && !sym->is_const()) {
    func = sym->get_int();
    sym = vm.find_symbol_by_index(uint32_t(func));
    }
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

int Ikarus::str_toint(std::string_view str) {
  int ret = 0;
  auto err = std::from_chars(str.data(), str.data()+str.size(), ret).ec;
  if(err==std::errc())
    return ret;
  Log::d("STR_ToInt: cannot convert string: ", str);
  return 0;
  }

std::string Ikarus::mem_getgothopt(std::string_view section, std::string_view option) {
  return std::string(Gothic::inst().settingsGetS(section, option));
  }

std::string Ikarus::mem_getmodopt(std::string_view section, std::string_view option) {
  Log::e("TODO: mem_getmodopt(", section, ", ", option, ")");
  return "";
  }

bool Ikarus::mem_gothoptaectionexists(std::string_view section) {
  Log::e("TODO: mem_gothoptaectionexists(", section, ")");
  return false;
  }

bool Ikarus::mem_gothoptexists(std::string_view section, std::string_view option) {
  Log::e("TODO: mem_gothoptexists(", section, ", ", option, ")");
  return false;
  }

bool Ikarus::mem_modoptsectionexists(std::string_view section) {
  Log::e("TODO: mem_modoptsectionexists(", section, ")");
  return false;
  }

bool Ikarus::mem_modoptexists(std::string_view section, std::string_view option) {
  Log::e("TODO: mem_modoptexists(", section, ", ", option, ")");
  return false;
  }

void Ikarus::mem_setgothopt(std::string_view section, std::string_view option, std::string_view value) {
  Log::e("TODO: mem_setgothopt(", section, ", ", option, ", ", value, ")");
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
  if(amount==0) {
    Log::d("alocation zero bytes");
    return 0;
    }
  auto ptr = allocator.alloc(uint32_t(amount));
  return int32_t(ptr);
  }

void Ikarus::mem_free(int ptr) {
  allocator.free(Mem32::ptr32_t(ptr));
  }

int Ikarus::mem_realloc(int address, int oldsz, int size) {
  auto ptr = allocator.realloc(Mem32::ptr32_t(address), uint32_t(size));
  return int32_t(ptr);
  }

std::shared_ptr<phoenix::instance> Ikarus::mem_ptrtoinst(ptr32_t address) {
  if(address==0)
    Log::d("mem_ptrtoinst: address is null");
  return std::make_shared<memory_instance>(*this, address);
  }

phoenix::naked_call Ikarus::repeat(phoenix::vm& vm) {
  const int        len = vm.pop_int();
  phoenix::symbol* i   = std::get<phoenix::symbol*>(vm.pop_reference());
  // Log::i("repeat: ", i->get_int(), " < ", len);
  if(i->get_int() < len) {
    loop_start.push_back({vm.pc(), i, len});
    return phoenix::naked_call();
    }
  loop_out(vm);
  return phoenix::naked_call();
  }

phoenix::naked_call Ikarus::while_(phoenix::vm& vm) {
  const int cond = vm.pop_int();
  // Log::i("while: ", cond);
  if(cond !=0) {
    loop_start.push_back({vm.pc() - 5*2, nullptr});
    return phoenix::naked_call();
    }
  loop_out(vm);
  return phoenix::naked_call();
  }

void Ikarus::loop_trap(phoenix::symbol* i) {
  auto instr = vm.instruction_at(vm.pc());
  if(instr.op != phoenix::opcode::pushv)
    return; // Ikarus keywords are always use pushv

  // Log::i("end");
  if(loop_start.size()==0) {
    Log::e("bad loop end");
    return;
    }

  const auto ret = loop_start.back();
  loop_start.pop_back();
  if(ret.i!=nullptr) {
    ret.i->set_int(ret.i->get_int()+1);
    vm.push_reference(ret.i);
    vm.push_int(ret.loopLen);
    }

  auto trap = vm.instruction_at(vm.pc());
  vm.unsafe_jump(ret.pc-trap.size);
  }

void Ikarus::loop_out(phoenix::vm& vm) {
  auto end = vm.find_symbol_by_name("END");
  auto rep = vm.find_symbol_by_name("REPEAT");
  auto whl = vm.find_symbol_by_name("WHILE");

  uint32_t repAddr = (rep!=nullptr ? rep->address() : uint32_t(-1));
  uint32_t whlAddr = (whl!=nullptr ? whl->address() : uint32_t(-1));

  int depth = 0;
  for(uint32_t i=vm.pc(); i<vm.size(); ) {
    const auto inst = vm.instruction_at(i);
    if(inst.op==phoenix::opcode::pushv && vm.find_symbol_by_index(inst.symbol)==end) {
      depth--;
      }
    else if(inst.op==phoenix::opcode::bl && inst.address==repAddr) {
      depth++;
      }
    else if(inst.op==phoenix::opcode::bl && inst.address==whlAddr) {
      depth++;
      }
    i += inst.size;
    if(depth==0) {
      auto trap = vm.instruction_at(vm.pc());
      vm.unsafe_jump(i-trap.size);
      break;
      }
    }
  }
