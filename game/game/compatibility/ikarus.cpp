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
using namespace Compatibility;

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

static uint32_t nextPot(uint32_t x) {
  x--;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  x++;
  return x;
  }

enum {
  GothicFirstInstructionAddress =  4198400,  // 0x401000
  ContentParserAddress          = 11223232,  // 0xAB40C0
  vfxParserPointerAddress       =  9234156,  // 0x8CE6EC
  menuParserPointerAddress      =  9248360,  // 0x8D1E68
  pfxParserPointerAddress       =  9278004,  // 0x8D9234

  MEMINT_SENDTOSPY_IMPLEMENTATION_ZERR_G2 = 9231568,
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

  if(auto zs = owner.allocator.deref<const zString>(addr)) {
    static std::string ret;
    owner.memFromString(ret, *zs);
    return ret;
    }

  Log::d("TODO: memory_instance::get_string ", sym.name());
  (void)addr;
  static std::string empty;
  return empty;
  }


Ikarus::Ikarus(GameScript& owner, zenkit::DaedalusVm& vm) : gameScript(owner), vm(vm), cpu(*this, allocator) {
  if(auto v = vm.find_symbol_by_name("Ikarus_Version")) {
    const int version = v->type()==zenkit::DaedalusDataType::INT ? v->get_int() : 0;
    Log::i("DMA mod detected: Ikarus v", version);
    }

  setupFunctionTable();
  setupIkarusLoops();
  setupEngineText();
  setupEngineMemory();

  // Inline ASM
  vm.override_function("ASMINT_Init",           [this]() { ASMINT_Init(); });
  vm.override_function("ASMINT_MyExternal",     [](){});
  vm.override_function("ASMINT_CallMyExternal", [this]() { ASMINT_CallMyExternal(); });

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
  vm.override_function("MEM_CallByID",                 [this](zenkit::DaedalusVm& vm) { return mem_callbyid(vm);       });
  vm.override_function("MEM_CallByPtr",                [this](zenkit::DaedalusVm& vm) { return mem_callbyptr(vm);      });
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
  vm.override_function("MEM_PtrToInst",        [this](int address)           { return mem_ptrtoinst(ptr32_t(address)); });
  vm.override_function("_^",                   [this](int address)           { return mem_ptrtoinst(ptr32_t(address)); });
  vm.override_function("MEM_InstToPtr",        [this](int index)             { return mem_insttoptr(index); });
  vm.override_function("MEM_GetIntAddress",    [this](zenkit::DaedalusVm& vm){ return _takeref(vm);         });
  vm.override_function("_@",                   [this](zenkit::DaedalusVm& vm){ return _takeref(vm);         });
  vm.override_function("MEM_GetStringAddress", [this](zenkit::DaedalusVm& vm){ return _takeref(vm);         });
  vm.override_function("_@s",                  [this](zenkit::DaedalusVm& vm){ return _takeref(vm);         });
  vm.override_function("MEM_GetFloatAddress",  [this](zenkit::DaedalusVm& vm){ return _takeref(vm);         });
  vm.override_function("_@f",                  [this](zenkit::DaedalusVm& vm){ return _takeref(vm);         });

  // ## MEM_Alloc and MEM_Free ##
  vm.override_function("MEM_Alloc",   [this](int amount )                      { return mem_alloc(amount);               });
  vm.override_function("MEM_Free",    [this](int address)                      { mem_free(address);                      });
  vm.override_function("MEM_Realloc", [this](int address, int oldsz, int size) { return mem_realloc(address,oldsz,size); });

  // ## Strings
  vm.override_function("STR_ToInt",    [this](std::string_view str){ return str_toint(str); });
  vm.override_function("STR_Upper",    [this](std::string_view str){ return str_upper(str); });
  vm.override_function("STR_FromChar", [this](int ptr){ return str_fromchar(ptr); });
  vm.override_function("STR_SubStr",   [this](std::string_view str, int start, int count){ return str_substr(str,start,count); });
  // STR_Compare - needs ASM working
  // STR_ToInt - needs jumps
  // TODO: STR_IndexOf, STR_Split
  // STR_Upper, STR_Lower - needs ASM working

  // ## Ini-file
  vm.override_function("MEM_GetGothOpt",           [this](std::string_view sec, std::string_view opt) { return mem_getgothopt(sec,opt);       });
  vm.override_function("MEM_GetModOpt",            [this](std::string_view sec, std::string_view opt) { return mem_getmodopt(sec,opt);        });
  vm.override_function("MEM_GothOptSectionExists", [this](std::string_view sec)                       { return mem_gothoptaectionexists(sec); });
  vm.override_function("MEM_GothOptExists",        [this](std::string_view sec, std::string_view opt) { return mem_gothoptexists(sec,opt);    });
  vm.override_function("MEM_ModOptSectionExists",  [this](std::string_view sec)                       { return mem_modoptsectionexists(sec);  });
  vm.override_function("MEM_ModOptExists",         [this](std::string_view sec, std::string_view opt) { return mem_modoptexists(sec,opt);     });
  vm.override_function("MEM_SetGothOpt",           [this](std::string_view sec, std::string_view opt, std::string_view v) { return mem_setgothopt(sec,opt,v); });

  const int SYSGETTIMEPTR_G2 = 5264000;
  cpu.register_cdecl(SYSGETTIMEPTR_G2, [this](){
    return uint32_t(gameScript.tickCount());
    });
  const int ZERROR__SETTARGET = 4513616;
  cpu.register_thiscall(ZERROR__SETTARGET, [](ptr32_t,int){ Log::d("TODO: ZERROR__SETTARGET"); });

  const int NPC_GETSLOTITEM = 7544720;
  cpu.register_thiscall(NPC_GETSLOTITEM, [](ptr32_t, std::string slot){
    Log::d("TODO: NPC_GETSLOTITEM (\"", slot, "\")");
    return 0;
    });

  const int ZCPARSER__GETINDEX_G2 = 7943280;
  cpu.register_thiscall(ZCPARSER__GETINDEX_G2, [this](ptr32_t, std::string name){
    return parserGetIndex(name);
    });

  vm.override_function("HASH",           [this](int v){ return hash(v); });
  //const int GETBUFFERCRC32_G2 = 6265360;
  //cpu.register_stdcall(GETBUFFERCRC32_G2, [](){});

  // TODO: original code of _PM_INSTNAME, from LeGo requires symbol table to be mapped
  // vm.override_function("_PM_INSTNAME", [this](int inst) { return _pm_instName(inst); });

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
  const int GETLOCALTIME = 8079184;
  register_stdcall(GETUSERNAMEA, [this](ptr32_t lpBuffer, ptr32_t pcbBuffer){ getusernamea(lpBuffer, pcbBuffer); });
  register_stdcall(GETLOCALTIME, [this](ptr32_t lpSystemTime)               { getlocaltime(lpSystemTime);        });

  // ## ZSpy ##
  vm.override_function("MEM_SendToSpy", [this](int cat, std::string_view msg){ return mem_sendtospy(cat,msg); });

  // ## Constants
  if(auto v = vm.find_symbol_by_name("CurrSymbolTableLength")) {
    v->set_int(int(vm.symbols().size()));
    }

  vm.override_function("memint_stackpushint",  [this](zenkit::DaedalusVm& vm){ return memint_stackpushint(vm);  });
  vm.override_function("memint_stackpushinst", [this](zenkit::DaedalusVm& vm){ return memint_stackpushinst(vm); });

  // Disable some high-level functions, until basic stuff is done
  auto dummyfy = [&](std::string_view name, auto hook) {
    auto f = vm.find_symbol_by_name(name);
    if(f==nullptr || f->type()!=zenkit::DaedalusDataType::FUNCTION)
      return;
    vm.override_function(name, hook);
    };

  // CoM
  allocator.alloc(0x42ebe3, 5, "unknown, INIT_MOD");
  allocator.alloc(0x50A048, 4, "unknown, PATCHAMBIENTVOBS");
  allocator.alloc(0x723ee2, 6, "unknown, pick-lock related"); // CoM: INIT_RANDOMIZEPICKLOCKS_GAMESTART

  // Atariar
  dummyfy("STOPALLSOUNDS", [](){});
  dummyfy("GETMASTERVOLUMESOUNDS", [](){ return 0; });
  dummyfy("R_DEFAULTINIT", [](){});
  dummyfy("PLAYER_RANGED_NO_AMMO_INIT", [](){}); // inline asm
  if(auto v = vm.find_symbol_by_name("PRINT_BARVALUES")) {
    //TESTING for bar UI!
    //v->set_int(1);
    (void)v;
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
  const uint32_t BAD_BUILTIN_PTR = 0xBAD40000;
  // built-in data with assumed address
  const int ZFACTORY = 9276912;
  const int MEMINT_oGame_Pointer_Address   = 11208836; //0xAB0884
  const int MEMINT_zTimer_Address          = 10073044; //0x99B3D4
  const int MEMINT_gameMan_Pointer_Address = 9185624;  //0x8C2958

  allocator.setCallbackR(Mem32::Type::zCParser, [this](zCParser& p, uint32_t){ memoryCallback(p, std::memory_order::acquire); });
  allocator.setCallbackW(Mem32::Type::zCParser, [this](zCParser& p, uint32_t){ memoryCallback(p, std::memory_order::release); });

  allocator.setCallbackR(Mem32::Type::ZCParser_variables, [this](ScriptVar& v, uint32_t id) {
    memoryCallback(v, id, std::memory_order::acquire);
    });
  allocator.setCallbackW(Mem32::Type::ZCParser_variables, [this](ScriptVar& v, uint32_t id) {
    memoryCallback(v, id, std::memory_order::release);
    });
  allocator.setCallbackR(Mem32::Type::zCPar_Symbol, [this](zCPar_Symbol& p, uint32_t id){
    memoryCallback(p, id, std::memory_order::acquire);
    });
  allocator.setCallbackW(Mem32::Type::zCPar_Symbol, [this](zCPar_Symbol& p, uint32_t id){
    memoryCallback(p, id, std::memory_order::release);
    });

  versionHint = 504628679; // G2
  allocator.pin(&versionHint,   GothicFirstInstructionAddress,  4, "MEMINT_ReportVersionCheck");
  allocator.pin(&oGame_Pointer, MEMINT_oGame_Pointer_Address,   4, "oGame*");
  allocator.pin(&gameman_Ptr,   MEMINT_gameMan_Pointer_Address, 4, "GameMan*");
  allocator.pin(&zTimer,        MEMINT_zTimer_Address,          sizeof(zTimer), "zTimer");
  allocator.pin(&zFactory_Ptr,  ZFACTORY,                       4, "zFactory*");

  const int LODENABLED_ADDR = 8596020;
  allocator.alloc(LODENABLED_ADDR, 4, "LODENABLED");

  const int AMBIENTVOBSENABLED_ADDR = 9079488;
  allocator.alloc(AMBIENTVOBSENABLED_ADDR, 4, "AMBIENTVOBSENABLED");

  const int GAME_HOLDTIME_ADDRESS = 11208840;
  allocator.alloc(GAME_HOLDTIME_ADDRESS, 4, "GAME_HOLDTIME_ADDRESS");

  const int INV_MAX_ITEMS_ADDR = 8635508;
  allocator.alloc(INV_MAX_ITEMS_ADDR, 4, "INV_MAX_ITEMS");

  // const int zCParser_symtab_table_array_offset        = 24; //0x18
  // const int zCParser_sorted_symtab_table_array_offset = 36; //0x24
  // const int zCParser_stack_offset                     = 72; //0x48
  // const int zCParser_stack_stackPtr_offset            = 76; //0x4C
  // allocator.pin(&parserProxy,     ContentParserAddress, sizeof(parserProxy), "zCParser proxy");
  allocator.pin(ContentParserAddress, sizeof(zCParser), Mem32::Type::zCParser);

  const int OCNPCFOCUS__FOCUSLIST_G2 = 11208440;
  allocator.pin(&focusList, OCNPCFOCUS__FOCUSLIST_G2, sizeof(focusList), "OCNPCFOCUS__FOCUSLIST_G2");

  // built-in data without assumed address
  oGame_Pointer = allocator.pin(&memGame, sizeof(memGame), "oGame");
  memGame._ZCSESSION_WORLD = allocator.alloc(sizeof(oWorld));
  memGame.WLDTIMER         = BAD_BUILTIN_PTR;
  memGame.TIMESTEP         = 1; // used as boolend in anim8

  auto& mem_world = *allocator.deref<oWorld>(memGame._ZCSESSION_WORLD);
  mem_world.WAYNET       = BAD_BUILTIN_PTR; //TODO: add implement some proxy to waynet
  mem_world.VOBLIST_NPCS = 0; //BAD_BUILTIN_PTR; // zCListSort*

  gameman_Ptr   = allocator.alloc(sizeof(GameMgr));
  symbolsPtr    = allocator.alloc(uint32_t(vm.symbols().size() * sizeof(zenkit::DaedalusSymbol)));

  // Storage for local variables, so script may address them thru pointers
  scriptVariables = allocator.alloc(uint32_t(vm.symbols().size() * sizeof(ScriptVar)),    Mem32::Type::ZCParser_variables);
  scriptSymbols   = allocator.alloc(uint32_t(vm.symbols().size() * sizeof(zCPar_Symbol)), Mem32::Type::zCPar_Symbol);

  // ## Builtin instances in dynamic memory
  allocator.alloc(MEMINT_SENDTOSPY_IMPLEMENTATION_ZERR_G2, 64, "ZERROR");

  if(auto p = allocator.deref<std::array<ptr32_t,6>>(OCNPCFOCUS__FOCUSLIST_G2)) {
    gameScript.focusMage();
    (*p)[5] = BAD_BUILTIN_PTR;
    }
  }

void Ikarus::setupEngineText() {
  // pin functions - CoM and some other mods rewriting assembly for them
  const uint32_t OCNPC__ENABLE_EQUIPBESTWEAPONS = 7626662;
  allocator.alloc(OCNPC__ENABLE_EQUIPBESTWEAPONS, 18, "OCNPC__ENABLE_EQUIPBESTWEAPONS");

  const uint32_t OCNPC__GETNEXTENEMY = 7556941;
  allocator.alloc(OCNPC__GETNEXTENEMY, 48, "OCNPC__GETNEXTENEMY");

  const uint32_t OCITEMCONTAINER__CHECKSELECTEDITEM_ISACTIVE = 7378665;
  allocator.alloc(OCITEMCONTAINER__CHECKSELECTEDITEM_ISACTIVE, 5, ".text");

  const uint32_t OCITEMCONTAINER__CHECKSELECTEDITEM_ISACTIVEP = 7378700;
  allocator.alloc(OCITEMCONTAINER__CHECKSELECTEDITEM_ISACTIVEP, 5, ".text");

  const int OCSTEALCONTAINER__CREATELIST_ISARMOR_SP18 = 7384908;
  allocator.alloc(OCSTEALCONTAINER__CREATELIST_ISARMOR_SP18, 8, ".text");

  const int OCNPCCONTAINER__CREATELIST_ISARMOR_SP18 = 7386812;
  allocator.alloc(OCNPCCONTAINER__CREATELIST_ISARMOR_SP18, 8, ".text");

  const int OCSTEALCONTAINER__CREATELIST_ISARMOR = 7384900;
  allocator.alloc(OCSTEALCONTAINER__CREATELIST_ISARMOR, 8, ".text");

  const int OCNPCCONTAINER__CREATELIST_ISARMOR = 7386805;
  allocator.alloc(OCNPCCONTAINER__CREATELIST_ISARMOR, 5, ".text");

  const int OCNPCCONTAINER__HANDLEEVENT_ISEMPTY = 7387581;
  allocator.alloc(OCNPCCONTAINER__HANDLEEVENT_ISEMPTY, 5, ".text");

  const int OCNPCINVENTORY__HANDLEEVENT_KEYWEAPONJZ = 7402077;
  allocator.alloc(OCNPCINVENTORY__HANDLEEVENT_KEYWEAPONJZ, 4, ".text");

  const int OCNPCINVENTORY__HANDLEEVENT_KEYWEAPON = 7402065;
  allocator.alloc(OCNPCINVENTORY__HANDLEEVENT_KEYWEAPON, 8, ".text");

  const int OCAIHUMAN__CHANGECAMMODEBYSITUATION_SWITCHMOBCAM = 6935573;
  allocator.alloc(OCAIHUMAN__CHANGECAMMODEBYSITUATION_SWITCHMOBCAM, 8, ".text");
  }

void Ikarus::memoryCallback(zCParser& p, std::memory_order ord) {
  if(vm.pc()==0x286a)
    Log::d("MEM_ARRAYINDEXOF dbg");

  if(ord==std::memory_order::acquire) {
    if(p.symtab_table.ptr==0) {
      //NOTE: initialize once
      p.symtab_table.numInArray = int32_t(vm.symbols().size());
      p.symtab_table.numAlloc   = int32_t(nextPot(uint32_t(p.symtab_table.numInArray)));
      p.symtab_table.ptr        = allocator.realloc(p.symtab_table.ptr, uint32_t(p.symtab_table.numAlloc)*uint32_t(sizeof(uint32_t)));

      auto v = allocator.deref<ptr32_t>(p.symtab_table.ptr);
      for(int32_t i=0; i<p.symtab_table.numInArray; ++i) {
        v[i] = scriptSymbols + uint32_t(i)*uint32_t(sizeof(zCPar_Symbol));
        }
      }

    auto trap = vm.instruction_at(vm.pc());
    p.stack_stackPtr = vm.pc() + trap.size;
    }
  else {
    auto instr = vm.instruction_at(vm.pc());

    auto src = findSymbolByAddress(vm.pc() - instr.size);
    auto dst = findSymbolByAddress(p.stack_stackPtr);

    if(src!=dst || dst==nullptr) {
      if(src!=nullptr && dst!=nullptr)
        Log::e("FIXME: unsafe jump: `\"", src->name(), "\" -> \"", dst->name(), "\""); else
        Log::e("FIXME: unsafe jump!");
      return;
      }
    vm.unsafe_jump(p.stack_stackPtr - instr.size);
    }
  }

void Ikarus::memoryCallback(ScriptVar& v, uint32_t index, std::memory_order ord) {
  if(index>=vm.symbols().size()) {
    // should never happend
    Log::d("ikarus: symbol table is corrupted");
    assert(false);
    return;
    }

  auto& str = v.data;
  auto& sym = *vm.find_symbol_by_index(index);
  if(sym.is_member()) {
    Log::e("Ikarus: accessing member symbol (\"", sym.name(), "\")");
    return;
    }

  switch (sym.type()) {
    case zenkit::DaedalusDataType::FLOAT: {
      if(ord==std::memory_order::release) {
        //TODO
        Log::e("Ikarus: unable to write to mapped symbol (\"", sym.name(), "\")");
        } else {
        auto val = sym.get_float();
        str._VTBL = floatBitsToInt(val); // first 4 bytes
        Log::d("VAR: ", sym.name(), " -> ", val);
        }
      break;
      }
    case zenkit::DaedalusDataType::INT: {
      if(ord==std::memory_order::release) {
        sym.set_int(str._VTBL);
        } else {
        auto val = sym.get_int();
        str._VTBL = val; // first 4 bytes
        }
      break;
      }
    case zenkit::DaedalusDataType::STRING: {
      if(ord==std::memory_order::release) {
        Log::e("Ikarus: unable to write to mapped symbol (\"", sym.name(), "\")");
        } else {
        auto& cstr = sym.get_string();
        memAssignString(str, cstr);
        // Log::d("VAR: ", sym.name(), " {", str.ptr, "} -> ", chr);
        }
      break;
      }
    default:
      Log::e("Ikarus: unable to map symbol (\"", sym.name(), "\") to virtual memory");
      break;
    }
  }

void Ikarus::memoryCallback(zCPar_Symbol& s, uint32_t index, std::memory_order ord) {
  if(index>=vm.symbols().size()) {
    // should never happend
    Log::e("ikarus: symbol table is corrupted");
    assert(false);
    return;
    }

  if(ord!=std::memory_order::acquire) {
    Log::e("ikarus: writes to symbol table is not implemented");
    return;
    }

  auto& sym = *vm.find_symbol_by_index(index);
  memAssignString(s.name, sym.name());
  s.next     = scriptSymbols + uint32_t(index+1)*uint32_t(sizeof(zCPar_Symbol)); // ???
  //---
  s.content  = 0; // TODO
  s.offset   = 0; // pointer to value
  //---
  s.bitfield = 0;
  s.filenr   = int32_t(sym.file_index());
  s.line     = int32_t(sym.line_start());
  s.line_anz = int32_t(sym.line_count());
  s.pos_beg  = int32_t(sym.char_start());
  s.pos_anz  = int32_t(sym.char_count());
  //---
  s.parent   = scriptSymbols + uint32_t(sym.parent())*uint32_t(sizeof(zCPar_Symbol));

  int32_t bitfield = 0;
  if(sym.is_const())
    bitfield |= zenkit::DaedalusSymbolFlag::CONST;
  if(sym.has_return())
    bitfield |= zenkit::DaedalusSymbolFlag::RETURN;
  if(sym.is_member())
    bitfield |= zenkit::DaedalusSymbolFlag::MEMBER;
  if(sym.is_external())
    bitfield |= zenkit::DaedalusSymbolFlag::EXTERNAL;
  if(sym.is_merged())
    bitfield |= zenkit::DaedalusSymbolFlag::MERGED;

  s.bitfield = 0;
  s.bitfield |= (int32_t(sym.type()) << 12);
  s.bitfield |= (bitfield << 16);
  // other flags are internal
  // Log::d("Ikarus: accessing symbol (\"", sym.name(), "\")");
  }

void Ikarus::memAssignString(zString& str, std::string_view cstr) {
  str.ptr = allocator.realloc(str.ptr, uint32_t(cstr.size()+1));
  str.len = int32_t(cstr.size());

  auto* chr  = reinterpret_cast<char*>(allocator.derefv(str.ptr, uint32_t(str.len)));
  std::memcpy(chr, cstr.data(), cstr.length());
  chr[str.len] = '\0';
  }

void Ikarus::memFromString(std::string& dst, const zString& str) {
  if(str.len<=0) {
    dst.clear();
    return;
    }

  const char* chr = reinterpret_cast<const char*>(allocator.deref(str.ptr, uint32_t(str.len)));
  dst.resize(size_t(str.len));
  std::memcpy(dst.data(), chr, dst.size());
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
  static bool enable = true;
  if(!enable)
    return;
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

zenkit::DaedalusNakedCall Ikarus::mem_callbyid(zenkit::DaedalusVm& vm) {
  const uint32_t symbId = uint32_t(vm.pop_int());
  auto* sym = vm.find_symbol_by_index(symbId);
  if(sym==nullptr || sym->type()!=zenkit::DaedalusDataType::FUNCTION) {
    Log::e("MEM_CallByID: Provided symbol is not callable (not function, prototype or instance): ", symbId);
    return zenkit::DaedalusNakedCall();
    }
  directCall(vm, *sym);
  return zenkit::DaedalusNakedCall();
  }

zenkit::DaedalusNakedCall Ikarus::mem_callbyptr(zenkit::DaedalusVm& vm) {
  auto address = vm.pop_int();
  //FIXME: map function into memory for real!
  uint32_t symbId = uint32_t(address);
  auto* sym = vm.find_symbol_by_index(uint32_t(symbId));
  if(sym==nullptr || sym->type()!=zenkit::DaedalusDataType::FUNCTION) {
    Log::e("MEM_CallByPtr: Provided symbol is not callable (not function, prototype or instance): ", symbId);
    return zenkit::DaedalusNakedCall();
    }
  directCall(vm, *sym);
  return zenkit::DaedalusNakedCall();
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
  Log::e("TODO: mem_searchvobbyname");
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
  // TODO: allow writes to mapped memory
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
  // TODO: do coherency maintanace for STR_FromChar
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
  std::string_view uname = "OpenGothic";

  const uint32_t max = pcbBuffer ? uint32_t(allocator.readInt(pcbBuffer)) : 0;
  if(auto ptr = reinterpret_cast<char*>(allocator.derefv(lpBuffer, max))) {
    if(max>=uname.size()) {
      std::strncpy(reinterpret_cast<char*>(ptr), "OpenGothic", max);
      ptr[uname.size()] = '\0';
      return 1;
      }
    }
  // buffer is too small
  allocator.writeInt(pcbBuffer, int32_t(uname.size()+1));
  return -1;
  }

void Ikarus::getlocaltime(ptr32_t lpSystemTime) {
  struct SystemTime {
    uint16_t wYear;
    uint16_t wMonth;
    uint16_t wDayOfWeek;
    uint16_t wDay;
    uint16_t wHour;
    uint16_t wMinute;
    uint16_t wSecond;
    uint16_t wMilliseconds;
    };

  if(auto ptr = allocator.deref<SystemTime>(lpSystemTime)) {
    const auto        timePoint = std::chrono::system_clock::now();
    const std::time_t time      = std::chrono::system_clock::to_time_t(timePoint);
    const auto        t         = std::localtime(&time);
    const auto        ms        = std::chrono::time_point_cast<std::chrono::milliseconds>(timePoint);

    ptr->wYear         = uint16_t(t->tm_year + 1900);
    ptr->wMonth        = uint16_t(t->tm_mon + 1);
    ptr->wDayOfWeek    = uint16_t(t->tm_wday);
    ptr->wDay          = uint16_t(t->tm_mday);
    ptr->wHour         = uint16_t(t->tm_hour);
    ptr->wMinute       = uint16_t(t->tm_min);
    ptr->wSecond       = uint16_t(t->tm_sec);
    ptr->wMilliseconds = uint16_t(ms.time_since_epoch().count()%1000);
    }
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

auto Ikarus::_takeref(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall {
  if(vm.top_is_reference()) {
    auto [ref, idx, context] = vm.pop_reference();
    if(idx!=0) {
      Log::e("Ikarus: _takeref - unable take reference to array element");
      vm.push_int(int32_t(0xBAD10000));
      return zenkit::DaedalusNakedCall();
      }
    const uint32_t id  = ref->index();
    const ptr32_t  ptr = scriptVariables + id*ptr32_t(sizeof(ScriptVar));
    vm.push_int(int32_t(ptr));
    return zenkit::DaedalusNakedCall();
    }

  auto symbol = vm.pop_int();
  auto sym    = vm.find_symbol_by_index(uint32_t(symbol));
  if(sym==nullptr) {
    Log::e("Ikarus: _takeref - unable to resolve symbol");
    vm.push_int(int32_t(0xBAD10000));
    return zenkit::DaedalusNakedCall();
    }

  if(sym->type()==zenkit::DaedalusDataType::INSTANCE) {
    auto inst = sym->get_instance().get();
    if(auto d = dynamic_cast<memory_instance*>(inst)) {
      const ptr32_t ptr = d->address;
      vm.push_int(int32_t(ptr));
      return zenkit::DaedalusNakedCall();
      }
    }

  Log::e("Ikarus: _takeref - not a memory-instance: ", sym->name());
  vm.push_int(int32_t(0xBAD20000));
  return zenkit::DaedalusNakedCall();
  }

void Ikarus::ASMINT_Init() {
  const int ASMINT_InternalStackSize = 1024;

  if(!ASMINT_InternalStack) {
    ASMINT_InternalStack = allocator.alloc(4 * ASMINT_InternalStackSize);
    }
  if(auto sym = vm.find_symbol_by_name("ASMINT_InternalStack")) {
    if(sym->type()==zenkit::DaedalusDataType::INT)
      sym->set_int(int32_t(ASMINT_InternalStack));
    }

  auto p = allocator.pin(&ASMINT_CallTarget, sizeof(ASMINT_CallTarget), "ASMINT_CallTarget");
  if(auto sym = vm.find_symbol_by_name("ASMINT_CallTarget")) {
    if(sym->type()==zenkit::DaedalusDataType::INT)
      sym->set_int(int32_t(p));
    }
  }

void Ikarus::ASMINT_CallMyExternal() {
  auto mem = allocator.deref(ASMINT_CallTarget);
  auto ins = reinterpret_cast<const uint8_t*>(std::get<0>(mem));
  auto len = std::get<1>(mem);

  cpu.exec(ASMINT_CallTarget, ins, len);
  }

std::string Ikarus::_pm_instName(int inst) {
  if(auto sym = vm.find_symbol_by_index(uint32_t(inst))) {
    return sym->name();
    }
  return "";
  }

int Ikarus::parserGetIndex(std::string name) {
  if(auto sym = vm.find_symbol_by_name(name)) {
    return int(sym->index());
    }
  return -1;
  }

auto Ikarus::memint_stackpushint(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall {
  return zenkit::DaedalusNakedCall();
  }

auto Ikarus::memint_stackpushinst(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall {
  int id = vm.pop_int();
  auto sym = vm.find_symbol_by_index(uint32_t(id));
  if(sym!=nullptr && sym->type()==zenkit::DaedalusDataType::INSTANCE) {
    vm.push_instance(sym->get_instance());
    return zenkit::DaedalusNakedCall();
    }
  vm.push_instance(nullptr);
  return zenkit::DaedalusNakedCall();
  }

void Ikarus::directCall(zenkit::DaedalusVm& vm, zenkit::DaedalusSymbol& func) {
  if(func.type()!=zenkit::DaedalusDataType::FUNCTION) {
    Log::e("Bas unsafe function call");
    return;
    }

  std::vector<zenkit::DaedalusSymbol*> params = vm.find_parameters_for_function(&func);
  if(params.size()>0)
    Log::d("");
  //vm.call_function(sym);
  //zenkit::StackGuard guard {&vm, func.rtype()};
  vm.unsafe_call(&func);
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

  auto rp = vm.instruction_at(vm.pc());
  if(len==0 || i==nullptr) {
    auto jmp = loopBacktrack[vm.pc()];
    vm.unsafe_jump(jmp-rp.size);
    return zenkit::DaedalusNakedCall();
    }
  // Log::i("repeat: ", i->get_int(), " < ", len);
  auto& pl = loopPayload[vm.pc()];
  pl.i   = i;
  pl.len = len;

  i->set_int(0);
  return zenkit::DaedalusNakedCall();
  }

zenkit::DaedalusNakedCall Ikarus::while_(zenkit::DaedalusVm& vm) {
  const int cond = vm.pop_int();
  // Log::i("while: ", cond);
  if(cond!=0) {
    return zenkit::DaedalusNakedCall();
    }
  auto rp  = vm.instruction_at(vm.pc());
  auto jmp = loopBacktrack[vm.pc()];
  vm.unsafe_jump(jmp-rp.size);
  return zenkit::DaedalusNakedCall();
  }

void Ikarus::loop_trap(zenkit::DaedalusSymbol* i) {
  auto instr = vm.instruction_at(vm.pc());
  if(instr.op != zenkit::DaedalusOpcode::PUSHV)
    return; // Ikarus keywords are always use pushv

  auto end = vm.find_symbol_by_name("END");
  if(end!=nullptr && instr.symbol==end->index()) {
    auto bt = loopBacktrack[vm.pc()];

    if(loopPayload.find(bt)==loopPayload.end()) {
      // while
      vm.unsafe_jump(bt-instr.size);
      return;
      }

    auto& pl = loopPayload[bt];
    if(pl.i==nullptr) {
      Log::e("bad loop end");
      assert(0);
      return;
      }
    const int32_t i = pl.i->get_int();
    pl.i->set_int(i+1);
    if(i+1 < pl.len) {
      const uint32_t BLsize = 5;
      vm.unsafe_jump(bt-instr.size+BLsize);
      } else {
      loopPayload.erase(bt);
      }
    return;
    }
  Log::e("bad loop trap");
  assert(0);
  }

void Ikarus::setupIkarusLoops() {
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

  auto end       = vm.find_symbol_by_name("END");
  auto rep       = vm.find_symbol_by_name("REPEAT");
  auto whl       = vm.find_symbol_by_name("WHILE");
  auto continue_ = vm.find_symbol_by_name("CONTINUE");

  uint32_t endIdx  = (end!=nullptr       ? end->index()       : uint32_t(-1));
  uint32_t cntIdx  = (continue_!=nullptr ? continue_->index() : uint32_t(-1));
  uint32_t repAddr = (rep!=nullptr       ? rep->address()     : uint32_t(-1));
  uint32_t whlAddr = (whl!=nullptr       ? whl->address()     : uint32_t(-1));

  // auto cont = vm.find_symbol_by_name("CONTINUE");

  // ## Control-flow ##
  vm.override_function("repeat", [this](zenkit::DaedalusVm& vm) { return repeat(vm);    });
  vm.override_function("while",  [this](zenkit::DaedalusVm& vm) { return while_(vm);    });
  vm.register_access_trap([this](zenkit::DaedalusSymbol& i)     { return loop_trap(&i); });

  for(auto& i:vm.symbols()) {
    if(i.type()!=zenkit::DaedalusDataType::FUNCTION)
      continue;
    }

  struct LoopDecr {
    uint32_t pc      = 0;
    uint32_t backJmp = 0;
    };

  std::vector<LoopDecr> loopStk;
  for(uint32_t i=0; i<vm.size();) {
    const auto pc = i;
    auto instr = vm.instruction_at(i);
    i += instr.size;

    if(instr.op==zenkit::DaedalusOpcode::PUSHV && instr.symbol==endIdx) {
      auto loop = loopStk.back();
      loopBacktrack[loop.pc] = i; // loop exit
      loopBacktrack[pc]      = loop.backJmp;
      loopStk.pop_back();
      }
    else if(instr.op==zenkit::DaedalusOpcode::PUSHV && instr.symbol==cntIdx) {
      if(!loopStk.empty()) {
        auto func = findSymbolByAddress(pc); (void)func;
        auto loop = loopStk.back();
        loopBacktrack[pc] = loop.pc;
        }
      }
    else if(instr.op==zenkit::DaedalusOpcode::BL && (instr.address==repAddr || instr.address==whlAddr)) {
      LoopDecr d;
      if(instr.address==whlAddr) {
        d.backJmp = traceBackExpression(vm, pc);
        d.pc      = pc;
        }
      else if(instr.address==repAddr) {
        d.pc      = pc;
        d.backJmp = pc;
        }
      loopStk.push_back(d);
      }
    }
  }

uint32_t Ikarus::traceBackExpression(zenkit::DaedalusVm& vm, uint32_t pc) {
  /*
   NOTE: can be expression based 'while';
   while(var){}
     PUSHV @var
     BL @while

   while(i < l) {}
     PUSHV @l
     PUSHV @i
     LT
     BL

    handles by MEMINT_TraceParameter in ikarus
  */

  auto func = findSymbolByAddress(pc);
  if(func==nullptr)
    return uint32_t(-1); //error

  std::vector<zenkit::DaedalusInstruction> icodes;
  for(uint32_t i=func->address(); i<pc && i<vm.size();) {
    auto instr = vm.instruction_at(i);
    i += instr.size;
    icodes.push_back(instr);
    }

  if(icodes.empty())
    return uint32_t(-1); //error

  int paramsNeeded = 1;
  size_t at = icodes.size();
  while(paramsNeeded>0 && at>0) {
    --at;
    const auto instr = icodes[at];
    if(zenkit::DaedalusOpcode::ADD <= instr.op && instr.op <= zenkit::DaedalusOpcode::DIVMOVI)
      paramsNeeded += 1;
    else if(instr.op==zenkit::DaedalusOpcode::PUSHI || instr.op==zenkit::DaedalusOpcode::PUSHV ||
            instr.op==zenkit::DaedalusOpcode::PUSHVI) {
      paramsNeeded -= 1;
      }
    else if(instr.op==zenkit::DaedalusOpcode::BL) {
      auto sym = vm.find_symbol_by_address(instr.address);
      if(sym==nullptr)
        return uint32_t(-1); //error

      paramsNeeded += sym->count();
      if(sym->has_return())
        paramsNeeded -= 1;
      }
    else {
      // non handled
      return uint32_t(-1); //error
      }
    }

  if(paramsNeeded!=0)
    return uint32_t(-1); //error

  pc = func->address();
  for(uint32_t i=0; i<at; ++i) {
    auto instr = vm.instruction_at(pc);
    pc += instr.size;
    }

  return pc;
  }

void Ikarus::setupFunctionTable() {
  for(auto& i:vm.symbols()) {
    if(i.type()!=zenkit::DaedalusDataType::FUNCTION)
      continue;
    if(i.address()==0)
      continue;
    symbolsByAddress.push_back(&i);
    }

  std::sort(symbolsByAddress.begin(), symbolsByAddress.end(), [](const zenkit::DaedalusSymbol* l, const zenkit::DaedalusSymbol* r){
    return l->address() < r->address();
    });
  }

const zenkit::DaedalusSymbol* Ikarus::findSymbolByAddress(uint32_t addr) {
  auto fn = std::lower_bound(symbolsByAddress.begin(), symbolsByAddress.end(), addr, [](const zenkit::DaedalusSymbol* l, uint32_t r){
    return l->address()<r;
    });
  //auto id = std::distance(symbolsByAddress.begin(), fn); (void)id;
  if(fn==symbolsByAddress.end())
    return symbolsByAddress.back();
  if((*fn)->address()==addr)
    return *fn;
  if(fn==symbolsByAddress.begin())
    return nullptr;
  --fn;
  return *fn;
  }
