#include "worldscript.h"

#include "gothic.h"
#include "npc.h"

#include <Tempest/Log>

using namespace Tempest;
using namespace Daedalus::GameState;

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
  vm.registerExternalFunction("hlp_random",    &WorldScript::hlp_random   );

  vm.registerExternalFunction("wld_insertnpc", [this](Daedalus::DaedalusVM& vm){ wld_insertnpc(vm);  });
  vm.registerExternalFunction("wld_insertitem",[this](Daedalus::DaedalusVM& vm){ wld_insertitem(vm); });
  vm.registerExternalFunction("wld_settime",   [this](Daedalus::DaedalusVM& vm){ wld_settime(vm);    });

  vm.registerExternalFunction("mdl_setvisual",       [this](Daedalus::DaedalusVM& vm){ mdl_setvisual(vm);       });
  vm.registerExternalFunction("mdl_setvisualbody",   [this](Daedalus::DaedalusVM& vm){ mdl_setvisualbody(vm);   });
  vm.registerExternalFunction("mdl_setmodelfatness", [this](Daedalus::DaedalusVM& vm){ mdl_setmodelfatness(vm); });
  vm.registerExternalFunction("mdl_applyoverlaymds", [this](Daedalus::DaedalusVM& vm){ mdl_applyoverlaymds(vm); });
  vm.registerExternalFunction("mdl_setmodelscale",   [this](Daedalus::DaedalusVM& vm){ mdl_setmodelscale(vm);   });

  vm.registerExternalFunction("npc_settalentskill",  [this](Daedalus::DaedalusVM& vm){ npc_settalentskill(vm); });
  vm.registerExternalFunction("npc_settofightmode",  [this](Daedalus::DaedalusVM& vm){ npc_settofightmode(vm); });
  vm.registerExternalFunction("npc_settofistmode",   [this](Daedalus::DaedalusVM& vm){ npc_settofistmode(vm);  });

  vm.registerExternalFunction("log_createtopic",     [this](Daedalus::DaedalusVM& vm){ log_createtopic(vm);    });
  vm.registerExternalFunction("log_settopicstatus",  [this](Daedalus::DaedalusVM& vm){ log_settopicstatus(vm); });
  vm.registerExternalFunction("log_addentry",        [this](Daedalus::DaedalusVM& vm){ log_addentry(vm);       });

  vm.registerExternalFunction("equipitem",           [this](Daedalus::DaedalusVM& vm){ equipitem(vm);          });
  vm.registerExternalFunction("createinvitems",      [this](Daedalus::DaedalusVM& vm){ createinvitems(vm);     });

  vm.registerExternalFunction("playvideo",           [this](Daedalus::DaedalusVM& vm){ playvideo(vm);          });

  DaedalusGameState::GameExternals ext;
  ext.wld_insertnpc      = [this](NpcHandle h,const std::string& s){ onInserNpc(h,s); };
  ext.post_wld_insertnpc = [](NpcHandle){};
  ext.createinvitem      = [this](NpcHandle i,NpcHandle n){ onCreateInventoryItem(i,n); };

  ext.wld_removenpc      = notImplementedFn<void,NpcHandle>();
  ext.wld_insertitem     = notImplementedFn<void,NpcHandle>();
  ext.wld_GetDay         = notImplementedFn<int>();
  ext.log_createtopic    = notImplementedFn<void,std::string>();
  ext.log_settopicstatus = notImplementedFn<void,std::string>();
  ext.log_addentry       = notImplementedFn<void,std::string,std::string>();
  vm.getGameState().setGameExternals(ext);
  }

const std::list<Daedalus::GameState::ItemHandle> &WorldScript::getInventoryOf(Daedalus::GameState::NpcHandle h) {
  return vm.getGameState().getInventoryOf(h);
  }

DaedalusGameState &WorldScript::getGameState() {
  return vm.getGameState();
  }

bool WorldScript::hasSymbolName(const std::string &fn) {
  return vm.getDATFile().hasSymbolName(fn);
  }

int32_t WorldScript::runFunction(const std::string& fname, bool clearDataStack) {
  if(!hasSymbolName(fname))
    throw std::runtime_error("script bad call");
  vm.prepareRunFunction();
  auto    id  = vm.getDATFile().getSymbolIndexByName(fname);
  int32_t ret = vm.runFunctionBySymIndex(id,clearDataStack);
  return ret;
  }

void WorldScript::initDialogs(Gothic& gothic) {
  const char* german       ="_work/data/Scripts/content/CUTSCENE/OU.BIN";
  const char* international="_work/data/Scripts/content/CUTSCENE/OU.DAT";

  dialogs.reset(new Daedalus::GameState::DaedalusDialogManager(vm,gothic.path() + international,dlgKnownInfos));
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

  if(pos!=nullptr)
    ptr->setPosition(pos->position.x,pos->position.y,pos->position.z);

  npcArr.emplace_back(std::move(ptr));
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
  return *reinterpret_cast<Npc*>(npcData.userPtr);
  }

Npc& WorldScript::getNpcById(size_t id) {
  auto handle = vm.getDATFile().getSymbolByIndex(id);
  assert(handle.instanceDataClass == Daedalus::EInstanceClass::IC_Npc);

  NpcHandle hnpc = ZMemory::handleCast<NpcHandle>(vm.getDATFile().getSymbolByIndex(id).instanceDataHandle);
  return getNpc(hnpc);
  }

void WorldScript::onCreateInventoryItem(ItemHandle item, NpcHandle npcHandle) {
  Daedalus::GEngineClasses::C_Item& itemData = vm.getGameState().getItem(item);
  Npc&                              npc      = getNpc(npcHandle);

  if((itemData.mainflag & Daedalus::GEngineClasses::C_Item::ITM_CAT_ARMOR)!=0) {
    // TODO: equiping armor

    std::string visual = itemData.visual_change.substr(0, itemData.visual_change.size() - 4) + ".MDM";
    //VobTypes::NPC_SetBodyMesh(vob, visual);
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
  int32_t x = vm.popDataValue();
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
  int32_t mod = vm.popDataValue();
  vm.setReturn(std::rand() % mod);
  }

void WorldScript::wld_settime(Daedalus::DaedalusVM &vm) {
  auto minute = vm.popDataValue();
  auto hour   = vm.popDataValue();
  Log::i("wld_settime(",hour,":",minute,")");
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
  uint32_t    self = vm.popVar(arr_self);

  auto& npc   = getNpcById(self);
  auto  vhead = head.empty() ? StaticObjects::Mesh() : owner.getView(head+".MMB");
  auto  vbody = body.empty() ? StaticObjects::Mesh() : owner.getView(body+".MDM");
  npc.setVisualBody(std::move(vhead),std::move(vbody));

  if(armor>=0)
    ;//vm.getGameState().createInventoryItem(size_t(armor), hnpc);
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

  auto& npc = getNpcById(self);
  npc.setOverlay(overlayname,1.0);
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

  vm.getGameState().insertNPC(size_t(npcInstance),spawnpoint);
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

void WorldScript::playvideo(Daedalus::DaedalusVM &vm) {
  const std::string& filename = popString(vm);
  Log::i("video not implemented [",filename,"]");
  vm.setReturn(0);
  }
