#pragma once

#include <daedalus/DaedalusDialogManager.h>
#include <daedalus/DaedalusStdlib.h>
#include <daedalus/DaedalusVM.h>

#include <memory>
#include <unordered_set>
#include <random>

#include "game/aistate.h"
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

    enum Attitude : int32_t {
      ATT_HOSTILE  = 0,
      ATT_ANGRY    = 1,
      ATT_NEUTRAL  = 2,
      ATT_FRIENDLY = 3
      };

    bool    hasSymbolName(const std::string& fn);
    int32_t runFunction(const std::string &fname);
    int32_t runFunction(const size_t       fid, bool clearStk=true);

    void       initDialogs(Gothic &gothic);

    const World& world() const { return owner; }
    uint64_t     tickCount() const;

    Npc*       inserNpc(const char* npcInstance,const char *at);
    void       removeItem(Item& it);

    Npc*       getNpc(Daedalus::GameState::NpcHandle handle);
    void       setInstanceNPC(const char* name,Npc& npc);

    size_t      goldId() const { return itMi_Gold; }
    const char* currencyName() const { return goldTxt.c_str(); }

    const Daedalus::GEngineClasses::C_Focus&          focusNorm()  const { return cFocusNorm;  }
    const Daedalus::GEngineClasses::C_Focus&          focusMele()  const { return cFocusMele;  }
    const Daedalus::GEngineClasses::C_Focus&          focusRange() const { return cFocusRange; }
    const Daedalus::GEngineClasses::C_Focus&          focusMage()  const { return cFocusMage;  }

    Daedalus::GameState::DaedalusGameState&           getGameState();
    Daedalus::PARSymbol&                              getSymbol(const char*  s);
    Daedalus::PARSymbol&                              getSymbol(const size_t s);
    size_t                                            getSymbolIndex(const char* s);
    size_t                                            getSymbolIndex(const std::string& s);
    const AiState&                                    getAiState(size_t id);

    Daedalus::GEngineClasses::C_Npc&                  vmNpc (Daedalus::GameState::NpcHandle  handle);
    Daedalus::GEngineClasses::C_Item&                 vmItem(Daedalus::GameState::ItemHandle handle);

    auto dialogChoises(Daedalus::GameState::NpcHandle self, Daedalus::GameState::NpcHandle npc, const std::vector<uint32_t> &except) -> std::vector<DlgChoise>;
    auto updateDialog (const WorldScript::DlgChoise &dlg, Npc &player, Npc &npc) -> std::vector<WorldScript::DlgChoise>;

    void exec(const DlgChoise &dlg, Daedalus::GameState::NpcHandle player, Daedalus::GameState::NpcHandle hnpc);
    int  printCannotUseError (Npc &npc, int32_t atr, int32_t nValue);
    int  printCannotCastError(Npc &npc, int32_t plM, int32_t itM);

    int  invokeState(Daedalus::GameState::NpcHandle hnpc, Daedalus::GameState::NpcHandle hother, const char* name);
    int  invokeState(Npc* npc, Npc* other, Npc *victum, size_t fn);
    int  invokeItem (Npc* npc, size_t fn);
    int  invokeMana (Npc& npc, Item&  fn);
    int  invokeSpell(Npc& npc, Item&  fn);

    int  spellCastAnim(Npc& npc, Item&  fn);

    bool aiUseMob  (Npc &pl, const std::string& name);

    void useInteractive(Daedalus::GameState::NpcHandle hnpc, const std::string &func);

    Attitude guildAttitude(const Npc& p0,const Npc& p1) const;

  private:
    void               initCommon();

    struct ScopeVar;

    static const std::string& popString  (Daedalus::DaedalusVM &vm);
    Npc*                      popInstance(Daedalus::DaedalusVM &vm);

    template<class Ret,class ... Args>
    std::function<Ret(Args...)> notImplementedFn();

    void notImplementedRoutine(Daedalus::DaedalusVM&);

    void onNpcReady (Daedalus::GameState::NpcHandle handle);

    Item* getItem(Daedalus::GameState::ItemHandle handle);
    Npc*  getNpcById(size_t id);
    Npc*  inserNpc  (size_t npcInstance, const char *at);
    auto  getFocus(const char* name) -> Daedalus::GEngineClasses::C_Focus;

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
    void wld_setguildattitude(Daedalus::DaedalusVM& vm);
    void wld_getguildattitude(Daedalus::DaedalusVM& vm);
    void wld_istime          (Daedalus::DaedalusVM& vm);
    void wld_isfpavailable   (Daedalus::DaedalusVM& vm);
    void wld_ismobavailable  (Daedalus::DaedalusVM& vm);
    void wld_setmobroutine   (Daedalus::DaedalusVM& vm);
    void wld_assignroomtoguild(Daedalus::DaedalusVM& vm);

    void mdl_setvisual       (Daedalus::DaedalusVM& vm);
    void mdl_setvisualbody   (Daedalus::DaedalusVM& vm);
    void mdl_setmodelfatness (Daedalus::DaedalusVM& vm);
    void mdl_applyoverlaymds (Daedalus::DaedalusVM& vm);
    void mdl_applyoverlaymdstimed(Daedalus::DaedalusVM& vm);
    void mdl_removeoverlaymds(Daedalus::DaedalusVM& vm);
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
    void npc_setperctime     (Daedalus::DaedalusVM &vm);
    void npc_percenable      (Daedalus::DaedalusVM &vm);
    void npc_percdisable     (Daedalus::DaedalusVM &vm);
    void npc_getnearestwp    (Daedalus::DaedalusVM &vm);
    void npc_clearaiqueue    (Daedalus::DaedalusVM &vm);
    void npc_isplayer        (Daedalus::DaedalusVM &vm);
    void npc_getstatetime    (Daedalus::DaedalusVM &vm);
    void npc_setstatetime    (Daedalus::DaedalusVM &vm);
    void npc_changeattribute (Daedalus::DaedalusVM &vm);
    void npc_isonfp          (Daedalus::DaedalusVM &vm);
    void npc_getheighttonpc  (Daedalus::DaedalusVM &vm);
    void npc_getequippedarmor(Daedalus::DaedalusVM &vm);
    void npc_canseenpc       (Daedalus::DaedalusVM &vm);
    void npc_hasequippedmeleeweapon(Daedalus::DaedalusVM &vm);
    void npc_hasequippedrangedweapon(Daedalus::DaedalusVM &vm);
    void npc_getactivespell  (Daedalus::DaedalusVM &vm);
    void npc_getactivespellisscroll(Daedalus::DaedalusVM &vm);
    void npc_canseenpcfreelos(Daedalus::DaedalusVM &vm);
    void npc_isinfightmode   (Daedalus::DaedalusVM &vm);
    void npc_settarget       (Daedalus::DaedalusVM &vm);
    void npc_gettarget       (Daedalus::DaedalusVM &vm);
    void npc_sendpassiveperc (Daedalus::DaedalusVM &vm);
    void npc_checkinfo       (Daedalus::DaedalusVM &vm);
    void npc_getportalguild  (Daedalus::DaedalusVM &vm);
    void npc_isinplayersroom (Daedalus::DaedalusVM &vm);

    void ai_output           (Daedalus::DaedalusVM &vm);
    void ai_stopprocessinfos (Daedalus::DaedalusVM &vm);
    void ai_processinfos     (Daedalus::DaedalusVM &vm);
    void ai_standup          (Daedalus::DaedalusVM &vm);
    void ai_standupquick     (Daedalus::DaedalusVM &vm);
    void ai_continueroutine  (Daedalus::DaedalusVM &vm);
    void ai_stoplookat       (Daedalus::DaedalusVM &vm);
    void ai_lookatnpc        (Daedalus::DaedalusVM &vm);
    void ai_removeweapon     (Daedalus::DaedalusVM &vm);
    void ai_turntonpc        (Daedalus::DaedalusVM &vm);
    void ai_outputsvm        (Daedalus::DaedalusVM &vm);
    void ai_startstate       (Daedalus::DaedalusVM &vm);
    void ai_playani          (Daedalus::DaedalusVM &vm);
    void ai_setwalkmode      (Daedalus::DaedalusVM &vm);
    void ai_wait             (Daedalus::DaedalusVM &vm);
    void ai_waitms           (Daedalus::DaedalusVM &vm);
    void ai_aligntowp        (Daedalus::DaedalusVM &vm);
    void ai_gotowp           (Daedalus::DaedalusVM &vm);
    void ai_gotofp           (Daedalus::DaedalusVM &vm);
    void ai_playanibs        (Daedalus::DaedalusVM &vm);
    void ai_equipbestmeleeweapon(Daedalus::DaedalusVM &vm);
    void ai_usemob           (Daedalus::DaedalusVM &vm);
    void ai_teleport         (Daedalus::DaedalusVM &vm);
    void ai_stoppointat      (Daedalus::DaedalusVM &vm);
    void ai_readymeleeweapon (Daedalus::DaedalusVM &vm);
    void ai_readyspell       (Daedalus::DaedalusVM &vm);
    void ai_atack            (Daedalus::DaedalusVM &vm);
    void ai_flee             (Daedalus::DaedalusVM &vm);
    void ai_dodge            (Daedalus::DaedalusVM &vm);
    void ai_unequipweapons   (Daedalus::DaedalusVM &vm);

    void mob_hasitems        (Daedalus::DaedalusVM &vm);

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

    void introducechapter    (Daedalus::DaedalusVM &vm);
    void playvideo           (Daedalus::DaedalusVM &vm);
    void printscreen         (Daedalus::DaedalusVM &vm);
    void print               (Daedalus::DaedalusVM &vm);
    void perc_setrange       (Daedalus::DaedalusVM &vm);

    void sort(std::vector<DlgChoise>& dlg);

    Daedalus::DaedalusVM vm;
    uint8_t              invokeRecursive=0;
    World&               owner;
    std::mt19937         randGen;

    std::map<size_t,std::set<size_t>>                           dlgKnownInfos;
    std::vector<Daedalus::GameState::InfoHandle>                dialogsInfo;
    std::unique_ptr<Daedalus::GameState::DaedalusDialogManager> dialogs;
    std::unordered_map<size_t,AiState>                          aiStates;

    QuestLog                                                    quests;
    size_t                                                      itMi_Gold=0;
    size_t                                                      spellFxInstanceNames=0;
    size_t                                                      spellFxAniLetters=0;
    std::string                                                 goldTxt;
    float                                                       viewTimePerChar=0.5;
    size_t                                                      gilCount=0;
    std::vector<int32_t>                                        gilAttitudes;

    Daedalus::GEngineClasses::C_Focus                           cFocusNorm,cFocusMele,cFocusRange,cFocusMage;
  };
