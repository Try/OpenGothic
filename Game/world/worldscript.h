#pragma once

#include <daedalus/DaedalusDialogManager.h>
#include <daedalus/DaedalusStdlib.h>
#include <daedalus/DaedalusVM.h>

#include <memory>
#include <unordered_set>
#include <random>

#include "game/questlog.h"

class Gothic;
class World;
class Npc;
class Item;

class WorldScript final {
  public:
    WorldScript(World& owner,Gothic &gothic, const char* world);

    struct DlgChoise final {
      std::string                     title;
      int32_t                         sort=0;
      uint32_t                        scriptFn=0;
      Daedalus::GameState::InfoHandle handle={};
      };

    bool    hasSymbolName(const std::string& fn);
    int32_t runFunction(const std::string &fname);
    int32_t runFunction(const size_t       fid);

    void       initDialogs(Gothic &gothic);

    const World& world() const { return owner; }
    uint64_t     tickCount() const;

    Npc*       inserNpc(const char* npcInstance,const char *at);
    Npc*       getNpc(Daedalus::GameState::NpcHandle handle);
    void       setInstanceNPC(const char* name,Npc& npc);

    const std::list<Daedalus::GameState::ItemHandle>& getInventoryOf(Daedalus::GameState::NpcHandle h);
    Daedalus::GameState::DaedalusGameState&           getGameState();
    Daedalus::PARSymbol&                              getSymbol(const char* s);
    size_t                                            getSymbolIndex(const char* s);

    Daedalus::GEngineClasses::C_Npc&                  vmNpc (Daedalus::GameState::NpcHandle  handle);
    Daedalus::GEngineClasses::C_Item&                 vmItem(Daedalus::GameState::ItemHandle handle);

    auto dialogChoises(Daedalus::GameState::NpcHandle self, Daedalus::GameState::NpcHandle npc) -> std::vector<DlgChoise>;
    auto updateDialog (const WorldScript::DlgChoise &dlg, Npc &player, Npc &npc) -> std::vector<WorldScript::DlgChoise>;

    void exec(const DlgChoise &dlg, Daedalus::GameState::NpcHandle player, Daedalus::GameState::NpcHandle hnpc);

    int  invokeState(Daedalus::GameState::NpcHandle hnpc, Daedalus::GameState::NpcHandle hother, const char* name);

    void useInteractive(Daedalus::GameState::NpcHandle hnpc, const std::string &func);

  private:
    void               initCommon();

    enum Attitude : int32_t {
      ATT_HOSTILE  = 0,
      ATT_ANGRY    = 1,
      ATT_NEUTRAL  = 2,
      ATT_FRIENDLY = 3
      };

    struct ScopeVar;

    static const std::string& popString  (Daedalus::DaedalusVM &vm);
    Npc*                      popInstance(Daedalus::DaedalusVM &vm);

    template<class Ret,class ... Args>
    std::function<Ret(Args...)> notImplementedFn();

    void notImplementedRoutine(Daedalus::DaedalusVM&);

    void onNpcReady (Daedalus::GameState::NpcHandle handle);

    void onCreateInventoryItem(Daedalus::GameState::ItemHandle item,Daedalus::GameState::NpcHandle npc);

    Item* getItem(Daedalus::GameState::ItemHandle handle);

    Npc*  getNpcById(size_t id);

    Npc*  inserNpc  (size_t npcInstance, const char *at);

    static void concatstrings(Daedalus::DaedalusVM& vm);
    static void inttostring  (Daedalus::DaedalusVM& vm);
    static void floattostring(Daedalus::DaedalusVM& vm);
    static void floattoint   (Daedalus::DaedalusVM& vm);
    static void inttofloat   (Daedalus::DaedalusVM& vm);

    static void hlp_strcmp   (Daedalus::DaedalusVM& vm);
    void hlp_random          (Daedalus::DaedalusVM& vm);
    void hlp_isvalidnpc      (Daedalus::DaedalusVM &vm);
    void hlp_getinstanceid   (Daedalus::DaedalusVM &vm);
    void hlp_getnpc          (Daedalus::DaedalusVM &vm);

    void wld_insertitem      (Daedalus::DaedalusVM& vm);
    void wld_insertnpc       (Daedalus::DaedalusVM& vm);
    void wld_settime         (Daedalus::DaedalusVM& vm);
    void wld_getday          (Daedalus::DaedalusVM& vm);
    void wld_playeffect      (Daedalus::DaedalusVM& vm);
    void wld_stopeffect      (Daedalus::DaedalusVM& vm);
    void wld_getplayerportalguild(Daedalus::DaedalusVM& vm);
    void wld_getguildattitude(Daedalus::DaedalusVM& vm);

    void mdl_setvisual       (Daedalus::DaedalusVM& vm);
    void mdl_setvisualbody   (Daedalus::DaedalusVM& vm);
    void mdl_setmodelfatness (Daedalus::DaedalusVM& vm);
    void mdl_applyoverlaymds (Daedalus::DaedalusVM& vm);
    void mdl_setmodelscale   (Daedalus::DaedalusVM& vm);
    void mdl_startfaceani    (Daedalus::DaedalusVM& vm);

    void npc_settofightmode  (Daedalus::DaedalusVM &vm);
    void npc_settofistmode   (Daedalus::DaedalusVM &vm);
    void npc_isinstate       (Daedalus::DaedalusVM &vm);
    void npc_getdisttowp     (Daedalus::DaedalusVM &vm);
    void npc_exchangeroutine (Daedalus::DaedalusVM &vm);
    void npc_isdead          (Daedalus::DaedalusVM &vm);
    void npc_knowsinfo       (Daedalus::DaedalusVM &vm);
    void npc_settalentskill  (Daedalus::DaedalusVM &vm);
    void npc_gettalentskill  (Daedalus::DaedalusVM &vm);
    void npc_settalentvalue  (Daedalus::DaedalusVM &vm);
    void npc_gettalentvalue  (Daedalus::DaedalusVM &vm);
    void npc_setrefusetalk   (Daedalus::DaedalusVM &vm);
    void npc_refusetalk      (Daedalus::DaedalusVM &vm);
    void npc_hasitems        (Daedalus::DaedalusVM &vm);
    void npc_removeinvitems  (Daedalus::DaedalusVM &vm);
    void npc_getbodystate    (Daedalus::DaedalusVM &vm);
    void npc_getlookattarget (Daedalus::DaedalusVM &vm);
    void npc_getdisttonpc    (Daedalus::DaedalusVM &vm);
    void npc_hasequippedarmor(Daedalus::DaedalusVM &vm);
    void npc_getattitude     (Daedalus::DaedalusVM &vm);

    void ai_output           (Daedalus::DaedalusVM &vm);
    void ai_stopprocessinfos (Daedalus::DaedalusVM &vm);
    void ai_processinfos     (Daedalus::DaedalusVM &vm);
    void ai_standup          (Daedalus::DaedalusVM &vm);
    void ai_continueroutine  (Daedalus::DaedalusVM &vm);
    void ai_stoplookat       (Daedalus::DaedalusVM &vm);
    void ai_lookatnpc        (Daedalus::DaedalusVM &vm);
    void ai_removeweapon     (Daedalus::DaedalusVM &vm);
    void ai_turntonpc        (Daedalus::DaedalusVM &vm);
    void ai_outputsvm        (Daedalus::DaedalusVM &vm);

    void ta_min              (Daedalus::DaedalusVM &vm);

    void log_createtopic     (Daedalus::DaedalusVM &vm);
    void log_settopicstatus  (Daedalus::DaedalusVM &vm);
    void log_addentry        (Daedalus::DaedalusVM &vm);

    void equipitem           (Daedalus::DaedalusVM &vm);
    void createinvitem       (Daedalus::DaedalusVM &vm);
    void createinvitems      (Daedalus::DaedalusVM &vm);

    void info_addchoice      (Daedalus::DaedalusVM &vm);
    void info_clearchoices   (Daedalus::DaedalusVM &vm);
    void infomanager_hasfinished(Daedalus::DaedalusVM &vm);

    void playvideo           (Daedalus::DaedalusVM &vm);
    void printscreen         (Daedalus::DaedalusVM &vm);
    void print               (Daedalus::DaedalusVM &vm);

    void sort(std::vector<DlgChoise>& dlg);

    Daedalus::DaedalusVM vm;
    bool                 invokeRecursive=false;
    World&               owner;
    std::mt19937         randGen;

    std::map<size_t,std::set<size_t>>                           dlgKnownInfos;
    std::vector<Daedalus::GameState::InfoHandle>                dialogsInfo;
    std::unique_ptr<Daedalus::GameState::DaedalusDialogManager> dialogs;

    QuestLog                                                    quests;
    //std::vector<std::unique_ptr<Npc>>                           npcArr;
    //std::vector<std::unique_ptr<Item>>                          itemArr;
  };
