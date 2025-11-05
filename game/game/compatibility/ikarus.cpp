#include "ikarus.h"

#include <Tempest/Application>
#include <Tempest/Log>
#include <zenkit/vobs/Misc.hh>

#include <charconv>
#include <cstddef>
#include <cassert>

#include "game/gamescript.h"
#include "gothic.h"

using namespace Tempest;

static int32_t floatBitsToInt(float f) {
  int32_t i = 0;
  std::memcpy(&i, &f, sizeof(i));
  return i;
  }

static float intBitsToFloat(int32_t i) {
  float f = 0;
  std::memcpy(&f, &i, sizeof(f));
  return f;
  }

enum {
  GothicFirstInstructionAddress =  4198400,  // 0x401000
  ContentParserAddress          = 11223232,  // 0xAB40C0
  vfxParserPointerAddress       =  9234156,  // 0x8CE6EC
  menuParserPointerAddress      =  9248360,  // 0x8D1E68
  pfxParserPointerAddress       =  9278004,  // 0x8D9234

  MEMINT_SENDTOSPY_IMPLEMENTATION_ZERR_G2 = 9231568,
  INV_MAX_ITEMS_ADDR                      = 8635508,
  OCNPC__ENABLE_EQUIPBESTWEAPONS          = 7626662, //0x745FA6
  };

void Ikarus::memory_instance::set_int(const zenkit::DaedalusSymbol& sym, uint16_t index, int32_t value) {
  ptr32_t addr = address + ptr32_t(sym.offset_as_member()) + ptr32_t(index*sym.class_size());
  owner.allocator.writeInt(addr, value);
  }

int32_t Ikarus::memory_instance::get_int(const zenkit::DaedalusSymbol& sym, uint16_t index) const {
  ptr32_t addr = address + ptr32_t(sym.offset_as_member()) + ptr32_t(index * sym.class_size());
  int32_t v = owner.allocator.readInt(addr);
  return v;
  }

void Ikarus::memory_instance::set_float(const zenkit::DaedalusSymbol& sym, uint16_t index, float value) {
  ptr32_t addr = address + ptr32_t(sym.offset_as_member()) + ptr32_t(index*sym.class_size());
  owner.allocator.writeInt(addr, *reinterpret_cast<const int32_t*>(&value));
  }

float Ikarus::memory_instance::get_float(const zenkit::DaedalusSymbol& sym, uint16_t index) const {
  ptr32_t addr = address + ptr32_t(sym.offset_as_member()) + ptr32_t(index * sym.class_size());
  int32_t v = owner.allocator.readInt(addr);

  float f = 0;
  std::memcpy(&f, &v, 4);
  return f;
  }

void Ikarus::memory_instance::set_string(const zenkit::DaedalusSymbol& sym, uint16_t index, std::string_view value) {
  ptr32_t addr = address + ptr32_t(sym.offset_as_member()) + ptr32_t(index*sym.class_size());
  (void)addr;
  Log::d("TODO: memory_instance::set_string ", sym.name());
  // allocator.writeInt(addr, 0);
  }

const std::string& Ikarus::memory_instance::get_string(const zenkit::DaedalusSymbol& sym, uint16_t index) const {
  ptr32_t addr = address + ptr32_t(sym.offset_as_member()) + ptr32_t(index*sym.class_size());

  Log::d("TODO: memory_instance::get_string ", sym.name());
  (void)addr;
  static std::string empty;
  return empty;
  }


Ikarus::Ikarus(GameScript& owner, zenkit::DaedalusVm& vm) : gameScript(owner), vm(vm) {
  if(auto v = vm.find_symbol_by_name("Ikarus_Version")) {
    const int version = v->type()==zenkit::DaedalusDataType::INT ? v->get_int() : 0;
    Log::i("DMA mod detected: Ikarus v", version);
    }

  setupEngineMemory();

  // Note: no inline asm
  // TODO: Make sure this actually works!
  vm.override_function("ASMINT_Push",           [](int){});
  vm.override_function("ASMINT_Pop",            []() -> int { return 0; });
  vm.override_function("ASMINT_MyExternal",     [](){});
  vm.override_function("ASMINT_CallMyExternal", [](){});
  vm.override_function("ASMINT_Init",           [](){});
  vm.override_function("ASM_Open",              [this](int len){ ASM_Open(len); });
  vm.override_function("ASM_Close",             []() -> int { return 0; });
  vm.override_function("ASM",                   [](int,int){});
  vm.override_function("ASM_Run",               [](int){});
  vm.override_function("ASM_RunOnce",           [](){});

  // Floats
  vm.override_function("MKF",    [](int i) { return mkf(i); });
  vm.override_function("TRUNCF", [](int i) { return truncf(i); });
  vm.override_function("ROUNDF", [](int i) { return roundf(i); });
  vm.override_function("ADDF",   [](int a, int b) { return addf(a,b); });
  vm.override_function("SUBF",   [](int a, int b) { return subf(a,b); });
  vm.override_function("MULF",   [](int a, int b) { return mulf(a,b); });
  vm.override_function("DIVF",   [](int a, int b) { return divf(a,b); });

  vm.override_function("MEMINT_SetupExceptionHandler",   [this](){ mem_setupexceptionhandler(); });
  vm.override_function("MEMINT_ReplaceSlowFunctions",    [    ](){ });
  //vm.override_function("MEMINT_ReplaceLoggingFunctions", [    ](){ });
  vm.override_function("MEM_InitStatArrs",               [    ](){ });
  vm.override_function("MEM_InitRepeat",                 [    ](){ });

  vm.override_function("MEM_GetAddress_Init",          [this](){ mem_getaddress_init(); });
  vm.override_function("MEM_PrintStackTrace",          [this](){ mem_printstacktrace_implementation();   });
  vm.override_function("MEM_GetFuncOffset",            [this](zenkit::DaedalusFunction func){ return mem_getfuncoffset(func); });
  vm.override_function("MEM_GetFuncID",                [this](zenkit::DaedalusFunction sym) { return mem_getfuncid(sym);      });
  vm.override_function("MEM_CallByID",                 [this](int sym) { return mem_callbyid(sym);       });
  vm.override_function("MEM_GetFuncPtr",               [this](int sym) { return mem_getfuncptr(sym);     });
  vm.override_function("MEM_ReplaceFunc",              [this](zenkit::DaedalusFunction dest, zenkit::DaedalusFunction func){ mem_replacefunc(dest, func); });
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
  vm.override_function("MEM_ReadStatArr",              [this](zenkit::DaedalusVm& vm){ return mem_readstatarr(vm); });

  vm.override_function("MEM_GetCommandLine",           [this](){ return mem_getcommandline(); });

  // ## Basic zCParser related functions ##
  vm.override_function("MEM_PtrToInst",       [this](int address)          { return mem_ptrtoinst(ptr32_t(address)); });
  vm.override_function("_^",                  [this](int address)          { return mem_ptrtoinst(ptr32_t(address)); });
  vm.override_function("MEM_InstToPtr",       [this](int index)            { return mem_insttoptr(index); });
  vm.override_function("MEM_GetIntAddress",   [this](int val)              { return _takeref(val);      });
  vm.override_function("_@",                  [this](int val)              { return _takeref(val);     });
  vm.override_function("MEM_GetStringAddress",[this](std::string_view val) { return _takeref_s(val);   });
  vm.override_function("_@s",                 [this](std::string_view val) { return _takeref_s(val);   });
  vm.override_function("MEM_GetFloatAddress", [this](float val)            { return _takeref_f(val);   });
  vm.override_function("_@f",                 [this](float val)            { return _takeref_f(val);   });

  // ## MEM_Alloc and MEM_Free ##
  vm.override_function("MEM_Alloc",   [this](int amount )                      { return mem_alloc(amount);               });
  vm.override_function("MEM_Free",    [this](int address)                      { mem_free(address);                      });
  vm.override_function("MEM_Realloc", [this](int address, int oldsz, int size) { return mem_realloc(address,oldsz,size); });


  // ## Control-flow ##
  vm.override_function("repeat", [this](zenkit::DaedalusVm& vm) { return repeat(vm);    });
  vm.override_function("while",  [this](zenkit::DaedalusVm& vm) { return while_(vm);    });
  vm.register_access_trap([this](zenkit::DaedalusSymbol& i)     { return loop_trap(&i); });

  // ## Strings
  vm.override_function("STR_SubStr",   [this](std::string_view str, int start, int count){ return str_substr(str,start,count); });
  vm.override_function("STR_Len",      [this](std::string_view str){ return str_len(str);   });
  vm.override_function("STR_ToInt",    [this](std::string_view str){ return str_toint(str); });
  vm.override_function("STR_Upper",    [this](std::string_view str){ return str_upper(str); });
  vm.override_function("STR_FromChar", [this](int ptr){ return str_fromchar(ptr); });

  // ## Ini-file
  vm.override_function("MEM_GetGothOpt",           [this](std::string_view sec, std::string_view opt) { return mem_getgothopt(sec,opt);       });
  vm.override_function("MEM_GetModOpt",            [this](std::string_view sec, std::string_view opt) { return mem_getmodopt(sec,opt);        });
  vm.override_function("MEM_GothOptSectionExists", [this](std::string_view sec)                       { return mem_gothoptaectionexists(sec); });
  vm.override_function("MEM_GothOptExists",        [this](std::string_view sec, std::string_view opt) { return mem_gothoptexists(sec,opt);    });
  vm.override_function("MEM_ModOptSectionExists",  [this](std::string_view sec)                       { return mem_modoptsectionexists(sec);  });
  vm.override_function("MEM_ModOptExists",         [this](std::string_view sec, std::string_view opt) { return mem_modoptexists(sec,opt);     });
  vm.override_function("MEM_SetGothOpt",           [this](std::string_view sec, std::string_view opt, std::string_view v) { return mem_setgothopt(sec,opt,v); });

  // ##
  CALLINT_numParams = vm.find_symbol_by_name("CALLINT_numParams");
  vm.override_function("CALL_intparam",        [this](int p) { call_intparam(p); });
  vm.override_function("CALL_ptrparam",        [this](int p) { call_ptrparam(p); });
  vm.override_function("CALL_floatparam",      [this](int p) { call_floatparam(p); });
  vm.override_function("CALL_zstringptrparam", [this](std::string_view p) { call_zstringptrparam(p); });
  vm.override_function("CALL_cstringptrparam", [this](std::string_view p) { call_cstringptrparam(p); });
  vm.override_function("CALL_retvalasint",     [this]() { return call_retvalasint(); });
  vm.override_function("CALL_retvalasptr",     [this]() { return call_retvalasptr(); });
  vm.override_function("CALL__thiscall",       [this](int thisptr, int address){ call__thiscall(thisptr, ptr32_t(address)); });
  vm.override_function("CALLINT_makecall",     [this](int address, int clr){ callint_makecall(ptr32_t(address), clr); } );

  vm.override_function("HASH",           [this](int v){ return hash(v); });

  // ## Windows utilities
  vm.override_function("LoadLibrary", [](std::string_view name){
    Log::d("suppress LoadLibrary: ",name);
    return 0x1;
    });
  vm.override_function("GetProcAddress", [    ](int hmodule, std::string_view name){
    Log::d("suppress GetProcAddress: ",name);
    return 0x1;
    });
  vm.override_function("FindKernelDllFunction", [](std::string_view name){
    Log::d("suppress FindKernelDllFunction: ",name);
    return 0x1;
    });
  vm.override_function("VirtualProtect", [](int lpAddress, int dwSize, int flNewProtect){
    return lpAddress;
    });
  vm.override_function("MEM_MessageBox", [](std::string_view txt, std::string_view caption, int type){
    //TODO: real message box
    Log::i("MEM_MessageBox: ", caption);
    Log::i("  ", txt);
    Log::i("====");
    return 0;
    });
  vm.override_function("MEM_InfoBox", [](std::string_view txt){
    //TODO: real message box
    Log::i("MEM_InfoBox: ", txt);
    });

  // ## Windows api (basic)
  const int GETUSERNAMEA = 8080162;
  //const int GETLOCALTIME = 8079184;
  register_stdcall(GETUSERNAMEA, [this](ptr32_t lpBuffer, ptr32_t pcbBuffer){
    getusernamea(lpBuffer, pcbBuffer);
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

  // Disable some high-level functions, until basic stuff is done
  auto dummyfy = [&](std::string_view name, auto hook) {
    auto f = vm.find_symbol_by_name(name);
    if(f==nullptr || f->type()!=zenkit::DaedalusDataType::FUNCTION)
      return;
    vm.override_function(name, hook);
    };
  // CoM
  allocator.alloc(0x723ee2, 6,  "unknown, pick-lock related"); // CoM: INIT_RANDOMIZEPICKLOCKS_GAMESTART
  allocator.alloc(0x70b6bc, 4,  "unknown"); // CoM: INIT_ARMORUNLOCKINNPCOVERRIDE
  allocator.alloc(0x70af4b, 20, "unknown");
  allocator.alloc(OCNPC__ENABLE_EQUIPBESTWEAPONS, 18, "OCNPC__ENABLE_EQUIPBESTWEAPONS");
  // Atariar
  dummyfy("STOPALLSOUNDS", [](){});
  dummyfy("GETMASTERVOLUMESOUNDS", [](){ return 0; });
  dummyfy("R_DEFAULTINIT", [](){});
  dummyfy("PLAYER_RANGED_NO_AMMO_INIT", [](){}); // inline asm
  if(auto v = vm.find_symbol_by_name("PRINT_BARVALUES")) {
    //TESTING for bar UI!
    v->set_int(1);
    }
  }

bool Ikarus::isRequired(zenkit::DaedalusScript& vm) {
  return
      vm.find_symbol_by_name("Ikarus_Version") != nullptr &&
      vm.find_symbol_by_name("MEM_InitAll") != nullptr &&
      vm.find_symbol_by_name("MEM_ReadInt") != nullptr &&
      vm.find_symbol_by_name("MEM_WriteInt") != nullptr &&
      vm.find_symbol_by_name("_@") != nullptr &&
      vm.find_symbol_by_name("_^") != nullptr;
  }

void Ikarus::setupEngineMemory() {
  const int BAD_BUILTIN_PTR = 0xBAD4000;
  // built-in data with assumed address
  const int ZFACTORY = 9276912;
  const int MEMINT_oGame_Pointer_Address        = 11208836; //0xAB0884
  const int MEMINT_zTimer_Address               = 10073044; //0x99B3D4
  // const int MEMINT_oCInformationManager_Address = 11191384; //0xAAC458
  const int MEMINT_gameMan_Pointer_Address      = 9185624;  //0x8C2958

  versionHint = 504628679; // G2
  allocator.pin(&versionHint,   GothicFirstInstructionAddress,  4, "MEMINT_ReportVersionCheck");
  allocator.pin(&oGame_Pointer, MEMINT_oGame_Pointer_Address,   4, "oGame*");
  allocator.pin(&gameman_Ptr,   MEMINT_gameMan_Pointer_Address, 4, "GameMan*");
  allocator.pin(&zTimer,        MEMINT_zTimer_Address,          sizeof(zTimer), "zTimer");
  allocator.pin(&invMaxItems,   INV_MAX_ITEMS_ADDR,             4, "INV_MAX_ITEMS"); //TODO: callback memory
  allocator.pin(&zFactory_Ptr,  ZFACTORY,                       4, "zFactory*");

  // const int zCParser_symtab_table_array_offset        = 24; //0x18
  // const int zCParser_sorted_symtab_table_array_offset = 36; //0x24
  // const int zCParser_stack_offset                     = 72; //0x48
  // const int zCParser_stack_stackPtr_offset            = 76; //0x4C
  allocator.pin(&parserProxy,     ContentParserAddress, sizeof(parserProxy), "zCParser proxy");
  //allocator.pin(&MEMINT_StackPos, ContentParserAddress + zCParser_stack_stackPtr_offset);

  const int OCNPCFOCUS__FOCUSLIST_G2 = 11208440;
  allocator.pin(&focusList, OCNPCFOCUS__FOCUSLIST_G2, sizeof(focusList), "OCNPCFOCUS__FOCUSLIST_G2");

  // built-in data without assumed address
  oGame_Pointer = allocator.pin(&memGame, sizeof(memGame), "oGame");
  memGame._ZCSESSION_WORLD = allocator.alloc(sizeof(oWorld));
  memGame.WLDTIMER         = BAD_BUILTIN_PTR;

  auto& mem_world = *allocator.deref<oWorld>(memGame._ZCSESSION_WORLD);
  mem_world.WAYNET = BAD_BUILTIN_PTR; //TODO: add implement some proxy to waynet

  gameman_Ptr   = allocator.alloc(sizeof(GameMgr));
  symbolsPtr    = allocator.alloc(uint32_t(vm.symbols().size() * sizeof(zenkit::DaedalusSymbol)));

  parserProxy.symtab_table.numInArray = int32_t(vm.symbols().size());
  parserProxy.symtab_table.numAlloc   = 0;//parserProxy.symtab_table.numInArray;

  // ## Builtin instances in dynamic memory
  allocator.alloc(MEMINT_SENDTOSPY_IMPLEMENTATION_ZERR_G2, 64, "ZERROR");

  if(auto p = allocator.deref<std::array<ptr32_t,6>>(OCNPCFOCUS__FOCUSLIST_G2)) {
    gameScript.focusMage();
    (*p)[5] = BAD_BUILTIN_PTR;
    }
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

int Ikarus::mkf(int v) {
  return floatBitsToInt(float(v));
  }

int Ikarus::truncf(int v) {
  float ret = intBitsToFloat(v);
  ret = std::truncf(ret);
  return floatBitsToInt(ret);
  }

int Ikarus::roundf(int v) {
  float ret = intBitsToFloat(v);
  ret = std::roundf(ret);
  return floatBitsToInt(ret);
  }

int Ikarus::addf(int ia, int ib) {
  float a = intBitsToFloat(ia);
  float b = intBitsToFloat(ib);
  return floatBitsToInt(a+b);
  }

int Ikarus::subf(int ia, int ib) {
  float a = intBitsToFloat(ia);
  float b = intBitsToFloat(ib);
  return floatBitsToInt(a-b);
  }

int Ikarus::mulf(int ia, int ib) {
  float a = intBitsToFloat(ia);
  float b = intBitsToFloat(ib);
  return floatBitsToInt(a*b);
  }

int Ikarus::divf(int ia, int ib) {
  float a = intBitsToFloat(ia);
  float b = intBitsToFloat(ib);
  return floatBitsToInt(a/b);
  }

void Ikarus::ASM_Open(int) {
  Log::e("Ikarus: ASM_Open not implemented");
  }

void Ikarus::mem_getaddress_init() { /* nop */ }

void Ikarus::mem_replacefunc(zenkit::DaedalusFunction dest, zenkit::DaedalusFunction func) {
  auto* sf  = func.value;
  auto* sd  = dest.value;
  if(sd->name()=="MEM_SENDTOSPY")
    return;
  if(sd->name()=="MEM_PRINTSTACKTRACE")
    return;
  Log::d("mem_replacefunc: ",sd->name()," -> ",sf->name());

  //auto& bin = vm.getDATFile().rawCode();
  //bin[sd.address]->op      = EParOp_Jump;
  //bin[sd.address]->address = func;
  }

int Ikarus::mem_getfuncidbyoffset(int off) {
  if(off==0) {
    // MEM_INITALL
    return 0;
    }
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

int Ikarus::mem_getfuncoffset(zenkit::DaedalusFunction func) {
  auto* sym = func.value;
  if(sym == nullptr) {
    Log::e("mem_getfuncptr: invalid function ptr");
    return 0;
    }
  return int(sym->address());
  }

int Ikarus::mem_getfuncid(zenkit::DaedalusFunction func) {
  return int(func.value->index());
  }

void Ikarus::mem_callbyid(int symbId) {
  auto* sym = vm.find_symbol_by_index(uint32_t(symbId));
  if(sym==nullptr) {
    Log::e("MEM_CallByID: Provided symbol is not callable (not function, prototype or instance): ", symbId);
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
  const size_t symIndex = (size_t(address) - symbolsPtr)/sizeof(zenkit::DaedalusSymbol);
  const size_t sub      = (size_t(address) - symbolsPtr)%sizeof(zenkit::DaedalusSymbol);
  if(symIndex>=vm.symbols().size() || sub!=0)
    return "";
  auto& s = vm.symbols()[symIndex];
  return s.name();
  }

zenkit::DaedalusNakedCall Ikarus::mem_readstatarr(zenkit::DaedalusVm& vm) {
  const int  index = vm.pop_int();
  auto [ref, idx, context] = vm.pop_reference();

  const int ret = vm.get_int(context, ref, uint16_t(idx+index));

  vm.push_int(ret);
  return zenkit::DaedalusNakedCall();
  }

int Ikarus::_mem_readstatarr(int address, int off) {
  return allocator.readInt(ptr32_t(address) + ptr32_t(off)*4);
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
  return int(ptr32_t(symbolsPtr + size_t(index)*sizeof(zenkit::DaedalusSymbol)));
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

std::string Ikarus::str_upper(std::string_view str) {
  std::string s = std::string(str);
  for(auto& c:s)
    c = char(std::toupper(c));
  return s;
  }

std::string Ikarus::str_fromchar(int iptr) {
  auto [ptr, size] = allocator.deref(ptr32_t(iptr));
  const char* chr = reinterpret_cast<const char*>(ptr);
  if(chr==nullptr || size==0)
    return "";
  size_t strsz = 0;
  while(chr[strsz] && strsz<size)
    ++strsz;
  return std::string(chr, strsz);
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
  if(section=="INTERNAL" && option=="UnionActivated") {
    // Fake Union, so ikarus won't setup crazy workarounds for unpatched G2.
    return true;
    }
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

int Ikarus::getusernamea(ptr32_t lpBuffer, ptr32_t pcbBuffer) {
  //NOTE: max is broken, as _@ is not implemented for local variables in script
  int max = allocator.readInt(pcbBuffer); (void)max;
  if(auto ptr = allocator.deref(lpBuffer, 16)) {
    std::strncpy(reinterpret_cast<char*>(ptr), "OpenGothic", 16);
    }
  return 1;
  }

void Ikarus::call_intparam(int prm) {
  //TODO: implement inline asm instead?!
  call.iprm.push_back(prm);
  if(CALLINT_numParams!=nullptr && CALLINT_numParams->type()==zenkit::DaedalusDataType::INT)
    CALLINT_numParams->set_int(CALLINT_numParams->get_int()+1);
  }

void Ikarus::call_ptrparam(int prm) {
  call_intparam(prm);
  }

void Ikarus::call_floatparam(int prm) {
  call_intparam(prm);
  }

void Ikarus::call_zstringptrparam(std::string_view prm) {
  //NOTE: asm-based version has much more quirks and requires _@ operator
  call.sprm.push_back(std::string(prm));
  }

void Ikarus::call_cstringptrparam(std::string_view prm) {
  //NOTE: asm-based version has much more quirks and requires _@ operator
  call.sprm.push_back(std::string(prm));
  }

int Ikarus::call_retvalasint() {
  if(!call.hasEax)
    Log::e("Ikarus: call_retvalasint: EAX wasn't initialized");
  call.hasEax = false;
  return call.eax;
  }

int Ikarus::call_retvalasptr() {
  return call_retvalasint();
  }

void Ikarus::call__thiscall(int32_t pthis, ptr32_t func) {
  call.iprm.push_back(pthis);
  callint_makecall(func, false);
  }

void Ikarus::callint_makecall(ptr32_t func, bool cleanStk) {
  if(CALLINT_numParams!=nullptr && CALLINT_numParams->type()==zenkit::DaedalusDataType::INT) {
    //Log::d("argc = ", CALLINT_numParams->get_int());
    CALLINT_numParams->set_int(0);
    }
  auto fn = stdcall_overrides.find(func);
  if(fn!=stdcall_overrides.end()) {
    fn->second(*this);
    assert(call.iprm.size()==0); // not nesseserly true, but usefull for debug
    assert(call.sprm.size()==0); // not nesseserly true, but usefull for debug
    return;
    }
  call.iprm.clear();
  call.sprm.clear();
  static std::unordered_set<ptr32_t> once;
  if(!once.insert(func).second)
    return;

  auto str = demangleAddress(func);
  if(!str.empty())
    Log::d("Ikarus: callint_makecall(", str, ")"); else
    Log::d("Ikarus: callint_makecall(", func, ")");
  }

int Ikarus::hash(int x) {
  // original ikrus has uses pointer to stack, so it get's dicy
  // TODO: CRC32 to match behaviour
  return int(std::hash<int>()(x));
  }

std::string_view Ikarus::demangleAddress(ptr32_t addr) {
  for(auto& i:vm.symbols()) {
    if(!i.is_const() || i.is_member())
      continue;
    if(i.type()!=zenkit::DaedalusDataType::INT)// && i.type() != zenkit::DaedalusDataType::FUNCTION)
      continue;
    if(ptr32_t(i.get_int())!=addr)
      continue;
    return i.name();
    }
  return "";
  }

int Ikarus::_takeref(int symbol) {
  auto sym = vm.find_symbol_by_index(uint32_t(symbol));
  if(sym!=nullptr && sym->type()==zenkit::DaedalusDataType::INSTANCE) {
    auto inst = sym->get_instance().get();
    if(auto d = dynamic_cast<memory_instance*>(inst)) {
      return int32_t(d->address);
      }
    }
  if(sym==nullptr)
    Log::e("Ikarus: _takeref - unable to resolve symbol"); else
    Log::e("Ikarus: _takeref - unable to resolve address (", sym->name(), ")");
  return 0xBAD1000;
  }

int Ikarus::_takeref_s(std::string_view val) {
  Log::e("Ikarus: _takeref_s - unimplemented");
  (void)val;
  return 0xBAD2000;
  }

int Ikarus::_takeref_f(float val) {
  Log::e("Ikarus: _takeref_f - unimplemented");
  (void)val;
  return 0xBAD3000;
  }

int Ikarus::mem_alloc(int amount) {
  if(amount==0) {
    Log::d("alocation zero bytes");
    return 0;
    }
  auto ptr = allocator.alloc(uint32_t(amount));
  return int32_t(ptr);
  }

int Ikarus::mem_alloc(int amount, const char* comment) {
  if(amount==0) {
    Log::d("alocation zero bytes");
    return 0;
    }
  auto ptr = allocator.alloc(uint32_t(amount), comment);
  return int32_t(ptr);
  }

void Ikarus::mem_free(int ptr) {
  allocator.free(Mem32::ptr32_t(ptr));
  }

int Ikarus::mem_realloc(int address, int oldsz, int size) {
  auto ptr = allocator.realloc(Mem32::ptr32_t(address), uint32_t(size));
  return int32_t(ptr);
  }

void Ikarus::register_stdcall_inner(ptr32_t addr, std::function<void(Ikarus& ikarus)> f) {
  stdcall_overrides[addr] = std::move(f);
  }

std::shared_ptr<zenkit::DaedalusInstance> Ikarus::mem_ptrtoinst(ptr32_t address) {
  if(address==0)
    Log::d("mem_ptrtoinst: address is null");
  return std::make_shared<memory_instance>(*this, address);
  }

int Ikarus::mem_insttoptr(int index) {
  auto* sym  = vm.find_symbol_by_index(uint32_t(index));
  if(sym==nullptr || sym->type() != zenkit::DaedalusDataType::INSTANCE) {
    Log::e("MEM_InstToPtr: Invalid instance: ", index);
    return 0;
    }

  const std::shared_ptr<zenkit::DaedalusInstance>& inst = sym->get_instance();
  if(auto mem = dynamic_cast<memory_instance*>(inst.get())) {
    return int32_t(mem->address);
    }

  Log::d("MEM_InstToPtr: not a memory instance");
  return 0;
  }

zenkit::DaedalusNakedCall Ikarus::repeat(zenkit::DaedalusVm& vm) {
  const int               len = vm.pop_int();
  zenkit::DaedalusSymbol* i   = std::get<zenkit::DaedalusSymbol*>(vm.pop_reference());
  // Log::i("repeat: ", i->get_int(), " < ", len);
  if(i->get_int() < len) {
    loop_start.push_back({vm.pc(), i, len});
    return zenkit::DaedalusNakedCall();
    }
  loop_out(vm);
  return zenkit::DaedalusNakedCall();
  }

zenkit::DaedalusNakedCall Ikarus::while_(zenkit::DaedalusVm& vm) {
  const int cond = vm.pop_int();
  // Log::i("while: ", cond);
  if(cond !=0) {
    loop_start.push_back({vm.pc() - 5*2, nullptr});
    return zenkit::DaedalusNakedCall();
    }
  loop_out(vm);
  return zenkit::DaedalusNakedCall();
  }

void Ikarus::loop_trap(zenkit::DaedalusSymbol* i) {
  auto instr = vm.instruction_at(vm.pc());
  if(instr.op != zenkit::DaedalusOpcode::PUSHV)
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

void Ikarus::loop_out(zenkit::DaedalusVm& vm) {
  auto end = vm.find_symbol_by_name("END");
  auto rep = vm.find_symbol_by_name("REPEAT");
  auto whl = vm.find_symbol_by_name("WHILE");

  uint32_t repAddr = (rep!=nullptr ? rep->address() : uint32_t(-1));
  uint32_t whlAddr = (whl!=nullptr ? whl->address() : uint32_t(-1));

  int depth = 0;
  for(uint32_t i=vm.pc(); i<vm.size(); ) {
    const auto inst = vm.instruction_at(i);
    if(inst.op==zenkit::DaedalusOpcode::PUSHV && vm.find_symbol_by_index(inst.symbol)==end) {
      depth--;
      }
    else if(inst.op==zenkit::DaedalusOpcode::BL && inst.address==repAddr) {
      depth++;
      }
    else if(inst.op==zenkit::DaedalusOpcode::BL && inst.address==whlAddr) {
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

