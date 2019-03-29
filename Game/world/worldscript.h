#pragma once

#include <daedalus/DaedalusDialogManager.h>
#include <daedalus/DaedalusStdlib.h>
#include <daedalus/DaedalusVM.h>

#include <memory>
#include <unordered_set>
#include <random>

#include <Tempest/Matrix4x4>
#include <Tempest/Painter>

#include "game/fightai.h"
#include "game/aistate.h"
#include "game/questlog.h"

class Gothic;
class GameSession;
class World;
class Npc;
class Item;

class WorldScript final {
  public:
    WorldScript(GameSession &gothic);

    struct DlgChoise final {
      std::string                       title;
      int32_t                           sort=0;
      uint32_t                          scriptFn=0;
      bool                              isTrade=false;
      Daedalus::GEngineClasses::C_Info* handle=nullptr;
      };

    enum Attitude : int32_t {
      ATT_HOSTILE  = 0,
      ATT_ANGRY    = 1,
      ATT_NEUTRAL  = 2,
      ATT_FRIENDLY = 3
      };

    bool       hasSymbolName(const char *fn);
    int32_t    runFunction  (const char* fname);
    int32_t    runFunction  (const size_t       fid, bool clearStk=true);

    void       initDialogs (Gothic &gothic);
    void       loadDialogOU(Gothic &gothic);

    const World& world() const;
    World&       world();
    uint64_t     tickCount() const;

    uint32_t     rand(uint32_t max);

    Npc*       inserNpc(const char* npcInstance,const char *at);
    void       removeItem(Item& it);

    Npc*       getNpc(Daedalus::GEngineClasses::C_Npc* handle);
    void       setInstanceNPC(const char* name,Npc& npc);

    size_t      goldId() const { return itMi_Gold; }
    const char* currencyName() const { return goldTxt.c_str(); }

    const Daedalus::GEngineClasses::C_Focus&          focusNorm()  const { return cFocusNorm;  }
    const Daedalus::GEngineClasses::C_Focus&          focusMele()  const { return cFocusMele;  }
    const Daedalus::GEngineClasses::C_Focus&          focusRange() const { return cFocusRange; }
    const Daedalus::GEngineClasses::C_Focus&          focusMage()  const { return cFocusMage;  }
    const Daedalus::GEngineClasses::C_GilValues&      guildVal()   const { return cGuildVal;   }
    const FightAi::FA&                                getFightAi(size_t i) const;

    Daedalus::GameState::DaedalusGameState&           getGameState();
    Daedalus::PARSymbol&                              getSymbol(const char*  s);
    Daedalus::PARSymbol&                              getSymbol(const size_t s);
    size_t                                            getSymbolIndex(const char* s);
    size_t                                            getSymbolIndex(const std::string& s);
    const AiState&                                    getAiState(size_t id);

    Daedalus::GEngineClasses::C_Item&                 vmItem(Daedalus::GEngineClasses::C_Item *handle);

    auto dialogChoises(Daedalus::GEngineClasses::C_Npc *self, Daedalus::GEngineClasses::C_Npc *npc, const std::vector<uint32_t> &except) -> std::vector<DlgChoise>;
    auto updateDialog (const WorldScript::DlgChoise &dlg, Npc &player, Npc &npc) -> std::vector<WorldScript::DlgChoise>;

    void exec(const DlgChoise &dlg, Daedalus::GEngineClasses::C_Npc *player, Daedalus::GEngineClasses::C_Npc *hnpc);
    int  printCannotUseError (Npc &npc, int32_t atr, int32_t nValue);
    int  printCannotCastError(Npc &npc, int32_t plM, int32_t itM);
    int  printCannotBuyError (Npc &npc);

    int  invokeState(Daedalus::GEngineClasses::C_Npc *hnpc, Daedalus::GEngineClasses::C_Npc *hother, const char* name);
    int  invokeState(Npc* npc, Npc* other, Npc *victum, size_t fn);
    int  invokeItem (Npc* npc, size_t fn);
    int  invokeMana (Npc& npc, Item&  fn);
    int  invokeSpell(Npc& npc, Item&  fn);

    int  spellCastAnim(Npc& npc, Item&  fn);

    bool aiUseMob  (Npc &pl, const std::string& name);
    bool aiOutput  (Npc &from, Npc& to, const std::string& name);
    bool aiClose   ();

    bool isDead       (const Npc &pl);
    bool isUnconscious(const Npc &pl);
    bool isTalk       (const Npc &pl);

    const std::string& messageByName(const std::string &id) const;
    uint32_t           messageTime(const std::string &id) const;

    int printNothingToGet();

    float              tradeValueMultiplier() const { return tradeValMult; }

    void useInteractive(Daedalus::GEngineClasses::C_Npc *hnpc, const std::string &func);

    Attitude guildAttitude(const Npc& p0,const Npc& p1) const;

  private:
    void               initCommon();

    struct ScopeVar;

    static const std::string& popString  (Daedalus::DaedalusVM &vm);
    Npc*                      popInstance(Daedalus::DaedalusVM &vm);
    Item*                     popItem    (Daedalus::DaedalusVM &vm);

    template<class Ret,class ... Args>
    std::function<Ret(Args...)> notImplementedFn();

    template<void(WorldScript::*)(Daedalus::DaedalusVM &vm)>
    void notImplementedFn(const char* name);

    void notImplementedRoutine(Daedalus::DaedalusVM&);

    Item* getItem(Daedalus::GEngineClasses::C_Item *handle);
    Item* getItemById(size_t id);
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
    void hlp_isitem          (Daedalus::DaedalusVM &vm);
    void hlp_isvaliditem     (Daedalus::DaedalusVM &vm);
    void hlp_getinstanceid   (Daedalus::DaedalusVM &vm);
    void hlp_getnpc          (Daedalus::DaedalusVM &vm);

    void wld_insertitem      (Daedalus::DaedalusVM& vm);
    void wld_insertnpc       (Daedalus::DaedalusVM& vm);
    void wld_settime         (Daedalus::DaedalusVM& vm);
    void wld_getday          (Daedalus::DaedalusVM& vm);
    void wld_playeffect      (Daedalus::DaedalusVM& vm);
    void wld_stopeffect      (Daedalus::DaedalusVM& vm);
    void wld_getplayerportalguild(Daedalus::DaedalusVM& vm);
    void wld_setguildattitude (Daedalus::DaedalusVM& vm);
    void wld_getguildattitude (Daedalus::DaedalusVM& vm);
    void wld_istime           (Daedalus::DaedalusVM& vm);
    void wld_isfpavailable    (Daedalus::DaedalusVM& vm);
    void wld_isnextfpavailable(Daedalus::DaedalusVM& vm);
    void wld_ismobavailable   (Daedalus::DaedalusVM& vm);
    void wld_setmobroutine    (Daedalus::DaedalusVM& vm);
    void wld_assignroomtoguild(Daedalus::DaedalusVM& vm);
    void wld_detectnpc        (Daedalus::DaedalusVM& vm);

    void mdl_setvisual       (Daedalus::DaedalusVM& vm);
    void mdl_setvisualbody   (Daedalus::DaedalusVM& vm);
    void mdl_setmodelfatness (Daedalus::DaedalusVM& vm);
    void mdl_applyoverlaymds (Daedalus::DaedalusVM& vm);
    void mdl_applyoverlaymdstimed(Daedalus::DaedalusVM& vm);
    void mdl_removeoverlaymds(Daedalus::DaedalusVM& vm);
    void mdl_setmodelscale   (Daedalus::DaedalusVM& vm);
    void mdl_startfaceani    (Daedalus::DaedalusVM& vm);
    void mdl_applyrandomani  (Daedalus::DaedalusVM& vm);
    void mdl_applyrandomanifreq(Daedalus::DaedalusVM& vm);

    void npc_settofightmode  (Daedalus::DaedalusVM &vm);
    void npc_settofistmode   (Daedalus::DaedalusVM &vm);
    void npc_isinstate       (Daedalus::DaedalusVM &vm);
    void npc_wasinstate      (Daedalus::DaedalusVM &vm);
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
    void npc_getequippedmeleweapon  (Daedalus::DaedalusVM &vm);
    void npc_getequippedrangedweapon(Daedalus::DaedalusVM &vm);
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
    void npc_getreadiedweapon(Daedalus::DaedalusVM &vm);
    void npc_isdrawingspell  (Daedalus::DaedalusVM &vm);
    void npc_perceiveall     (Daedalus::DaedalusVM& vm);
    void npc_stopani         (Daedalus::DaedalusVM& vm);
    void npc_settrueguild    (Daedalus::DaedalusVM& vm);
    void npc_gettrueguild    (Daedalus::DaedalusVM& vm);
    void npc_clearinventory  (Daedalus::DaedalusVM& vm);

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
    void ai_outputsvm_overlay(Daedalus::DaedalusVM &vm);
    void ai_startstate       (Daedalus::DaedalusVM &vm);
    void ai_playani          (Daedalus::DaedalusVM &vm);
    void ai_setwalkmode      (Daedalus::DaedalusVM &vm);
    void ai_wait             (Daedalus::DaedalusVM &vm);
    void ai_waitms           (Daedalus::DaedalusVM &vm);
    void ai_aligntowp        (Daedalus::DaedalusVM &vm);
    void ai_gotowp           (Daedalus::DaedalusVM &vm);
    void ai_gotofp           (Daedalus::DaedalusVM &vm);
    void ai_playanibs        (Daedalus::DaedalusVM &vm);
    void ai_equiparmor       (Daedalus::DaedalusVM &vm);
    void ai_equipbestmeleeweapon (Daedalus::DaedalusVM &vm);
    void ai_equipbestrangedweapon(Daedalus::DaedalusVM &vm);
    void ai_usemob           (Daedalus::DaedalusVM &vm);
    void ai_teleport         (Daedalus::DaedalusVM &vm);
    void ai_stoppointat      (Daedalus::DaedalusVM &vm);
    void ai_readymeleeweapon (Daedalus::DaedalusVM &vm);
    void ai_readyrangedweapon(Daedalus::DaedalusVM &vm);
    void ai_readyspell       (Daedalus::DaedalusVM &vm);
    void ai_atack            (Daedalus::DaedalusVM &vm);
    void ai_flee             (Daedalus::DaedalusVM &vm);
    void ai_dodge            (Daedalus::DaedalusVM &vm);
    void ai_unequipweapons   (Daedalus::DaedalusVM &vm);
    void ai_gotonpc          (Daedalus::DaedalusVM &vm);
    void ai_gotonextfp       (Daedalus::DaedalusVM &vm);
    void ai_aligntofp        (Daedalus::DaedalusVM &vm);
    void ai_useitemtostate   (Daedalus::DaedalusVM &vm);

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

    void snd_play            (Daedalus::DaedalusVM &vm);

    void introducechapter    (Daedalus::DaedalusVM &vm);
    void playvideo           (Daedalus::DaedalusVM &vm);
    void printscreen         (Daedalus::DaedalusVM &vm);
    void printdialog         (Daedalus::DaedalusVM &vm);
    void print               (Daedalus::DaedalusVM &vm);
    void perc_setrange       (Daedalus::DaedalusVM &vm);

    void sort(std::vector<DlgChoise>& dlg);
    void setNpcInfoKnown(const Daedalus::GEngineClasses::C_Npc& npc, const Daedalus::GEngineClasses::C_Info& info);
    bool doesNpcKnowInfo(const Daedalus::GEngineClasses::C_Npc& npc, size_t infoInstance) const;

    Daedalus::DaedalusVM                                        vm;
    uint8_t                                                     invokeRecursive=0;
    GameSession&                                                owner;
    std::mt19937                                                randGen;

    std::set<std::pair<size_t,size_t>>                          dlgKnownInfos;
    std::vector<Daedalus::GEngineClasses::C_Info*>              dialogsInfo;
    std::unique_ptr<ZenLoad::zCCSLib>                           dialogs;
    std::unordered_map<size_t,AiState>                          aiStates;

    QuestLog                                                    quests;
    size_t                                                      itMi_Gold=0;
    float                                                       tradeValMult=0.3f;
    size_t                                                      spellFxInstanceNames=0;
    size_t                                                      spellFxAniLetters=0;
    std::string                                                 goldTxt;
    float                                                       viewTimePerChar=0.5;
    size_t                                                      gilCount=0;
    std::vector<int32_t>                                        gilAttitudes;

    size_t                                                      ZS_Dead=0;
    size_t                                                      ZS_Unconscious=0;
    size_t                                                      ZS_Talk=0;

    Daedalus::GEngineClasses::C_Focus                           cFocusNorm,cFocusMele,cFocusRange,cFocusMage;
    Daedalus::GEngineClasses::C_GilValues                       cGuildVal;
  };
