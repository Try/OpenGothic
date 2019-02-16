#include "worldscript.h"

#include "gothic.h"
#include "npc.h"

#include <Tempest/Log>

using namespace Tempest;
using namespace Daedalus::GameState;

static std::string addExt(const std::string& s,const char* ext){
  if(s.size()>0 && s.back()=='.')
    return s+&ext[1];
  return s+ext;
  }

WorldScript::WorldScript(World &owner, Gothic& gothic, const char *world)
  :vm(gothic.path()+world),owner(owner){
  Daedalus::registerGothicEngineClasses(vm);
  initCommon();
  }

void WorldScript::initCommon() {
  auto& tbl = vm.getDATFile().getSymTable();
  for(auto& i:tbl.symbolsByName) {
    if(i.first.size()==0 || std::isdigit(i.first[0]))
      continue;
    //Log::d(i.first);
    vm.registerExternalFunction(i.first,[this](Daedalus::DaedalusVM& vm){return notImplementedRoutine(vm);});
    }

  vm.registerExternalFunction("concatstrings", &WorldScript::concatstrings);
  vm.registerExternalFunction("inttostring",   &WorldScript::inttostring  );
  vm.registerExternalFunction("floattostring", &WorldScript::floattostring);
  vm.registerExternalFunction("floattoint",    &WorldScript::floattoint   );

  vm.registerExternalFunction("hlp_random",          [this](Daedalus::DaedalusVM& vm){ hlp_random(vm);         });
  vm.registerExternalFunction("hlp_strcmp",          &WorldScript::hlp_strcmp                                   );
  vm.registerExternalFunction("hlp_isvalidnpc",      [this](Daedalus::DaedalusVM& vm){ hlp_isvalidnpc(vm);     });
  vm.registerExternalFunction("hlp_getinstanceid",   [this](Daedalus::DaedalusVM& vm){ hlp_getinstanceid(vm);  });
  vm.registerExternalFunction("hlp_getnpc",          [this](Daedalus::DaedalusVM& vm){ hlp_getnpc(vm);         });

  vm.registerExternalFunction("wld_insertnpc",       [this](Daedalus::DaedalusVM& vm){ wld_insertnpc(vm);  });
  vm.registerExternalFunction("wld_insertitem",      [this](Daedalus::DaedalusVM& vm){ wld_insertitem(vm); });
  vm.registerExternalFunction("wld_settime",         [this](Daedalus::DaedalusVM& vm){ wld_settime(vm);    });

  vm.registerExternalFunction("mdl_setvisual",       [this](Daedalus::DaedalusVM& vm){ mdl_setvisual(vm);       });
  vm.registerExternalFunction("mdl_setvisualbody",   [this](Daedalus::DaedalusVM& vm){ mdl_setvisualbody(vm);   });
  vm.registerExternalFunction("mdl_setmodelfatness", [this](Daedalus::DaedalusVM& vm){ mdl_setmodelfatness(vm); });
  vm.registerExternalFunction("mdl_applyoverlaymds", [this](Daedalus::DaedalusVM& vm){ mdl_applyoverlaymds(vm); });
  vm.registerExternalFunction("mdl_setmodelscale",   [this](Daedalus::DaedalusVM& vm){ mdl_setmodelscale(vm);   });

  vm.registerExternalFunction("npc_settalentskill",  [this](Daedalus::DaedalusVM& vm){ npc_settalentskill(vm);  });
  vm.registerExternalFunction("npc_settofightmode",  [this](Daedalus::DaedalusVM& vm){ npc_settofightmode(vm);  });
  vm.registerExternalFunction("npc_settofistmode",   [this](Daedalus::DaedalusVM& vm){ npc_settofistmode(vm);   });
  vm.registerExternalFunction("npc_isinstate",       [this](Daedalus::DaedalusVM& vm){ npc_isinstate(vm);       });
  vm.registerExternalFunction("npc_getdisttowp",     [this](Daedalus::DaedalusVM& vm){ npc_getdisttowp(vm);     });
  vm.registerExternalFunction("npc_exchangeroutine", [this](Daedalus::DaedalusVM& vm){ npc_exchangeroutine(vm); });
  vm.registerExternalFunction("npc_isdead",          [this](Daedalus::DaedalusVM& vm){ npc_isdead(vm);          });

  vm.registerExternalFunction("ai_output",           [this](Daedalus::DaedalusVM& vm){ ai_output(vm);            });
  vm.registerExternalFunction("ai_stopprocessinfos", [this](Daedalus::DaedalusVM& vm){ ai_stopprocessinfos(vm);  });
  vm.registerExternalFunction("ai_standup",          [this](Daedalus::DaedalusVM& vm){ ai_standup(vm);           });
  vm.registerExternalFunction("ai_continueroutine",  [this](Daedalus::DaedalusVM& vm){ ai_continueroutine(vm);   });

  vm.registerExternalFunction("ta_min",              [this](Daedalus::DaedalusVM& vm){ ta_min(vm);             });

  vm.registerExternalFunction("log_createtopic",     [this](Daedalus::DaedalusVM& vm){ log_createtopic(vm);    });
  vm.registerExternalFunction("log_settopicstatus",  [this](Daedalus::DaedalusVM& vm){ log_settopicstatus(vm); });
  vm.registerExternalFunction("log_addentry",        [this](Daedalus::DaedalusVM& vm){ log_addentry(vm);       });

  vm.registerExternalFunction("equipitem",           [this](Daedalus::DaedalusVM& vm){ equipitem(vm);          });
  vm.registerExternalFunction("createinvitems",      [this](Daedalus::DaedalusVM& vm){ createinvitems(vm);     });

  vm.registerExternalFunction("info_addchoice",      [this](Daedalus::DaedalusVM& vm){ info_addchoice(vm);     });
  vm.registerExternalFunction("info_clearchoices",   [this](Daedalus::DaedalusVM& vm){ info_clearchoices(vm);  });

  vm.registerExternalFunction("playvideo",           [this](Daedalus::DaedalusVM& vm){ playvideo(vm);          });
  vm.registerExternalFunction("printscreen",         [this](Daedalus::DaedalusVM& vm){ printscreen(vm);        });

  DaedalusGameState::GameExternals ext;
  ext.wld_insertnpc      = [this](NpcHandle h,const std::string& s){ onInserNpc(h,s); };
  ext.post_wld_insertnpc = [this](NpcHandle h){ onNpcReady(h); };
  ext.createinvitem      = [this](NpcHandle i,NpcHandle n){ onCreateInventoryItem(i,n); };

  ext.wld_removenpc      = notImplementedFn<void,NpcHandle>();
  ext.wld_insertitem     = notImplementedFn<void,NpcHandle>();
  ext.wld_GetDay         = notImplementedFn<int>();
  ext.log_createtopic    = notImplementedFn<void,std::string>();
  ext.log_settopicstatus = notImplementedFn<void,std::string>();
  ext.log_addentry       = notImplementedFn<void,std::string,std::string>();
  vm.getGameState().setGameExternals(ext);
  }

void WorldScript::initDialogs(Gothic& gothic) {
  const char* german       ="_work/data/Scripts/content/CUTSCENE/OU.BIN";
  const char* international="_work/data/Scripts/content/CUTSCENE/OU.DAT";

  dialogs.reset(new Daedalus::GameState::DaedalusDialogManager(vm, gothic.path()+international, dlgKnownInfos));

  vm.getDATFile().iterateSymbolsOfClass("C_Info", [this](size_t i,Daedalus::PARSymbol&){
    InfoHandle h = vm.getGameState().createInfo();
    // Daedalus::GEngineClasses::C_Info& info = vm.getGameState().getInfo(h);
    vm.initializeInstance(ZMemory::toBigHandle(h), i, Daedalus::IC_Info);
    dialogsInfo.push_back(h);
    });
  }

const std::list<Daedalus::GameState::ItemHandle> &WorldScript::getInventoryOf(Daedalus::GameState::NpcHandle h) {
  return vm.getGameState().getInventoryOf(h);
  }

DaedalusGameState &WorldScript::getGameState() {
  return vm.getGameState();
  }

Daedalus::PARSymbol &WorldScript::getSymbol(const char *s) {
  return vm.getDATFile().getSymbolByName(s);
  }

Daedalus::GEngineClasses::C_Npc& WorldScript::vmNpc(Daedalus::GameState::NpcHandle handle) {
  auto  hnpc      = ZMemory::handleCast<NpcHandle>(handle);
  auto& npcData   = vm.getGameState().getNpc(hnpc);
  return npcData;
  }

std::vector<WorldScript::DlgChoise> WorldScript::dialogChoises(Daedalus::GameState::NpcHandle player,
                                                               Daedalus::GameState::NpcHandle hnpc) {
  auto& npc = vmNpc(hnpc);
  auto& pl  = vmNpc(player);

  vm.setInstance("self",  ZMemory::toBigHandle(hnpc),   Daedalus::IC_Npc);
  vm.setInstance("other", ZMemory::toBigHandle(player), Daedalus::IC_Npc);

  std::vector<InfoHandle> hDialog;
  for(auto& infoHandle : dialogsInfo) {
    Daedalus::GEngineClasses::C_Info& info = vm.getGameState().getInfo(infoHandle);
    if(info.npc==int32_t(npc.instanceSymbol)) {
      hDialog.push_back(infoHandle);
      }
    }

  std::vector<DlgChoise> choise;

  for(int important=1;important>=0;--important){
    for(auto& i:hDialog) {
      const Daedalus::GEngineClasses::C_Info& info = vm.getGameState().getInfo(i);
      if(info.important!=important)
        continue;
      bool npcKnowsInfo = dialogs->doesNpcKnowInfo(pl.instanceSymbol,info.instanceSymbol);
      if(npcKnowsInfo)
        continue;
      bool valid=false;
      if(info.condition)
        valid=runFunction(info.condition,true)!=0;
      if(valid) {
        DlgChoise ch;
        ch.title    = info.description;
        ch.sort     = info.nr;
        ch.scriptFn = info.information;
        ch.handle   = i;
        choise.emplace_back(std::move(ch));
        }
      }
    if(!choise.empty()){
      sort(choise);
      return choise;
      }
    }
  sort(choise);
  return choise;
  }

void WorldScript::exec(const WorldScript::DlgChoise &dlg,
                       Daedalus::GameState::NpcHandle player,
                       Daedalus::GameState::NpcHandle hnpc) {
  vm.setInstance("self",  ZMemory::toBigHandle(hnpc),   Daedalus::IC_Npc);
  vm.setInstance("other", ZMemory::toBigHandle(player), Daedalus::IC_Npc);

  Daedalus::GEngineClasses::C_Info& info = vm.getGameState().getInfo(dlg.handle);
  info.subChoices.clear();

  runFunction(dlg.scriptFn,true);
  }

bool WorldScript::hasSymbolName(const std::string &fn) {
  return vm.getDATFile().hasSymbolName(fn);
  }

int32_t WorldScript::runFunction(const std::string& fname, bool clearDataStack) {
  if(!hasSymbolName(fname))
    throw std::runtime_error("script bad call");
  auto id = vm.getDATFile().getSymbolIndexByName(fname);
  return runFunction(id,clearDataStack);
  }

int32_t WorldScript::runFunction(const size_t fid, bool clearDataStack) {
  vm.prepareRunFunction();
  return vm.runFunctionBySymIndex(fid,clearDataStack);
  }

void WorldScript::tick(uint64_t dt) {
  for(auto& i:npcArr){
    i->tick(dt);
    }
  }

uint64_t WorldScript::tickCount() const {
  return owner.tickCount();
  }

template<class Ret,class ... Args>
std::function<Ret(Args...)> WorldScript::notImplementedFn(){
  struct _{
    static Ret fn(Args...){
      static bool first=true;
      if(first){
        Log::e("not implemented routine call");
        first=false;
        }
      return Ret();
      }
    };
  return std::function<Ret(Args...)>(_::fn);
  }

void WorldScript::notImplementedRoutine(Daedalus::DaedalusVM &vm) {
  static std::unordered_set<std::string> s;
  auto fn = vm.getCallStack();

  if(s.find(fn[0])==s.end()){
    s.insert(fn[0]);
    Log::e("not implemented call [",fn[0],"]");
    }
  }

void WorldScript::onInserNpc(Daedalus::GameState::NpcHandle handle,const std::string& point) {
  auto  pos = owner.findPoint(point);
  if(pos==nullptr){
    Log::e("onInserNpc: invalid waypoint");
    }
  auto  hnpc      = ZMemory::handleCast<NpcHandle>(handle);
  auto& npcData   = vm.getGameState().getNpc(hnpc);

  std::unique_ptr<Npc> ptr{new Npc(*this,hnpc)};
  npcData.userPtr = ptr.get();
  if(!npcData.name[0].empty())
    ptr->setName(npcData.name[0]);

  if(pos!=nullptr) {
    ptr->setPosition (pos->position.x,pos->position.y,pos->position.z);
    ptr->setDirection(pos->direction.x,
                      pos->direction.y,
                      pos->direction.z);
    }

  npcArr.emplace_back(std::move(ptr));
  }

void WorldScript::onNpcReady(NpcHandle handle) {
  auto  hnpc    = ZMemory::handleCast<NpcHandle>(handle);
  auto& npcData = vm.getGameState().getNpc(hnpc);

  Npc& npc=getNpc(hnpc);
  if(!npcData.name[0].empty())
    npc.setName(npcData.name[0]);

  if(npcData.daily_routine!=0) {
    vm.prepareRunFunction();

    vm.setInstance("self", ZMemory::toBigHandle(handle), Daedalus::IC_Npc);
    vm.setCurrentInstance(vm.getDATFile().getSymbolIndexByName("self"));

    vm.runFunctionBySymIndex(npcData.daily_routine);
    }
  }

void WorldScript::onRemoveNpc(NpcHandle handle) {
  const Npc* p = &getNpc(handle);
  for(size_t i=0;i<npcArr.size();++i)
    if(p==npcArr[i].get()){
      npcArr[i]=std::move(npcArr.back());
      npcArr.pop_back();
      return;
      }
  }

Npc& WorldScript::getNpc(NpcHandle handle) {
  auto  hnpc      = ZMemory::handleCast<NpcHandle>(handle);
  auto& npcData   = vm.getGameState().getNpc(hnpc);
  assert(npcData.userPtr);
  return *reinterpret_cast<Npc*>(npcData.userPtr);
  }

Npc& WorldScript::getNpcById(size_t id) {
  auto handle = vm.getDATFile().getSymbolByIndex(id);
  assert(handle.instanceDataClass == Daedalus::EInstanceClass::IC_Npc);

  NpcHandle hnpc = ZMemory::handleCast<NpcHandle>(handle.instanceDataHandle);
  return getNpc(hnpc);
  }

Npc* WorldScript::inserNpc(const char *npcInstance, const char *at) {
  size_t id = vm.getDATFile().getSymbolIndexByName(npcInstance);
  if(id==0)
    return nullptr;
  return inserNpc(id,at);
  }

void WorldScript::setInstanceNPC(const char *name, Npc &npc) {
  assert(vm.getDATFile().hasSymbolName(name));
  vm.setInstance(name, ZMemory::toBigHandle(npc.handle()), Daedalus::EInstanceClass::IC_Npc);
  }

Npc* WorldScript::inserNpc(size_t npcInstance, const char* at) {
  auto pos = owner.findPoint(at);
  if(pos==nullptr){
    Log::e("inserNpc: invalid waypoint");
    return nullptr;
    }
  auto h = vm.getGameState().insertNPC(npcInstance,at);
  return &getNpc(h);
  }

void WorldScript::onCreateInventoryItem(ItemHandle item, NpcHandle npcHandle) {
  Daedalus::GEngineClasses::C_Item& itemData = vm.getGameState().getItem(item);
  Npc&                              npc      = getNpc(npcHandle);

  if((itemData.mainflag & Daedalus::GEngineClasses::C_Item::ITM_CAT_ARMOR)!=0) {
    // TODO: equiping armor
    std::string visual = itemData.visual_change.substr(0, itemData.visual_change.size() - 4) + ".MDM";
    auto        vbody  = visual.empty() ? StaticObjects::Mesh() : owner.getView(visual);
    npc.setArmour(std::move(vbody));
    }

  if((itemData.mainflag & (Daedalus::GEngineClasses::C_Item::ITM_CAT_NF | Daedalus::GEngineClasses::C_Item::ITM_CAT_FF))) {
    // TODO
    }
  }

const std::string& WorldScript::popString(Daedalus::DaedalusVM &vm) {
  uint32_t arr;
  uint32_t idx = vm.popVar(arr);

  return vm.getDATFile().getSymbolByIndex(idx).getString(arr, vm.getCurrentInstanceDataPtr());
  }



void WorldScript::concatstrings(Daedalus::DaedalusVM &vm) {
  const std::string& s2 = popString(vm);
  const std::string& s1 = popString(vm);

  vm.setReturn(s1 + s2);
  }

void WorldScript::inttostring(Daedalus::DaedalusVM &vm){
  int32_t x = vm.popDataValue<int32_t>();
  vm.setReturn(std::to_string(x)); //TODO: std::move?
  }

void WorldScript::floattostring(Daedalus::DaedalusVM &vm) {
  auto x = vm.popFloatValue();
  vm.setReturn(std::to_string(x)); //TODO: std::move?
  }

void WorldScript::floattoint(Daedalus::DaedalusVM &vm) {
  auto x = vm.popFloatValue();
  vm.setReturn(int32_t(x));
  }

void WorldScript::inttofloat(Daedalus::DaedalusVM &vm) {
  auto x = vm.popDataValue();
  vm.setReturn(float(x));
  }

void WorldScript::hlp_random(Daedalus::DaedalusVM &vm) {
  uint32_t mod = uint32_t(std::max(1,vm.popDataValue()));
  vm.setReturn(int32_t(randGen() % mod));
  }

void WorldScript::hlp_strcmp(Daedalus::DaedalusVM &vm) {
  auto& s1 = popString(vm);
  auto& s2 = popString(vm);
  vm.setReturn(s1 == s2 ? 1 : 0);
  }

void WorldScript::wld_settime(Daedalus::DaedalusVM &vm) {
  auto minute = vm.popDataValue();
  auto hour   = vm.popDataValue();
  owner.setDayTime(hour,minute);
  }

void WorldScript::mdl_setvisual(Daedalus::DaedalusVM &vm) {
  const std::string& visual   = popString(vm);
  uint32_t           arr_self = 0;
  uint32_t           self     = vm.popVar(arr_self);

  auto& npc = getNpcById(self);

  auto skelet = Resources::loadSkeleton (visual);
  npc.setVisual(skelet);
  }

void WorldScript::mdl_setvisualbody(Daedalus::DaedalusVM &vm) {
  int32_t     armor        = vm.popDataValue();
  int32_t     teethTexNr   = vm.popDataValue();
  int32_t     headTexNr    = vm.popDataValue();
  auto&       head         = popString(vm);
  int32_t     bodyTexColor = vm.popDataValue();
  int32_t     bodyTexNr    = vm.popDataValue();
  auto&       body         = popString(vm);
  uint32_t    arr_self     = 0;
  uint32_t    self         = vm.popVar(arr_self);

  auto& npc   = getNpcById(self);
  auto  vname = addExt(body,".MDM");
  auto  vhead = head.empty() ? StaticObjects::Mesh() : owner.getView(addExt(head,".MMB"),headTexNr,teethTexNr,bodyTexColor);
  auto  vbody = body.empty() ? StaticObjects::Mesh() : owner.getView(vname,bodyTexNr,0,bodyTexColor);

  npc.setPhysic(owner.getPhysic(vname));
  npc.setVisualBody(std::move(vhead),std::move(vbody));

  if(armor>=0) {
    auto handle = vm.getDATFile().getSymbolByIndex(self);
    auto hnpc   = ZMemory::handleCast<NpcHandle>(handle.instanceDataHandle);
    vm.getGameState().createInventoryItem(size_t(armor),hnpc);
    }
  }

void WorldScript::mdl_setmodelfatness(Daedalus::DaedalusVM &vm) {
  float    fat = vm.popFloatValue();
  uint32_t arr_self=0;
  uint32_t self = vm.popVar(arr_self);

  auto& npc = getNpcById(self);
  npc.setFatness(fat);
  }

void WorldScript::mdl_applyoverlaymds(Daedalus::DaedalusVM &vm) {
  auto&       overlayname = popString(vm);
  uint32_t    arr_self    = 0;
  uint32_t    self        = vm.popVar(arr_self);

  auto& npc    = getNpcById(self);
  auto  skelet = Resources::loadSkeleton(overlayname);
  npc.setOverlay(skelet,1.0);
  }

void WorldScript::mdl_setmodelscale(Daedalus::DaedalusVM &vm) {
  float z = vm.popFloatValue();
  float y = vm.popFloatValue();
  float x = vm.popFloatValue();
  uint32_t arr_self=0;
  uint32_t self = vm.popVar(arr_self);

  auto& npc = getNpcById(self);
  npc.setScale(x,y,z);
  }

void WorldScript::wld_insertnpc(Daedalus::DaedalusVM &vm) {
  const std::string& spawnpoint = popString(vm);
  int32_t npcInstance = vm.popDataValue();

  if(spawnpoint.empty() || npcInstance<0)
    return;

  auto at=owner.findPoint(spawnpoint);
  if(at==nullptr){
    Log::e("invalid waypoint \"",spawnpoint,"\"");
    return;
    }
  inserNpc(size_t(npcInstance),spawnpoint.c_str());
  }

void WorldScript::wld_insertitem(Daedalus::DaedalusVM &vm) {
  const std::string& spawnpoint   = popString(vm);
  int32_t            itemInstance = vm.popDataValue();

  Daedalus::GameState::ItemHandle   h    = vm.getGameState().insertItem(size_t(itemInstance));
  Daedalus::GEngineClasses::C_Item& data = vm.getGameState().getItem(h);

  //TODO
  }

void WorldScript::npc_settalentskill(Daedalus::DaedalusVM &vm){
  uint32_t arr_n0;
  int lvl     = vm.popDataValue();
  int t       = vm.popDataValue();
  uint32_t id = vm.popVar(arr_n0);
  getNpcById(id).setTalentSkill(Npc::Talent(t),lvl);
  }

void WorldScript::npc_settofightmode(Daedalus::DaedalusVM &vm) {
  int32_t weaponSymbol = vm.popDataValue();
  size_t  id           = vm.popVar();
  if(weaponSymbol<0)
    return;
  getNpcById(id).setToFightMode(uint32_t(weaponSymbol));
  }

void WorldScript::npc_settofistmode(Daedalus::DaedalusVM &vm) {
  uint32_t arr_self = 0;
  uint32_t self     = vm.popVar(arr_self);
  getNpcById(self).setToFistMode();
  }

void WorldScript::npc_isinstate(Daedalus::DaedalusVM &vm) {
  uint32_t stateFn = uint32_t(vm.popVar());
  uint32_t self    = vm.popVar();

  const bool ret=getNpcById(self).isState(stateFn);
  vm.setReturn(ret ? 1: 0);
  }

void WorldScript::npc_getdisttowp(Daedalus::DaedalusVM &vm) {
  auto&    wpname   = popString(vm);
  uint32_t arr_self = 0;
  uint32_t self     = vm.popVar(arr_self);

  auto* wp = owner.findPoint(wpname);
  if(wp!=nullptr){
    auto& npc=getNpcById(self);
    auto  dx = npc.position()[0]-wp->position.x;
    auto  dy = npc.position()[1]-wp->position.y;
    auto  dz = npc.position()[2]-wp->position.z;

    float ret = std::sqrt(dx*dx+dy*dy+dz*dz);
    if(ret<float(std::numeric_limits<int32_t>::max()))
      vm.setReturn(int32_t(ret));else
      vm.setReturn(std::numeric_limits<int32_t>::max());
    } else {
    vm.setReturn(std::numeric_limits<int32_t>::max());
    }
  }

void WorldScript::npc_exchangeroutine(Daedalus::DaedalusVM &vm) {
  auto&    wpname   = popString(vm);
  uint32_t arr_self = 0;
  uint32_t self     = vm.popVar(arr_self);
  Log::d("TODO: npc_exchangeroutine"); // TODO: npc_exchangeroutine
  }

void WorldScript::npc_isdead(Daedalus::DaedalusVM &vm) {
  uint32_t n   = vm.popVar();
  Npc&     npc = getNpcById(n);
  if(npc.isDead())
    vm.setReturn(1); else
    vm.setReturn(0);
  }

void WorldScript::ai_output(Daedalus::DaedalusVM &vm) {
  auto&    outputname = popString(vm);
  uint32_t target     = vm.popVar();
  uint32_t self       = vm.popVar();

  NpcHandle hself   = ZMemory::handleCast<NpcHandle>(vm.getDATFile().getSymbolByIndex(self).instanceDataHandle);
  NpcHandle htarget = ZMemory::handleCast<NpcHandle>(vm.getDATFile().getSymbolByIndex(target).instanceDataHandle);

  auto& message = dialogs->getMessageLib().getMessageByName(outputname);
  owner.aiOutput(message.text.c_str());
  }

void WorldScript::ai_stopprocessinfos(Daedalus::DaedalusVM &vm) {
  uint32_t self = vm.popVar();
  (void)self;
  owner.aiCloseDialog();
  }

void WorldScript::ai_standup(Daedalus::DaedalusVM &vm) {
  uint32_t self = vm.popVar();
  (void)self;
  Log::d("TODO: ai_standup");
  }

void WorldScript::ai_continueroutine(Daedalus::DaedalusVM &vm) {
  uint32_t arr_self = 0;
  uint32_t self     = vm.popVar(arr_self);
  (void)self;
  Log::d("TODO: ai_continueroutine");
  }

void WorldScript::ta_min(Daedalus::DaedalusVM &vm) {
  auto&    waypoint = popString(vm);
  int32_t  action   = vm.popDataValue();
  int32_t  stop_m   = vm.popDataValue();
  int32_t  stop_h   = vm.popDataValue();
  int32_t  start_m  = vm.popDataValue();
  int32_t  start_h  = vm.popDataValue();
  uint32_t arr_self = 0;
  uint32_t self     = vm.popVar(arr_self);

  // TODO
  Npc& npc = getNpcById(self);
  auto at=owner.findPoint(waypoint);
  if(at!=nullptr){
    npc.setPosition (at->position.x, at->position.y, at->position.z);
    npc.setDirection(at->direction.x,at->direction.y,at->direction.z);
    }

  npc.addRoutine(gtime(start_h,start_m),gtime(stop_h,stop_m),uint32_t(action));
  }

void WorldScript::log_createtopic(Daedalus::DaedalusVM &vm) {
  int32_t section   = vm.popDataValue();
  auto&   topicName = popString(vm);

  quests.add(topicName,QuestLog::Section(section));
  }

void WorldScript::log_settopicstatus(Daedalus::DaedalusVM &vm) {
  int32_t status    = vm.popDataValue();
  auto&   topicName = popString(vm);

  quests.setStatus(topicName,QuestLog::Status(status));
  }

void WorldScript::log_addentry(Daedalus::DaedalusVM &vm) {
  auto&   entry     = popString(vm);
  auto&   topicName = popString(vm);

  quests.addEntry(topicName,entry);
  }

void WorldScript::equipitem(Daedalus::DaedalusVM &vm) {
  uint32_t weaponSymbol = uint32_t(vm.popDataValue());
  uint32_t self         = vm.popVar();

  auto handle = vm.getDATFile().getSymbolByIndex(self);
  assert(handle.instanceDataClass==Daedalus::EInstanceClass::IC_Npc);

  NpcHandle hnpc = ZMemory::handleCast<NpcHandle>(vm.getDATFile().getSymbolByIndex(self).instanceDataHandle);
  auto&     npc  = getNpc(hnpc);

  npc.equipItem(weaponSymbol);
  }

void WorldScript::createinvitems(Daedalus::DaedalusVM &vm) {
  uint32_t amount       = uint32_t(vm.popDataValue());
  uint32_t itemInstance = uint32_t(vm.popDataValue());

  uint32_t  arr_n0 = 0;
  uint32_t  npc    = vm.popVar(arr_n0);
  NpcHandle hnpc   = ZMemory::handleCast<NpcHandle>(vm.getDATFile().getSymbolByIndex(npc).instanceDataHandle);

  vm.getGameState().createInventoryItem(itemInstance, hnpc, amount);
  }

void WorldScript::hlp_getinstanceid(Daedalus::DaedalusVM &vm) {
  uint32_t instance = vm.popVar();
  NpcHandle hnpc = ZMemory::handleCast<NpcHandle>(vm.getDATFile().getSymbolByIndex(instance).instanceDataHandle);
  if(hnpc.isValid()){
    auto& n=vmNpc(hnpc);
    vm.setReturn(int32_t(n.instanceSymbol));
    return;
    }
  vm.setReturn(0);
  }

void WorldScript::hlp_isvalidnpc(Daedalus::DaedalusVM &vm) {
  uint32_t instance = vm.popVar();
  NpcHandle hnpc = ZMemory::handleCast<NpcHandle>(vm.getDATFile().getSymbolByIndex(instance).instanceDataHandle);
  if(hnpc.isValid())
    vm.setReturn(1); else
    vm.setReturn(0);
  }

void WorldScript::hlp_getnpc(Daedalus::DaedalusVM &vm) {
  uint32_t instance = vm.popVar();
  // auto& npc=getNpcById(instance); //Debug
  vm.setReturn(int32_t(instance));
  }

void WorldScript::info_addchoice(Daedalus::DaedalusVM &vm) {
  uint32_t func         = vm.popVar();
  auto&    text         = popString(vm);
  uint32_t infoInstance = uint32_t(vm.popDataValue());

  ZMemory::BigHandle                h     = vm.getDATFile().getSymbolByIndex(infoInstance).instanceDataHandle;
  Daedalus::GameState::InfoHandle   hInfo = ZMemory::handleCast<Daedalus::GameState::InfoHandle>(h);
  Daedalus::GEngineClasses::C_Info& cInfo = vm.getGameState().getInfo(hInfo);

  cInfo.addChoice(Daedalus::GEngineClasses::SubChoice{text, func});
  }

void WorldScript::info_clearchoices(Daedalus::DaedalusVM &vm) {
  uint32_t infoInstance = uint32_t(vm.popDataValue());

  ZMemory::BigHandle                h     = vm.getDATFile().getSymbolByIndex(infoInstance).instanceDataHandle;
  Daedalus::GameState::InfoHandle   hInfo = ZMemory::handleCast<Daedalus::GameState::InfoHandle>(h);
  Daedalus::GEngineClasses::C_Info& cInfo = vm.getGameState().getInfo(hInfo);

  cInfo.subChoices.clear();
  }

void WorldScript::playvideo(Daedalus::DaedalusVM &vm) {
  const std::string& filename = popString(vm);
  Log::i("video not implemented [",filename,"]");
  vm.setReturn(0);
  }

void WorldScript::printscreen(Daedalus::DaedalusVM &vm) {
  int32_t            timesec  = vm.popDataValue();
  const std::string& font     = popString(vm);
  int32_t            posy     = vm.popDataValue();
  int32_t            posx     = vm.popDataValue();
  const std::string& msg      = popString(vm);
  int32_t            dialognr = vm.popDataValue();
  (void)dialognr;
  owner.printScreen(msg.c_str(),posx,posy,timesec,Resources::fontByName(font));
  }

void WorldScript::sort(std::vector<WorldScript::DlgChoise> &dlg) {
  std::sort(dlg.begin(),dlg.end(),[](const WorldScript::DlgChoise& l,const WorldScript::DlgChoise& r){
    return l.sort<r.sort;
    });
  }
