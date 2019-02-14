#pragma once

#include <daedalus/DaedalusDialogManager.h>
#include <daedalus/DaedalusStdlib.h>
#include <daedalus/DaedalusVM.h>

#include <memory>
#include <unordered_set>

#include "game/questlog.h"

class Gothic;
class World;
class Npc;

class WorldScript final {
  public:
    WorldScript(World& owner,Gothic &gothic, const char* world);

    struct DlgChoise final {
      std::string title;
      int32_t     sort=0;
      };

    bool    hasSymbolName(const std::string& fn);
    int32_t runFunction(const std::string &fname, bool clearDataStack);
    int32_t runFunction(const size_t       fid,   bool clearDataStack);

    void       initDialogs(Gothic &gothic);

    const World& world() const { return owner; }
    void         tick(uint64_t dt);
    uint64_t     tickCount() const;

    size_t     npcCount()    const { return npcArr.size(); }
    const Npc& npc(size_t i) const { return *npcArr[i];    }
    Npc&       npc(size_t i)       { return *npcArr[i];    }

    Npc*       inserNpc(const char* npcInstance,const char *at);

    const std::list<Daedalus::GameState::ItemHandle>& getInventoryOf(Daedalus::GameState::NpcHandle h);
    Daedalus::GameState::DaedalusGameState&           getGameState();
    Daedalus::PARSymbol&                              getSymbol(const char* s);
    Daedalus::GEngineClasses::C_Npc&                  vmNpc(Daedalus::GameState::NpcHandle handle);

    std::vector<DlgChoise> dialogChoises(Daedalus::GameState::NpcHandle self,
                                         Daedalus::GameState::NpcHandle npc);

  private:
    void               initCommon();

    static const std::string& popString(Daedalus::DaedalusVM &vm);

    template<class Ret,class ... Args>
    std::function<Ret(Args...)> notImplementedFn();

    void notImplementedRoutine(Daedalus::DaedalusVM&);

    void onInserNpc (Daedalus::GameState::NpcHandle handle, const std::string &s);
    void onNpcReady (Daedalus::GameState::NpcHandle handle);
    void onRemoveNpc(Daedalus::GameState::NpcHandle handle);
    void onCreateInventoryItem(Daedalus::GameState::ItemHandle item,Daedalus::GameState::NpcHandle npc);

    Npc& getNpc(Daedalus::GameState::NpcHandle handle);
    Npc& getNpcById(size_t id);

    Npc* inserNpc(size_t npcInstance,const char *at);

    static void concatstrings(Daedalus::DaedalusVM& vm);
    static void inttostring  (Daedalus::DaedalusVM& vm);
    static void floattostring(Daedalus::DaedalusVM& vm);
    static void floattoint   (Daedalus::DaedalusVM& vm);
    static void inttofloat   (Daedalus::DaedalusVM& vm);
    static void hlp_random   (Daedalus::DaedalusVM& vm);

    void wld_insertitem      (Daedalus::DaedalusVM& vm);
    void wld_insertnpc       (Daedalus::DaedalusVM& vm);
    void wld_settime         (Daedalus::DaedalusVM& vm);

    void mdl_setvisual       (Daedalus::DaedalusVM& vm);
    void mdl_setvisualbody   (Daedalus::DaedalusVM& vm);
    void mdl_setmodelfatness (Daedalus::DaedalusVM& vm);
    void mdl_applyoverlaymds (Daedalus::DaedalusVM& vm);
    void mdl_setmodelscale   (Daedalus::DaedalusVM& vm);

    void npc_settalentskill  (Daedalus::DaedalusVM &vm);
    void npc_settofightmode  (Daedalus::DaedalusVM &vm);
    void npc_settofistmode   (Daedalus::DaedalusVM &vm);

    void ta_min              (Daedalus::DaedalusVM &vm);

    void log_createtopic     (Daedalus::DaedalusVM &vm);
    void log_settopicstatus  (Daedalus::DaedalusVM &vm);
    void log_addentry        (Daedalus::DaedalusVM &vm);

    void equipitem           (Daedalus::DaedalusVM &vm);
    void createinvitems      (Daedalus::DaedalusVM &vm);

    void playvideo           (Daedalus::DaedalusVM &vm);

    Daedalus::DaedalusVM vm;
    World&               owner;
    uint64_t             ticks=0;

    std::map<size_t,std::set<size_t>>                           dlgKnownInfos;
    std::vector<Daedalus::GameState::InfoHandle>                dialogsInfo;
    std::unique_ptr<Daedalus::GameState::DaedalusDialogManager> dialogs;

    QuestLog                                                    quests;
    std::vector<std::unique_ptr<Npc>>                           npcArr;
  };
