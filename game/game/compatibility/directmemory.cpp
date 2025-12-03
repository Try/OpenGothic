#include "directmemory.h"

#include <zenkit/DaedalusScript.hh>
#include <Tempest/Log>

#include <cassert>
#include <charconv>

#include "gothic.h"

using namespace Tempest;
using namespace Compatibility;

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

void DirectMemory::memory_instance::set_int(const zenkit::DaedalusSymbol& sym, uint16_t index, int32_t value) {
  if(sym.name()=="ZCARRAY.NUMINARRAY" && value>1024)
    Log::d("");
  ptr32_t addr = address + ptr32_t(sym.offset_as_member()) + ptr32_t(index*sym.class_size());
  owner.mem32.writeInt(addr, value);
  }

int32_t DirectMemory::memory_instance::get_int(const zenkit::DaedalusSymbol& sym, uint16_t index) const {
  ptr32_t addr = address + ptr32_t(sym.offset_as_member()) + ptr32_t(index * sym.class_size());
  int32_t v = owner.mem32.readInt(addr);
  return v;
  }

void DirectMemory::memory_instance::set_float(const zenkit::DaedalusSymbol& sym, uint16_t index, float value) {
  ptr32_t addr = address + ptr32_t(sym.offset_as_member()) + ptr32_t(index*sym.class_size());
  owner.mem32.writeInt(addr, *reinterpret_cast<const int32_t*>(&value));
  }

float DirectMemory::memory_instance::get_float(const zenkit::DaedalusSymbol& sym, uint16_t index) const {
  ptr32_t addr = address + ptr32_t(sym.offset_as_member()) + ptr32_t(index * sym.class_size());
  int32_t v = owner.mem32.readInt(addr);

  float f = 0;
  std::memcpy(&f, &v, 4);
  return f;
  }

void DirectMemory::memory_instance::set_string(const zenkit::DaedalusSymbol& sym, uint16_t index, std::string_view value) {
  ptr32_t addr = address + ptr32_t(sym.offset_as_member()) + ptr32_t(index*sym.class_size());
  (void)addr;
  Log::d("TODO: memory_instance::set_string ", sym.name());
  // allocator.writeInt(addr, 0);
  }

const std::string& DirectMemory::memory_instance::get_string(const zenkit::DaedalusSymbol& sym, uint16_t index) const {
  ptr32_t addr = address + ptr32_t(sym.offset_as_member()) + ptr32_t(index*sym.class_size());

  if(auto zs = owner.mem32.deref<const zString>(addr)) {
    static std::string ret;
    owner.memFromString(ret, *zs);
    return ret;
    }

  Log::d("TODO: memory_instance::get_string ", sym.name());
  (void)addr;
  static std::string empty;
  return empty;
  }


DirectMemory::DirectMemory(GameScript& owner, zenkit::DaedalusVm& vm) : gameScript(owner), vm(vm), cpu(*this, mem32) {
  if(auto v = vm.find_symbol_by_name("Ikarus_Version")) {
    const int version = v->type()==zenkit::DaedalusDataType::INT ? v->get_int() : 0;
    Log::i("DMA mod detected: Ikarus v", version);
    }
  if(auto v = vm.find_symbol_by_name("LeGo_Version")) {
    const char* version = v->type()==zenkit::DaedalusDataType::STRING ? v->get_string().c_str() : "LeGo";
    Log::i("DMA mod detected: ", version);
    }

  setupFunctionTable();
  setupIkarusLoops();
  setupEngineText();
  setupEngineMemory();

  // Inline ASM
  vm.override_function("ASMINT_Init",           [this]() { ASMINT_Init(); });
  vm.override_function("ASMINT_MyExternal",     [](){});
  vm.override_function("ASMINT_CallMyExternal", [this]() { ASMINT_CallMyExternal(); });

  // Pointers
  setupMemoryFunctions();

  // Dynamic calls by id's
  setupDirectFunctions();

  setupWinapiFunctions();

  setupUtilityFunctions();

  // LeGo hooks
  setupHookEngine();

  setupMathFunctions();
  setupStringsFunctions();

  setupzCParserFunctions();

  setupInitFileFunctions();
  setupUiFunctions();
  setupFontFunctions();
  setupNpcFunctions();

  // various
  vm.override_function("MEM_PrintStackTrace", [this](){ memPrintstacktraceImplementation(); });
  vm.override_function("MEM_SendToSpy",       [this](int cat, std::string_view msg){ return memSendToSpy(cat,msg); });
  vm.override_function("MEM_ReplaceFunc",     [this](zenkit::DaedalusFunction dest, zenkit::DaedalusFunction func){ memReplaceFunc(dest, func); });
  //
  vm.override_function("MEMINT_SetupExceptionHandler",   [](){ });
  vm.override_function("MEMINT_ReplaceSlowFunctions",    [](){ });
  //vm.override_function("MEMINT_ReplaceLoggingFunctions", [](){ });
  vm.override_function("MEM_InitStatArrs",               [](){ });
  vm.override_function("MEM_InitRepeat",                 [](){ });
  vm.override_function("MEM_GetCommandLine",             [](){ return std::string(""); });

  vm.override_function("_RENDER_INIT", []() {
    // unclear how exactly it should behave - need to find testing sample
    Log::e("not implemented call [_RENDER_INIT]");
    });
  vm.override_function("PRINT_FIXPS", [](){
    // function patches asm code of zCView::PrintTimed* to fix text coloring - we can ignore it
    });

  // Disable some high-level functions, until basic stuff is done
  auto dummyfy = [&](std::string_view name, auto hook) {
    auto f = vm.find_symbol_by_name(name);
    if(f==nullptr || f->type()!=zenkit::DaedalusDataType::FUNCTION)
      return;
    vm.override_function(name, hook);
    };
  dummyfy("WRITENOP",    [](int,int){}); // hook-related mess
  dummyfy("MEM_SETKEYS", [](std::string_view,int,int){});
  dummyfy("INIT_QUIVERS_ALWAYS", [](){}); // requires sorted npc list
  }

bool DirectMemory::isRequired(zenkit::DaedalusScript& vm) {
  return
      vm.find_symbol_by_name("Ikarus_Version") != nullptr &&
      vm.find_symbol_by_name("MEM_InitAll") != nullptr &&
      vm.find_symbol_by_name("MEM_ReadInt") != nullptr &&
      vm.find_symbol_by_name("MEM_WriteInt") != nullptr &&
      vm.find_symbol_by_name("_@") != nullptr &&
      vm.find_symbol_by_name("_^") != nullptr;
  }

void DirectMemory::tick(uint64_t dt) {
  //TODO: propper hook-engine
  if(auto* sym = vm.find_symbol_by_name("_FF_Hook")) {
    vm.call_function(sym);
    return;
    }
  }

void DirectMemory::eventPlayAni(std::string_view ani) {
  if(ani.find("CALL ")!=0)
    return;
  Log::d("LeGo::eventPlayAni: ", ani);

  if(5<ani.size()) {
    if(std::isdigit(ani[5])) {
      uint32_t fncID = 0;
      auto err = std::from_chars(ani.data()+5, ani.data()+ani.size(), fncID).ec;
      if(err!=std::errc())
        return;
      if(auto* sym = vm.find_symbol_by_index(fncID)) {
        vm.call_function(sym);
        }
      }
    }
  }

void DirectMemory::setupFunctionTable() {
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

void DirectMemory::setupIkarusLoops() {
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
  vm.override_function("repeat",   [this](zenkit::DaedalusVm& vm) { return repeat(vm);    });
  vm.override_function("while",    [this](zenkit::DaedalusVm& vm) { return while_(vm);    });
  vm.override_function("mem_goto", [this](zenkit::DaedalusVm& vm) { return mem_goto(vm);  });
  vm.register_access_trap([this](zenkit::DaedalusSymbol& i) { return loop_trap(&i); });

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

void DirectMemory::setupEngineMemory() {
  mem32.setCallbackR(Mem32::Type::zCParser, [this](zCParser& p, uint32_t){ memoryCallback(p, std::memory_order::acquire); });
  mem32.setCallbackW(Mem32::Type::zCParser, [this](zCParser& p, uint32_t){ memoryCallback(p, std::memory_order::release); });

  mem32.setCallbackR(Mem32::Type::zCParser_variables, [this](ScriptVar& v, uint32_t id) {
    memoryCallback(v, id, std::memory_order::acquire);
    });
  mem32.setCallbackW(Mem32::Type::zCParser_variables, [this](ScriptVar& v, uint32_t id) {
    memoryCallback(v, id, std::memory_order::release);
    });
  mem32.setCallbackR(Mem32::Type::zCPar_Symbol, [this](zCPar_Symbol& p, uint32_t id) {
    memoryCallback(p, id, std::memory_order::acquire);
    });
  mem32.setCallbackW(Mem32::Type::zCPar_Symbol, [this](zCPar_Symbol& p, uint32_t id) {
    memoryCallback(p, id, std::memory_order::release);
    });

  const ptr32_t BAD_BUILTIN_PTR                = 0xBAD40000;
  const ptr32_t GothicFirstInstructionAddress  = 4198400;
  const ptr32_t MEMINT_oGame_Pointer_Address   = 11208836; //0xAB0884
  const ptr32_t MEMINT_zTimer_Address          = 10073044; //0x99B3D4
  const ptr32_t MEMINT_gameMan_Pointer_Address = 9185624;  //0x8C2958
  const ptr32_t ZFACTORY                       = 9276912;
  const ptr32_t ContentParserAddress           = 11223232;
  const ptr32_t ZFONTMAN                       = 11221460;
  const ptr32_t OCNPCFOCUS__FOCUSLIST_G2       = 11208440;

  mem32.pin(&versionHint,   GothicFirstInstructionAddress,  4, "MEMINT_ReportVersionCheck");
  mem32.pin(&oGame_Pointer, MEMINT_oGame_Pointer_Address,   4, "oGame*");
  mem32.pin(&gameman_Ptr,   MEMINT_gameMan_Pointer_Address, 4, "GameMan*");
  mem32.pin(&zTimer,        MEMINT_zTimer_Address,          sizeof(zTimer), "zTimer");
  mem32.pin(&zFactory_Ptr,  ZFACTORY,                       4, "zFactory*");
  mem32.pin(&fontMan_Ptr,   ZFONTMAN,                       4, "zCFontMan*");
  mem32.pin(ContentParserAddress, sizeof(zCParser), Mem32::Type::zCParser);
  mem32.pin(&focusList, OCNPCFOCUS__FOCUSLIST_G2, sizeof(focusList), "OCNPCFOCUS__FOCUSLIST_G2");

  const ptr32_t INGAME_MENU_INSTANCE = 8980576;
  mem32.pin(menuName, INGAME_MENU_INSTANCE, sizeof(menuName), "MENU_NAME");
  std::strncpy(menuName, "MENU_MAIN", sizeof(menuName));

  const ptr32_t ZERRPTR = 9231568;
  mem32.alloc(ZERRPTR, sizeof(zError));

  const ptr32_t LODENABLED = 8596020;
  mem32.alloc(LODENABLED, 4, "LODENABLED"); // can always ignore that

  const ptr32_t AMBIENTVOBSENABLED = 9079488;
  mem32.alloc(AMBIENTVOBSENABLED, 4, "AMBIENTVOBSENABLED");

  const ptr32_t GAME_HOLDTIME_ADDRESS = 11208840;
  mem32.alloc(GAME_HOLDTIME_ADDRESS, 4, "GAME_HOLDTIME_ADDRESS"); // need to investigate how exactly it affects game

  if(auto p = mem32.deref<std::array<ptr32_t,6>>(OCNPCFOCUS__FOCUSLIST_G2)) {
    gameScript.focusMage();
    (*p)[5] = BAD_BUILTIN_PTR; //TODO: pick-lock spells
    }

  // Trialog: need to implement reinterpret_cast to avoid in script-exception
  //const ptr32_t SPAWN_INSERTRANGE = 9153744;
  //mem32.pin(&spawnRange, SPAWN_INSERTRANGE, sizeof(spawnRange), "spawnRange");

  // Storage for local variables, so script may address them thru pointers
  scriptVariables = mem32.alloc(uint32_t(vm.symbols().size() * sizeof(ScriptVar)),    Mem32::Type::zCParser_variables);
  scriptSymbols   = mem32.alloc(uint32_t(vm.symbols().size() * sizeof(zCPar_Symbol)), Mem32::Type::zCPar_Symbol);

  // Built-in data without assumed address
  oGame_Pointer = mem32.pin(&memGame, sizeof(memGame), "oGame");
  memGame._ZCSESSION_WORLD = mem32.alloc(sizeof(oWorld));
  memGame.WLDTIMER         = BAD_BUILTIN_PTR;
  memGame.TIMESTEP         = 1; // used as boolean in anim8

  auto& mem_world = *mem32.deref<oWorld>(memGame._ZCSESSION_WORLD);
  mem_world.WAYNET       = BAD_BUILTIN_PTR; //TODO: add implement some proxy to waynet
  mem_world.VOBLIST_NPCS = 0; //BAD_BUILTIN_PTR; // zCListSort*

  gameman_Ptr = mem32.alloc(sizeof(GameMgr));
  if(auto v = vm.find_symbol_by_name("CurrSymbolTableLength")) {
    v->set_int(int(vm.symbols().size()));
    }

  fontMan_Ptr = mem32.alloc(sizeof(zCFontMan));

  // ## UI data
  memGame.HPBAR    = mem32.alloc(sizeof(oCViewStatusBar));
  memGame.MANABAR  = mem32.alloc(sizeof(oCViewStatusBar));
  memGame.FOCUSBAR = mem32.alloc(sizeof(oCViewStatusBar));

  mem32.deref<oCViewStatusBar>(memGame.HPBAR)   ->RANGE_BAR = mem32.alloc(sizeof(zCView));
  mem32.deref<oCViewStatusBar>(memGame.HPBAR)   ->VALUE_BAR = mem32.alloc(sizeof(zCView));
  mem32.deref<oCViewStatusBar>(memGame.FOCUSBAR)->RANGE_BAR = mem32.alloc(sizeof(zCView));
  mem32.deref<oCViewStatusBar>(memGame.FOCUSBAR)->VALUE_BAR = mem32.alloc(sizeof(zCView));
  }

void DirectMemory::setupEngineText() {
  // pin functions - CoM and some other mods rewriting assembly for them
  const uint32_t OCNPC__ENABLE_EQUIPBESTWEAPONS = 7626662;
  mem32.alloc(OCNPC__ENABLE_EQUIPBESTWEAPONS, 18, "OCNPC__ENABLE_EQUIPBESTWEAPONS");

  const uint32_t OCNPC__GETNEXTENEMY = 7556941;
  mem32.alloc(OCNPC__GETNEXTENEMY, 48, "OCNPC__GETNEXTENEMY");

  const uint32_t OCITEMCONTAINER__CHECKSELECTEDITEM_ISACTIVE = 7378665;
  mem32.alloc(OCITEMCONTAINER__CHECKSELECTEDITEM_ISACTIVE, 5, ".text");

  const uint32_t OCITEMCONTAINER__CHECKSELECTEDITEM_ISACTIVEP = 7378700;
  mem32.alloc(OCITEMCONTAINER__CHECKSELECTEDITEM_ISACTIVEP, 5, ".text");

  const int OCSTEALCONTAINER__CREATELIST_ISARMOR_SP18 = 7384908;
  mem32.alloc(OCSTEALCONTAINER__CREATELIST_ISARMOR_SP18, 8, ".text");

  const int OCNPCCONTAINER__CREATELIST_ISARMOR_SP18 = 7386812;
  mem32.alloc(OCNPCCONTAINER__CREATELIST_ISARMOR_SP18, 8, ".text");

  const int OCSTEALCONTAINER__CREATELIST_ISARMOR = 7384900;
  mem32.alloc(OCSTEALCONTAINER__CREATELIST_ISARMOR, 8, ".text");

  const int OCNPCCONTAINER__CREATELIST_ISARMOR = 7386805;
  mem32.alloc(OCNPCCONTAINER__CREATELIST_ISARMOR, 5, ".text");

  const int OCNPCCONTAINER__HANDLEEVENT_ISEMPTY = 7387581;
  mem32.alloc(OCNPCCONTAINER__HANDLEEVENT_ISEMPTY, 5, ".text");

  const int OCNPCINVENTORY__HANDLEEVENT_KEYWEAPONJZ = 7402077;
  mem32.alloc(OCNPCINVENTORY__HANDLEEVENT_KEYWEAPONJZ, 4, ".text");

  const int OCNPCINVENTORY__HANDLEEVENT_KEYWEAPON = 7402065;
  mem32.alloc(OCNPCINVENTORY__HANDLEEVENT_KEYWEAPON, 8, ".text");

  const int OCAIHUMAN__CHANGECAMMODEBYSITUATION_SWITCHMOBCAM = 6935573;
  mem32.alloc(OCAIHUMAN__CHANGECAMMODEBYSITUATION_SWITCHMOBCAM, 8, ".text");

  // unknown, code segment used by INIT_RESTOREMOBFIRESTATES in CoM
  const int INIT_RESTOREMOBFIRESTATES_P0 = 7482368;
  const int INIT_RESTOREMOBFIRESTATES_P1 = 7482688;
  mem32.alloc(INIT_RESTOREMOBFIRESTATES_P0, 4, ".text");
  mem32.alloc(INIT_RESTOREMOBFIRESTATES_P1, 4, ".text");
  }

zenkit::DaedalusNakedCall DirectMemory::repeat(zenkit::DaedalusVm& vm) {
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
  if(len>1024)
    Log::d("");
  return zenkit::DaedalusNakedCall();
  }

zenkit::DaedalusNakedCall DirectMemory::while_(zenkit::DaedalusVm& vm) {
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

zenkit::DaedalusNakedCall DirectMemory::mem_goto(zenkit::DaedalusVm& vm) {
  const int idx = vm.pop_int();

  auto at   = vm.pc();
  auto func = findSymbolByAddress(at);
  if(func==nullptr) {
    Log::e("Ikarus: invalid vm state");
    return zenkit::DaedalusNakedCall();
    }

  auto     rp      = vm.instruction_at(at);
  auto     label   = vm.find_symbol_by_name("mem_label");
  uint32_t lblAddr = (label!=nullptr ? label->address() : uint32_t(-1));
  uint32_t pc      = func->address();

  for(uint32_t i=0, prev=0; i<at; ++i) {
    auto instr = vm.instruction_at(pc);
    if(instr.op==zenkit::DaedalusOpcode::BL && instr.address==lblAddr) {
      auto lId = vm.instruction_at(prev);
      if(lId.immediate==idx) {
        vm.unsafe_jump(pc + instr.size - rp.size);
        return zenkit::DaedalusNakedCall();
        }
      }
    prev = pc;
    pc += instr.size;
    }

  Log::e("Ikarus: invalid goto");
  return zenkit::DaedalusNakedCall();
  }

void DirectMemory::loop_trap(zenkit::DaedalusSymbol* i) {
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

  if(loopBacktrack.find(vm.pc())!=loopBacktrack.end()) {
    Log::e("bad loop trap");
    assert(0);
    }
  }

uint32_t DirectMemory::traceBackExpression(zenkit::DaedalusVm& vm, uint32_t pc) {
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

const zenkit::DaedalusSymbol* DirectMemory::findSymbolByAddress(uint32_t addr) {
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

std::string_view DirectMemory::demangleAddress(uint32_t addr){
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

void DirectMemory::memoryCallback(zCParser& p, std::memory_order ord) {
  if(ord==std::memory_order::acquire) {
    if(p.symtab_table.ptr==0) {
      // NOTE: initialize once
      // currSymbolTableAddress / currSymbolTableLength
      p.symtab_table.numInArray = int32_t(vm.symbols().size());
      p.symtab_table.numAlloc   = int32_t(nextPot(uint32_t(p.symtab_table.numInArray)));
      p.symtab_table.ptr        = mem32.realloc(p.symtab_table.ptr, uint32_t(p.symtab_table.numAlloc)*uint32_t(sizeof(uint32_t)));      
      auto v = mem32.deref<ptr32_t>(p.symtab_table.ptr);
      for(int32_t i=0; i<p.symtab_table.numInArray; ++i) {
        v[i] = scriptSymbols + uint32_t(i)*uint32_t(sizeof(zCPar_Symbol));
        }

      // TODO: currSortedSymbolTableAddress
      // TODO: currParserStackAddress
      }

    p.stack_stack = 0; //TODO: map insteructions into mem32?

    auto trap = vm.instruction_at(vm.pc());
    p.stack_stackptr = vm.pc() + trap.size;
    }
  else {
    auto instr = vm.instruction_at(vm.pc());

    auto src = findSymbolByAddress(vm.pc() - instr.size);
    auto dst = findSymbolByAddress(p.stack_stackptr);

    if(src!=dst || dst==nullptr) {
      if(src!=nullptr && dst!=nullptr)
        Log::e("FIXME: unsafe jump: `\"", src->name(), "\" -> \"", dst->name(), "\""); else
        Log::e("FIXME: unsafe jump!");
      return;
      }
    vm.unsafe_jump(p.stack_stackptr - instr.size);
    }
  }

void DirectMemory::memoryCallback(ScriptVar& v, uint32_t index, std::memory_order ord) {
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
        //NOTE: since string is composite object, mixing reads and writes can cause bugs
        /*
        std::string dst;
        memFromString(dst, str);
        sym.set_string(dst);
        Log::d("VAR: ", sym.name(), " {", str.ptr, "} <- ", dst);
        */
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

void DirectMemory::memoryCallback(zCPar_Symbol& s, uint32_t index, std::memory_order ord) {
  if(index>=vm.symbols().size()) {
    // should never happend
    Log::e("ikarus: symbol table is corrupted");
    assert(false);
    return;
    }

  if(ord!=std::memory_order::acquire) {
    Log::e("ikarus: write to symbol table is not implemented");
    return;
    }

  auto symBitfield = [](zenkit::DaedalusSymbol& sym) {
    int32_t flags = 0;
    if(sym.is_const())
      flags |= zenkit::DaedalusSymbolFlag::CONST;
    if(sym.has_return())
      flags |= zenkit::DaedalusSymbolFlag::RETURN;
    if(sym.is_member())
      flags |= zenkit::DaedalusSymbolFlag::MEMBER;
    if(sym.is_external())
      flags |= zenkit::DaedalusSymbolFlag::EXTERNAL;
    if(sym.is_merged())
      flags |= zenkit::DaedalusSymbolFlag::MERGED;

    int32_t bitfield = 0;
    bitfield |= (sym.count() & 0xFFF);       // 12 bits
    bitfield |= (int32_t(sym.type()) << 12); // 4 bits
    bitfield |= (flags << 16);               // 6 bits
    return bitfield;
    };

  auto symOffset = [](zenkit::DaedalusSymbol& sym) {
    if(sym.is_member()) {
      return int32_t(sym.offset_as_member());
      }
    else if(sym.type()==zenkit::DaedalusDataType::CLASS) {
      // see 'create' in LeGo
      return int32_t(sym.class_size());
      }
    else if(sym.type()==zenkit::DaedalusDataType::FUNCTION) {
      // return int32_t(sym.address());
      return int32_t(sym.rtype());
      }
     // TODO: pointer to value
    return 0;
    };

  auto symContent = [](zenkit::DaedalusSymbol& sym) {
    switch (sym.type()) {
      case zenkit::DaedalusDataType::VOID:
        return 0;
      case zenkit::DaedalusDataType::FLOAT:
        return floatBitsToInt(sym.get_float());
      case zenkit::DaedalusDataType::INT:
        return int32_t(sym.get_int());
      case zenkit::DaedalusDataType::FUNCTION:
        if(sym.is_const())
          return int32_t(sym.address());
        return int32_t(sym.get_int());
      case zenkit::DaedalusDataType::STRING:
      case zenkit::DaedalusDataType::CLASS:
      case zenkit::DaedalusDataType::PROTOTYPE:
      case zenkit::DaedalusDataType::INSTANCE:
        //TODO
        return 0;
      }
    return 0;
    };

  auto& sym = *vm.find_symbol_by_index(index);
  memAssignString(s.name, sym.name());
  s.next     = scriptSymbols + uint32_t(index+1)*uint32_t(sizeof(zCPar_Symbol)); // ???
  //---
  s.content  = symContent(sym);
  s.offset   = symOffset(sym);
  //---
  s.bitfield = symBitfield(sym);
  s.filenr   = int32_t(sym.file_index());
  s.line     = int32_t(sym.line_start());
  s.line_anz = int32_t(sym.line_count());
  s.pos_beg  = int32_t(sym.char_start());
  s.pos_anz  = int32_t(sym.char_count());
  //---
  s.parent   = scriptSymbols + uint32_t(sym.parent())*uint32_t(sizeof(zCPar_Symbol));

  // other flags are internal
  // Log::d("Ikarus: accessing symbol (\"", sym.name(), "\")");
  }

void DirectMemory::memAssignString(zString& str, std::string_view cstr) {
  str.ptr = mem32.realloc(str.ptr, uint32_t(cstr.size()+1));
  str.len = int32_t(cstr.size());

  auto* chr  = reinterpret_cast<char*>(mem32.derefv(str.ptr, uint32_t(str.len)));
  std::memcpy(chr, cstr.data(), cstr.length());
  chr[str.len] = '\0';
  }

void DirectMemory::memFromString(std::string& dst, const zString& str) {
  if(str.len<=0) {
    dst.clear();
    return;
    }

  if(const char* chr = reinterpret_cast<const char*>(mem32.deref(str.ptr, uint32_t(str.len)))) {
    dst.resize(size_t(str.len));
    std::memcpy(dst.data(), chr, dst.size());
    }
  }


void DirectMemory::memPrintstacktraceImplementation() {
  static bool enable = true;
  if(!enable)
    return;
  Log::e("[start of stacktrace]");
  vm.print_stack_trace();
  Log::e("[end of stacktrace]");
  }

void DirectMemory::memSendToSpy(int cat, std::string_view msg) {
  Log::d("[zpy]: ", msg);
  }

void DirectMemory::memReplaceFunc(zenkit::DaedalusFunction dest, zenkit::DaedalusFunction func) {
  auto* sf  = func.value;
  auto* sd  = dest.value;
  if(sd->name()=="MEM_SENDTOSPY")
    return;
  if(sd->name()=="MEM_PRINTSTACKTRACE")
    return;
  Log::d("mem_replacefunc: ",sd->name()," -> ",sf->name());
  }

void DirectMemory::ASMINT_Init() {
  const int ASMINT_InternalStackSize = 1024;

  if(!ASMINT_InternalStack) {
    ASMINT_InternalStack = mem32.alloc(4 * ASMINT_InternalStackSize);
    }
  if(auto sym = vm.find_symbol_by_name("ASMINT_InternalStack")) {
    if(sym->type()==zenkit::DaedalusDataType::INT)
      sym->set_int(int32_t(ASMINT_InternalStack));
    }

  auto p = mem32.pin(&ASMINT_CallTarget, sizeof(ASMINT_CallTarget), "ASMINT_CallTarget");
  if(auto sym = vm.find_symbol_by_name("ASMINT_CallTarget")) {
    if(sym->type()==zenkit::DaedalusDataType::INT)
      sym->set_int(int32_t(p));
    }
  }

void DirectMemory::ASMINT_CallMyExternal() {
  auto mem = mem32.deref(ASMINT_CallTarget);
  auto ins = reinterpret_cast<const uint8_t*>(std::get<0>(mem));
  auto len = std::get<1>(mem);

  cpu.exec(ASMINT_CallTarget, ins, len);
  }

void DirectMemory::setupMemoryFunctions() {
  vm.override_function("MEM_GetAddress_Init",  [    ](){ });

  vm.override_function("MEM_PtrToInst",        [this](int address)           { return mem_ptrtoinst(ptr32_t(address)); });
  vm.override_function("_^",                   [this](int address)           { return mem_ptrtoinst(ptr32_t(address)); });
  vm.override_function("MEM_InstToPtr",        [this](int index)             { return mem_insttoptr(index); });
  vm.override_function("MEM_GetIntAddress",    [this](zenkit::DaedalusVm& vm){ return _takeref(vm);         });
  vm.override_function("_@",                   [this](zenkit::DaedalusVm& vm){ return _takeref(vm);         });
  vm.override_function("MEM_GetStringAddress", [this](zenkit::DaedalusVm& vm){ return _takeref(vm);         });
  vm.override_function("_@s",                  [this](zenkit::DaedalusVm& vm){ return _takeref(vm);         });
  vm.override_function("MEM_GetFloatAddress",  [this](zenkit::DaedalusVm& vm){ return _takeref(vm);         });
  vm.override_function("_@f",                  [this](zenkit::DaedalusVm& vm){ return _takeref(vm);         });

  vm.override_function("MEM_ReadInt",          [this](int address){ return mem_readint(address);                  });
  vm.override_function("MEM_WriteInt",         [this](int address, int val){ mem_writeint(address, val);          });
  vm.override_function("MEM_CopyBytes",        [this](int src, int dst, int size){ mem_copybytes(src, dst, size); });
  vm.override_function("MEM_ReadString",       [this](int sym){ return mem_readstring(sym);           });
  vm.override_function("MEM_ReadStatArr",      [this](zenkit::DaedalusVm& vm){ return mem_readstatarr(vm); });

  // ## MEM_Alloc and MEM_Free ##
  vm.override_function("MEM_Alloc",   [this](int amount )                      { return mem_alloc(amount);               });
  vm.override_function("MEM_Free",    [this](int address)                      { mem_free(address);                      });
  vm.override_function("MEM_Realloc", [this](int address, int oldsz, int size) { return mem_realloc(address,oldsz,size); });
  }

auto DirectMemory::_takeref(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall {
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

std::shared_ptr<zenkit::DaedalusInstance> DirectMemory::mem_ptrtoinst(ptr32_t address) {
  if(address==0)
    Log::d("mem_ptrtoinst: address is null");
  return std::make_shared<memory_instance>(*this, address);
  }

int DirectMemory::mem_insttoptr(int index) {
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

int DirectMemory::mem_readint(int address) {
  return mem32.readInt(ptr32_t(address));
  }

void DirectMemory::mem_writeint(int address, int val) {
  mem32.writeInt(uint32_t(address),val);
  }

void DirectMemory::mem_copybytes(int src, int dst, int size) {
  mem32.copyBytes(ptr32_t(src),ptr32_t(dst),ptr32_t(size));
  }

std::string DirectMemory::mem_readstring(int address) {
  if(auto s = mem32.deref<const zString>(ptr32_t(address))) {
    std::string str;
    memFromString(str, *s);
    return str;
    }
  return "";
  }

zenkit::DaedalusNakedCall DirectMemory::mem_readstatarr(zenkit::DaedalusVm& vm) {
  const int  index = vm.pop_int();
  auto [ref, idx, context] = vm.pop_reference();

  const int ret = vm.get_int(context, ref, uint16_t(idx+index));

  vm.push_int(ret);
  return zenkit::DaedalusNakedCall();
  }

int DirectMemory::mem_alloc(int amount) {
  if(amount==0) {
    Log::d("alocation zero bytes");
    return 0;
    }
  auto ptr = mem32.alloc(uint32_t(amount));
  return int32_t(ptr);
  }

int DirectMemory::mem_alloc(int amount, const char* comment) {
  if(amount==0) {
    Log::d("alocation zero bytes");
    return 0;
    }
  auto ptr = mem32.alloc(uint32_t(amount), comment);
  return int32_t(ptr);
  }

void DirectMemory::mem_free(int ptr) {
  mem32.free(Mem32::ptr32_t(ptr));
  }

int DirectMemory::mem_realloc(int address, int oldsz, int size) {
  auto ptr = mem32.realloc(Mem32::ptr32_t(address), uint32_t(size));
  return int32_t(ptr);
  }


void DirectMemory::setupDirectFunctions() {
  vm.override_function("MEM_GetFuncIdByOffset", [this](int off) { return mem_getfuncidbyoffset(off); });
  vm.override_function("MEM_AssignInst",        [this](int index, int ptr) { mem_assigninst(index, ptr); });

  vm.override_function("MEM_CallByID",          [this](zenkit::DaedalusVm& vm) { return mem_callbyid(vm);         });
  vm.override_function("MEM_CallByPtr",         [this](zenkit::DaedalusVm& vm) { return mem_callbyptr(vm);        });

  vm.override_function("memint_stackpushint",   [this](zenkit::DaedalusVm& vm) { return memint_stackpushint(vm);  });
  vm.override_function("memint_stackpushinst",  [this](zenkit::DaedalusVm& vm) { return memint_stackpushinst(vm); });

  vm.override_function("mem_popintresult",      [this](zenkit::DaedalusVm& vm) { return mem_popintresult(vm);     });
  vm.override_function("mem_popstringresult",   [this](zenkit::DaedalusVm& vm) { return mem_popstringresult(vm);  });
  vm.override_function("mem_popinstresult",     [this](zenkit::DaedalusVm& vm) { return mem_popinstresult(vm);    });
  }

int DirectMemory::mem_getfuncidbyoffset(int off) {
  if(off==0) {
    // MEM_INITALL
    return 0;
    }
  Log::e("TODO: mem_getfuncidbyoffset ", off);
  return 0;
  }

void DirectMemory::mem_assigninst(int index, int ptr) {
  auto* sym  = vm.find_symbol_by_index(uint32_t(index));
  if(sym==nullptr) {
    Log::e("MEM_AssignInst: Invalid instance: ",index);
    return;
    }
  sym->set_instance(std::make_shared<memory_instance>(*this, ptr32_t(ptr)));
  }

void DirectMemory::directCall(zenkit::DaedalusVm& vm, zenkit::DaedalusSymbol& func) {
  if(func.type()!=zenkit::DaedalusDataType::FUNCTION) {
    Log::e("Bad unsafe function call");
    return;
    }

  std::span<zenkit::DaedalusSymbol> params = vm.find_parameters_for_function(&func);
  if(params.size()>0)
    Log::d("");
  //vm.call_function(sym);
  //zenkit::StackGuard guard {&vm, func.rtype()};
  vm.unsafe_call(&func);
  }

zenkit::DaedalusNakedCall DirectMemory::mem_callbyid(zenkit::DaedalusVm& vm) {
  const uint32_t symbId = uint32_t(vm.pop_int());
  auto* sym = vm.find_symbol_by_index(symbId);
  if(sym==nullptr || sym->type()!=zenkit::DaedalusDataType::FUNCTION) {
    Log::e("MEM_CallByID: Provided symbol is not callable (not function, prototype or instance): ", symbId);
    return zenkit::DaedalusNakedCall();
    }
  directCall(vm, *sym);
  return zenkit::DaedalusNakedCall();
  }

zenkit::DaedalusNakedCall DirectMemory::mem_callbyptr(zenkit::DaedalusVm& vm) {
  //FIXME: map function into memory for real!
  const auto address = uint32_t(vm.pop_int());
  auto sym = vm.find_symbol_by_address(address);
  if(sym==nullptr || sym->type()!=zenkit::DaedalusDataType::FUNCTION) {
    char buf[16] = {};
    std::snprintf(buf, sizeof(buf), "%x", address);
    Log::e("MEM_CallByPtr: Provided symbol is not callable (not function, prototype or instance): 0x", buf);
    return zenkit::DaedalusNakedCall();
    }
  directCall(vm, *sym);
  return zenkit::DaedalusNakedCall();
  }

auto DirectMemory::memint_stackpushint(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall {
  return zenkit::DaedalusNakedCall();
  }

auto DirectMemory::memint_stackpushinst(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall {
  int id = vm.pop_int();
  auto sym = vm.find_symbol_by_index(uint32_t(id));
  if(sym!=nullptr && sym->type()==zenkit::DaedalusDataType::INSTANCE) {
    vm.push_instance(sym->get_instance());
    return zenkit::DaedalusNakedCall();
    }
  vm.push_instance(nullptr);
  return zenkit::DaedalusNakedCall();
  }

auto DirectMemory::mem_popintresult(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall {
  return zenkit::DaedalusNakedCall();
  }

auto DirectMemory::mem_popstringresult(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall {
  return zenkit::DaedalusNakedCall();
  }

auto DirectMemory::mem_popinstresult(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall {
  return zenkit::DaedalusNakedCall();
  }

void DirectMemory::setupMathFunctions() {
  vm.override_function("MKF",    [](int i) { return mkf(i); });
  vm.override_function("TRUNCF", [](int i) { return truncf(i); });
  vm.override_function("ROUNDF", [](int i) { return roundf(i); });
  vm.override_function("ADDF",   [](int a, int b) { return addf(a,b); });
  vm.override_function("SUBF",   [](int a, int b) { return subf(a,b); });
  vm.override_function("MULF",   [](int a, int b) { return mulf(a,b); });
  vm.override_function("DIVF",   [](int a, int b) { return divf(a,b); });
  }

int DirectMemory::mkf(int v) {
  return floatBitsToInt(float(v));
  }

int DirectMemory::truncf(int v) {
  float ret = intBitsToFloat(v);
  ret = std::truncf(ret);
  return int(ret); //floatBitsToInt(ret);
  }

int DirectMemory::roundf(int v) {
  float ret = intBitsToFloat(v);
  ret = std::roundf(ret);
  return floatBitsToInt(ret);
  }

int DirectMemory::addf(int ia, int ib) {
  float a = intBitsToFloat(ia);
  float b = intBitsToFloat(ib);
  return floatBitsToInt(a+b);
  }

int DirectMemory::subf(int ia, int ib) {
  float a = intBitsToFloat(ia);
  float b = intBitsToFloat(ib);
  return floatBitsToInt(a-b);
  }

int DirectMemory::mulf(int ia, int ib) {
  float a = intBitsToFloat(ia);
  float b = intBitsToFloat(ib);
  return floatBitsToInt(a*b);
  }

int DirectMemory::divf(int ia, int ib) {
  float a = intBitsToFloat(ia);
  float b = intBitsToFloat(ib);
  return floatBitsToInt(a/b);
  }

void DirectMemory::setupStringsFunctions() {
  // TODO: allow writes to mapped memory
  vm.override_function("STR_SubStr", [](std::string_view str, int start, int count) -> std::string {
    if(start < 0 || (count < 0)) {
      Log::e("STR_SubStr: start and count may not be negative.");
      return "";
      };

    if(size_t(start)>=str.size()) {
      Log::e("STR_SubStr: The desired start of the substring lies beyond the end of the string.");
      return "";
      };

    return std::string(str.substr(size_t(start), size_t(count)));
    });

  //NOTE: native implementation requires coherency protocol
  // const ptr32_t ZSTRING__UPPER_G2 = 4631296;
  vm.override_function("STR_Upper", [](std::string_view str){
    std::string s = std::string(str);
    for(auto& c:s)
      c = char(std::toupper(c));
    return s;
    });

  // LeGo immplementation requires 'rw' access to 'callback memory'
  vm.override_function("SB_TOSTRING", [this]() -> std::string {
    struct StringBuilder {
      ptr32_t ptr;
      int     cln;
      int     CAL;
      };
    const auto _SB_CURRENT = this->vm.find_symbol_by_name("_SB_CURRENT");
    if(_SB_CURRENT==nullptr || _SB_CURRENT->type()!=zenkit::DaedalusDataType::INT)
      return "";

    auto text = mem32.deref<StringBuilder>(ptr32_t(_SB_CURRENT->get_int()));
    if(text->cln<=0 || text->ptr==0)
      return "";
    auto cstr = reinterpret_cast<const char*>(mem32.deref(text->ptr, uint32_t(text->cln)));
    return std::string(cstr, size_t(text->cln));
    });

  const ptr32_t STR_FROMCHAR = 4198592;
  cpu.register_thiscall(STR_FROMCHAR, [this](ptr32_t self, ptr32_t pchr) {
    if(self<scriptVariables)
      return; // error

    const uint32_t id = (self - scriptVariables)/ptr32_t(sizeof(ScriptVar));
    const auto     sx = vm.find_symbol_by_index(id);
    if(sx==nullptr || sx->type()!=zenkit::DaedalusDataType::STRING)
      return;

    auto [ptr, size] = mem32.deref(pchr);
    const char* chr = reinterpret_cast<const char*>(ptr);
    if(chr==nullptr || size==0) {
      sx->set_string("");
      return;
      }
    size_t strsz = 0;
    while(chr[strsz] && strsz<size)
      ++strsz;
    sx->set_string(std::string(chr, strsz));
    });
  }

void DirectMemory::setupWinapiFunctions() {
  const ptr32_t WINAPI__LOADLIBRARY_ptr    = 8577604;
  const ptr32_t WINAPI__GETPROCADDRESS_ptr = 8577688;
  mem32.alloc(WINAPI__LOADLIBRARY_ptr,    4, "WINAPI__LOADLIBRARY*");
  mem32.alloc(WINAPI__GETPROCADDRESS_ptr, 4, "WINAPI__GETPROCADDRESS*");

  const ptr32_t WINAPI__LOADLIBRARY = mem32.alloc(4);
  mem32.writeInt(WINAPI__LOADLIBRARY_ptr, int32_t(WINAPI__LOADLIBRARY));

  const ptr32_t WINAPI__GETPROCADDRESS = mem32.alloc(4);
  mem32.writeInt(WINAPI__GETPROCADDRESS_ptr, int32_t(WINAPI__GETPROCADDRESS));

  cpu.register_stdcall(WINAPI__LOADLIBRARY, [this](ptr32_t pname) {
    auto [ptr, size] = mem32.deref(pname);
    auto name = std::string_view(reinterpret_cast<const char*>(ptr), reinterpret_cast<const char*>(ptr)+size);
    Log::d("suppress LoadLibrary: ", name);
    return 0x1;
    });

  cpu.register_stdcall(WINAPI__GETPROCADDRESS, [this](ptr32_t module, ptr32_t pname) {
    auto [ptr, size] = mem32.deref(pname);
    auto name = std::string_view(reinterpret_cast<const char*>(ptr), reinterpret_cast<const char*>(ptr)+size);
    Log::d("suppress GetProcAddress: ",name);
    return 0x1;
    });

  const ptr32_t GETUSERNAMEA = 8080162;
  cpu.register_stdcall(GETUSERNAMEA, [this](ptr32_t lpBuffer, ptr32_t pcbBuffer){
    std::string_view uname = "OpenGothic";

    const uint32_t max = pcbBuffer ? uint32_t(mem32.readInt(pcbBuffer)) : 0;
    if(auto ptr = reinterpret_cast<char*>(mem32.derefv(lpBuffer, max))) {
      if(max>=uname.size()) {
        std::strncpy(reinterpret_cast<char*>(ptr), "OpenGothic", max);
        ptr[uname.size()] = '\0';
        return 1;
        }
      }
    // buffer is too small
    mem32.writeInt(pcbBuffer, int32_t(uname.size()+1));
    return -1;
    });

  const ptr32_t GETLOCALTIME = 8079184;
  cpu.register_stdcall(GETLOCALTIME, [this](ptr32_t lpSystemTime) {
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

    if(auto ptr = mem32.deref<SystemTime>(lpSystemTime)) {
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
    });
  }

void DirectMemory::setupUtilityFunctions() {
  static auto generateCrcTable = []() {
    std::array<uint32_t, 256> table;
    uint32_t polynomial = 0xEDB88320;
    for (uint32_t i = 0; i < 256; i++) {
      uint32_t c = i;
      for (size_t j = 0; j < 8; j++) {
        if(c & 1) {
          c = polynomial ^ (c >> 1);
          } else {
          c >>= 1;
          }
        }
      table[i] = c;
      }
    return table;
    };

  const ptr32_t GETBUFFERCRC32_G2 = 6265360;
  cpu.register_cdecl(GETBUFFERCRC32_G2, [this](ptr32_t buf, int bufLen, int unused) {
    if(buf==0 || bufLen<=0)
      return 0;

    const auto  ptr   = reinterpret_cast<const uint8_t*>(mem32.deref(buf,uint32_t(bufLen)));
    static auto table = generateCrcTable();

    uint32_t initial = 0;
    uint32_t c       = initial ^ 0xFFFFFFFF;
    for(int i = 0; i < bufLen; ++i) {
      c = table[(c ^ ptr[i]) & 0xFF] ^ (c >> 8);
      }
    return int32_t(c ^ 0xFFFFFFFF);
    });

  const ptr32_t ZERROR__SETTARGET = 4513616;
  cpu.register_thiscall(ZERROR__SETTARGET, [](ptr32_t, int) {
    // nop
    });

  const int SYSGETTIMEPTR_G2 = 5264000;
  cpu.register_cdecl(SYSGETTIMEPTR_G2, [this](){
    return uint32_t(gameScript.tickCount());
    });
  }

void DirectMemory::setupHookEngine() {
  vm.override_function("HookEngineI", [](int address, int oldInstr, zenkit::DaedalusFunction function){
    auto sym  = function.value;
    auto name = sym==nullptr ? "" : sym->name().c_str();
    Log::e("not implemented call [HookEngineI] (", reinterpret_cast<void*>(uint64_t(address)),
           " -> ", name, ")");
    });

  const ptr32_t ZCCONSOLE__REGISTER = 7875296;
  cpu.register_thiscall(ZCCONSOLE__REGISTER, [](ptr32_t, ptr32_t pcmd, ptr32_t pdescr) {
    (void)pcmd;
    (void)pdescr;
    // auto sym  = func.value;
    // auto name = sym==nullptr ? "" : sym->name().c_str();
    // Log::e("not implemented call [ZCCONSOLE__REGISTER] (", prefix, " -> ", name, ")");
    });

  // PermMem
  vm.override_function("LOCALS", [this](zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall {
    auto sym = findSymbolByAddress(vm.pc());
    if(sym!=nullptr) {
      auto sx = vm.find_symbol_by_index(sym->index());
      sx->set_local_variables_enable(true);
      }
    return zenkit::DaedalusNakedCall();
    });
  vm.override_function("FINAL", []() {
    Log::e("LeGo: 'final' is not implemented");
    return 0;
    });
  }


void DirectMemory::setupzCParserFunctions() {
  const ptr32_t ZCPARSER__GETINDEX_G2 = 7943280;
  cpu.register_thiscall(ZCPARSER__GETINDEX_G2, [this](ptr32_t, std::string sym) {
    if(auto s = vm.find_symbol_by_name(sym))
      return int32_t(s->index());
    return -1;
    });

  const ptr32_t ZCPARSER__CREATEINSTANCE = 7942048;
  cpu.register_thiscall(ZCPARSER__CREATEINSTANCE, [this](ptr32_t, int32_t instId, ptr32_t ptr){
    auto *sym = vm.find_symbol_by_index(uint32_t(instId));
    auto *cls = sym;
    if(sym != nullptr && sym->type() == zenkit::DaedalusDataType::INSTANCE) {
      cls = vm.find_symbol_by_index(sym->parent());
      }

    if(sym==nullptr || cls == nullptr) {
      Log::e("LeGo::createInstance invalid symbold id (", instId, ")");
      return 0;
      }

    auto inst = std::make_shared<memory_instance>(*this, ptr);

    auto self = vm.find_symbol_by_name("SELF");
    auto prevSelf = self != nullptr ? self->get_instance() : nullptr;
    auto prevGi   = vm.unsafe_get_gi();
    if(self!=nullptr)
      self->set_instance(inst);

    vm.unsafe_set_gi(inst);
    vm.unsafe_call(sym);

    vm.unsafe_set_gi(prevGi);
    if(self!=nullptr)
      self->set_instance(prevSelf);

    sym->set_instance(inst);
    return int(ptr); // no idea, what return-value suppose to be - unused in LeGo
    });
  }

void DirectMemory::setupInitFileFunctions() {
  vm.override_function("MEM_GetGothOpt", [](std::string_view sec, std::string_view opt) {
    return std::string(Gothic::inst().settingsGetS(sec, opt));
    });
  vm.override_function("MEM_GetModOpt", [](std::string_view sec, std::string_view opt) {
    Log::e("TODO: mem_getmodopt(", sec, ", ", opt, ")");
    return std::string("");
    });
  vm.override_function("MEM_GothOptSectionExists", [](std::string_view sec) {
    Log::e("TODO: mem_gothoptaectionexists(", sec, ")");
    return false;
    });
  vm.override_function("MEM_GothOptExists", [](std::string_view sec, std::string_view opt) {
    if(sec=="INTERNAL" && opt=="UnionActivated") {
      // Fake Union, so ikarus won't setup crazy workarounds for unpatched G2.
      return true;
      }
    Log::e("TODO: mem_gothoptexists(", sec, ", ", opt, ")");
    return false;
    });
  vm.override_function("MEM_ModOptSectionExists", [](std::string_view sec) {
    Log::e("TODO: mem_modoptsectionexists(", sec, ")");
    return false;
    });
  vm.override_function("MEM_ModOptExists", [](std::string_view sec, std::string_view opt) {
    Log::e("TODO: mem_modoptexists(", sec, ", ", opt, ")");
    return false;
    });
  vm.override_function("MEM_SetGothOpt", [](std::string_view sec, std::string_view opt, std::string_view v) {
    Log::e("TODO: mem_setgothopt(", sec, ", ", opt, ", ", v, ")");
    });
  }

void DirectMemory::setupUiFunctions() {
  // https://github.com/Lehona/LeGo/blob/dev/View.d
  const int ZCVIEW__ZCVIEW     = 8017664;
  const int ZCVIEW__OPEN       = 8023040;
  const int ZCVIEW__CLOSE      = 8023600;
  const int ZCVIEW_TOP         = 8021904;
  const int ZCVIEW__SETSIZE    = 8026016;
  const int zCVIEW__MOVE       = 8025824;
  const int ZCVIEW__INSERTBACK = 8020272;
  cpu.register_thiscall(ZCVIEW__ZCVIEW, [this](ptr32_t ptr, int x1, int y1, int x2, int y2, int arg) {
    auto view = mem32.deref<zCView>(ptr);
    if(view==nullptr) {
      Log::e("LeGo: zCView__zCView - unable to resolve address");
      return;
      }

    view->VPOSX  = x1;
    view->VPOSY  = y1;
    view->VSIZEX = x2-x1;
    view->VSIZEY = y2-y1;
    });

  cpu.register_thiscall(ZCVIEW__OPEN, [this](ptr32_t ptr) {
    auto view = mem32.deref<zCView>(ptr);
    if(view==nullptr) {
      Log::e("LeGo: zCView__Open - unable to resolve address");
      return;
      }
    Log::e("LeGo: zCView__Open");
    });

  cpu.register_thiscall(ZCVIEW__CLOSE, [this](ptr32_t ptr) {
    auto view = mem32.deref<zCView>(ptr);
    if(view==nullptr) {
      Log::e("LeGo: zCView__Close - unable to resolve address");
      return;
      }
    });

  cpu.register_thiscall(ZCVIEW_TOP, [this](ptr32_t ptr) {
    auto view = mem32.deref<zCView>(ptr);
    if(view==nullptr) {
      Log::e("LeGo: zCView__Top - unable to resolve address");
      return;
      }
    });

  cpu.register_thiscall(ZCVIEW__SETSIZE, [this](ptr32_t ptr, int x, int y) {
    auto view = mem32.deref<zCView>(ptr);
    if(view==nullptr) {
      Log::e("LeGo: zCView__SetSize - unable to resolve address");
      return;
      }

    view->VSIZEX = x;
    view->VSIZEY = y;
    });

  cpu.register_thiscall(zCVIEW__MOVE, [this](ptr32_t ptr, int x, int y) {
    auto view = mem32.deref<zCView>(ptr);
    if(view==nullptr) {
      Log::e("LeGo: zCView__Move - unable to resolve address");
      return;
      }
    view->VPOSX  = x;
    view->VPOSY  = y;
    });

  cpu.register_thiscall(ZCVIEW__INSERTBACK, [this](ptr32_t ptr, std::string img) {
    auto view = mem32.deref<zCView>(ptr);
    if(view==nullptr) {
      Log::e("LeGo: zCView__InsertBack - unable to resolve address");
      return;
      }
    Log::e("LeGo: zCView__InsertBack: ", img);
    });

  // ## Textures
  const int ZCTEXTURE__LOAD = 6239904;
  cpu.register_stdcall(ZCTEXTURE__LOAD, [](std::string img, int flag) {
    Log::e("LeGo: zCTexture__Load: ", img);
    return 0;
    });
  }

void DirectMemory::setupFontFunctions() {
  const ptr32_t ZCFONTMAN__LOAD    = 7897808;
  const ptr32_t ZCFONTMAN__GETFONT = 7898288;
  cpu.register_thiscall(ZCFONTMAN__LOAD, [](ptr32_t ptr, std::string font) {
    Log::e("LeGo: zCFontMan__Load");
    return 1234;
    });
  cpu.register_thiscall(ZCFONTMAN__GETFONT, [](ptr32_t ptr, int handle) {
    Log::e("LeGo: zCFontMan__GetFont");
    return handle;
    });
  }

void DirectMemory::setupNpcFunctions() {
  const ptr32_t OCNPC__GETSLOTITEM = 7544720;
  cpu.register_thiscall(OCNPC__GETSLOTITEM, [](ptr32_t pHero, std::string slot) {
    Log::e("LeGo: OCNPC__GETSLOTITEM(", slot, ")");
    return 0x0;
    });
  }
