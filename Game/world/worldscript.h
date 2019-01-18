#pragma once

#include <daedalus/DaedalusDialogManager.h>
#include <daedalus/DaedalusVM.h>

#include <memory>
#include <unordered_set>

class Gothic;
class World;
class Npc;

class WorldScript {
  public:
    WorldScript(World& owner,Gothic &gothic, const char* world);

    bool    hasSymbolName(const std::string& fn);
    int32_t runFunction(const std::string &fname, bool clearDataStack);

    void    initDialogs(Gothic &gothic);

  private:
    Daedalus::DaedalusVM vm;
    World&               owner;

    std::map<size_t,std::set<size_t>>                           dlgKnownInfos;
    std::unique_ptr<Daedalus::GameState::DaedalusDialogManager> dialogs;
    std::unordered_set<std::unique_ptr<Npc>>                    npc;

    void               initCommon();
    static const std::string& popString(Daedalus::DaedalusVM &vm);

    template<class Ret,class ... Args>
    std::function<Ret(Args...)> notImplementedFn();

    void notImplementedRoutine(Daedalus::DaedalusVM&);

    void onInserNpc (Daedalus::GameState::NpcHandle handle, const std::string &s);
    void onRemoveNpc(Daedalus::GameState::NpcHandle handle);
    Npc& getNpc(Daedalus::GameState::NpcHandle handle);
    Npc& getNpcById(size_t id);
    void onCreateInventoryItem(Daedalus::GameState::ItemHandle item,Daedalus::GameState::NpcHandle npc);

    static void concatstrings(Daedalus::DaedalusVM& vm);
    static void inttostring  (Daedalus::DaedalusVM& vm);
    static void hlp_random   (Daedalus::DaedalusVM& vm);

    void wld_insertitem(Daedalus::DaedalusVM& vm);
    void wld_insertnpc (Daedalus::DaedalusVM& vm);
    void wld_settime   (Daedalus::DaedalusVM& vm);

    void npc_settalentskill(Daedalus::DaedalusVM &vm);
    void equipitem         (Daedalus::DaedalusVM &vm);
    void createinvitems    (Daedalus::DaedalusVM &vm);
  };
