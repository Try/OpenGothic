#include "gamescript.h"

#include <Tempest/Log>
#include <Tempest/SoundEffect>

#include <fstream>
#include <cctype>
#include <concepts>

#include "game/definitions/spelldefinitions.h"
#include "game/serialize.h"
#include "game/globaleffects.h"
#include "game/compatibility/ikarus.h"
#include "game/compatibility/lego.h"
#include "world/objects/npc.h"
#include "world/objects/item.h"
#include "world/objects/interactive.h"
#include "graphics/visualfx.h"
#include "utils/fileutil.h"
#include "gothic.h"

using namespace Tempest;
using namespace Daedalus::GameState;

template <typename T>
requires (std::derived_from<T, phoenix::daedalus::instance>)
struct ScopeVar final {
  ScopeVar(phoenix::daedalus::symbol* sym, std::shared_ptr<T> h) : prev(sym->get_instance()), sym(sym) {
    sym->set_instance(h);
    }

  ScopeVar(const ScopeVar&)=delete;
  ~ScopeVar(){
    sym->set_instance(prev);
    }

  std::shared_ptr<phoenix::daedalus::instance> prev;
  phoenix::daedalus::symbol*  sym;
  };


bool GameScript::GlobalOutput::output(Npc& npc, std::string_view text) {
  return owner.aiOutput(npc,text,false);
  }

bool GameScript::GlobalOutput::outputSvm(Npc &npc, std::string_view text) {
  return owner.aiOutputSvm(npc,text,false);
  }

bool GameScript::GlobalOutput::outputOv(Npc &npc, std::string_view text) {
  return owner.aiOutputSvm(npc,text,true);
  }

bool GameScript::GlobalOutput::isFinished() {
  return true;
  }

GameScript::GameScript(GameSession &owner)
  :owner(owner), vm(Gothic::inst().loadPhoenixScriptCode("GOTHIC.DAT"), (phoenix::daedalus::execution_flag)
        (phoenix::daedalus::vm_allow_empty_stack_pop | phoenix::daedalus::vm_allow_null_instance_access)) {
  phoenix::daedalus::register_all_script_classes(vm);
  Gothic::inst().setupVmCommonApi(vm);
  aiDefaultPipe.reset(new GlobalOutput(*this));
  initCommon();
  }

GameScript::~GameScript() {
  }


// Beware: amazing hacks from this point!
template<typename T>
struct DetermineSignature {
  using signature = void(void);
};

template <typename C, typename R, typename ... P>
struct DetermineSignature<R(C::*)(P...)> {
  using signature = R(P...);
};

#define bind_this(func) std::function<DetermineSignature<decltype(&func)>::signature> {std::bind_front(&func, this)}

void GameScript::initCommon() {
  vm.register_external("hlp_random",          bind_this(GameScript::hlp_random));
  vm.register_external("hlp_isvalidnpc",      bind_this(GameScript::hlp_isvalidnpc));
  vm.register_external("hlp_isvaliditem",     bind_this(GameScript::hlp_isvaliditem));
  vm.register_external("hlp_isitem",          bind_this(GameScript::hlp_isitem));
  vm.register_external("hlp_getnpc",          bind_this(GameScript::hlp_getnpc));
  vm.register_external("hlp_getinstanceid",   bind_this(GameScript::hlp_getinstanceid));

  vm.register_external("wld_insertnpc",                  bind_this(GameScript::wld_insertnpc));
  vm.register_external("wld_insertitem",                 bind_this(GameScript::wld_insertitem));
  vm.register_external("wld_settime",                    bind_this(GameScript::wld_settime));
  vm.register_external("wld_getday",                     bind_this(GameScript::wld_getday));
  vm.register_external("wld_playeffect",                 bind_this(GameScript::wld_playeffect));
  vm.register_external("wld_stopeffect",                 bind_this(GameScript::wld_stopeffect));
  vm.register_external("wld_getplayerportalguild",       bind_this(GameScript::wld_getplayerportalguild));
  vm.register_external("wld_getformerplayerportalguild", bind_this(GameScript::wld_getformerplayerportalguild));
  vm.register_external("wld_setguildattitude",           bind_this(GameScript::wld_setguildattitude));
  vm.register_external("wld_getguildattitude",           bind_this(GameScript::wld_getguildattitude));
  vm.register_external("wld_istime",                     bind_this(GameScript::wld_istime));
  vm.register_external("wld_isfpavailable",              bind_this(GameScript::wld_isfpavailable));
  vm.register_external("wld_isnextfpavailable",          bind_this(GameScript::wld_isnextfpavailable));
  vm.register_external("wld_ismobavailable",             bind_this(GameScript::wld_ismobavailable));
  vm.register_external("wld_setmobroutine",              bind_this(GameScript::wld_setmobroutine));
  vm.register_external("wld_getmobstate",                bind_this(GameScript::wld_getmobstate));
  vm.register_external("wld_assignroomtoguild",          bind_this(GameScript::wld_assignroomtoguild));
  vm.register_external("wld_detectnpc",                  bind_this(GameScript::wld_detectnpc));
  vm.register_external("wld_detectnpcex",                bind_this(GameScript::wld_detectnpcex));
  vm.register_external("wld_detectitem",                 bind_this(GameScript::wld_detectitem));
  vm.register_external("wld_spawnnpcrange",              bind_this(GameScript::wld_spawnnpcrange));
  vm.register_external("wld_sendtrigger",                bind_this(GameScript::wld_sendtrigger));
  vm.register_external("wld_senduntrigger",              bind_this(GameScript::wld_senduntrigger));
  vm.register_external("wld_israining",                  bind_this(GameScript::wld_israining));

  vm.register_external("mdl_setvisual",            bind_this(GameScript::mdl_setvisual));
  vm.register_external("mdl_setvisualbody",        bind_this(GameScript::mdl_setvisualbody));
  vm.register_external("mdl_setmodelfatness",      bind_this(GameScript::mdl_setmodelfatness));
  vm.register_external("mdl_applyoverlaymds",      bind_this(GameScript::mdl_applyoverlaymds));
  vm.register_external("mdl_applyoverlaymdstimed", bind_this(GameScript::mdl_applyoverlaymdstimed));
  vm.register_external("mdl_removeoverlaymds",     bind_this(GameScript::mdl_removeoverlaymds));
  vm.register_external("mdl_setmodelscale",        bind_this(GameScript::mdl_setmodelscale));
  vm.register_external("mdl_startfaceani",         bind_this(GameScript::mdl_startfaceani));
  vm.register_external("mdl_applyrandomani",       bind_this(GameScript::mdl_applyrandomani));
  vm.register_external("mdl_applyrandomanifreq",   bind_this(GameScript::mdl_applyrandomanifreq));
  vm.register_external("mdl_applyrandomfaceani",   bind_this(GameScript::mdl_applyrandomfaceani));

  vm.register_external("npc_settofightmode",             bind_this(GameScript::npc_settofightmode));
  vm.register_external("npc_settofistmode",              bind_this(GameScript::npc_settofistmode));
  vm.register_external("npc_isinstate",                  bind_this(GameScript::npc_isinstate));
  vm.register_external("npc_isinroutine",                bind_this(GameScript::npc_isinroutine));
  vm.register_external("npc_wasinstate",                 bind_this(GameScript::npc_wasinstate));
  vm.register_external("npc_getdisttowp",                bind_this(GameScript::npc_getdisttowp));
  vm.register_external("npc_exchangeroutine",            bind_this(GameScript::npc_exchangeroutine));
  vm.register_external("npc_isdead",                     bind_this(GameScript::npc_isdead));
  vm.register_external("npc_knowsinfo",                  bind_this(GameScript::npc_knowsinfo));
  vm.register_external("npc_settalentskill",             bind_this(GameScript::npc_settalentskill));
  vm.register_external("npc_gettalentskill",             bind_this(GameScript::npc_gettalentskill));
  vm.register_external("npc_settalentvalue",             bind_this(GameScript::npc_settalentvalue));
  vm.register_external("npc_gettalentvalue",             bind_this(GameScript::npc_gettalentvalue));
  vm.register_external("npc_setrefusetalk",              bind_this(GameScript::npc_setrefusetalk));
  vm.register_external("npc_refusetalk",                 bind_this(GameScript::npc_refusetalk));
  vm.register_external("npc_hasitems",                   bind_this(GameScript::npc_hasitems));
  vm.register_external("npc_getinvitem",                 bind_this(GameScript::npc_getinvitem));
  vm.register_external("npc_removeinvitem",              bind_this(GameScript::npc_removeinvitem));
  vm.register_external("npc_removeinvitems",             bind_this(GameScript::npc_removeinvitems));
  vm.register_external("npc_getbodystate",               bind_this(GameScript::npc_getbodystate));
  vm.register_external("npc_getlookattarget",            bind_this(GameScript::npc_getlookattarget));
  vm.register_external("npc_getdisttonpc",               bind_this(GameScript::npc_getdisttonpc));
  vm.register_external("npc_hasequippedarmor",           bind_this(GameScript::npc_hasequippedarmor));
  vm.register_external("npc_setperctime",                bind_this(GameScript::npc_setperctime));
  vm.register_external("npc_percenable",                 bind_this(GameScript::npc_percenable));
  vm.register_external("npc_percdisable",                bind_this(GameScript::npc_percdisable));
  vm.register_external("npc_getnearestwp",               bind_this(GameScript::npc_getnearestwp));
  vm.register_external("npc_clearaiqueue",               bind_this(GameScript::npc_clearaiqueue));
  vm.register_external("npc_isplayer",                   bind_this(GameScript::npc_isplayer));
  vm.register_external("npc_getstatetime",               bind_this(GameScript::npc_getstatetime));
  vm.register_external("npc_setstatetime",               bind_this(GameScript::npc_setstatetime));
  vm.register_external("npc_changeattribute",            bind_this(GameScript::npc_changeattribute));
  vm.register_external("npc_isonfp",                     bind_this(GameScript::npc_isonfp));
  vm.register_external("npc_getheighttonpc",             bind_this(GameScript::npc_getheighttonpc));
  vm.register_external("npc_getequippedmeleeweapon",     bind_this(GameScript::npc_getequippedmeleeweapon));
  vm.register_external("npc_getequippedrangedweapon",    bind_this(GameScript::npc_getequippedrangedweapon));
  vm.register_external("npc_getequippedarmor",           bind_this(GameScript::npc_getequippedarmor));
  vm.register_external("npc_canseenpc",                  bind_this(GameScript::npc_canseenpc));
  vm.register_external("npc_hasequippedweapon",          bind_this(GameScript::npc_hasequippedweapon));
  vm.register_external("npc_hasequippedmeleeweapon",     bind_this(GameScript::npc_hasequippedmeleeweapon));
  vm.register_external("npc_hasequippedrangedweapon",    bind_this(GameScript::npc_hasequippedrangedweapon));
  vm.register_external("npc_getactivespell",             bind_this(GameScript::npc_getactivespell));
  vm.register_external("npc_getactivespellisscroll",     bind_this(GameScript::npc_getactivespellisscroll));
  vm.register_external("npc_getactivespellcat",          bind_this(GameScript::npc_getactivespellcat));
  vm.register_external("npc_setactivespellinfo",         bind_this(GameScript::npc_setactivespellinfo));
  vm.register_external("npc_getactivespelllevel",        bind_this(GameScript::npc_getactivespelllevel));
  vm.register_external("npc_canseenpcfreelos",           bind_this(GameScript::npc_canseenpcfreelos));
  vm.register_external("npc_isinfightmode",              bind_this(GameScript::npc_isinfightmode));
  vm.register_external("npc_settarget",                  bind_this(GameScript::npc_settarget));
  vm.register_external("npc_gettarget",                  bind_this(GameScript::npc_gettarget));
  vm.register_external("npc_getnexttarget",              bind_this(GameScript::npc_getnexttarget));
  vm.register_external("npc_sendpassiveperc",            bind_this(GameScript::npc_sendpassiveperc));
  vm.register_external("npc_checkinfo",                  bind_this(GameScript::npc_checkinfo));
  vm.register_external("npc_getportalguild",             bind_this(GameScript::npc_getportalguild));
  vm.register_external("npc_isinplayersroom",            bind_this(GameScript::npc_isinplayersroom));
  vm.register_external("npc_getreadiedweapon",           bind_this(GameScript::npc_getreadiedweapon));
  vm.register_external("npc_hasreadiedmeleeweapon",      bind_this(GameScript::npc_hasreadiedmeleeweapon));
  vm.register_external("npc_isdrawingspell",             bind_this(GameScript::npc_isdrawingspell));
  vm.register_external("npc_isdrawingweapon",            bind_this(GameScript::npc_isdrawingweapon));
  vm.register_external("npc_perceiveall",                bind_this(GameScript::npc_perceiveall));
  vm.register_external("npc_stopani",                    bind_this(GameScript::npc_stopani));
  vm.register_external("npc_settrueguild",               bind_this(GameScript::npc_settrueguild));
  vm.register_external("npc_gettrueguild",               bind_this(GameScript::npc_gettrueguild));
  vm.register_external("npc_clearinventory",             bind_this(GameScript::npc_clearinventory));
  vm.register_external("npc_getattitude",                bind_this(GameScript::npc_getattitude));
  vm.register_external("npc_getpermattitude",            bind_this(GameScript::npc_getpermattitude));
  vm.register_external("npc_setattitude",                bind_this(GameScript::npc_setattitude));
  vm.register_external("npc_settempattitude",            bind_this(GameScript::npc_settempattitude));
  vm.register_external("npc_hasbodyflag",                bind_this(GameScript::npc_hasbodyflag));
  vm.register_external("npc_getlasthitspellid",          bind_this(GameScript::npc_getlasthitspellid));
  vm.register_external("npc_getlasthitspellcat",         bind_this(GameScript::npc_getlasthitspellcat));
  vm.register_external("npc_playani",                    bind_this(GameScript::npc_playani));
  vm.register_external("npc_isdetectedmobownedbynpc",    bind_this(GameScript::npc_isdetectedmobownedbynpc));
  vm.register_external("npc_getdetectedmob",             bind_this(GameScript::npc_getdetectedmob));
  vm.register_external("npc_isdetectedmobownedbyguild",  bind_this(GameScript::npc_isdetectedmobownedbyguild));
  vm.register_external("npc_ownedbynpc",                 bind_this(GameScript::npc_ownedbynpc));
  vm.register_external("npc_canseesource",               bind_this(GameScript::npc_canseesource));
  vm.register_external("npc_getdisttoitem",              bind_this(GameScript::npc_getdisttoitem));
  vm.register_external("npc_getheighttoitem",            bind_this(GameScript::npc_getheighttoitem));
  vm.register_external("npc_getdisttoplayer",            bind_this(GameScript::npc_getdisttoplayer));

  vm.register_external("ai_output",                 bind_this(GameScript::ai_output));
  vm.register_external("ai_stopprocessinfos",       bind_this(GameScript::ai_stopprocessinfos));
  vm.register_external("ai_processinfos",           bind_this(GameScript::ai_processinfos));
  vm.register_external("ai_standup",                bind_this(GameScript::ai_standup));
  vm.register_external("ai_standupquick",           bind_this(GameScript::ai_standupquick));
  vm.register_external("ai_continueroutine",        bind_this(GameScript::ai_continueroutine));
  vm.register_external("ai_stoplookat",             bind_this(GameScript::ai_stoplookat));
  vm.register_external("ai_lookatnpc",              bind_this(GameScript::ai_lookatnpc));
  vm.register_external("ai_removeweapon",           bind_this(GameScript::ai_removeweapon));
  vm.register_external("ai_turntonpc",              bind_this(GameScript::ai_turntonpc));
  vm.register_external("ai_outputsvm",              bind_this(GameScript::ai_outputsvm));
  vm.register_external("ai_outputsvm_overlay",      bind_this(GameScript::ai_outputsvm_overlay));
  vm.register_external("ai_startstate",             bind_this(GameScript::ai_startstate));
  vm.register_external("ai_playani",                bind_this(GameScript::ai_playani));
  vm.register_external("ai_setwalkmode",            bind_this(GameScript::ai_setwalkmode));
  vm.register_external("ai_wait",                   bind_this(GameScript::ai_wait));
  vm.register_external("ai_waitms",                 bind_this(GameScript::ai_waitms));
  vm.register_external("ai_aligntowp",              bind_this(GameScript::ai_aligntowp));
  vm.register_external("ai_gotowp",                 bind_this(GameScript::ai_gotowp));
  vm.register_external("ai_gotofp",                 bind_this(GameScript::ai_gotofp));
  vm.register_external("ai_playanibs",              bind_this(GameScript::ai_playanibs));
  vm.register_external("ai_equiparmor",             bind_this(GameScript::ai_equiparmor));
  vm.register_external("ai_equipbestarmor",         bind_this(GameScript::ai_equipbestarmor));
  vm.register_external("ai_equipbestmeleeweapon",   bind_this(GameScript::ai_equipbestmeleeweapon));
  vm.register_external("ai_equipbestrangedweapon",  bind_this(GameScript::ai_equipbestrangedweapon));
  vm.register_external("ai_usemob",                 bind_this(GameScript::ai_usemob));
  vm.register_external("ai_teleport",               bind_this(GameScript::ai_teleport));
  vm.register_external("ai_stoppointat",            bind_this(GameScript::ai_stoppointat));
  vm.register_external("ai_drawweapon",             bind_this(GameScript::ai_drawweapon));
  vm.register_external("ai_readymeleeweapon",       bind_this(GameScript::ai_readymeleeweapon));
  vm.register_external("ai_readyrangedweapon",      bind_this(GameScript::ai_readyrangedweapon));
  vm.register_external("ai_readyspell",             bind_this(GameScript::ai_readyspell));
  vm.register_external("ai_attack",                 bind_this(GameScript::ai_atack));
  vm.register_external("ai_flee",                   bind_this(GameScript::ai_flee));
  vm.register_external("ai_dodge",                  bind_this(GameScript::ai_dodge));
  vm.register_external("ai_unequipweapons",         bind_this(GameScript::ai_unequipweapons));
  vm.register_external("ai_unequiparmor",           bind_this(GameScript::ai_unequiparmor));
  vm.register_external("ai_gotonpc",                bind_this(GameScript::ai_gotonpc));
  vm.register_external("ai_gotonextfp",             bind_this(GameScript::ai_gotonextfp));
  vm.register_external("ai_aligntofp",              bind_this(GameScript::ai_aligntofp));
  vm.register_external("ai_useitem",                bind_this(GameScript::ai_useitem));
  vm.register_external("ai_useitemtostate",         bind_this(GameScript::ai_useitemtostate));
  vm.register_external("ai_setnpcstostate",         bind_this(GameScript::ai_setnpcstostate));
  vm.register_external("ai_finishingmove",          bind_this(GameScript::ai_finishingmove));
  vm.register_external("ai_takeitem",               bind_this(GameScript::ai_takeitem));
  vm.register_external("ai_gotoitem",               bind_this(GameScript::ai_gotoitem));
  vm.register_external("ai_pointat",                bind_this(GameScript::ai_pointat));
  vm.register_external("ai_pointatnpc",             bind_this(GameScript::ai_pointatnpc));

  vm.register_external("mob_hasitems",        bind_this(GameScript::mob_hasitems));

  vm.register_external("ta_min",              bind_this(GameScript::ta_min));

  vm.register_external("log_createtopic",     bind_this(GameScript::log_createtopic));
  vm.register_external("log_settopicstatus",  bind_this(GameScript::log_settopicstatus));
  vm.register_external("log_addentry",        bind_this(GameScript::log_addentry));

  vm.register_external("equipitem",           bind_this(GameScript::equipitem));
  vm.register_external("createinvitem",       bind_this(GameScript::createinvitem));
  vm.register_external("createinvitems",      bind_this(GameScript::createinvitems));

  vm.register_external("info_addchoice",          bind_this(GameScript::info_addchoice));
  vm.register_external("info_clearchoices",       bind_this(GameScript::info_clearchoices));
  vm.register_external("infomanager_hasfinished", bind_this(GameScript::infomanager_hasfinished));

  vm.register_external("snd_play",            bind_this(GameScript::snd_play));
  vm.register_external("snd_play3d",          bind_this(GameScript::snd_play3d));

  vm.register_external("game_initgerman",     bind_this(GameScript::game_initgerman));
  vm.register_external("game_initenglish",    bind_this(GameScript::game_initenglish));

  vm.register_external("exitsession",         bind_this(GameScript::exitsession));

  // vm.validateExternals();

  spells               = std::make_unique<SpellDefinitions>(vm);
  svm                  = std::make_unique<SvmDefinitions>(vm);

  cFocusNorm           = getFocus("Focus_Normal");
  cFocusMele           = getFocus("Focus_Melee");
  cFocusRange          = getFocus("Focus_Ranged");
  cFocusMage           = getFocus("Focus_Magic");

  ZS_Dead              = aiState(getSymbolIndex("ZS_Dead")).funcIni;
  ZS_Unconscious       = aiState(getSymbolIndex("ZS_Unconscious")).funcIni;
  ZS_Talk              = aiState(getSymbolIndex("ZS_Talk")).funcIni;
  ZS_Attack            = aiState(getSymbolIndex("ZS_Attack")).funcIni;
  ZS_MM_Attack         = aiState(getSymbolIndex("ZS_MM_Attack")).funcIni;

  spellFxInstanceNames = vm.find_symbol_by_name("spellFxInstanceNames");
  spellFxAniLetters    = vm.find_symbol_by_name("spellFxAniLetters");

  if(owner.version().game==2){
    auto* currency = vm.find_symbol_by_name("TRADE_CURRENCY_INSTANCE");
    itMi_Gold      = vm.find_symbol_by_name(currency->get_string());
    if(itMi_Gold!=nullptr){ // FIXME
      auto item = vm.init_instance<phoenix::daedalus::c_item>(itMi_Gold);
      goldTxt = item->name;
      }
    auto* tradeMul = vm.find_symbol_by_name("TRADE_VALUE_MULTIPLIER");
    tradeValMult   = tradeMul->get_float();

    auto* vtime     = vm.find_symbol_by_name("VIEW_TIME_PER_CHAR");
    viewTimePerChar = vtime->get_float(0);
    if(viewTimePerChar<=0.f)
      viewTimePerChar=0.55f;
    } else {
    itMi_Gold      = vm.find_symbol_by_name("ItMiNugget");
    if(itMi_Gold!=nullptr) { // FIXME
      auto item = vm.init_instance<phoenix::daedalus::c_item>(itMi_Gold);
      goldTxt = item->name;
      }
    //
    tradeValMult   = 1.f;
    viewTimePerChar=0.55f;
    }

  auto* gilMax = vm.find_symbol_by_name("GIL_MAX");
  gilCount=size_t(gilMax->get_int());

  auto* tableSz = vm.find_symbol_by_name("TAB_ANZAHL");
  auto* guilds  = vm.find_symbol_by_name("GIL_ATTITUDES");
  gilAttitudes.resize(gilCount*gilCount,ATT_HOSTILE);

  size_t tbSz=size_t(std::sqrt(tableSz->get_int()));
  for(size_t i=0;i<tbSz;++i)
    for(size_t r=0;r<tbSz;++r) {
      gilAttitudes[i*gilCount+r]=guilds->get_int(i*tbSz+r);
      gilAttitudes[r*gilCount+i]=guilds->get_int(r*tbSz+i);
      }

  auto id = vm.find_symbol_by_name("Gil_Values");
  if(id!=nullptr){
    cGuildVal = vm.init_instance<phoenix::daedalus::c_gil_values>(id);
    for(size_t i=0;i<Guild::GIL_PUBLIC;++i){
      cGuildVal->water_depth_knee   [i]=cGuildVal->water_depth_knee   [Guild::GIL_HUMAN];
      cGuildVal->water_depth_chest  [i]=cGuildVal->water_depth_chest  [Guild::GIL_HUMAN];
      cGuildVal->jumpup_height      [i]=cGuildVal->jumpup_height      [Guild::GIL_HUMAN];
      cGuildVal->swim_time          [i]=cGuildVal->swim_time          [Guild::GIL_HUMAN];
      cGuildVal->dive_time          [i]=cGuildVal->dive_time          [Guild::GIL_HUMAN];
      cGuildVal->step_height        [i]=cGuildVal->step_height        [Guild::GIL_HUMAN];
      cGuildVal->jumplow_height     [i]=cGuildVal->jumplow_height     [Guild::GIL_HUMAN];
      cGuildVal->jumpmid_height     [i]=cGuildVal->jumpmid_height     [Guild::GIL_HUMAN];
      cGuildVal->slide_angle        [i]=cGuildVal->slide_angle        [Guild::GIL_HUMAN];
      cGuildVal->slide_angle2       [i]=cGuildVal->slide_angle2       [Guild::GIL_HUMAN];
      cGuildVal->disable_autoroll   [i]=cGuildVal->disable_autoroll   [Guild::GIL_HUMAN];
      cGuildVal->surface_align      [i]=cGuildVal->surface_align      [Guild::GIL_HUMAN];
      cGuildVal->climb_heading_angle[i]=cGuildVal->climb_heading_angle[Guild::GIL_HUMAN];
      cGuildVal->climb_horiz_angle  [i]=cGuildVal->climb_horiz_angle  [Guild::GIL_HUMAN];
      cGuildVal->climb_ground_angle [i]=cGuildVal->climb_ground_angle [Guild::GIL_HUMAN];
      cGuildVal->fight_range_base   [i]=cGuildVal->fight_range_base   [Guild::GIL_HUMAN];
      cGuildVal->fight_range_fist   [i]=cGuildVal->fight_range_fist   [Guild::GIL_HUMAN];
      cGuildVal->fight_range_g      [i]=cGuildVal->fight_range_g      [Guild::GIL_HUMAN];
      cGuildVal->fight_range_1hs    [i]=cGuildVal->fight_range_1hs    [Guild::GIL_HUMAN];
      cGuildVal->fight_range_1ha    [i]=cGuildVal->fight_range_1ha    [Guild::GIL_HUMAN];
      cGuildVal->fight_range_2hs    [i]=cGuildVal->fight_range_2hs    [Guild::GIL_HUMAN];
      cGuildVal->fight_range_2ha    [i]=cGuildVal->fight_range_2ha    [Guild::GIL_HUMAN];
      cGuildVal->falldown_height    [i]=cGuildVal->falldown_height    [Guild::GIL_HUMAN];
      cGuildVal->falldown_damage    [i]=cGuildVal->falldown_damage    [Guild::GIL_HUMAN];
      cGuildVal->blood_disabled     [i]=cGuildVal->blood_disabled     [Guild::GIL_HUMAN];
      cGuildVal->blood_max_distance [i]=cGuildVal->blood_max_distance [Guild::GIL_HUMAN];
      cGuildVal->blood_amount       [i]=cGuildVal->blood_amount       [Guild::GIL_HUMAN];
      cGuildVal->blood_flow         [i]=cGuildVal->blood_flow         [Guild::GIL_HUMAN];
      cGuildVal->blood_emitter      [i]=cGuildVal->blood_emitter      [Guild::GIL_HUMAN];
      cGuildVal->blood_texture      [i]=cGuildVal->blood_texture      [Guild::GIL_HUMAN];
      cGuildVal->turn_speed         [i]=cGuildVal->turn_speed         [Guild::GIL_HUMAN];
      }
    }

  if(Ikarus::isRequired(vm)) {
    plugins.emplace_back(std::make_unique<Ikarus>(*this,vm));
    }
  if(LeGo::isRequired(vm)) {
    plugins.emplace_back(std::make_unique<LeGo>(*this,vm));
    }
  }

void GameScript::initDialogs() {
  loadDialogOU();

  vm.enumerate_instances_by_class_name("C_INFO", [this](phoenix::daedalus::symbol& sym){
    dialogsInfo.push_back(vm.init_instance<phoenix::daedalus::c_info>(&sym));
    });
  }

void GameScript::loadDialogOU() {
  auto gCutscene = Gothic::inst().nestedPath({u"_work",u"Data",u"Scripts",u"content",u"CUTSCENE"},Dir::FT_Dir);
  static const char* names[] = {
    "OU.DAT",
    "OU.BIN",
    };

  for(auto OU:names) {
    if(Resources::hasFile(OU)) {
      auto buf = Resources::getFileBuffer(OU);
      dialogs = phoenix::messages::parse(buf);
      return;
      }

    char16_t str16[256] = {};
    for(size_t i=0; OU[i] && i<255; ++i)
      str16[i] = char16_t(OU[i]);
    std::u16string full = FileUtil::caseInsensitiveSegment(gCutscene,str16,Dir::FT_File);
    try {
      auto buf = phoenix::buffer::mmap(full);
      dialogs = phoenix::messages::parse(buf);
      return;
      }
    catch(...){
      // loop to next possible path
      }
    }
  Log::e("unable to load Zen-file: \"OU.DAT\" or \"OU.BIN\"");
  }

void GameScript::initializeInstanceNpc(const std::shared_ptr<phoenix::daedalus::c_npc>& npc, size_t instance) {
  auto sym = vm.find_symbol_by_index(instance);
  vm.init_instance(npc, sym);

  if(npc->daily_routine!=0) {
    ScopeVar self(vm.global_self(), npc);
    auto* daily_routine = vm.find_symbol_by_index(npc->daily_routine);
    vm.call_function(daily_routine);
    }

  }

void GameScript::initializeInstanceItem(const std::shared_ptr<phoenix::daedalus::c_item>& item, size_t instance) {
  auto sym = vm.find_symbol_by_index(instance);
  vm.init_instance(item, sym);
  }

void GameScript::saveQuests(Serialize &fout) {
  quests.save(fout);
  fout.write(uint32_t(dlgKnownInfos.size()));
  for(auto& i:dlgKnownInfos)
    fout.write(uint32_t(i.first),uint32_t(i.second));

  fout.write(gilAttitudes);
  }

void GameScript::loadQuests(Serialize& fin) {
  quests.load(fin);
  uint32_t sz=0;
  fin.read(sz);
  for(size_t i=0;i<sz;++i){
    uint32_t f=0,s=0;
    fin.read(f,s);
    dlgKnownInfos.insert(std::make_pair(f,s));
    }

  fin.read(gilAttitudes);
  }

void GameScript::saveVar(Serialize &fout) {
  auto& dat = vm.symbols();
  fout.write(uint32_t(dat.size()));
  for(unsigned i = 0; i < dat.size(); ++i){
    saveSym(fout,*vm.find_symbol_by_index(i));
    }
  }

void GameScript::loadVar(Serialize &fin) {
  std::string name;
  uint32_t sz=0;
  fin.read(sz);
  for(size_t i=0;i<sz;++i){
    uint32_t t=phoenix::daedalus::dt_void;
    fin.read(t);
    switch(phoenix::daedalus::datatype(t)) {
      case phoenix::daedalus::dt_integer:{
        fin.read(name);
        auto* s = getSymbol(name.c_str());

        int v;
        for (unsigned j = 0; j < s->count(); ++j) {
          fin.read(v);
          s->set_int(v, j);
        }

        break;
        }
      case phoenix::daedalus::dt_float:{
        fin.read(name);
        auto* s = getSymbol(name.c_str());

        float v;
        for (unsigned j = 0; j < s->count(); ++j) {
          fin.read(v);
          s->set_float(v, j);
        }

        break;
        }
      case phoenix::daedalus::dt_string:{
        fin.read(name);
        auto* s = getSymbol(name.c_str());

        std::string v;
        for (unsigned j = 0; j < s->count(); ++j) {
          fin.read(v);
          s->set_string(v, j);
        }

        break;
        }
      case phoenix::daedalus::dt_instance:{
        uint8_t dataClass=0;
        fin.read(dataClass);
        if(dataClass>0){
          uint32_t id=0;
          fin.read(name,id);
          auto* s = getSymbol(name.c_str());
          if(dataClass==1) {
            auto npc = world().npcById(id);
            s->set_instance(npc ? npc->handle() : nullptr);
            }
          else if(dataClass==2) {
            auto itm = world().itmById(id);
            s->set_instance(itm != nullptr ? itm->handle() : nullptr);
            }
          else if(dataClass==3) {
            uint32_t itmClass=0;
            fin.read(itmClass);
            if(auto npc = world().npcById(id)) {
              auto itm = npc->getItem(itmClass);
              s->set_instance(itm ? itm->handle() : nullptr);
              }
            }
          }
        break;
        }
        default:
          break;
      }
    }
  }

void GameScript::resetVarPointers() {
  for(size_t i=0;i<vm.symbols().size();++i){
    auto* s = vm.find_symbol_by_index(i);
    if(s->is_instance_of<phoenix::daedalus::c_npc>() || s->is_instance_of<phoenix::daedalus::c_item>()){
      s->set_instance(nullptr);
      }
    }
  }

const QuestLog& GameScript::questLog() const {
  return quests;
  }

void GameScript::saveSym(Serialize &fout, phoenix::daedalus::symbol &i) {
  auto& w = world();
  switch(i.type()) {
    case phoenix::daedalus::dt_integer:
      if(i.count()>0){
        fout.write(i.type(), i.name());

        for (unsigned j = 0; j < i.count(); ++j)
          fout.write(i.get_int(j));
        return;
        }
      break;
    case phoenix::daedalus::dt_float:
      if(i.count()>0){
        fout.write(i.type(), i.name());

        for (unsigned j = 0; j < i.count(); ++j)
          fout.write(i.get_float(j));
        return;
        }
      break;
    case phoenix::daedalus::dt_string:
      if(i.count()>0){
        fout.write(i.type(), i.name());

        for (unsigned j = 0; j < i.count(); ++j)
          fout.write(i.get_string(j));
        return;
        }
      break;
    case phoenix::daedalus::dt_instance:
      fout.write(i.type());

      if(i.is_instance_of<phoenix::daedalus::c_npc>()){
        auto hnpc = reinterpret_cast<const phoenix::daedalus::c_npc*>(i.get_instance().get());
        auto npc  = reinterpret_cast<const Npc*>(hnpc==nullptr ? nullptr : hnpc->user_ptr);
        fout.write(uint8_t(1),i.name(),world().npcId(npc));
        }
      else if(i.is_instance_of<phoenix::daedalus::c_item>()){
          auto     item = reinterpret_cast<const phoenix::daedalus::c_item*>(i.get_instance().get());
          uint32_t id   = w.itmId(item);
          if(id!=uint32_t(-1) || item==nullptr) {
              fout.write(uint8_t(2),i.name(),id);
          } else {
              uint32_t idNpc = uint32_t(-1);
              for(uint32_t r=0; r<w.npcCount(); ++r) {
                  auto& n = *w.npcById(r);
                  if(n.itemCount(item->symbol_index())>0) {
                      idNpc = r;
                      fout.write(uint8_t(3),i.name(),idNpc,uint32_t(item->symbol_index()));
                      break;
                  }
              }
              if(idNpc==uint32_t(-1))
                  fout.write(uint8_t(2),i.name(),uint32_t(-1));
          }
        }
      else if(i.is_instance_of<phoenix::daedalus::c_focus>() ||
              i.is_instance_of<phoenix::daedalus::c_gil_values>() ||
              i.is_instance_of<phoenix::daedalus::c_info>()) {
        fout.write(uint8_t(0));
        }
      else {
        fout.write(uint8_t(0));
        }
      return;
    default:
      break;
    }
  fout.write(phoenix::daedalus::dt_void);
  }

void GameScript::fixNpcPosition(Npc& npc, float angle0, float distBias) {
  auto& dyn  = *world().physic();
  auto  pos0 = npc.position();

  for(int r = 0; r<=800; r+=20) {
    for(float ang = 0; ang<360; ang+=30.f) {
      float a = float((ang+angle0)*M_PI/180.0);
      float d = float(r)+distBias;
      auto  p = pos0+Vec3(std::cos(a)*d, 0, std::sin(a)*d);

      auto ray = dyn.ray(p+Vec3(0,100,0), p+Vec3(0,-1000,0));
      if(!ray.hasCol)
        continue;
      p.y = ray.v.y;
      npc.setPosition(p);
      if(!npc.hasCollision())
        return;
      }
    }
  npc.setPosition(pos0);
  }

const World &GameScript::world() const {
  return *owner.world();
  }

World &GameScript::world() {
  return *owner.world();
  }

std::shared_ptr<phoenix::daedalus::c_focus> GameScript::getFocus(const char *name) {
  phoenix::daedalus::c_focus ret={};
  auto id = vm.find_symbol_by_name(name);
  if(id==nullptr)
    return nullptr;
  return vm.init_instance<phoenix::daedalus::c_focus>(id);
  }

void GameScript::storeItem(Item *itm) {
  auto* s = vm.global_item();
  if(itm!=nullptr) {
    s->set_instance(itm->handle());
    } else {
    s->set_instance(nullptr);
    }
  }

phoenix::daedalus::symbol* GameScript::getSymbol(std::string_view s) {
  char buf[256] = {};
  std::snprintf(buf,sizeof(buf),"%.*s",int(s.size()),s.data());
  return vm.find_symbol_by_name(buf);
  }

phoenix::daedalus::symbol* GameScript::getSymbol(const size_t s) {
  return vm.find_symbol_by_index(s);
  }

size_t GameScript::getSymbolIndex(std::string_view s) {
  char buf[256] = {};
  std::snprintf(buf,sizeof(buf),"%.*s",int(s.size()),s.data());
  auto sym = vm.find_symbol_by_name(buf);
  return sym == nullptr ? size_t(-1) : sym->index();
  }

size_t GameScript::getSymbolCount() const {
  return vm.symbols().size();
  }

const AiState& GameScript::aiState(ScriptFn id) {
  auto it = aiStates.find(id.ptr);
  if(it!=aiStates.end())
    return it->second;
  auto ins = aiStates.emplace(id.ptr,AiState(*this,id.ptr));
  return ins.first->second;
  }

const phoenix::daedalus::c_spell& GameScript::spellDesc(int32_t splId) {
  auto& tag = spellFxInstanceNames->get_string(splId);
  return spells->find(tag);
  }

const VisualFx* GameScript::spellVfx(int32_t splId) {
  auto& tag = spellFxInstanceNames->get_string(splId);

  char name[256]={};
  std::snprintf(name,sizeof(name),"spellFX_%s",tag.c_str());
  return Gothic::inst().loadVisualFx(name);
  }

std::vector<GameScript::DlgChoise> GameScript::dialogChoises(std::shared_ptr<phoenix::daedalus::c_npc> player,
                                                               std::shared_ptr<phoenix::daedalus::c_npc> hnpc,
                                                               const std::vector<uint32_t>& except,
                                                               bool includeImp) {
  ScopeVar self (vm.global_self(),  hnpc);
  ScopeVar other(vm.global_other(), player);
  std::vector<phoenix::daedalus::c_info*> hDialog;
  for(auto& info : dialogsInfo) {
    if(info->npc==static_cast<int>(hnpc->symbol_index())) {
      hDialog.push_back(info.get());
      }
    }

  std::vector<DlgChoise> choise;

  for(int important=includeImp ? 1 : 0;important>=0;--important){
    for(auto& i:hDialog) {
      const phoenix::daedalus::c_info& info = *i;
      if(info.important!=important)
        continue;
      bool npcKnowsInfo = doesNpcKnowInfo(*player,vm.find_symbol_by_instance(info)->index());
      if(npcKnowsInfo && !info.permanent)
        continue;

      if(info.important && info.permanent){
        bool skip=false;
        for(auto i:except)
          if(static_cast<int>(i)==info.information){
            skip=true;
            break;
            }
        if(skip)
          continue;
        }

      bool valid=true;
      if(info.condition)
        valid = vm.call_function<int>(vm.find_symbol_by_index(info.condition))!=0;
      if(!valid)
        continue;

      DlgChoise ch;
      ch.title    = info.description.c_str();
      ch.scriptFn = info.information;
      ch.handle   = i;
      ch.isTrade  = info.trade!=0;
      ch.sort     = info.nr;
      choise.emplace_back(std::move(ch));
      }
    if(!choise.empty()){
      sort(choise);
      return choise;
      }
    }
  sort(choise);
  return choise;
  }

std::vector<GameScript::DlgChoise> GameScript::updateDialog(const GameScript::DlgChoise &dlg, Npc& player,Npc& npc) {
  if(dlg.handle==nullptr)
    return {};
  const phoenix::daedalus::c_info& info = *dlg.handle;
  std::vector<GameScript::DlgChoise>     ret;

  ScopeVar self (vm.global_self(),  npc.handle());
  ScopeVar other(vm.global_other(), player.handle());

  for(size_t i=0;i<info.choices.size();++i){
    auto& sub = info.choices[i];
    GameScript::DlgChoise ch;
    ch.title    = sub.text.c_str();
    ch.scriptFn = sub.function;
    ch.handle   = dlg.handle;
    ch.isTrade  = false;
    ch.sort     = int(i);
    ret.push_back(ch);
    }

  sort(ret);
  return ret;
  }

void GameScript::exec(const GameScript::DlgChoise &dlg,Npc& player, Npc& npc) {
  ScopeVar self (vm.global_self(),  npc.handle());
  ScopeVar other(vm.global_other(), player.handle());

  phoenix::daedalus::c_info& info = *dlg.handle;

  if(&player!=&npc)
    player.stopAnim("");
  auto pl = *player.handle();

  if(info.information==int(dlg.scriptFn)) {
    setNpcInfoKnown(pl,info);
    } else {
    for(size_t i=0;i<info.choices.size();){
      if(info.choices[i].function==int(dlg.scriptFn))
        info.choices.erase(info.choices.begin()+int(i)); else
        ++i;
      }
    }
  vm.call_function(vm.find_symbol_by_index(dlg.scriptFn));
  }

void GameScript::printCannotUseError(Npc& npc, int32_t atr, int32_t nValue) {
  auto id = vm.find_symbol_by_name("G_CanNotUse");
  if(id==nullptr)
    return;

  ScopeVar self(vm.global_self(), npc.handle());
  vm.call_function<void>(id, npc.isPlayer(), atr, nValue);
  }

void GameScript::printCannotCastError(Npc &npc, int32_t plM, int32_t itM) {
  auto id = vm.find_symbol_by_name("G_CanNotCast");
  if(id==nullptr)
    return;

  ScopeVar self(vm.global_self(), npc.handle());
  vm.call_function<void>(id, npc.isPlayer(), itM, plM);
  }

void GameScript::printCannotBuyError(Npc &npc) {
  auto id = vm.find_symbol_by_name("player_trade_not_enough_gold");
  if(id==nullptr)
    return;
  ScopeVar self(vm.global_self(), npc.handle());
  vm.call_function<void>(id);
  }

void GameScript::printMobMissingItem(Npc &npc) {
  auto id = vm.find_symbol_by_name("player_mob_missing_item");
  if(id==nullptr)
    return;
  ScopeVar self(vm.global_self(), npc.handle());
  vm.call_function<void>(id);
  }

void GameScript::printMobMissingKey(Npc& npc) {
  auto id = vm.find_symbol_by_name("player_mob_missing_key");
  if(id==nullptr)
    return;
  ScopeVar self(vm.global_self(), npc.handle());
  vm.call_function<void>(id);
  }

void GameScript::printMobAnotherIsUsing(Npc &npc) {
  auto id = vm.find_symbol_by_name("player_mob_another_is_using");
  if(id==nullptr)
    return;
  ScopeVar self(vm.global_self(), npc.handle());
  vm.call_function<void>(id);
  }

void GameScript::printMobMissingKeyOrLockpick(Npc& npc) {
  auto id = vm.find_symbol_by_name("player_mob_missing_key_or_lockpick");
  if(id==nullptr)
    return;
  ScopeVar self(vm.global_self(), npc.handle());
  vm.call_function<void>(id);
  }

void GameScript::printMobMissingLockpick(Npc& npc) {
  auto id = vm.find_symbol_by_name("player_mob_missing_lockpick");
  if(id==nullptr)
    return;
  ScopeVar self(vm.global_self(), npc.handle());
  vm.call_function<void>(id);
  }

void GameScript::printMobTooFar(Npc& npc) {
  auto id = vm.find_symbol_by_name("player_mob_too_far_away");
  if(id==nullptr)
    return;
  ScopeVar self(vm.global_self(), npc.handle());
  vm.call_function<void>(id);
  }

void GameScript::invokeState(const std::shared_ptr<phoenix::daedalus::c_npc>& hnpc, const std::shared_ptr<phoenix::daedalus::c_npc>& oth, const char *name) {
  auto id = vm.find_symbol_by_name(name);
  if(id==nullptr)
    return;

  ScopeVar self (vm.global_self(),  hnpc);
  ScopeVar other(vm.global_other(), oth);
  vm.call_function<void>(id);
  }

int GameScript::invokeState(Npc* npc, Npc* oth, Npc* vic, ScriptFn fn) {
  if(!fn.isValid())
    return 0;
  if(oth==nullptr){
    // oth=npc; //FIXME: PC_Levelinspektor?
    }
  if(vic==nullptr)
    vic=owner.player();

  if(fn==ZS_Talk){
    if(oth==nullptr || !oth->isPlayer()) {
      Log::e("unxepected perc acton");
      return 0;
      }
    }

  ScopeVar self  (vm.global_self(),   npc != nullptr ? npc->handle() : nullptr);
  ScopeVar other (vm.global_other(),  oth != nullptr ? oth->handle() : nullptr);
  ScopeVar victum(vm.global_victim(), vic != nullptr ? vic->handle() : nullptr);

  auto sym = vm.find_symbol_by_index(fn.ptr);
  int ret = 0;
  if (sym->rtype() == phoenix::daedalus::dt_integer) {
    ret = vm.call_function<int>(sym);
  } else {
    vm.call_function<void>(sym);
  }
  if(vm.global_other()->is_instance_of<phoenix::daedalus::c_npc>()){
    auto oth2 = reinterpret_cast<phoenix::daedalus::c_npc*>(vm.global_other()->get_instance().get());
    if(oth!=nullptr && oth2!=oth->handle().get()) {
      Npc* other = getNpc(oth2);
      npc->setOther(other);
      }
    }
  return ret;
  }

void GameScript::invokeItem(Npc *npc, ScriptFn fn) {
  if(fn==size_t(-1) || fn == 0)
    return;
  ScopeVar self(vm.global_self(), npc->handle());
  return vm.call_function<void>(vm.find_symbol_by_index(fn.ptr));
  }

int GameScript::invokeMana(Npc &npc, Npc* target, Item &) {
  auto fn = vm.find_symbol_by_name("Spell_ProcessMana");
  if(fn==nullptr)
    return SpellCode::SPL_SENDSTOP;

  ScopeVar self (vm.global_self(),  npc.handle());
  ScopeVar other(vm.global_other(), target->handle());

  return vm.call_function<int>(fn, npc.attribute(ATR_MANA));
  }

void GameScript::invokeSpell(Npc &npc, Npc* target, Item &it) {
  auto& tag       = spellFxInstanceNames->get_string(it.spellId());
  char  str[256]={};
  std::snprintf(str,sizeof(str),"Spell_Cast_%s",tag.c_str());

  auto  fn  = vm.find_symbol_by_name(str);
  if(fn==nullptr)
    return;

  // FIXME: actually set the spell level!
  int32_t splLevel = 0;

  ScopeVar self (vm.global_self(),  npc.handle());
  ScopeVar other(vm.global_other(), target->handle());
  try {
    if (fn->count() == 1) {
      // this is a leveled spell
      vm.call_function<void>(fn, splLevel);
    } else {
      vm.call_function<void>(fn);
    }
    }
  catch(...){
    Log::d("unable to call spell-script: \"",str,"\'");
    }
  }

int GameScript::invokeCond(Npc& npc, const std::string& func) {
  auto fn = vm.find_symbol_by_name(func);
  if(fn==nullptr) {
    Gothic::inst().onPrint("MOBSI::conditionFunc is not invalid");
    return 1;
    }
  ScopeVar self(vm.global_self(), npc.handle());
  return vm.call_function<int>(fn);
  }

void GameScript::invokePickLock(Npc& npc, int bSuccess, int bBrokenOpen) {
  auto fn   = vm.find_symbol_by_name("G_PickLock");
  if(fn==nullptr)
    return;
  ScopeVar self(vm.global_self(), npc.handle());
  vm.call_function<void>(fn, bSuccess, bBrokenOpen);
  }

CollideMask GameScript::canNpcCollideWithSpell(Npc& npc, Npc* shooter, int32_t spellId) {
  auto fn   = vm.find_symbol_by_name("C_CanNpcCollideWithSpell");
  if(fn==nullptr)
    return COLL_DOEVERYTHING;

  ScopeVar self (vm.global_self(),  npc.handle());
  ScopeVar other(vm.global_other(), shooter->handle());
  return CollideMask(vm.call_function<int>(fn, spellId));
  }

int GameScript::playerHotKeyScreenMap(Npc& pl) {
  auto fn   = vm.find_symbol_by_name("player_hotkey_screen_map");
  if(fn==nullptr)
    return -1;

  ScopeVar self(vm.global_self(), pl.handle());
  int map = vm.call_function<int>(fn);
  if(map>=0)
    pl.useItem(size_t(map));
  return map;
  }

const std::string& GameScript::spellCastAnim(Npc&, Item &it) {
  if(spellFxAniLetters==nullptr) {
    static const std::string FIB = "FIB";
    return FIB;
    }
  return spellFxAniLetters->get_string(it.spellId());
  }

bool GameScript::aiOutput(Npc &npc, std::string_view outputname, bool overlay) {
  char buf[256]={};
  std::snprintf(buf,sizeof(buf),"%s.WAV",outputname.data());

  uint64_t dt=0;
  world().addDlgSound(buf,npc.position()+Vec3{0,180,0},WorldSound::talkRange,dt);
  npc.setAiOutputBarrier(messageTime(outputname),overlay);

  return true;
  }

bool GameScript::aiOutputSvm(Npc &npc, std::string_view outputname, bool overlay) {
  if(overlay) {
    if(tickCount()<svmBarrier)
      return true;
    svmBarrier = tickCount()+messageTime(outputname);
    }

  if(!outputname.empty())
    return aiOutput(npc,outputname,overlay);
  return true;
  }

bool GameScript::isDead(const Npc &pl) {
  return pl.isState(ZS_Dead);
  }

bool GameScript::isUnconscious(const Npc &pl) {
  return pl.isState(ZS_Unconscious);
  }

bool GameScript::isTalk(const Npc &pl) {
  return pl.isState(ZS_Talk);
  }

bool GameScript::isAtack(const Npc& pl) const {
  return pl.isState(ZS_Attack) || pl.isState(ZS_MM_Attack);
  }

const std::string& GameScript::messageFromSvm(std::string_view id, int voice) const {
  return svm->find(id,voice);
  }

const std::string& GameScript::messageByName(const std::string& id) const {
  auto* blk = dialogs.block_by_name(id);
  if(blk == nullptr){
    static std::string empty {};
    return empty;
    }
  return blk->message.text;
  }

uint32_t GameScript::messageTime(std::string_view id) const {
  uint32_t& time = msgTimings[id.data()];
  if(time>0)
    return time;

  char buf[256]={};
  std::snprintf(buf,sizeof(buf),"%s.wav",id.data());
  auto  s   = Resources::loadSoundBuffer(buf);
  if(s.timeLength()>0) {
    time = uint32_t(s.timeLength());
    } else {
    auto&  txt  = messageByName(id.data());
    size_t size = std::strlen(txt.c_str());

    time = uint32_t(float(size)*viewTimePerChar);
    }
  return time;
  }

void GameScript::printNothingToGet() {
  auto id = vm.find_symbol_by_name("player_plunder_is_empty");
  if(id==nullptr)
    return;
  ScopeVar self(vm.global_self(), owner.player()->handle());
  vm.call_function<void>(id);
  }

void GameScript::useInteractive(const std::shared_ptr<phoenix::daedalus::c_npc>& hnpc,const std::string& func) {
  auto fn = vm.find_symbol_by_name(func);
  if(fn == nullptr)
    return;

  ScopeVar self(vm.global_self(),hnpc);
  try {
    vm.call_function<void>(func);
    }
  catch (...) {
    Log::i("unable to use interactive [",func,"]");
    }
  }

Attitude GameScript::guildAttitude(const Npc &p0, const Npc &p1) const {
  auto selfG = std::min<size_t>(gilCount-1,p0.guild());
  auto npcG  = std::min<size_t>(gilCount-1,p1.guild());
  auto ret   = gilAttitudes[selfG*gilCount+npcG];
  return Attitude(ret);
  }

Attitude GameScript::personAttitude(const Npc &p0, const Npc &p1) const {
  if(!p0.isPlayer() && !p1.isPlayer())
    return guildAttitude(p0,p1);

  Attitude att=ATT_NULL;
  const Npc& npc = p0.isPlayer() ? p1 : p0;
  att = npc.attitude();
  if(att!=ATT_NULL)
    return att;
  if(npc.isFriend())
    return ATT_FRIENDLY;
  att = guildAttitude(p0,p1);
  return att;
  }

BodyState GameScript::schemeToBodystate(std::string_view sc) {
  if(searchScheme(sc,"MOB_SIT"))
    return BS_SIT;
  if(searchScheme(sc,"MOB_LIE"))
    return BS_LIE;
  if(searchScheme(sc,"MOB_CLIMB"))
    return BS_CLIMB;
  if(searchScheme(sc,"MOB_NOTINTERRUPTABLE"))
    return BS_MOBINTERACT;
  return BS_MOBINTERACT_INTERRUPT;
  }

void GameScript::onWldItemRemoved(const Item& itm) {
  onWldInstanceRemoved(itm.handle().get());
  }

void GameScript::onWldInstanceRemoved(const phoenix::daedalus::instance* obj) {
  vm.find_symbol_by_instance(*obj)->set_instance(nullptr);
  }

void GameScript::makeCurrent(Item* w) {
  if(w==nullptr)
    return;
  auto* s = vm.find_symbol_by_index(w->clsId());
  s->set_instance(w->handle());
  }

bool GameScript::searchScheme(std::string_view sc, std::string_view listName) {
  auto& list = getSymbol(listName)->get_string();
  const char* l = list.c_str();
  for(const char* e = l;;++e) {
    if(*e=='\0' || *e==',') {
      size_t len = size_t(std::distance(l,e));
      if(sc==std::string_view(l,len))
        return true;
      l = e+1;
      }
    if(*e=='\0')
      break;
    }
  return false;
  }

bool GameScript::hasSymbolName(std::string_view s) {
  char buf[256] = {};
  std::snprintf(buf,sizeof(buf),"%.*s",int(s.size()),s.data());
  return vm.find_symbol_by_name(buf) != nullptr;
  }

uint64_t GameScript::tickCount() const {
  return owner.tickCount();
  }

uint32_t GameScript::rand(uint32_t max) {
  return uint32_t(randGen())%max;
  }

Npc* GameScript::getNpc(phoenix::daedalus::c_npc *handle) {
  if(handle==nullptr)
    return nullptr;
  assert(handle->user_ptr); // engine bug, if null
  return reinterpret_cast<Npc*>(handle->user_ptr);
  }

Npc* GameScript::getNpc(const std::shared_ptr<phoenix::daedalus::c_npc>& handle) {
  if(handle==nullptr)
    return nullptr;
  assert(handle->user_ptr); // engine bug, if null
  return reinterpret_cast<Npc*>(handle->user_ptr);
  }

Item *GameScript::getItem(phoenix::daedalus::c_item* handle) {
  if(handle==nullptr)
    return nullptr;
  auto& itData = *handle;
  assert(itData.user_ptr); // engine bug, if null
  return reinterpret_cast<Item*>(itData.user_ptr);
  }

Item *GameScript::getItemById(size_t id) {
  auto* handle = vm.find_symbol_by_index(id);
  if(!handle->is_instance_of<phoenix::daedalus::c_item>())
    return nullptr;
  auto hnpc = reinterpret_cast<phoenix::daedalus::c_item*>(handle->get_instance().get());
  return getItem(hnpc);
  }

Npc* GameScript::getNpcById(size_t id) {
  auto* handle = vm.find_symbol_by_index(id);
  if(!handle->is_instance_of<phoenix::daedalus::c_npc>())
    return nullptr;

  auto hnpc = reinterpret_cast<phoenix::daedalus::c_npc*>(handle->get_instance().get());
  if(hnpc==nullptr) {
    auto obj = world().findNpcByInstance(id);
    handle->set_instance(obj ? obj->handle() : nullptr);
    hnpc = reinterpret_cast<phoenix::daedalus::c_npc*>(handle->get_instance().get());
    }
  return getNpc(hnpc);
  }

phoenix::daedalus::c_info* GameScript::getInfo(size_t id) {
  auto* sym = vm.find_symbol_by_index(id);
  if(!sym->is_instance_of<phoenix::daedalus::c_info>())
    return nullptr;
  auto* h = sym->get_instance().get();
  if(h==nullptr)
    Log::e("invalid c_info object: \"",sym->name(),"\"");
  return reinterpret_cast<phoenix::daedalus::c_info*>(h);
  }

void GameScript::removeItem(Item &it) {
  world().removeItem(it);
  }

void GameScript::setInstanceNPC(std::string_view name, Npc &npc) {
  char buf[256] = {};
  std::snprintf(buf,sizeof(buf),"%.*s",int(name.size()),name.data());
  auto sym = vm.find_symbol_by_name(buf);
  assert(sym != nullptr);
  sym->set_instance(npc.handle());
  }

void GameScript::setInstanceItem(Npc &holder, size_t itemId) {
  storeItem(holder.getItem(itemId));
  }

AiOuputPipe *GameScript::openAiOuput() {
  return aiDefaultPipe.get();
  }

AiOuputPipe *GameScript::openDlgOuput(Npc &player, Npc &npc) {
  if(player.isPlayer())
    return owner.openDlgOuput(player,npc);
  return owner.openDlgOuput(npc,player);
  }

ScriptFn GameScript::playerPercAssessMagic() {
  auto id = vm.find_symbol_by_name("PLAYER_PERC_ASSESSMAGIC");
  if(id==nullptr)
    return ScriptFn();

  if(id->count()>0)
    return ScriptFn(id->get_int());
  return ScriptFn();
  }

int GameScript::npcDamDiveTime() {
  auto id = vm.find_symbol_by_name("NPC_DAM_DIVE_TIME");
  if(id==nullptr)
    return 0;
  return id->get_int();
  }

bool GameScript::game_initgerman() {
  return true;
  }

bool GameScript::game_initenglish() {
  return true;
  }

void GameScript::wld_settime(int hour, int minute) {
  world().setDayTime(hour,minute);
  }

int GameScript::wld_getday() {
  return owner.time().day();
  }

void GameScript::wld_playeffect(std::string_view visual, std::shared_ptr<phoenix::daedalus::instance> sourceId, std::shared_ptr<phoenix::daedalus::instance> targetId, int effectLevel, int damage, int damageType, int isProjectile) {
  if(isProjectile!=0 || damageType!=0 || damage!=0 || effectLevel!=0) {
    // TODO
    Log::i("effect not implemented [",visual.data(),"]");
    return;
    }
  const VisualFx* vfx = Gothic::inst().loadVisualFx(visual);
  if(vfx==nullptr) {
    Log::i("invalid effect [",visual.data(),"]");
    return;
    }

  auto dstNpc = getNpcById(targetId->symbol_index());
  auto srcNpc = getNpcById(sourceId->symbol_index());

  auto dstItm = getItemById(targetId->symbol_index());
  auto srcItm = getItemById(sourceId->symbol_index());

  if(srcNpc!=nullptr && dstNpc!=nullptr) {
    srcNpc->startEffect(*dstNpc,*vfx);
    } else
  if(srcItm!=nullptr && dstItm!=nullptr){
    Effect e(*vfx,world(),srcItm->position());
    e.setActive(true);
    world().runEffect(std::move(e));
    }
  }

void GameScript::wld_stopeffect(std::string_view visual) {
  const VisualFx*          vfx    = Gothic::inst().loadVisualFx(visual);
  if(vfx==nullptr) {
    Log::i("invalid effect [",visual.data(),"]");
    return;
    }
  if(auto w = owner.world())
    w->stopEffect(*vfx);
  }

int GameScript::wld_getplayerportalguild() {
  int32_t g = GIL_NONE;
  if(auto p = world().player())
    g = world().guildOfRoom(p->portalName());
  return g;
  }

int GameScript::wld_getformerplayerportalguild() {
  int32_t g = GIL_NONE;
  if(auto p = world().player())
    g = world().guildOfRoom(p->formerPortalName());
  return g;
  }

void GameScript::wld_setguildattitude(int gil1, int att, int gil2) {
  if(gilCount==0 || gil1>=int(gilCount) || gil2>=int(gilCount))
    return;
  gilAttitudes[gil1*gilCount+gil2]=att;
  gilAttitudes[gil2*gilCount+gil1]=att;
  }

int GameScript::wld_getguildattitude(int g1, int g0) {
  if(g0<0 || g1<0 || gilCount==0) {
    return ATT_HOSTILE; // error
    }

  auto selfG = std::min(gilCount-1,size_t(g0));
  auto npcG  = std::min(gilCount-1,size_t(g1));
  auto ret   = gilAttitudes[selfG*gilCount+npcG];
  return ret;
  }

bool GameScript::wld_istime(int hour0, int min0, int hour1, int min1) {
  gtime begin{hour0,min0}, end{hour1,min1};
  gtime now = owner.time();
  now = gtime(0,now.hour(),now.minute());

  if(begin<=end && begin<=now && now<end)
    return true;
  else if(end<begin && (now<end || begin<=now))
    return true;
  else
    return 0;
  }

bool GameScript::wld_isfpavailable(std::shared_ptr<phoenix::daedalus::c_npc> self, std::string_view name) {
  if(self==nullptr){
    return false;
    }

  auto wp = world().findFreePoint(*getNpc(self.get()),name);
  return wp != nullptr;
  }

bool GameScript::wld_isnextfpavailable(std::shared_ptr<phoenix::daedalus::c_npc> self, std::string_view name) {
  if(self==nullptr){
    return false;
    }
  auto fp = world().findNextFreePoint(*getNpc(self.get()),name);
  return fp != nullptr;
  }

bool GameScript::wld_ismobavailable(std::shared_ptr<phoenix::daedalus::c_npc> self, std::string_view name) {
  if(self==nullptr){
    return false;
    }

  auto wp = world().aviableMob(*getNpc(self.get()),name.data());
  return wp != nullptr;
  }

void GameScript::wld_setmobroutine(int h, int m, std::string_view name, int st) {
  world().setMobRoutine(gtime(h,m), name, st);
  }

int GameScript::wld_getmobstate(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, std::string_view scheme) {
  auto npc = getNpc(npcRef);

  if(npc==nullptr) {
    return -1;
    }

  auto mob = npc->detectedMob();
  if(mob==nullptr || mob->schemeName()!=scheme) {
    return -1;
    }

  return std::max(0,mob->stateId());
  }

void GameScript::wld_assignroomtoguild(std::string_view name, int g) {
  world().assignRoomToGuild(name,g);
  }

bool GameScript::wld_detectnpc(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, int inst, int state, int guild) {
  auto npc = getNpc(npcRef);
  if(npc==nullptr) {
    return false;
    }

  Npc*  ret =nullptr;
  float dist=std::numeric_limits<float>::max();

  world().detectNpc(npc->position(), float(npc->handle()->senses_range), [inst,state,guild,&ret,&dist,npc](Npc& n){
    if((inst ==-1 || int32_t(n.instanceSymbol())==inst) &&
       (state==-1 || n.isState(uint32_t(state))) &&
       (guild==-1 || int32_t(n.guild())==guild) &&
       (&n!=npc) && !n.isDead()) {
      float d = n.qDistTo(*npc);
      if(d<dist){
        ret = &n;
        dist = d;
        }
      }
    });
  if(ret)
    vm.global_other()->set_instance(ret->handle());
  return ret != nullptr;
  }

bool GameScript::wld_detectnpcex(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, int inst, int state, int guild, int player) {
  auto npc = getNpc(npcRef);
  if(npc==nullptr) {
    return false;
    }
  Npc*  ret =nullptr;
  float dist=std::numeric_limits<float>::max();

  world().detectNpc(npc->position(), float(npc->handle()->senses_range), [inst,state,guild,&ret,&dist,npc,player](Npc& n){
    if((inst ==-1 || int32_t(n.instanceSymbol())==inst) &&
       (state==-1 || n.isState(uint32_t(state))) &&
       (guild==-1 || int32_t(n.guild())==guild) &&
       (&n!=npc) && !n.isDead() &&
       (player!=0 || !n.isPlayer())) {
      float d = n.qDistTo(*npc);
      if(d<dist){
        ret = &n;
        dist = d;
        }
      }
    });
  if(ret)
    vm.global_other()->set_instance(ret->handle());
  return ret != nullptr;
  }

bool GameScript::wld_detectitem(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, int flags) {
  auto npc = getNpc(npcRef);
  if(npc==nullptr) {
    return false;
    }

  Item* ret =nullptr;
  float dist=std::numeric_limits<float>::max();
  world().detectItem(npc->position(), float(npc->handle()->senses_range), [npc,&ret,&dist,flags](Item& it) {
    if((it.handle()->main_flag&flags)==0)
      return;
    float d = (npc->position()-it.position()).quadLength();
    if(d<dist) {
      ret = &it;
      dist= d;
      }
    });

  if(ret)
    vm.global_item()->set_instance(ret->handle());
  return ret != nullptr;
  }

void GameScript::wld_spawnnpcrange(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, int clsId, int count, float lifeTime) {
  auto at = getNpc(npcRef);
  if(at==nullptr || clsId<=0)
    return;

  (void)lifeTime;
  for(int32_t i=0;i<count;++i) {
    auto* npc = world().addNpc(size_t(clsId),at->position());
    fixNpcPosition(*npc,at->rotation()-90,100);
    }
  }

void GameScript::wld_sendtrigger(std::string_view triggerTarget) {
  if(triggerTarget.empty())
    return;
  auto& world = *owner.world();
  const TriggerEvent evt(std::string{triggerTarget},"",world.tickCount(),TriggerEvent::T_Trigger);
  world.triggerEvent(evt);
  }

void GameScript::wld_senduntrigger(std::string_view triggerTarget) {
  if(triggerTarget.empty())
    return;
  auto& world = *owner.world();
  const TriggerEvent evt(std::string{triggerTarget},"",world.tickCount(),TriggerEvent::T_Untrigger);
  world.triggerEvent(evt);
  }

bool GameScript::wld_israining() {
  static bool first=true;
  if(first){
    Log::e("not implemented call [wld_israining]");
    first=false;
  }
  return false;
  }

void GameScript::mdl_setvisual(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, std::string_view visual) {
  auto npc = getNpc(npcRef);
  if(npc==nullptr)
    return;
  npc->setVisual(visual);
  }

void GameScript::mdl_setvisualbody(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, std::string_view body, int bodyTexNr, int bodyTexColor, std::string_view head, int headTexNr, int teethTexNr, int armor) {
  auto npc = getNpc(npcRef);
  if(npc==nullptr)
    return;

  npc->setVisualBody(headTexNr,teethTexNr,bodyTexNr,bodyTexColor,body,head);
  if(armor>=0) {
    if(npc->itemCount(uint32_t(armor))==0)
      npc->addItem(uint32_t(armor),1);
    npc->useItem(uint32_t(armor),true);
    }
  }

void GameScript::mdl_setmodelfatness(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, float fat) {
  auto npc = getNpc(npcRef);
  if(npc!=nullptr)
    npc->setFatness(fat);
  }

void GameScript::mdl_applyoverlaymds(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, std::string_view overlayname) {
  auto npc = getNpc(npcRef);
  if(npc!=nullptr) {
    auto skelet = Resources::loadSkeleton(overlayname);
    npc->addOverlay(skelet,0);
  }
}

void GameScript::mdl_applyoverlaymdstimed(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, std::string_view overlayname, int ticks) {
  auto npc = getNpc(npcRef);
  if(npc!=nullptr && ticks>0) {
    auto skelet = Resources::loadSkeleton(overlayname);
    npc->addOverlay(skelet,uint64_t(ticks));
  }
}

void GameScript::mdl_removeoverlaymds(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, std::string_view overlayname) {
  auto npc = getNpc(npcRef);
  if(npc!=nullptr) {
    auto skelet = Resources::loadSkeleton(overlayname);
    npc->delOverlay(skelet);
  }
}

void GameScript::mdl_setmodelscale(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, float x, float y, float z) {
  auto npc = getNpc(npcRef);
  if(npcRef!=nullptr)
    npc->setScale(x,y,z);
  }

void GameScript::mdl_startfaceani(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, std::string_view ani, float intensity, float time) {
  if(npcRef!=nullptr)
    getNpc(npcRef.get())->startFaceAnim(ani,intensity,uint64_t(time*1000.f));
  }

void GameScript::mdl_applyrandomani(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, std::string_view s1, std::string_view s0) {
  (void)npcRef;
  (void)s1;
  (void)s0;

  static bool first=true;
  if(first){
    Log::e("not implemented call [mdl_applyrandomani]");
    first=false;
  }
  }

void GameScript::mdl_applyrandomanifreq(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, std::string_view s1, float f0) {
  (void)f0;
  (void)s1;
  (void)npcRef;

  static bool first=true;
  if(first){
    Log::e("not implemented call [mdl_applyrandomanifreq]");
    first=false;
  }
  }

void GameScript::mdl_applyrandomfaceani(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, std::string_view name, float timeMin, float timeMinVar, float timeMax, float timeMaxVar, float probMin) {
  (void)probMin;
  (void)timeMaxVar;
  (void)timeMax;
  (void)timeMinVar;
  (void)timeMin;
  (void)name;
  (void)npcRef;

  static bool first=true;
  if(first){
    Log::e("not implemented call [mdl_applyrandomfaceani]");
    first=false;
  }
  }

void GameScript::wld_insertnpc(int npcInstance, std::string_view spawnpoint) {
  if(spawnpoint.empty() || npcInstance<=0)
    return;

  auto npc = world().addNpc(size_t(npcInstance),spawnpoint);
  if(npc!=nullptr)
    fixNpcPosition(*npc,0,0);
  }

void GameScript::wld_insertitem(int itemInstance, std::string_view spawnpoint) {
  if(spawnpoint.empty() || itemInstance<=0)
    return;

  world().addItem(size_t(itemInstance),spawnpoint);
  }

void GameScript::npc_settofightmode(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, int weaponSymbol) {
  if(npcRef!=nullptr && weaponSymbol>=0)
    getNpc(npcRef.get())->setToFightMode(size_t(weaponSymbol));
  }

void GameScript::npc_settofistmode(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  if(npc!=nullptr)
    npc->setToFistMode();
  }

bool GameScript::npc_isinstate(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, int stateFn) {
  auto npc = getNpc(npcRef);

  if(npc!=nullptr){
    return npc->isState(stateFn);
    }
  return false;
  }

bool GameScript::npc_isinroutine(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, int stateFn) {
  auto npc = getNpc(npcRef);

  if(npc!=nullptr){
    return npc->isRoutine(stateFn);
    }
  return false;
  }

bool GameScript::npc_wasinstate(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, int stateFn) {
  auto npc = getNpc(npcRef);

  if(npc!=nullptr){
    return npc->wasInState(stateFn);
    }
  return false;
  }

int GameScript::npc_getdisttowp(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, std::string_view wpname) {
  auto npc = getNpc(npcRef);
  auto* wp     = world().findPoint(wpname);

  if(npc!=nullptr && wp!=nullptr){
    float ret = std::sqrt(npc->qDistTo(wp));
    if(ret<float(std::numeric_limits<int32_t>::max()))
      return int32_t(ret);
    else
      return std::numeric_limits<int32_t>::max();
    } else {
    return std::numeric_limits<int32_t>::max();
    }
  }

void GameScript::npc_exchangeroutine(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, std::string_view rname) {
  auto npc = getNpc(npcRef);
  if(npc!=nullptr) {
    auto& v = *npc->handle();
    char buf[256]={};
    std::snprintf(buf,sizeof(buf),"Rtn_%s_%d",rname.data(),v.id);
    size_t d = vm.find_symbol_by_name(buf)->index();
    if(d>0)
      npc->excRoutine(d);
    }
  }

bool GameScript::npc_isdead(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  return npc==nullptr || isDead(*npc);
  }

bool GameScript::npc_knowsinfo(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, int infoinstance) {
  auto npc = getNpc(npcRef);
  if(!npc){
    return false;
    }

  phoenix::daedalus::c_npc& vnpc = *npc->handle();
  return doesNpcKnowInfo(vnpc, infoinstance);
  }

void GameScript::npc_settalentskill(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, int t, int lvl) {
  auto npc = getNpc(npcRef);
  if(npc!=nullptr)
    npc->setTalentSkill(Talent(t),lvl);
  }

int GameScript::npc_gettalentskill(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, int skillId) {
  auto npc = getNpc(npcRef);
  return npc==nullptr ? 0 : npc->talentSkill(Talent(skillId));
  }

void GameScript::npc_settalentvalue(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, int t, int lvl) {
  auto npc = getNpc(npcRef);
  if(npc!=nullptr)
    npc->setTalentValue(Talent(t),lvl);
  }

int GameScript::npc_gettalentvalue(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, int skillId) {
  auto npc = getNpc(npcRef);
  return npc==nullptr ? 0 : npc->talentValue(Talent(skillId));
  }

void GameScript::npc_setrefusetalk(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, int timeSec) {
  auto npc = getNpc(npcRef);
  if(npc)
    npc->setRefuseTalk(uint64_t(std::max(timeSec*1000,0)));
  }

bool GameScript::npc_refusetalk(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  return npc && npc->isRefuseTalk();
  }

bool GameScript::npc_hasitems(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, int itemId) {
  auto npc = getNpc(npcRef);
  return npc!=nullptr ? npc->itemCount(itemId) : 0;
  }

int GameScript::npc_getinvitem(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, int itemId) {
  auto npc = getNpc(npcRef);
  auto     itm    = npc==nullptr ? nullptr : npc->getItem(itemId);
  storeItem(itm);
  if(itm!=nullptr) {
    return itm->handle()->symbol_index();
    } else {
    return -1;
    }
  }

int GameScript::npc_removeinvitem(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, int itemId) {
  auto npc = getNpc(npcRef);

  if(npc!=nullptr)
    npc->delItem(itemId,1);

  return 0;
  }

int GameScript::npc_removeinvitems(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, int itemId, int amount) {
  auto npc = getNpc(npcRef);

  if(npc!=nullptr && amount>0)
    npc->delItem(itemId,uint32_t(amount));

  return 0;
  }

int GameScript::npc_getbodystate(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);

  if(npc!=nullptr)
    return int32_t(npc->bodyState());
  else
    return int32_t(0);
  }

std::shared_ptr<phoenix::daedalus::c_npc> GameScript::npc_getlookattarget(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  return npc && npc->lookAtTarget() ? npc->lookAtTarget()->handle() : nullptr;
  }

int GameScript::npc_getdisttonpc(std::shared_ptr<phoenix::daedalus::c_npc> aRef, std::shared_ptr<phoenix::daedalus::c_npc> bRef) {
  auto a = getNpc(aRef);
  auto b = getNpc(bRef);

  if(a==nullptr || b==nullptr){
    return std::numeric_limits<int32_t>::max();
    }

  float ret = std::sqrt(a->qDistTo(*b));
  if(ret>float(std::numeric_limits<int32_t>::max()))
    return std::numeric_limits<int32_t>::max();
  else
    return int(ret);
  }

bool GameScript::npc_hasequippedarmor(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  return npc!=nullptr && npc->currentArmour()!=nullptr;
  }

void GameScript::npc_setperctime(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, float sec) {
  auto npc = getNpc(npcRef);
  if(npc)
    npc->setPerceptionTime(uint64_t(sec*1000));
  }

void GameScript::npc_percenable(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, int pr, int fn) {
  auto npc = getNpc(npcRef);
  if(npc && fn>=0)
    npc->setPerceptionEnable(PercType(pr),size_t(fn));
  }

void GameScript::npc_percdisable(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, int pr) {
  auto npc = getNpc(npcRef);
  if(npc)
    npc->setPerceptionDisable(PercType(pr));
  }

std::string GameScript::npc_getnearestwp(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  auto wp  = npc ? world().findWayPoint(npc->position()) : nullptr;
  if(wp)
    return (wp->name);
  else
    return "";
  }

void GameScript::npc_clearaiqueue(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  if(npc)
    npc->clearAiQueue();
  }

bool GameScript::npc_isplayer(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  return npc && npc->isPlayer();
  }

int GameScript::npc_getstatetime(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  if(npc)
    return int32_t(npc->stateTime()/1000);
  else
    return 0;
  }

void GameScript::npc_setstatetime(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, int val) {
  auto npc = getNpc(npcRef);
  if(npc)
    npc->setStateTime(val*1000);
  }

void GameScript::npc_changeattribute(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, int atr, int val) {
  auto npc = getNpc(npcRef);
  if(npc!=nullptr && atr>=0)
    npc->changeAttribute(Attribute(atr),val,false);
  }

bool GameScript::npc_isonfp(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, std::string_view val) {
  auto npc = getNpc(npcRef);
  if(npc==nullptr) {
    return false;
    }

  auto w = npc->currentWayPoint();
  if(w==nullptr || !MoveAlgo::isClose(npc->position(),*w) || !w->checkName(val)){
    return false;
    }

  return w->isFreePoint();
  }

int GameScript::npc_getheighttonpc(std::shared_ptr<phoenix::daedalus::c_npc> aRef, std::shared_ptr<phoenix::daedalus::c_npc> bRef) {
  auto a = getNpc(aRef);
  auto b = getNpc(bRef);
  float ret = 0;
  if(a!=nullptr && b!=nullptr)
    ret = std::abs(a->position().y - b->position().y);
  return int32_t(ret);
  }

std::shared_ptr<phoenix::daedalus::c_item> GameScript::npc_getequippedmeleeweapon(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  if(npc!=nullptr){
    return npc->currentMeleWeapon()->handle();
    }
  return nullptr;
  }

std::shared_ptr<phoenix::daedalus::c_item> GameScript::npc_getequippedrangedweapon(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  if(npc!=nullptr){
    return npc->currentRangeWeapon()->handle();
    }
  return nullptr;
  }

std::shared_ptr<phoenix::daedalus::c_item> GameScript::npc_getequippedarmor(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  if(npc!=nullptr && npc->currentArmour()!=nullptr){
    return npc->currentArmour()->handle();
    }
  return nullptr;
  }

bool GameScript::npc_canseenpc(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, std::shared_ptr<phoenix::daedalus::c_npc> otherRef) {
  auto other = getNpc(otherRef);
  auto npc   = getNpc(npcRef);
  bool ret   = false;
  if(npc!=nullptr && other!=nullptr){
    ret = npc->canSeeNpc(*other,false);
    }
  return ret;
  }

bool GameScript::npc_hasequippedweapon(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  return (npc!=nullptr &&
     (npc->currentMeleWeapon()!=nullptr ||
      npc->currentRangeWeapon()!=nullptr));
  }

bool GameScript::npc_hasequippedmeleeweapon(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  return npc!=nullptr && npc->currentMeleWeapon()!=nullptr;
  }

bool GameScript::npc_hasequippedrangedweapon(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  return npc!=nullptr && npc->currentRangeWeapon()!=nullptr;
  }

int GameScript::npc_getactivespell(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  if(npc==nullptr){
    return -1;
    }

  Item* w = npc->activeWeapon();
  if(w==nullptr || !w->isSpellOrRune()){
    return -1;
    }

  makeCurrent(w);
  return w->spellId();
  }

bool GameScript::npc_getactivespellisscroll(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  if(npc==nullptr){
    return false;
    }

  Item* w = npc->activeWeapon();
  if(w==nullptr || !w->isSpell()){
    return false;
    }

  return true;
  }

bool GameScript::npc_canseenpcfreelos(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, std::shared_ptr<phoenix::daedalus::c_npc> otherRef) {
  auto npc = getNpc(npcRef);
  auto oth = getNpc(otherRef);

  if(npc!=nullptr && oth!=nullptr){
    return npc->canSeeNpc(*oth,true);
    }
  return false;
  }

bool GameScript::npc_isinfightmode(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, int modeI) {
  auto npc = getNpc(npcRef);
  auto mode = FightMode(modeI);

  if(npc==nullptr){
    return false;
    }

  auto st  = npc->weaponState();
  bool ret = false;
  if(mode==FightMode::FMODE_NONE){
    ret = (st==WeaponState::NoWeapon);
    }
  else if(mode==FightMode::FMODE_FIST){
    ret = (st==WeaponState::Fist);
    }
  else if(mode==FightMode::FMODE_MELEE){
    ret = (st==WeaponState::W1H || st==WeaponState::W2H);
    }
  else if(mode==FightMode::FMODE_FAR){
    ret = (st==WeaponState::Bow || st==WeaponState::CBow);
    }
  else if(mode==FightMode::FMODE_MAGIC){
    ret = (st==WeaponState::Mage);
    }
  return ret;
  }

void GameScript::npc_settarget(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, std::shared_ptr<phoenix::daedalus::c_npc> otherRef) {
  auto oth = getNpc(otherRef);
  auto npc = getNpc(npcRef);
  if(npc)
    npc->setTarget(oth);
  }

/**
 * @brief WorldScript::npc_gettarget
 * Fill 'other' with the current npc target.
 * set by Npc_SetTarget () or Npc_GetNextTarget ().
 * - return: current target saved -> TRUE
 * no target saved -> FALSE
 */
bool GameScript::npc_gettarget(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  auto s = vm.global_other();

  if(npc!=nullptr && npc->target()) {
    s->set_instance(npc->target()->handle());
    return true;
    } else {
    s->set_instance(nullptr);
    return false;
    }
  }

bool GameScript::npc_getnexttarget(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  Npc* ret = nullptr;

  if(npc!=nullptr){
    float dist = float(npc->handle()->senses_range);
    dist*=dist;

    world().detectNpc(npc->position(),float(npc->handle()->senses_range),[&,npc](Npc& oth){
      if(&oth!=npc && !oth.isDown() && oth.isEnemy(*npc) && npc->canSeeNpc(oth,true)){
        float qd = oth.qDistTo(*npc);
        if(qd<dist){
          dist=qd;
          ret = &oth;
          }
        }
      return false;
      });
    if(ret!=nullptr)
      npc->setTarget(ret);
    }

  auto s = vm.global_other();
  if(ret!=nullptr) {
    s->set_instance(ret->handle());
    return true;
    } else {
    s->set_instance(nullptr);
    return false;
    }
  }

void GameScript::npc_sendpassiveperc(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, int id, std::shared_ptr<phoenix::daedalus::c_npc> victimRef, std::shared_ptr<phoenix::daedalus::c_npc> otherRef) {
  auto other  = getNpc(otherRef);
  auto victum = getNpc(victimRef);
  auto npc = getNpc(npcRef);

  if(npc && other && victum)
    world().sendPassivePerc(*npc,*other,*victum,id);
  }

bool GameScript::npc_checkinfo(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, int imp) {
  auto n = getNpc(npcRef);
  if(n==nullptr){
    return false;
    }

  auto* hero = vm.global_other();
  if(!hero->is_instance_of<phoenix::daedalus::c_npc>()){
    return false;
    }
  auto* hpl  = reinterpret_cast<phoenix::daedalus::c_npc*>(hero->get_instance().get());
  auto& pl   = *(hpl);
  auto& npc  = *(n->handle());

  for(auto& info:dialogsInfo) {
    if(info->npc!=int32_t(npc.symbol_index()) || info->important!=imp)
      continue;
    bool npcKnowsInfo = doesNpcKnowInfo(pl,info->symbol_index());
    if(npcKnowsInfo && !info->permanent)
      continue;
    bool valid=false;
    if(info->condition)
      valid = vm.call_function<int>(vm.find_symbol_by_index(info->condition))!=0;
    if(valid) {
      return true;
      }
    }
  return false;
  }

int GameScript::npc_getportalguild(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  int32_t g  = GIL_NONE;
  auto npc = getNpc(npcRef);
  if(npc!=nullptr)
    g = world().guildOfRoom(npc->portalName());
  return g;
  }

bool GameScript::npc_isinplayersroom(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  auto pl  = world().player();

  if(npc!=nullptr && pl!=nullptr) {
    int32_t g1 = world().guildOfRoom(pl->portalName());
    int32_t g2 = world().guildOfRoom(npc->portalName());
    if(g1==g2) {
      return true;
      }
    }
  return false;
  }

std::shared_ptr<phoenix::daedalus::c_item> GameScript::npc_getreadiedweapon(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  if(npc==nullptr) {
    return 0;
    }

  auto ret = npc->activeWeapon();
  if(ret!=nullptr) {
    makeCurrent(ret);
    return ret->handle();
    } else {
    return nullptr;
    }
  }

bool GameScript::npc_hasreadiedmeleeweapon(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  if(npc==nullptr) {
    return false;
    }
  auto ws = npc->weaponState();
  return ws==WeaponState::W1H || ws==WeaponState::W2H;
  }

int GameScript::npc_isdrawingspell(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  if(npc==nullptr){
    return 0;
    }
  auto ret = npc->activeWeapon();
  if(ret==nullptr || !ret->isSpell()){
    return 0;
    }
  makeCurrent(ret);
  return int32_t(ret->clsId());
  }

int GameScript::npc_isdrawingweapon(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  if(npc==nullptr){
    return 0;
    }

  auto ret = npc->activeWeapon();
  if(ret==nullptr || !ret->isSpell()){
    return 0;
    }
  makeCurrent(ret);
  return int32_t(ret->clsId());
  }

void GameScript::npc_perceiveall(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  (void)npcRef; // nop
  }

void GameScript::npc_stopani(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, std::string_view name) {
  auto npc = getNpc(npcRef);
  if(npc!=nullptr)
    npc->stopAnim(name);
  }

int GameScript::npc_settrueguild(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, int gil) {
  auto npc = getNpc(npcRef);
  if(npc!=nullptr)
    npc->setTrueGuild(gil);
  return 0;
  }

int GameScript::npc_gettrueguild(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  if(npc!=nullptr)
    return npc->trueGuild();
  else
    return int32_t(GIL_NONE);
  }

void GameScript::npc_clearinventory(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  if(npc!=nullptr)
    npc->clearInventory();
  }

int GameScript::npc_getattitude(std::shared_ptr<phoenix::daedalus::c_npc> aRef, std::shared_ptr<phoenix::daedalus::c_npc> bRef) {
  auto a = getNpc(aRef);
  auto b = getNpc(bRef);

  if(a!=nullptr && b!=nullptr){
    auto att=personAttitude(*a,*b);
      return att; //TODO: temp attitudes
    } else {
      return ATT_NEUTRAL;
    }
  }

int GameScript::npc_getpermattitude(std::shared_ptr<phoenix::daedalus::c_npc> aRef, std::shared_ptr<phoenix::daedalus::c_npc> bRef) {
  auto a = getNpc(aRef);
  auto b = getNpc(bRef);

  if(a!=nullptr && b!=nullptr){
    auto att=personAttitude(*a,*b);
    return att;
    } else {
    return ATT_NEUTRAL;
    }
  }

void GameScript::npc_setattitude(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, int att) {
  auto npc = getNpc(npcRef);
  if(npc!=nullptr)
    npc->setAttitude(Attitude(att));
  }

void GameScript::npc_settempattitude(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, int att) {
  auto npc = getNpc(npcRef);
  if(npc!=nullptr)
    npc->setTempAttitude(Attitude(att));
  }

bool GameScript::npc_hasbodyflag(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, int bodyflag) {
  auto npc = getNpc(npcRef);
  if(npc==nullptr){
    return false;
    }
  int32_t st = npc->bodyState()&BS_MAX_FLAGS;
  bodyflag&=BS_MAX_FLAGS;
  return bool(bodyflag&st);
  }

int GameScript::npc_getlasthitspellid(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  if(npc==nullptr){
    return 0;
    }
  return npc->lastHitSpellId();
  }

int GameScript::npc_getlasthitspellcat(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  if(npc==nullptr){
    return SPELL_GOOD;
    }
  const int id    = npc->lastHitSpellId();
  auto&     spell = spellDesc(id);
  return spell.spell_type;
  }

void GameScript::npc_playani(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, std::string_view name) {
  auto npc = getNpc(npcRef);
  if(npc!=nullptr)
    npc->playAnimByName(name,BS_NONE);
  }

bool GameScript::npc_isdetectedmobownedbynpc(std::shared_ptr<phoenix::daedalus::c_npc> usrRef, std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  auto usr = getNpc(usrRef);

  if(npc!=nullptr && usr!=nullptr && usr->interactive()!=nullptr){
    auto* inst = vm.find_symbol_by_index(npc->instanceSymbol());
    auto  ow   = usr->interactive()->ownerName();
    return inst->name() == ow;
    }
  return false;
  }

bool GameScript::npc_isdetectedmobownedbyguild(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, int guild) {
  static bool first=true;
  if(first){
    Log::e("not implemented call [npc_isdetectedmobownedbyguild]");
    first=false;
  }

  auto npc = getNpc(npcRef);
  (void)guild;

  if(npc!=nullptr && npc->detectedMob()!=nullptr) {
    auto  ow   = npc->detectedMob()->ownerName();
    (void)ow;
    //vm.setReturn(inst.name==ow ? 1 : 0);
    return false;
    }
  return false;
  }

std::string GameScript::npc_getdetectedmob(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto usr = getNpc(npcRef);
  if(usr!=nullptr && usr->detectedMob()!=nullptr){
    auto i = usr->detectedMob();
    return std::string(i->schemeName());
    }
  return "";
  }

bool GameScript::npc_ownedbynpc(std::shared_ptr<phoenix::daedalus::c_item> itmRef, std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  auto itm = getItem(itmRef.get());
  if(itm==nullptr || npc==nullptr) {
    return false;
    }

  auto* sym = vm.find_symbol_by_index(itm->handle()->owner);
  return npc->handle()==sym->get_instance();
  }

bool GameScript::npc_canseesource(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto self = getNpc(npcRef);
  if(self!=nullptr) {
    bool ret = owner.world()->sound()->canSeeSource(self->position()+Vec3(0,self->translateY(),0));
    return ret;
    } else {
    return false;
    }
  }

int GameScript::npc_getdisttoitem(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, std::shared_ptr<phoenix::daedalus::c_item> itmRef) {
  auto itm = getItem(itmRef.get());
  auto npc = getNpc(npcRef);
  if(itm==nullptr || npc==nullptr) {
    return std::numeric_limits<int32_t>::max();
    }
  auto dp = itm->position()-npc->position();
  return int32_t(dp.length());
  }

int GameScript::npc_getheighttoitem(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, std::shared_ptr<phoenix::daedalus::c_item> itmRef) {
  auto itm = getItem(itmRef.get());
  auto npc = getNpc(npcRef);
  if(itm==nullptr || npc==nullptr) {
    return std::numeric_limits<int32_t>::max();
    }
  auto dp = int32_t(itm->position().y-npc->position().y);
  return std::abs(dp);
  }

int GameScript::npc_getdisttoplayer(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto pl  = world().player();
  auto npc = getNpc(npcRef);
  if(pl==nullptr || npc==nullptr) {
    return std::numeric_limits<int32_t>::max();
    }
  auto dp = pl->position()-npc->position();
  auto l  = dp.length();
  if(l>float(std::numeric_limits<int32_t>::max())) {
    return std::numeric_limits<int32_t>::max();
    }
  return int32_t(l);
  }

int GameScript::npc_getactivespellcat(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  if(npc==nullptr){
    return SPELL_GOOD;
    }

  const Item* w = npc->activeWeapon();
  if(w==nullptr || !w->isSpellOrRune()){
    return SPELL_GOOD;
    }

  const int id    = w->spellId();
  auto&     spell = spellDesc(id);
  return spell.spell_type;
  }

int GameScript::npc_setactivespellinfo(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, int v) {
  auto npc = getNpc(npcRef);
  if(npc!=nullptr)
    npc->setActiveSpellInfo(v);
  return 0;
  }

int GameScript::npc_getactivespelllevel(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  int  v   = 0;
  auto npc = getNpc(npcRef);
  if(npc!=nullptr)
    v = npc->activeSpellLevel();
  return v;
  }

void GameScript::ai_processinfos(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  auto pl  = owner.player();
  if(pl!=nullptr && npc!=nullptr) {
    aiOutOrderId=0;
    npc->aiPush(AiQueue::aiProcessInfo(*pl));
    }
  }

void GameScript::ai_output(std::shared_ptr<phoenix::daedalus::c_npc> selfRef, std::shared_ptr<phoenix::daedalus::c_npc> targetRef, std::string_view outputname) {
  auto target = getNpc(targetRef);
  auto self = getNpc(selfRef);

  if(!self || !target)
    return;

  self->aiPush(AiQueue::aiOutput(*target,outputname,aiOutOrderId));
  ++aiOutOrderId;
  }

void GameScript::ai_stopprocessinfos(std::shared_ptr<phoenix::daedalus::c_npc> selfRef) {
  auto self = getNpc(selfRef);
  if(self)
    self->aiPush(AiQueue::aiStopProcessInfo());
  }

void GameScript::ai_standup(std::shared_ptr<phoenix::daedalus::c_npc> selfRef) {
  auto self = getNpc(selfRef);
  if(self!=nullptr)
    self->aiPush(AiQueue::aiStandup());
  }

void GameScript::ai_standupquick(std::shared_ptr<phoenix::daedalus::c_npc> selfRef) {
  auto self = getNpc(selfRef);
  if(self!=nullptr)
    self->aiPush(AiQueue::aiStandupQuick());
  }

void GameScript::ai_continueroutine(std::shared_ptr<phoenix::daedalus::c_npc> selfRef) {
  auto self = getNpc(selfRef);
  if(self!=nullptr)
    self->aiPush(AiQueue::aiContinueRoutine());
  }

void GameScript::ai_stoplookat(std::shared_ptr<phoenix::daedalus::c_npc> selfRef) {
  auto self = getNpc(selfRef);
  if(self!=nullptr)
    self->aiPush(AiQueue::aiStopLookAt());
  }

void GameScript::ai_lookatnpc(std::shared_ptr<phoenix::daedalus::c_npc> selfRef, std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  auto self = getNpc(selfRef);
  if(self!=nullptr)
    self->aiPush(AiQueue::aiLookAt(npc));
  }

void GameScript::ai_removeweapon(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  if(npc!=nullptr)
    npc->aiPush(AiQueue::aiRemoveWeapon());
  }

void GameScript::ai_turntonpc(std::shared_ptr<phoenix::daedalus::c_npc> selfRef, std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  auto self = getNpc(selfRef);
  if(self!=nullptr)
    self->aiPush(AiQueue::aiTurnToNpc(npc));
  }

void GameScript::ai_outputsvm(std::shared_ptr<phoenix::daedalus::c_npc> selfRef, std::shared_ptr<phoenix::daedalus::c_npc> targetRef, std::string_view name) {
  auto target = getNpc(targetRef);

  auto self = getNpc(selfRef);
  if(self!=nullptr && target!=nullptr) {
    self->aiPush(AiQueue::aiOutputSvm(*target,name,aiOutOrderId));
    ++aiOutOrderId;
    }
  }

void GameScript::ai_outputsvm_overlay(std::shared_ptr<phoenix::daedalus::c_npc> selfRef, std::shared_ptr<phoenix::daedalus::c_npc> targetRef, std::string_view name) {
  auto target = getNpc(targetRef);
  auto self = getNpc(selfRef);
  if(self!=nullptr && target!=nullptr) {
    self->aiPush(AiQueue::aiOutputSvmOverlay(*target,name,aiOutOrderId));
    ++aiOutOrderId;
    }
  }

void GameScript::ai_startstate(std::shared_ptr<phoenix::daedalus::c_npc> selfRef, int func, int state, std::string_view wp) {
  auto self = getNpc(selfRef);
  auto* sOth = vm.global_other();
  auto* sVic = vm.global_victim();
  if(self!=nullptr && func>0) {
    Npc* oth = nullptr;
    Npc* vic = nullptr;
    if(sOth->is_instance_of<phoenix::daedalus::c_npc>()){
      auto npc = reinterpret_cast<phoenix::daedalus::c_npc*>(sOth->get_instance().get());
      if(npc)
        oth = reinterpret_cast<Npc*>(npc->user_ptr);
      }
    if(sVic->is_instance_of<phoenix::daedalus::c_npc>()){
      auto npc = reinterpret_cast<phoenix::daedalus::c_npc*>(sVic->get_instance().get());
      if(npc)
        vic = reinterpret_cast<Npc*>(npc->user_ptr);
      }

    if(!self->isInState(ScriptFn()) && self->isPlayer()) {
      // avoid issue with B_StopMagicFreeze
      self->aiPush(AiQueue::aiStandup());
      return;
      }

    auto& st = aiState(size_t(func));
    self->aiPush(AiQueue::aiStartState(st.funcIni,state,oth,vic,wp));
    }
  }

void GameScript::ai_playani(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, std::string_view name) {
  auto npc = getNpc(npcRef);
  if(npc!=nullptr) {
    npc->aiPush(AiQueue::aiPlayAnim(name));
    }
  }

void GameScript::ai_setwalkmode(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, int modeBits) {
  int32_t weaponBit = 0x80;
  auto npc = getNpc(npcRef);

  int32_t mode = modeBits & (~weaponBit);
  if(npc!=nullptr && mode>=0 && mode<=3){ //TODO: weapon flags
    npc->aiPush(AiQueue::aiSetWalkMode(WalkBit(mode)));
    }
  }

void GameScript::ai_wait(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, float ms) {
  auto npc = getNpc(npcRef);
  if(npc!=nullptr && ms>0)
    npc->aiPush(AiQueue::aiWait(uint64_t(ms*1000)));
  }

void GameScript::ai_waitms(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, int ms) {
  auto npc = getNpc(npcRef);
  if(npc!=nullptr && ms>0)
    npc->aiPush(AiQueue::aiWait(uint64_t(ms)));
  }

void GameScript::ai_aligntowp(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  if(npc)
    npc->aiPush(AiQueue::aiAlignToWp());
  }

void GameScript::ai_gotowp(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, std::string_view waypoint) {
  auto npc = getNpc(npcRef);

  auto to = world().findPoint(waypoint);
  if(npc && to)
    npc->aiPush(AiQueue::aiGoToPoint(*to));
  }

void GameScript::ai_gotofp(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, std::string_view waypoint) {
  auto npc = getNpc(npcRef);

  if(npc) {
    auto to = world().findFreePoint(*npc,waypoint);
    if(to!=nullptr)
      npc->aiPush(AiQueue::aiGoToPoint(*to));
    }
  }

void GameScript::ai_playanibs(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, std::string_view ani, int bs) {
  auto npc = getNpc(npcRef);
  if(npc!=nullptr)
    npc->aiPush(AiQueue::aiPlayAnimBs(ani,BodyState(bs)));
  }

void GameScript::ai_equiparmor(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, int id) {
  auto npc = getNpc(npcRef);
  if(npc!=nullptr)
    npc->aiPush(AiQueue::aiEquipArmor(id));
  }

void GameScript::ai_equipbestarmor(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  if(npc!=nullptr)
    npc->aiPush(AiQueue::aiEquipBestArmor());
  }

int GameScript::ai_equipbestmeleeweapon(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  if(npc!=nullptr)
    npc->aiPush(AiQueue::aiEquipBestMeleWeapon());
  return 0;
  }

int GameScript::ai_equipbestrangedweapon(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  if(npc!=nullptr)
    npc->aiPush(AiQueue::aiEquipBestRangeWeapon());
  return 0;
  }

bool GameScript::ai_usemob(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, std::string_view tg, int state) {
  auto npc = getNpc(npcRef);
  if(npc!=nullptr)
    npc->aiPush(AiQueue::aiUseMob(tg,state));
  return 0;
  }

void GameScript::ai_teleport(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, std::string_view tg) {
  auto npc = getNpc(npcRef);
  auto     pt  = world().findPoint(tg);
  if(npc!=nullptr && pt!=nullptr)
    npc->aiPush(AiQueue::aiTeleport(*pt));
  }

void GameScript::ai_stoppointat(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  if(npc!=nullptr)
    npc->aiPush(AiQueue::aiStopPointAt());
  }

void GameScript::ai_drawweapon(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  if(npc!=nullptr)
    npc->aiPush(AiQueue::aiDrawWeapon());
  }

void GameScript::ai_readymeleeweapon(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  if(npc!=nullptr)
    npc->aiPush(AiQueue::aiReadyMeleWeapon());
  }

void GameScript::ai_readyrangedweapon(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  if(npc!=nullptr)
    npc->aiPush(AiQueue::aiReadyRangeWeapon());
  }

void GameScript::ai_readyspell(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, int spell, int mana) {
  auto npc = getNpc(npcRef);
  if(npc!=nullptr)
    npc->aiPush(AiQueue::aiReadySpell(spell,mana));
  }

void GameScript::ai_atack(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  if(npc!=nullptr)
    npc->aiPush(AiQueue::aiAtack());
  }

void GameScript::ai_flee(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  if(npc!=nullptr)
    npc->aiPush(AiQueue::aiFlee());
  }

void GameScript::ai_dodge(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  if(npc!=nullptr)
    npc->aiPush(AiQueue::aiDodge());
  }

void GameScript::ai_unequipweapons(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  if(npc!=nullptr)
    npc->aiPush(AiQueue::aiUnEquipWeapons());
  }

void GameScript::ai_unequiparmor(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  if(npc!=nullptr)
    npc->aiPush(AiQueue::aiUnEquipArmor());
  }

void GameScript::ai_gotonpc(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, std::shared_ptr<phoenix::daedalus::c_npc> toRef) {
  auto to  = getNpc(toRef);
  auto npc = getNpc(npcRef);
  if(npc!=nullptr)
    npc->aiPush(AiQueue::aiGoToNpc(to));
  }

void GameScript::ai_gotonextfp(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, std::string_view to) {
  auto npc = getNpc(npcRef);
  if(npc!=nullptr)
    npc->aiPush(AiQueue::aiGoToNextFp(to));
  }

void GameScript::ai_aligntofp(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto npc = getNpc(npcRef);
  if(npc!=nullptr)
    npc->aiPush(AiQueue::aiAlignToFp());
  }

void GameScript::ai_useitem(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, int item) {
  auto npc = getNpc(npcRef);
  if(npc)
    npc->aiPush(AiQueue::aiUseItem(item));
  }

void GameScript::ai_useitemtostate(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, int item, int state) {
  auto npc = getNpc(npcRef);
  if(npc)
    npc->aiPush(AiQueue::aiUseItemToState(item,state));
  }

void GameScript::ai_setnpcstostate(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, int state, int radius) {
  auto npc = getNpc(npcRef);
  if(npc && state>0)
    npc->aiPush(AiQueue::aiSetNpcsToState(size_t(state),radius));
  }

void GameScript::ai_finishingmove(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, std::shared_ptr<phoenix::daedalus::c_npc> othRef) {
  auto oth = getNpc(othRef);
  auto npc = getNpc(npcRef);
  if(npc!=nullptr && oth!=nullptr)
    npc->aiPush(AiQueue::aiFinishingMove(*oth));
  }

void GameScript::ai_takeitem(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, std::shared_ptr<phoenix::daedalus::c_item> itmRef) {
  auto itm = getItem(itmRef.get());
  auto npc = getNpc(npcRef);
  if(npc!=nullptr && itm!=nullptr)
    npc->aiPush(AiQueue::aiTakeItem(*itm));
  }

void GameScript::ai_gotoitem(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, std::shared_ptr<phoenix::daedalus::c_item> itmRef) {
  auto itm = getItem(itmRef.get());
  auto npc = getNpc(npcRef);
  if(npc!=nullptr && itm!=nullptr)
    npc->aiPush(AiQueue::aiGotoItem(*itm));
  }

void GameScript::ai_pointat(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, std::string_view waypoint) {
  auto npc = getNpc(npcRef);

  auto to       = world().findPoint(waypoint);
  if(npc!=nullptr && to!=nullptr)
    npc->aiPush(AiQueue::aiPointAt(*to));
  }

void GameScript::ai_pointatnpc(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, std::shared_ptr<phoenix::daedalus::c_npc> otherRef) {
  auto other = getNpc(otherRef);
  auto npc = getNpc(npcRef);
  if(npc!=nullptr && other!=nullptr)
    npc->aiPush(AiQueue::aiPointAtNpc(*other));
  }

int GameScript::mob_hasitems(std::string_view tag, int item) {
  return int(world().hasItems(tag,item));
  }

void GameScript::ta_min(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, int start_h, int start_m, int stop_h, int stop_m, int action, std::string_view waypoint) {
  auto npc = getNpc(npcRef);
  auto at  = world().findPoint(waypoint);

  if(npc!=nullptr)
    npc->addRoutine(gtime(start_h,start_m),gtime(stop_h,stop_m),uint32_t(action),at);
  }

void GameScript::log_createtopic(std::string_view topicName, int section) {
  if(section==QuestLog::Mission || section==QuestLog::Note)
    quests.add(topicName,QuestLog::Section(section));
  }

void GameScript::log_settopicstatus(std::string_view topicName, int status) {
  if(status==int32_t(QuestLog::Status::Running) ||
     status==int32_t(QuestLog::Status::Success) ||
     status==int32_t(QuestLog::Status::Failed ) ||
     status==int32_t(QuestLog::Status::Obsolete))
    quests.setStatus(topicName,QuestLog::Status(status));
  }

void GameScript::log_addentry(std::string_view topicName, std::string_view entry) {
  quests.addEntry(topicName,entry);
  }

void GameScript::equipitem(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, int cls) {
  auto self = getNpc(npcRef);
  if(self!=nullptr) {
    if(self->itemCount(cls)==0)
      self->addItem(cls,1);
    self->useItem(cls,true);
    }
  }

void GameScript::createinvitem(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, int itemInstance) {
  auto self = getNpc(npcRef);
  if(self!=nullptr) {
    Item* itm = self->addItem(itemInstance,1);
    storeItem(itm);
    }
  }

void GameScript::createinvitems(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, int itemInstance, int amount) {
  auto self = getNpc(npcRef);
  if(self!=nullptr && amount>0) {
    Item* itm = self->addItem(itemInstance,size_t(amount));
    storeItem(itm);
    }
  }

int GameScript::hlp_getinstanceid(std::shared_ptr<phoenix::daedalus::instance> instance) {
  // Log::d("hlp_getinstanceid: name \"",handle.name,"\" not found");
  return instance == nullptr ? -1 : instance->symbol_index();
  }

int GameScript::hlp_random(int bound) {
  uint32_t mod = uint32_t(std::max(1,bound));
  return int32_t(randGen() % mod);
  }

bool GameScript::hlp_isvalidnpc(std::shared_ptr<phoenix::daedalus::c_npc> npcRef) {
  auto self = getNpc(npcRef);
  return self != nullptr;
  }

bool GameScript::hlp_isitem(std::shared_ptr<phoenix::daedalus::c_item> itemRef, int instanceSymbol) {
  auto item = getItem(itemRef.get());
  if(item!=nullptr){
    auto& v = item->handle();
    return int(v->symbol_index()) == instanceSymbol;
    } else {
      return false;
    }
  }

bool GameScript::hlp_isvaliditem(std::shared_ptr<phoenix::daedalus::c_item> itemRef) {
  auto item = getItem(itemRef.get());
  return item!=nullptr;
  }

std::shared_ptr<phoenix::daedalus::c_npc> GameScript::hlp_getnpc(int instanceSymbol) {
  auto npc = getNpcById(instanceSymbol);
  if(npc != nullptr)
    return npc->handle();
  else
    return nullptr;
  }

void GameScript::info_addchoice(int infoInstance, std::string_view text, int func) {
  auto info = getInfo(infoInstance);
  if(info==nullptr)
    return;
  phoenix::daedalus::c_info_choice choice {};
  choice.text     = text;
  choice.function = func;
  info->add_choice(choice);
  }

void GameScript::info_clearchoices(int infoInstance) {
  auto info = getInfo(infoInstance);
  if(info==nullptr)
    return;
  info->choices.clear();
  }

bool GameScript::infomanager_hasfinished() {
  return owner.aiIsDlgFinished();
  }

void GameScript::snd_play(std::string_view fileS) {
  std::string file {fileS};
  for(auto& c:file)
    c = char(std::toupper(c));
  Gothic::inst().emitGlobalSound(file);
  }

void GameScript::snd_play3d(std::shared_ptr<phoenix::daedalus::c_npc> npcRef, std::string_view fileS) {
  std::string file {fileS};
  Npc*        npc  = getNpc(npcRef);
  if(npc==nullptr)
    return;
  for(auto& c:file)
    c = char(std::toupper(c));
  auto sfx = ::Sound(*owner.world(),::Sound::T_3D,file,npc->position(),0.f,false);
  sfx.play();
  owner.world()->sendPassivePerc(*npc,*npc,*npc,PERC_ASSESSQUIETSOUND);
  }

void GameScript::exitsession() {
  owner.exitSession();
  }

void GameScript::sort(std::vector<GameScript::DlgChoise> &dlg) {
  std::sort(dlg.begin(),dlg.end(),[](const GameScript::DlgChoise& l,const GameScript::DlgChoise& r){
    return std::tie(l.sort,l.scriptFn)<std::tie(r.sort,r.scriptFn); // small hack with scriptfn to reproduce behavior of original game
    });
  }

void GameScript::setNpcInfoKnown(const phoenix::daedalus::c_npc& npc, const phoenix::daedalus::c_info &info) {
  auto id = std::make_pair(vm.find_symbol_by_instance(npc)->index(),vm.find_symbol_by_instance(info)->index());
  dlgKnownInfos.insert(id);
  }

bool GameScript::doesNpcKnowInfo(const phoenix::daedalus::c_npc& npc, size_t infoInstance) const {
  auto id = std::make_pair(vm.find_symbol_by_instance(npc)->index(),infoInstance);
  return dlgKnownInfos.find(id)!=dlgKnownInfos.end();
  }
