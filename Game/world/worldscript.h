#pragma once

#include <daedalus/DaedalusDialogManager.h>
#include <daedalus/DaedalusVM.h>

#include <memory>
#include <unordered_set>

class Gothic;
class World;
class Npc;

class WorldScript final {
  public:
    WorldScript(World& owner,Gothic &gothic, const char* world);

    bool    hasSymbolName(const std::string& fn);
    int32_t runFunction(const std::string &fname, bool clearDataStack);

    void    initDialogs(Gothic &gothic);

    size_t     npcCount()    const { return npcArr.size(); }
    const Npc& npc(size_t i) const { return *npcArr[i];    }

    const std::list<Daedalus::GameState::ItemHandle>& getInventoryOf(Daedalus::GameState::NpcHandle h);
    Daedalus::GameState::DaedalusGameState&           getGameState();

private:
    Daedalus::DaedalusVM vm;
    World&               owner;

    std::map<size_t,std::set<size_t>>                           dlgKnownInfos;
    std::unique_ptr<Daedalus::GameState::DaedalusDialogManager> dialogs;
    std::vector<std::unique_ptr<Npc>>                           npcArr;

    void               initCommon();
    static const std::string& popString(Daedalus::DaedalusVM &vm);

    template<class Ret,class ... Args>
    std::function<Ret(Args...)> notImplementedFn();

    void notImplementedRoutine(Daedalus::DaedalusVM&);

    void onInserNpc (Daedalus::GameState::NpcHandle handle, const std::string &s);
    void onRemoveNpc(Daedalus::GameState::NpcHandle handle);
    void onCreateInventoryItem(Daedalus::GameState::ItemHandle item,Daedalus::GameState::NpcHandle npc);

    Npc& getNpc(Daedalus::GameState::NpcHandle handle);
    Npc& getNpcById(size_t id);

    static void concatstrings(Daedalus::DaedalusVM& vm);
    static void inttostring  (Daedalus::DaedalusVM& vm);
    static void floattostring(Daedalus::DaedalusVM& vm);
    static void floattoint   (Daedalus::DaedalusVM& vm);
    static void inttofloat   (Daedalus::DaedalusVM& vm);
    static void hlp_random   (Daedalus::DaedalusVM& vm);

    void wld_insertitem(Daedalus::DaedalusVM& vm);
    void wld_insertnpc (Daedalus::DaedalusVM& vm);
    void wld_settime   (Daedalus::DaedalusVM& vm);

    void mdl_setvisual      (Daedalus::DaedalusVM& vm);
    void mdl_setvisualbody  (Daedalus::DaedalusVM& vm);
    void mdl_setmodelfatness(Daedalus::DaedalusVM& vm);
    void mdl_applyoverlaymds(Daedalus::DaedalusVM& vm);
    void mdl_setmodelscale  (Daedalus::DaedalusVM& vm);

    void npc_settalentskill(Daedalus::DaedalusVM &vm);
    void npc_settofightmode(Daedalus::DaedalusVM &vm);
    void npc_settofistmode (Daedalus::DaedalusVM &vm);

    void equipitem         (Daedalus::DaedalusVM &vm);
    void createinvitems    (Daedalus::DaedalusVM &vm);
  };
