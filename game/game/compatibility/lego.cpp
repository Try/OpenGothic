#include "lego.h"

#include "ikarus.h"
#include "game/gamescript.h"

#include <Tempest/Log>

using namespace Tempest;


struct LeGo::zCView {
  int32_t _VTBL;
  int32_t _ZCINPUTCALLBACK_VTBL;
  int32_t M_BFILLZ;
  int32_t NEXT;
  int32_t VIEWID;
  int32_t FLAGS;
  int32_t INTFLAGS;
  int32_t ONDESK;
  int32_t ALPHAFUNC;
  int32_t COLOR;
  int32_t ALPHA;
  int32_t CHILDS_COMPARE;
  int32_t CHILDS_COUNT;
  int32_t CHILDS_LAST;
  int32_t CHILDS_WURZEL;
  int32_t OWNER;
  int32_t BACKTEX;
  int32_t VPOSX;
  int32_t VPOSY;
  int32_t VSIZEX;
  int32_t VSIZEY;
  int32_t PPOSX;
  int32_t PPOSY;
  int32_t PSIZEX;
  int32_t PSIZEY;
  int32_t FONT;
  int32_t FONTCOLOR;
  int32_t PX1;
  int32_t PY1;
  int32_t PX2;
  int32_t PY2;
  int32_t WINX;
  int32_t WINY;
  int32_t TEXTLINES_DATA;
  int32_t TEXTLINES_NEXT;
  int32_t SCROLLMAXTIME;
  int32_t SCROLLTIMER;
  int32_t FXOPEN;
  int32_t FXCLOSE;
  int32_t TIMEDIALOG;
  int32_t TIMEOPEN;
  int32_t TIMECLOSE;
  int32_t SPEEDOPEN;
  int32_t SPEEDCLOSE;
  int32_t ISOPEN;
  int32_t ISCLOSED;
  int32_t CONTINUEOPEN;
  int32_t CONTINUECLOSE;
  int32_t REMOVEONCLOSE;
  int32_t RESIZEONOPEN;
  int32_t MAXTEXTLENGTH;
  zString TEXTMAXLENGTH;
  int32_t POSCURRENT_0[2];
  int32_t POSCURRENT_1[2];
  int32_t POSOPENCLOSE_0[2];
  int32_t POSOPENCLOSE_1[2];
  };

struct LeGo::zCFontMan {
  };

LeGo::LeGo(GameScript& owner, Ikarus& ikarus, zenkit::DaedalusVm& vm_) : owner(owner), ikarus(ikarus), vm(vm_) {
  if(auto v = vm.find_symbol_by_name("LeGo_Version")) {
    const char* version = v->type()==zenkit::DaedalusDataType::STRING ? v->get_string().c_str() : "LeGo";
    Log::i("DMA mod detected: ", version);
    }

  // ## FrameFunctions
  vm.override_function("_FF_Create", [this](zenkit::DaedalusFunction function, int delay, int cycles,
                                            int hasData, int data, bool gametime) {
    return _FF_Create(function, delay, cycles, hasData, data, gametime);
    });
  vm.override_function("FF_RemoveData", [this](zenkit::DaedalusFunction function, int data){
    return FF_RemoveData(function, data);
    });
  vm.override_function("FF_ActiveData", [this](zenkit::DaedalusFunction function, int data){
    return FF_ActiveData(function, data);
    });
  vm.override_function("FF_Active", [this](zenkit::DaedalusFunction function){
    return FF_Active(function);
    });

  // HookEngine
  vm.override_function("HookEngineF", [](int address, int oldInstr, zenkit::DaedalusFunction function) {
    auto sym  = function.value;
    auto name = sym==nullptr ? "" : sym->name().c_str();
    Log::e("not implemented call [HookEngineF] (", reinterpret_cast<void*>(uint64_t(address)),
           " -> ", name, ")");
    });
  vm.override_function("HookEngineI", [](int address, int oldInstr, zenkit::DaedalusFunction function){
    auto sym  = function.value;
    auto name = sym==nullptr ? "" : sym->name().c_str();
    Log::e("not implemented call [HookEngineI] (", reinterpret_cast<void*>(uint64_t(address)),
           " -> ", name, ")");
    });
  vm.override_function("HookEngine", [](int address, int oldInstr, std::string_view function){
    Log::e("not implemented call [HookEngine] (", reinterpret_cast<void*>(uint64_t(address)),
           " -> ", function, ")");
    });

  // console commands
  vm.override_function("CC_Register", [](zenkit::DaedalusFunction func, std::string_view prefix, std::string_view desc){
    auto sym  = func.value;
    auto name = sym==nullptr ? "" : sym->name().c_str();
    Log::e("not implemented call [CC_Register] (", prefix, " -> ", name, ")");
    });

  // various
  vm.override_function("_RENDER_INIT", [](){
    Log::e("not implemented call [_RENDER_INIT]");
    });
  vm.override_function("PRINT_FIXPS", [](){
    // function patches asm code of zCView::PrintTimed* to fix text coloring - we can ignore it
    });

  // ## PermMem
  vm.override_function("CREATE", [this](int inst) { return create(inst); });

  vm.override_function("LOCALS", [](){
    //NOTE: push local-variables to in-flight memory and restore at function end
    Log::e("TODO: LeGo-LOCALS.");
    });

  // ## UI
  // https://github.com/Lehona/LeGo/blob/dev/View.d
  const int ZCVIEW__ZCVIEW     = 8017664;
  const int ZCVIEW__SETSIZE    = 8026016;
  const int zCVIEW__MOVE       = 8025824;
  const int ZCVIEW__INSERTBACK = 8020272;
  ikarus.register_stdcall(ZCVIEW__ZCVIEW, [this](ptr32_t ptr, int x1, int y1, int x2, int y2, int arg) {
    zCView__zCView(ptr, x1, y1, x2, y2);
    });
  ikarus.register_stdcall(ZCVIEW__SETSIZE, [this](ptr32_t ptr, int x, int y) {
    zCView__SetSize(ptr, x, y);
    });
  ikarus.register_stdcall(zCVIEW__MOVE, [this](ptr32_t ptr, int x, int y) {
    zCView__Move(ptr, x, y);
    });
  ikarus.register_stdcall(ZCVIEW__INSERTBACK, [this](ptr32_t ptr, std::string img) {
    zCView__InsertBack(ptr, img);
    });

  // ## UI data
  auto& memGame   = ikarus.memGame;
  auto& allocator = ikarus.allocator;

  memGame.HPBAR               = allocator.alloc(sizeof(zCView));
  memGame.MANABAR             = allocator.alloc(sizeof(zCView));
  memGame._ZCSESSION_VIEWPORT = allocator.alloc(sizeof(zCView));
  if(auto ptr = allocator.deref<zCView>(memGame._ZCSESSION_VIEWPORT)) {
    // Dummy window size, for now!
    ptr->PSIZEX = 800;
    ptr->PSIZEY = 600;
    }
  // needed during initialization in PRINT_EXT
  memGame.ARRAY_VIEW[0] = allocator.alloc(sizeof(zCView));

  // ## Font
  const int ZCFONTMAN__LOAD    = 7897808;
  const int ZCFONTMAN__GETFONT = 7898288;
  ikarus.register_stdcall(ZCFONTMAN__LOAD, [this](ptr32_t ptr, std::string font) {
    return zCFontMan__Load(ptr, font);
    });
  ikarus.register_stdcall(ZCFONTMAN__GETFONT, [this](ptr32_t ptr, int handle) {
    return zCFontMan__GetFont(ptr, handle);
    });

  const int ZFONTMAN = 11221460;
  fontMan_Ptr = allocator.alloc(sizeof(zCFontMan));
  allocator.pin(&fontMan_Ptr, ZFONTMAN, sizeof(fontMan_Ptr), "zCFontMan*");

  // ##
  const int ZCOBJECTFACTORY__CREATEWORLD = 5947120;
  ikarus.register_stdcall(ZCOBJECTFACTORY__CREATEWORLD, [this](ptr32_t ptr) {
    // CALL__THISCALL(MEM_READINT(ZFACTORY), ZCOBJECTFACTORY__CREATEWORLD);
    // QS_RENDERWORLD = CALL_RETVALASPTR();
    Log::e("LeGo: zCObjectFactory__CreateWorld");
    return int(this->ikarus.memGame._ZCSESSION_WORLD);
    });
  }

bool LeGo::isRequired(zenkit::DaedalusVm& vm) {
  return
      vm.find_symbol_by_name("LeGo_Version") != nullptr &&
      vm.find_symbol_by_name("LeGo_InitFlags") != nullptr &&
      vm.find_symbol_by_name("LeGo_Init") != nullptr &&
      Ikarus::isRequired(vm);
  }

int LeGo::create(int instId) {
  auto *sym = vm.find_symbol_by_index(uint32_t(instId));
  auto *cls = sym;
  if(sym != nullptr && sym->type() == zenkit::DaedalusDataType::INSTANCE) {
    cls = vm.find_symbol_by_index(sym->parent());
    }

  if(cls == nullptr) {
    Log::e("LeGo::create invalid symbold id (", instId, ")");
    return 0;
    }

  auto sz   = cls->class_size();
  auto ptr  = ikarus.mem_alloc(int32_t(sz), cls->name().c_str());
  auto inst = std::make_shared<Ikarus::memory_instance>(ikarus, ptr);

  auto self = vm.find_symbol_by_name("SELF");
  auto prev = self != nullptr ? self->get_instance() : nullptr;
  if(self!=nullptr)
    self->set_instance(inst);
  vm.unsafe_call(sym);
  if(self!=nullptr)
    self->set_instance(prev);
  return ptr;
  }

void LeGo::tick(uint64_t dt) {
  auto time = owner.tickCount();

  auto ff = std::move(frameFunc);
  frameFunc.clear();

  for(auto& i:ff) {
    if(i.next>time) {
      frameFunc.push_back(i);
      continue;
      }

    if(auto* sym = vm.find_symbol_by_index(i.fncID)) {
      try {
      if(i.hasData)
        vm.call_function(sym, i.data); else
        vm.call_function(sym);
        }
      catch(const std::exception& e){
        Tempest::Log::e("exception in \"", sym->name(), "\": ",e.what());
        }
      if(i.cycles>0)
        i.cycles--;
      if(i.cycles==0)
        continue;
      i.next += uint64_t(i.delay);
      frameFunc.push_back(i);
      }
    }
  }

void LeGo::_FF_Create(zenkit::DaedalusFunction func, int delay, int cycles, int hasData, int data, bool gametime) {
  FFItem itm;
  itm.fncID    = func.value->index();
  itm.cycles   = cycles;
  itm.delay    = std::max(delay, 0);
  itm.data     = data;
  itm.hasData  = hasData;
  itm.gametime = gametime;
  if(gametime) {
    itm.next = owner.tickCount() + uint64_t(delay);
    } else {
    itm.next = owner.tickCount(); // Timer() + itm.delay;
    };

  itm.cycles = std::max(itm.cycles, 0); // disable repetable callbacks for now
  frameFunc.emplace_back(itm);
  }

void LeGo::FF_Remove(zenkit::DaedalusFunction function) {

  }

void LeGo::FF_RemoveAll(zenkit::DaedalusFunction function) {

  }

void LeGo::FF_RemoveData(zenkit::DaedalusFunction func, int data) {
  auto* sym = func.value;
  if(sym == nullptr) {
    Log::e("FF_RemoveData: invalid function ptr");
    return;
    }

  size_t nsz = 0;
  for(size_t i=0; i<frameFunc.size(); ++i) {
    if(frameFunc[i].fncID==sym->index() && frameFunc[i].data==data)
      continue;
    frameFunc[nsz] = frameFunc[i];
    ++nsz;
    }
  frameFunc.resize(nsz);
  }

bool LeGo::FF_ActiveData(zenkit::DaedalusFunction func, int data) {
  auto* sym = func.value;
  if(sym == nullptr) {
    Log::e("FF_ActiveData: invalid function ptr");
    return false;
    }

  for(auto& f:frameFunc) {
    if(f.fncID==sym->index() && f.data==data)
      return true;
    }
  return false;
  }

bool LeGo::FF_Active(zenkit::DaedalusFunction func) {
  auto* sym = func.value;
  if(sym == nullptr) {
    Log::e("FF_Active: invalid function ptr");
    return false;
    }

  for(auto& f:frameFunc) {
    if(f.fncID==sym->index())
      return true;
    }
  return false;
  }

void LeGo::zCView__zCView(ptr32_t ptr, int x1, int y1, int x2, int y2) {
  auto view = ikarus.allocator.deref<zCView>(ptr);
  if(view==nullptr) {
    Log::e("LeGo: zCView__zCView - unable to resolve address");
    return;
    }

  view->VPOSX  = x1;
  view->VPOSY  = y1;
  view->VSIZEX = x2-x1;
  view->VSIZEY = y2-y1;
  }

void LeGo::zCView__SetSize(ptr32_t ptr, int x, int y) {
  auto view = ikarus.allocator.deref<zCView>(ptr);
  if(view==nullptr) {
    Log::e("LeGo: zCView__SetSize - unable to resolve address");
    return;
    }

  view->VSIZEX = x;
  view->VSIZEY = y;
  }

void LeGo::zCView__Move(ptr32_t ptr, int x, int y) {
  auto view = ikarus.allocator.deref<zCView>(ptr);
  if(view==nullptr) {
    Log::e("LeGo: zCView__Move - unable to resolve address");
    return;
    }
  view->VPOSX  = x;
  view->VPOSY  = y;
  }

void LeGo::zCView__InsertBack(ptr32_t ptr, std::string_view img) {
  auto view = ikarus.allocator.deref<zCView>(ptr);
  if(view==nullptr) {
    Log::e("LeGo: zCView__InsertBack - unable to resolve address");
    return;
    }
  Log::e("LeGo: zCView__InsertBack: ", img);
  }

int LeGo::zCFontMan__Load(ptr32_t ptr, std::string_view font) {
  Log::e("LeGo: zCFontMan__Load");
  return 1234;
  }

int LeGo::zCFontMan__GetFont(ptr32_t ptr, int handle) {
  Log::e("LeGo: zCFontMan__GetFont");
  return handle;
  }


