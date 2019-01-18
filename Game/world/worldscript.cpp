#include "worldscript.h"

#include "gothic.h"
#include "npc.h"

using namespace Daedalus::GameState;

WorldScript::WorldScript(World &owner, Gothic& gothic, const char *world)
  :vm(gothic.path()+world),owner(owner){
  Daedalus::registerGothicEngineClasses(vm);
  initCommon();
  }

void WorldScript::initCommon() {

  auto& tbl = vm.getDATFile().getSymTable();
  for(auto& i:tbl.symbolsByName)
    vm.registerExternalFunction(i.first,[this](Daedalus::DaedalusVM& vm){return notImplementedRoutine(vm);});

  vm.registerExternalFunction("concatstrings", &WorldScript::concatstrings);
  vm.registerExternalFunction("inttostring",   &WorldScript::inttostring  );
  vm.registerExternalFunction("hlp_random",    &WorldScript::hlp_random   );

  vm.registerExternalFunction("wld_insertnpc", [this](Daedalus::DaedalusVM& vm){ wld_insertnpc(vm);  });
  vm.registerExternalFunction("wld_insertitem",[this](Daedalus::DaedalusVM& vm){ wld_insertitem(vm); });
  vm.registerExternalFunction("wld_settime",   [this](Daedalus::DaedalusVM& vm){ wld_settime(vm);    });

  vm.registerExternalFunction("npc_settalentskill", [this](Daedalus::DaedalusVM& vm){ npc_settalentskill(vm); });
  vm.registerExternalFunction("equipitem",          [this](Daedalus::DaedalusVM& vm){ equipitem(vm);          });
  vm.registerExternalFunction("createinvitems",     [this](Daedalus::DaedalusVM& vm){ createinvitems(vm);     });

  DaedalusGameState::GameExternals ext;
  ext.wld_insertnpc      = [this](NpcHandle h,const std::string& s){ onInserNpc(h,s); };
  ext.post_wld_insertnpc = [](NpcHandle){};
  ext.createinvitem      = [this](NpcHandle i,NpcHandle n){ onCreateInventoryItem(i,n); };

  ext.wld_removenpc      = notImplementedFn<void,NpcHandle>();
  ext.wld_insertitem     = notImplementedFn<void,NpcHandle>();//[this](ItemHandle h){ wld_insertitem(h); };
  ext.wld_GetDay         = notImplementedFn<int>();
  ext.log_createtopic    = notImplementedFn<void,std::string>();
  ext.log_settopicstatus = notImplementedFn<void,std::string>();
  ext.log_addentry       = notImplementedFn<void,std::string,std::string>();
  vm.getGameState().setGameExternals(ext);
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
        LogInfo() << "not implemented call";
        first=false;
        }
      return Ret();
      }
    };
  return std::function<Ret(Args...)>(_::fn);
  }

void WorldScript::notImplementedRoutine(Daedalus::DaedalusVM &vm) {
  static bool first=true;
  if(first){
    auto fn = vm.getCallStack();
    LogInfo() << "not implemented call[" << fn[0] << "]";
    first=false;
    }
  }

void WorldScript::onInserNpc(Daedalus::GameState::NpcHandle handle,const std::string& point) {
  LogInfo() << "onInserNpc[" << point << "]";
  auto  hnpc      = ZMemory::handleCast<NpcHandle>(handle);
  auto& npcData   = vm.getGameState().getNpc(hnpc);

  std::unique_ptr<Npc> ptr{new Npc()};
  npcData.userPtr = ptr.get();
  if(!npcData.name[0].empty())
    ptr->setName(npcData.name[0]);

  this->npc.insert(std::move(ptr));
  }

void WorldScript::onRemoveNpc(NpcHandle handle) {
  LogInfo() << "onRemoveNpc";
  delete &getNpc(handle);
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
  auto x = vm.popDataValue();
  vm.setReturn(std::to_string(x)); //TODO: std::move?
  }

void WorldScript::hlp_random(Daedalus::DaedalusVM &vm) {
  int32_t mod = vm.popDataValue();
  vm.setReturn(std::rand() % mod);
  }

void WorldScript::wld_settime(Daedalus::DaedalusVM &vm) {
  auto minute = vm.popDataValue();
  auto hour   = vm.popDataValue();
  LogInfo() << "wld_settime(" << hour << ":" << minute << ")";
  }

void WorldScript::wld_insertnpc(Daedalus::DaedalusVM &vm) {
  const std::string& spawnpoint = popString(vm);
  int32_t npcInstance = vm.popDataValue();

  if(spawnpoint.empty() || npcInstance<0)
    return;

  auto at=owner.findPoint(spawnpoint);
  if(at==nullptr){
    LogWarn() << "invalid waypoint: " << spawnpoint;
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

void WorldScript::equipitem(Daedalus::DaedalusVM &vm) {
  uint32_t instance = uint32_t(vm.popDataValue());
  uint32_t self     = vm.popVar();

  auto handle = vm.getDATFile().getSymbolByIndex(self);
  assert(handle.instanceDataClass == Daedalus::EInstanceClass::IC_Npc);

  NpcHandle hnpc = ZMemory::handleCast<NpcHandle>(vm.getDATFile().getSymbolByIndex(self).instanceDataHandle);
  auto&     npc  = getNpc(hnpc);

  Daedalus::GameState::ItemHandle item = vm.getGameState().createInventoryItem(instance,hnpc);
  npc.equipItem();
  }

void WorldScript::createinvitems(Daedalus::DaedalusVM &vm) {
  uint32_t amount       = uint32_t(vm.popDataValue());
  uint32_t itemInstance = uint32_t(vm.popDataValue());

  uint32_t  arr_n0 = 0;
  uint32_t  npc    = vm.popVar(arr_n0);
  NpcHandle hnpc   = ZMemory::handleCast<NpcHandle>(vm.getDATFile().getSymbolByIndex(npc).instanceDataHandle);

  vm.getGameState().createInventoryItem(itemInstance, hnpc, amount);
  }
