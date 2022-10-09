#pragma once

#include <phoenix/vm.hh>
#include <phoenix/ext/daedalus_classes.hh>
#include <phoenix/messages.hh>

#include <memory>
#include <set>
#include <random>

#include <Tempest/Matrix4x4>
#include <Tempest/Painter>

#include "game/definitions/svmdefinitions.h"
#include "game/definitions/spelldefinitions.h"
#include "game/definitions/fightaidefinitions.h"
#include "game/aiouputpipe.h"
#include "game/constants.h"
#include "game/aistate.h"
#include "game/questlog.h"
#include "graphics/pfx/pfxobjects.h"
#include "ui/documentmenu.h"

class GameSession;
class World;
class ScriptPlugin;
class Npc;
class Item;
class VisualFx;
class ParticleFx;
class Serialize;

class ScriptFn final {
  public:
    ScriptFn()=default;
    ScriptFn(size_t v):ptr(v){}

    size_t ptr=size_t(-1);

    bool isValid() const { return ptr!=size_t(-1); }

  friend bool operator == (const ScriptFn& a,const ScriptFn& b) {
    return a.ptr==b.ptr;
    }
  };


class GameScript final {
  public:
    GameScript(GameSession &owner);
    ~GameScript();

    struct DlgChoise final {
      std::string                       title;
      int32_t                           sort=0;
      uint32_t                          scriptFn=0;
      bool                              isTrade=false;
      phoenix::c_info*        handle=nullptr;
      };

    bool         hasSymbolName(std::string_view fn);

    void         initDialogs ();
    void         loadDialogOU();

    void initializeInstanceNpc(const std::shared_ptr<phoenix::c_npc>& npc, size_t instance);
    void initializeInstanceItem(const std::shared_ptr<phoenix::c_item>& item, size_t instance);

    void         saveQuests(Serialize& fout);
    void         loadQuests(Serialize& fin);
    void         saveVar(Serialize& fout);
    void         loadVar(Serialize& fin);

    void         resetVarPointers();

    inline phoenix::vm& getVm() { return vm; }

    auto         questLog() const -> const QuestLog&;

    const World& world() const;
    World&       world();
    uint64_t     tickCount() const;

    uint32_t     rand(uint32_t max);
    void         removeItem(Item& it);

    void         setInstanceNPC (std::string_view name, Npc& npc);
    void         setInstanceItem(Npc& holder, size_t itemId);

    AiOuputPipe* openAiOuput();
    AiOuputPipe* openDlgOuput(Npc &player, Npc &npc);

    phoenix::symbol* goldId() const { return itMi_Gold; }
    ScriptFn         playerPercAssessMagic();
    std::string_view currencyName() const { return goldTxt; }
    int              npcDamDiveTime();

    const phoenix::c_focus&          focusNorm()  const { return cFocusNorm;  }
    const phoenix::c_focus&          focusMele()  const { return cFocusMele;  }
    const phoenix::c_focus&          focusRange() const { return cFocusRange; }
    const phoenix::c_focus&          focusMage()  const { return cFocusMage;  }
    const phoenix::c_gil_values&     guildVal()   const { return *cGuildVal;   }

    phoenix::symbol*                        getSymbol(std::string_view s);
    phoenix::symbol*                        getSymbol(const size_t s);
    size_t                                            getSymbolIndex(std::string_view s);
    size_t                                            getSymbolCount() const;

    const AiState&                                    aiState  (ScriptFn id);
    const phoenix::c_spell&                 spellDesc(int32_t splId);
    const VisualFx*                                   spellVfx (int32_t splId);

    auto dialogChoises(std::shared_ptr<phoenix::c_npc> self, std::shared_ptr<phoenix::c_npc> npc, const std::vector<uint32_t> &except, bool includeImp) -> std::vector<DlgChoise>;
    auto updateDialog (const GameScript::DlgChoise &dlg, Npc &player, Npc &npc) -> std::vector<GameScript::DlgChoise>;
    void exec(const DlgChoise &dlg, Npc &player, Npc &npc);

    void  printCannotUseError         (Npc &npc, int32_t atr, int32_t nValue);
    void  printCannotCastError        (Npc &npc, int32_t plM, int32_t itM);
    void  printCannotBuyError         (Npc &npc);
    void  printMobMissingItem         (Npc &npc);
    void  printMobAnotherIsUsing      (Npc &npc);
    void  printMobMissingKey          (Npc &npc);
    void  printMobMissingKeyOrLockpick(Npc &npc);
    void  printMobMissingLockpick     (Npc &npc);
    void  printMobTooFar              (Npc &npc);

    void invokeState(const std::shared_ptr<phoenix::c_npc>& hnpc, const std::shared_ptr<phoenix::c_npc>& hother, const char* name);
    int  invokeState(Npc* npc, Npc* other, Npc *victum, ScriptFn fn);
    void invokeItem (Npc* npc, ScriptFn fn);
    int  invokeMana (Npc& npc, Npc* target, Item&  fn);
    void invokeSpell(Npc& npc, Npc *target, Item&  fn);
    int  invokeCond (Npc& npc, const std::string& func);
    void invokePickLock(Npc& npc, int bSuccess, int bBrokenOpen);
    auto canNpcCollideWithSpell(Npc& npc, Npc* shooter, int32_t spellId) -> CollideMask;

    int  playerHotKeyScreenMap(Npc& pl);

    auto spellCastAnim(Npc& npc, Item&  fn) -> const std::string&;

    bool isDead       (const Npc &pl);
    bool isUnconscious(const Npc &pl);
    bool isTalk       (const Npc &pl);
    bool isAtack      (const Npc &pl) const;

    std::string_view messageFromSvm(std::string_view id,int voice) const;
    std::string_view messageByName (const std::string& id) const;
    uint32_t         messageTime   (std::string_view id) const;

    void      printNothingToGet();
    float    tradeValueMultiplier() const { return tradeValMult; }
    void     useInteractive(const std::shared_ptr<phoenix::c_npc>& hnpc, const std::string &func);
    Attitude guildAttitude(const Npc& p0,const Npc& p1) const;
    Attitude personAttitude(const Npc& p0,const Npc& p1) const;

    BodyState schemeToBodystate(std::string_view sc);

    void      onWldItemRemoved(const Item& itm);
    void      fixNpcPosition(Npc& npc, float angle0, float distBias);

  private:
    template<typename T>
    struct DetermineSignature {
        using signature = void();
    };

    template <typename C, typename R, typename ... P>
    struct DetermineSignature<R(C::*)(P...)> {
        using signature = R(P...);
    };

    template <class F>
    void bindExternal(const std::string& name, F function) {
      vm.register_external(name, std::function<typename DetermineSignature<F>::signature> (
        [this, function](auto ... v) { return (this->*function)(v...); }));
    }

    void               initCommon();

    struct GlobalOutput : AiOuputPipe {
      explicit GlobalOutput(GameScript& owner):owner(owner){}

      bool output   (Npc &npc, std::string_view text) override;
      bool outputSvm(Npc& npc, std::string_view text) override;
      bool outputOv (Npc& npc, std::string_view text) override;
      bool printScr (Npc& npc, int time, std::string_view msg, int x,int y, std::string_view font) override;
      bool isFinished() override;

      bool close() override { return true; }

      GameScript& owner;
      };

    Item* getItem(phoenix::c_item* handle);
    Item* getItemById(size_t id);
    Npc*  getNpc(phoenix::c_npc*   handle);
    Npc*  getNpc(const std::shared_ptr<phoenix::c_npc>& handle);
    Npc*  getNpcById (size_t id);
    auto  getInfo    (size_t id) -> phoenix::c_info*;
    auto  getFocus(const std::string& name) -> phoenix::c_focus;

    void  storeItem(Item* it);

    bool  aiOutput   (Npc &from, std::string_view name, bool overlay);
    bool  aiOutputSvm(Npc &from, std::string_view name, bool overlay);

    bool  searchScheme(std::string_view sc, std::string_view listName);

    bool game_initgerman     ();
    bool game_initenglish    ();

    int  hlp_random          (int bound);
    bool hlp_isvalidnpc      (std::shared_ptr<phoenix::c_npc> npcRef);
    bool hlp_isitem          (std::shared_ptr<phoenix::c_item> itemRef, int instanceSymbol);
    bool hlp_isvaliditem     (std::shared_ptr<phoenix::c_item> itemRef);
    int  hlp_getinstanceid   (std::shared_ptr<phoenix::instance> instance);
    std::shared_ptr<phoenix::c_npc> hlp_getnpc          (int instanceSymbol);

    void wld_insertitem      (int itemInstance, std::string_view spawnpoint);
    void wld_insertnpc       (int npcInstance, std::string_view spawnpoint);
    void wld_settime         (int hour, int minute);
    int  wld_getday          ();
    void wld_playeffect      (std::string_view visual, std::shared_ptr<phoenix::instance> sourceId, std::shared_ptr<phoenix::instance> targetId, int effectLevel, int damage, int damageType, int isProjectile);
    void wld_stopeffect      (std::string_view visual);
    int  wld_getplayerportalguild();
    int  wld_getformerplayerportalguild();
    void wld_setguildattitude (int gil1, int att, int gil2);
    int  wld_getguildattitude (int g1, int g0);
    bool wld_istime           (int hour0, int min0, int hour1, int min1);
    bool wld_isfpavailable    (std::shared_ptr<phoenix::c_npc> self, std::string_view name);
    bool wld_isnextfpavailable(std::shared_ptr<phoenix::c_npc> self, std::string_view name);
    bool wld_ismobavailable   (std::shared_ptr<phoenix::c_npc> self, std::string_view name);
    void wld_setmobroutine    (int h, int m, std::string_view name, int st);
    int  wld_getmobstate      (std::shared_ptr<phoenix::c_npc> npc, std::string_view scheme);
    void wld_assignroomtoguild(std::string_view name, int g);
    bool wld_detectnpc        (std::shared_ptr<phoenix::c_npc> npcRef, int inst, int state, int guild);
    bool wld_detectnpcex      (std::shared_ptr<phoenix::c_npc> npcRef, int inst, int state, int guild, int player);
    bool wld_detectitem       (std::shared_ptr<phoenix::c_npc> npcRef, int flags);
    void wld_spawnnpcrange    (std::shared_ptr<phoenix::c_npc> npcRef, int clsId, int count, float lifeTime);
    void wld_sendtrigger      (std::string_view triggerTarget);
    void wld_senduntrigger    (std::string_view triggerTarget);
    bool wld_israining        ();

    void mdl_setvisual       (std::shared_ptr<phoenix::c_npc> npcRef, std::string_view visual);
    void mdl_setvisualbody   (std::shared_ptr<phoenix::c_npc> npcRef, std::string_view body, int bodyTexNr, int bodyTexColor, std::string_view head, int headTexNr, int teethTexNr, int armor);
    void mdl_setmodelfatness (std::shared_ptr<phoenix::c_npc> npcRef, float fat);
    void mdl_applyoverlaymds (std::shared_ptr<phoenix::c_npc> npcRef, std::string_view overlayname);
    void mdl_applyoverlaymdstimed(std::shared_ptr<phoenix::c_npc> npcRef, std::string_view overlayname, int ticks);
    void mdl_removeoverlaymds(std::shared_ptr<phoenix::c_npc> npcRef, std::string_view overlayname);
    void mdl_setmodelscale   (std::shared_ptr<phoenix::c_npc> npcRef, float x, float y, float z);
    void mdl_startfaceani    (std::shared_ptr<phoenix::c_npc> npcRef, std::string_view ani, float intensity, float time);
    void mdl_applyrandomani  (std::shared_ptr<phoenix::c_npc> npcRef, std::string_view s1, std::string_view s0);
    void mdl_applyrandomanifreq(std::shared_ptr<phoenix::c_npc> npcRef, std::string_view s1, float f0);
    void mdl_applyrandomfaceani(std::shared_ptr<phoenix::c_npc> npcRef, std::string_view name, float timeMin, float timeMinVar, float timeMax, float timeMaxVar, float probMin);

    void npc_settofightmode  (std::shared_ptr<phoenix::c_npc> npcRef, int weaponSymbol);
    void npc_settofistmode   (std::shared_ptr<phoenix::c_npc> npcRef);
    bool npc_isinstate       (std::shared_ptr<phoenix::c_npc> npcRef, int stateFn);
    bool npc_isinroutine     (std::shared_ptr<phoenix::c_npc> npcRef, int stateFn);
    bool npc_wasinstate      (std::shared_ptr<phoenix::c_npc> npcRef, int stateFn);
    int  npc_getdisttowp     (std::shared_ptr<phoenix::c_npc> npcRef, std::string_view wpname);
    void npc_exchangeroutine (std::shared_ptr<phoenix::c_npc> npcRef, std::string_view rname);
    bool npc_isdead          (std::shared_ptr<phoenix::c_npc> npcRef);
    bool npc_knowsinfo       (std::shared_ptr<phoenix::c_npc> npcRef, int infoinstance);
    void npc_settalentskill  (std::shared_ptr<phoenix::c_npc> npcRef, int t, int lvl);
    int  npc_gettalentskill  (std::shared_ptr<phoenix::c_npc> npcRef, int skillId);
    void npc_settalentvalue  (std::shared_ptr<phoenix::c_npc> npcRef, int t, int lvl);
    int  npc_gettalentvalue  (std::shared_ptr<phoenix::c_npc> npcRef, int skillId);
    void npc_setrefusetalk   (std::shared_ptr<phoenix::c_npc> npcRef, int timeSec);
    bool npc_refusetalk      (std::shared_ptr<phoenix::c_npc> npcRef);
    int  npc_hasitems        (std::shared_ptr<phoenix::c_npc> npcRef, int itemId);
    int  npc_getinvitem      (std::shared_ptr<phoenix::c_npc> npcRef, int itemId);
    int  npc_removeinvitem   (std::shared_ptr<phoenix::c_npc> npcRef, int itemId);
    int  npc_removeinvitems  (std::shared_ptr<phoenix::c_npc> npcRef, int itemId, int amount);
    int  npc_getbodystate    (std::shared_ptr<phoenix::c_npc> npcRef);
    std::shared_ptr<phoenix::c_npc> npc_getlookattarget (std::shared_ptr<phoenix::c_npc> npcRef);
    int  npc_getdisttonpc    (std::shared_ptr<phoenix::c_npc> aRef, std::shared_ptr<phoenix::c_npc> bRef);
    bool npc_hasequippedarmor(std::shared_ptr<phoenix::c_npc> npcRef);
    void npc_setperctime     (std::shared_ptr<phoenix::c_npc> npcRef, float sec);
    void npc_percenable      (std::shared_ptr<phoenix::c_npc> npcRef, int pr, int fn);
    void npc_percdisable     (std::shared_ptr<phoenix::c_npc> npcRef, int pr);
    std::string npc_getnearestwp    (std::shared_ptr<phoenix::c_npc> npcRef);
    void npc_clearaiqueue    (std::shared_ptr<phoenix::c_npc> npcRef);
    bool npc_isplayer        (std::shared_ptr<phoenix::c_npc> npcRef);
    int  npc_getstatetime    (std::shared_ptr<phoenix::c_npc> npcRef);
    void npc_setstatetime    (std::shared_ptr<phoenix::c_npc> npcRef, int val);
    void npc_changeattribute (std::shared_ptr<phoenix::c_npc> npcRef, int atr, int val);
    bool npc_isonfp          (std::shared_ptr<phoenix::c_npc> npcRef, std::string_view val);
    int  npc_getheighttonpc  (std::shared_ptr<phoenix::c_npc> aRef, std::shared_ptr<phoenix::c_npc> bRef);
    std::shared_ptr<phoenix::c_item> npc_getequippedmeleeweapon (std::shared_ptr<phoenix::c_npc> npcRef);
    std::shared_ptr<phoenix::c_item> npc_getequippedrangedweapon(std::shared_ptr<phoenix::c_npc> npcRef);
    std::shared_ptr<phoenix::c_item> npc_getequippedarmor(std::shared_ptr<phoenix::c_npc> npcRef);
    bool npc_canseenpc       (std::shared_ptr<phoenix::c_npc> npcRef, std::shared_ptr<phoenix::c_npc> otherRef);
    bool npc_hasequippedweapon(std::shared_ptr<phoenix::c_npc> npcRef);
    bool npc_hasequippedmeleeweapon(std::shared_ptr<phoenix::c_npc> npcRef);
    bool npc_hasequippedrangedweapon(std::shared_ptr<phoenix::c_npc> npcRef);
    int  npc_getactivespell  (std::shared_ptr<phoenix::c_npc> npcRef);
    bool npc_getactivespellisscroll(std::shared_ptr<phoenix::c_npc> npcRef);
    bool npc_canseenpcfreelos(std::shared_ptr<phoenix::c_npc> npcRef, std::shared_ptr<phoenix::c_npc> otherRef);
    bool npc_isinfightmode   (std::shared_ptr<phoenix::c_npc> npcRef, int modeI);
    int  npc_getactivespellcat(std::shared_ptr<phoenix::c_npc> npcRef);
    int  npc_setactivespellinfo(std::shared_ptr<phoenix::c_npc> npcRef, int v);
    int  npc_getactivespelllevel(std::shared_ptr<phoenix::c_npc> npcRef);
    void npc_settarget       (std::shared_ptr<phoenix::c_npc> npcRef, std::shared_ptr<phoenix::c_npc> otherRef);
    bool npc_gettarget       (std::shared_ptr<phoenix::c_npc> npcRef);
    bool npc_getnexttarget   (std::shared_ptr<phoenix::c_npc> npcRef);
    void npc_sendpassiveperc (std::shared_ptr<phoenix::c_npc> npcRef, int id, std::shared_ptr<phoenix::c_npc> victimRef, std::shared_ptr<phoenix::c_npc> otherRef);
    bool npc_checkinfo       (std::shared_ptr<phoenix::c_npc> npcRef, int imp);
    int  npc_getportalguild  (std::shared_ptr<phoenix::c_npc> npcRef);
    bool npc_isinplayersroom (std::shared_ptr<phoenix::c_npc> npcRef);
    std::shared_ptr<phoenix::c_item> npc_getreadiedweapon(std::shared_ptr<phoenix::c_npc> npcRef);
    bool npc_hasreadiedmeleeweapon(std::shared_ptr<phoenix::c_npc> npcRef);
    int  npc_isdrawingspell  (std::shared_ptr<phoenix::c_npc> npcRef);
    int  npc_isdrawingweapon (std::shared_ptr<phoenix::c_npc> npcRef);
    void npc_perceiveall     (std::shared_ptr<phoenix::c_npc> npcRef);
    void npc_stopani         (std::shared_ptr<phoenix::c_npc> npcRef, std::string_view name);
    int  npc_settrueguild    (std::shared_ptr<phoenix::c_npc> npcRef, int gil);
    int  npc_gettrueguild    (std::shared_ptr<phoenix::c_npc> npcRef);
    void npc_clearinventory  (std::shared_ptr<phoenix::c_npc> npcRef);
    int  npc_getattitude     (std::shared_ptr<phoenix::c_npc> aRef, std::shared_ptr<phoenix::c_npc> bRef);
    int  npc_getpermattitude (std::shared_ptr<phoenix::c_npc> aRef, std::shared_ptr<phoenix::c_npc> bRef);
    void npc_setattitude     (std::shared_ptr<phoenix::c_npc> npcRef, int att);
    void npc_settempattitude (std::shared_ptr<phoenix::c_npc> npcRef, int att);
    bool npc_hasbodyflag     (std::shared_ptr<phoenix::c_npc> npcRef, int bodyflag);
    int  npc_getlasthitspellid(std::shared_ptr<phoenix::c_npc> npcRef);
    int  npc_getlasthitspellcat(std::shared_ptr<phoenix::c_npc> npcRef);
    void npc_playani         (std::shared_ptr<phoenix::c_npc> npcRef, std::string_view name);
    bool npc_isdetectedmobownedbynpc(std::shared_ptr<phoenix::c_npc> usrRef, std::shared_ptr<phoenix::c_npc> npcRef);
    std::string npc_getdetectedmob  (std::shared_ptr<phoenix::c_npc> npcRef);
    bool npc_ownedbynpc      (std::shared_ptr<phoenix::c_item> itmRef, std::shared_ptr<phoenix::c_npc> npcRef);
    bool npc_canseesource    (std::shared_ptr<phoenix::c_npc> npcRef);
    int  npc_getdisttoitem   (std::shared_ptr<phoenix::c_npc> npcRef, std::shared_ptr<phoenix::c_item> itmRef);
    int  npc_getheighttoitem (std::shared_ptr<phoenix::c_npc> npcRef, std::shared_ptr<phoenix::c_item> itmRef);
    int  npc_getdisttoplayer (std::shared_ptr<phoenix::c_npc> npcRef);
    bool npc_isdetectedmobownedbyguild(std::shared_ptr<phoenix::c_npc> npcRef, int guild);

    void ai_output           (std::shared_ptr<phoenix::c_npc> selfRef, std::shared_ptr<phoenix::c_npc> targetRef, std::string_view outputname);
    void ai_stopprocessinfos (std::shared_ptr<phoenix::c_npc> selfRef);
    void ai_processinfos     (std::shared_ptr<phoenix::c_npc> npcRef);
    void ai_standup          (std::shared_ptr<phoenix::c_npc> selfRef);
    void ai_standupquick     (std::shared_ptr<phoenix::c_npc> selfRef);
    void ai_continueroutine  (std::shared_ptr<phoenix::c_npc> selfRef);
    void ai_stoplookat       (std::shared_ptr<phoenix::c_npc> selfRef);
    void ai_lookatnpc        (std::shared_ptr<phoenix::c_npc> selfRef, std::shared_ptr<phoenix::c_npc> npcRef);
    void ai_removeweapon     (std::shared_ptr<phoenix::c_npc> npcRef);
    void ai_turntonpc        (std::shared_ptr<phoenix::c_npc> selfRef, std::shared_ptr<phoenix::c_npc> npcRef);
    void ai_outputsvm        (std::shared_ptr<phoenix::c_npc> selfRef, std::shared_ptr<phoenix::c_npc> targetRef, std::string_view name);
    void ai_outputsvm_overlay(std::shared_ptr<phoenix::c_npc> selfRef, std::shared_ptr<phoenix::c_npc> targetRef, std::string_view name);
    void ai_startstate       (std::shared_ptr<phoenix::c_npc> selfRef, int func, int state, std::string_view wp);
    void ai_playani          (std::shared_ptr<phoenix::c_npc> npcRef, std::string_view name);
    void ai_setwalkmode      (std::shared_ptr<phoenix::c_npc> npcRef, int modeBits);
    void ai_wait             (std::shared_ptr<phoenix::c_npc> npcRef, float ms);
    void ai_waitms           (std::shared_ptr<phoenix::c_npc> npcRef, int ms);
    void ai_aligntowp        (std::shared_ptr<phoenix::c_npc> npcRef);
    void ai_gotowp           (std::shared_ptr<phoenix::c_npc> npcRef, std::string_view waypoint);
    void ai_gotofp           (std::shared_ptr<phoenix::c_npc> npcRef, std::string_view waypoint);
    void ai_playanibs        (std::shared_ptr<phoenix::c_npc> npcRef, std::string_view ani, int bs);
    void ai_equiparmor       (std::shared_ptr<phoenix::c_npc> npcRef, int id);
    void ai_equipbestarmor   (std::shared_ptr<phoenix::c_npc> npcRef);
    int  ai_equipbestmeleeweapon (std::shared_ptr<phoenix::c_npc> npcRef);
    int  ai_equipbestrangedweapon(std::shared_ptr<phoenix::c_npc> npcRef);
    bool ai_usemob           (std::shared_ptr<phoenix::c_npc> npcRef, std::string_view tg, int state);
    void ai_teleport         (std::shared_ptr<phoenix::c_npc> npcRef, std::string_view tg);
    void ai_stoppointat      (std::shared_ptr<phoenix::c_npc> npcRef);
    void ai_drawweapon       (std::shared_ptr<phoenix::c_npc> npcRef);
    void ai_readymeleeweapon (std::shared_ptr<phoenix::c_npc> npcRef);
    void ai_readyrangedweapon(std::shared_ptr<phoenix::c_npc> npcRef);
    void ai_readyspell       (std::shared_ptr<phoenix::c_npc> npcRef, int spell, int mana);
    void ai_atack            (std::shared_ptr<phoenix::c_npc> npcRef);
    void ai_flee             (std::shared_ptr<phoenix::c_npc> npcRef);
    void ai_dodge            (std::shared_ptr<phoenix::c_npc> npcRef);
    void ai_unequipweapons   (std::shared_ptr<phoenix::c_npc> npcRef);
    void ai_unequiparmor     (std::shared_ptr<phoenix::c_npc> npcRef);
    void ai_gotonpc          (std::shared_ptr<phoenix::c_npc> npcRef, std::shared_ptr<phoenix::c_npc> toRef);
    void ai_gotonextfp       (std::shared_ptr<phoenix::c_npc> npcRef, std::string_view to);
    void ai_aligntofp        (std::shared_ptr<phoenix::c_npc> npcRef);
    void ai_useitem          (std::shared_ptr<phoenix::c_npc> npcRef, int item);
    void ai_useitemtostate   (std::shared_ptr<phoenix::c_npc> npcRef, int item, int state);
    void ai_setnpcstostate   (std::shared_ptr<phoenix::c_npc> npcRef, int state, int radius);
    void ai_finishingmove    (std::shared_ptr<phoenix::c_npc> npcRef, std::shared_ptr<phoenix::c_npc> othRef);
    void ai_takeitem         (std::shared_ptr<phoenix::c_npc> npcRef, std::shared_ptr<phoenix::c_item> itmRef);
    void ai_gotoitem         (std::shared_ptr<phoenix::c_npc> npcRef, std::shared_ptr<phoenix::c_item> itmRef);
    void ai_pointat          (std::shared_ptr<phoenix::c_npc> npcRef, std::string_view waypoint);
    void ai_pointatnpc       (std::shared_ptr<phoenix::c_npc> npcRef, std::shared_ptr<phoenix::c_npc> otherRef);
    int ai_printscreen       (std::string_view msg, int posx, int posy, std::string_view font, int timesec);

    int mob_hasitems        (std::string_view tag, int item);

    void ta_min              (std::shared_ptr<phoenix::c_npc> npcRef, int start_h, int start_m, int stop_h, int stop_m, int action, std::string_view waypoint);

    void log_createtopic     (std::string_view topicName, int section);
    void log_settopicstatus  (std::string_view topicName, int status);
    void log_addentry        (std::string_view topicName, std::string_view entry);

    void equipitem           (std::shared_ptr<phoenix::c_npc> npcRef, int cls);
    void createinvitem       (std::shared_ptr<phoenix::c_npc> npcRef, int itemInstance);
    void createinvitems      (std::shared_ptr<phoenix::c_npc> npcRef, int itemInstance, int amount);

    void info_addchoice      (int infoInstance, std::string_view text, int func);
    void info_clearchoices   (int infoInstance);
    bool infomanager_hasfinished();

    void snd_play            (std::string_view fileS);
    void snd_play3d          (std::shared_ptr<phoenix::c_npc> npcRef, std::string_view fileS);

    void exitsession         ();

    void sort(std::vector<DlgChoise>& dlg);
    void setNpcInfoKnown(const phoenix::c_npc& npc, const phoenix::c_info& info);
    bool doesNpcKnowInfo(const phoenix::c_npc& npc, size_t infoInstance) const;

    void saveSym(Serialize& fout, phoenix::symbol& s);

    void onWldInstanceRemoved(const phoenix::instance* obj);
    void makeCurrent(Item* w);

    GameSession&                                                owner;
    phoenix::vm                                       vm;
    std::mt19937                                                randGen;

    std::vector<std::unique_ptr<ScriptPlugin>>                  plugins;

    std::unique_ptr<SpellDefinitions>                           spells;
    std::unique_ptr<SvmDefinitions>                             svm;
    uint64_t                                                    svmBarrier=0;

    std::set<std::pair<size_t,size_t>>                          dlgKnownInfos;
    std::vector<std::shared_ptr<phoenix::c_info>>     dialogsInfo;
    phoenix::messages                                           dialogs;
    std::unordered_map<size_t,AiState>                          aiStates;
    std::unique_ptr<AiOuputPipe>                                aiDefaultPipe;

    QuestLog                                                    quests;
    phoenix::symbol*                                  itMi_Gold=nullptr;
    float                                                       tradeValMult=0.3f;
    phoenix::symbol*                                  spellFxInstanceNames=nullptr;
    phoenix::symbol*                                  spellFxAniLetters=nullptr;
    std::string                                                 goldTxt;
    float                                                       viewTimePerChar=0.5;
    mutable std::unordered_map<std::string,uint32_t>            msgTimings;
    size_t                                                      gilCount=0;
    std::vector<int32_t>                                        gilAttitudes;
    int                                                         aiOutOrderId=0;

    size_t                                                      ZS_Dead=0;
    size_t                                                      ZS_Unconscious=0;
    size_t                                                      ZS_Talk=0;
    size_t                                                      ZS_Attack=0;
    size_t                                                      ZS_MM_Attack=0;

    phoenix::c_focus                                            cFocusNorm,cFocusMele,cFocusRange,cFocusMage;
    std::shared_ptr<phoenix::c_gil_values>            cGuildVal;
  };
