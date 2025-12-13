#include "marvin.h"

#include <charconv>
#include <cstdint>
#include <cctype>

#include "utils/string_frm.h"
#include "world/objects/npc.h"
#include "world/objects/item.h"
#include "world/triggers/abstracttrigger.h"
#include "camera.h"
#include "gothic.h"

static bool startsWith(std::string_view str, std::string_view needle) {
  if(needle.size()>str.size())
    return false;
  for(size_t i=0; i<needle.size(); ++i) {
    int n = std::tolower(needle[i]);
    int s = std::tolower(str   [i]);
    if(n!=s)
      return false;
    }
  return true;
  }

static bool compareNoCase(std::string_view sa, std::string_view sb) {
  if(sa.size()!=sb.size())
    return false;
  for(size_t i=0; i<sa.size(); ++i) {
    int a = std::tolower(sa[i]);
    int b = std::tolower(sb[i]);
    if(a!=b)
      return false;
    }
  return true;
  }

template<class T>
static bool fromString(std::string_view str, T& t) {
  auto err = std::from_chars(str.data(), str.data()+str.size(),t).ec;
  if(err!=std::errc())
    return false;
  return true;
  }

static bool fromString(std::string_view str, float& t) {
  t = std::stof(std::string(str));
  return true;
  }

Marvin::Marvin() {
  /* Legend:
   %c - instance variable from script
   %v - variable from script
   %s - any string
   %d - integer
   %f - floating point number
   */
  cmd = std::vector<Cmd>{
    // gdb-like commands
    {"p %v",                       C_PrintVar},
    // similar to "Marvin Helper" https://steamcommunity.com/sharedfiles/filedetails/?id=2847617433
    {"set var %v %s",              C_SetVar},

    // animation
    {"play ani %s",                C_PlayAni},
    {"play faceani %s",            C_Invalid},
    {"remove overlaymds %s",       C_Invalid},
    {"toggle aniinfo",             C_Invalid},
    {"zoverlaymds apply",          C_Invalid},
    {"zoverlaymds remove",         C_Invalid},
    {"zstartani %s %s",            C_Invalid},
    {"ztoggle modelskeleton",      C_Invalid},
    {"ztoggle rootsmoothnode",     C_Invalid},
    {"ztoggle vobmorph",           C_Invalid},

    // rendering
    {"set clipfactor %d",          C_Invalid},
    {"toggle frame",               C_ToggleFrame},
    {"zfogzone",                   C_Invalid},
    {"zhighqualityrender",         C_Invalid},
    {"zmark",                      C_Invalid},
    {"zprogmeshlod",               C_Invalid},
    {"zrmode flat",                C_Invalid},
    {"zrmode mat",                 C_Invalid},
    {"zrmode wire",                C_Invalid},
    {"zrmode wmat",                C_Invalid},
    {"zset levelclipscaler %f",    C_Invalid},
    {"zset nearclipz %f",          C_Invalid},
    {"zset vobfarclipscaler %f",   C_Invalid},
    {"ztoggle ambientvobs",        C_Invalid},
    {"ztoggle flushambientcol",    C_Invalid},
    {"ztoggle lightstat",          C_Invalid},
    {"ztoggle markpmeshmaterials", C_Invalid},
    {"ztoggle matmorph",           C_Invalid},
    {"ztoggle renderorder",        C_Invalid},
    {"ztoggle renderportals",      C_Invalid},
    {"ztoggle rendervob",          C_Invalid},
    {"ztoggle showportals",        C_Invalid},
    {"ztoggle showtraceray",       C_Invalid},
    {"ztoggle tnl",                C_Invalid},
    {"ztoggle vobbox",             C_Invalid},
    {"zvideores %d %d %d",         C_Invalid},

    // game
    {"LC1",                        C_Invalid},
    {"LC2",                        C_Invalid},
    {"load game",                  C_Invalid},
    {"save game",                  C_Invalid},
    {"save zen",                   C_Invalid},
    {"set time %d %d",             C_SetTime},
    {"spawnmass %d",               C_Invalid},
    {"spawnmass giga %d",          C_Invalid},
    {"toggle desktop",             C_ToggleDesktop},
    {"toggle freepoints",          C_Invalid},
    {"toggle screen",              C_Invalid},
    {"toggle time",                C_ToggleTime},
    {"toggle wayboxes",            C_Invalid},
    {"toggle waynet",              C_Invalid},
    {"version",                    C_Invalid},
    {"zstartrain",                 C_Invalid},
    {"zstartsnow",                 C_Invalid},
    {"ztimer multiplyer %f",       C_TimeMultiplyer},
    {"ztimer realtime",            C_TimeRealtime},
    {"ztoggle helpervisuals",      C_Invalid},
    {"ztoggle showzones",          C_Invalid},
    {"ztrigger %s",                C_ZTrigger},
    {"zuntrigger %s",              C_ZUntrigger},

    {"cheat full",                 C_CheatFull},
    {"cheat god",                  C_CheatGod},
    {"kill",                       C_Kill},

    {"aigoto %s",                  C_AiGoTo},
    {"goto waypoint %s",           C_GoToWayPoint},
    {"goto pos %f %f %f",          C_GoToPos},
    {"goto vob %c %d",             C_GoToVob},
    {"goto camera",                C_GoToCamera},

    {"camera autoswitch",          C_CamAutoswitch},
    {"camera mode",                C_CamMode},
    {"toggle camdebug",            C_ToggleCamDebug},
    {"toggle camera",              C_ToggleCamera},
    {"toggle inertiatarget",       C_ToggleInertia},
    {"ztoggle timedemo",           C_ZToggleTimeDemo},
    {"insert %c",                  C_Insert},

    {"toggle gi",                  C_ToggleGI},
    {"toggle vsm",                 C_ToggleVsm},
    {"toggle rtsm",                C_ToggleRtsm},
    };
  }

Marvin::CmdVal Marvin::isMatch(std::string_view inp, const Cmd& cmd) const {
  std::string_view ref  = cmd.cmd;
  CmdVal           ret  = C_Invalid;
  int              argc = 0;

  while(!ref.empty()) {
    size_t wr = ref.find(' ');
    if(wr==std::string_view::npos)
      wr = ref.size();
    size_t wi = inp.find(' ');
    if(wi==std::string_view::npos)
      wi = inp.size();

    auto wref = ref.substr(0,wr);
    auto winp = inp.substr(0,wi);

    bool fullword = true;
    if(wref=="%c")
      wref = completeInstanceName(winp,fullword);
    else if(wref=="%v")
      wref = completeInstanceName(winp,fullword);
    else if(wref=="%s")
      wref = winp;
    else if(wref=="%f" || wref=="%d")
      wref = winp;

    if(!compareNoCase(wref,winp)) {
      if(!startsWith(wref,winp))
        return C_Invalid;
      ret.cmd      = cmd;
      ret.cmd.type = C_Incomplete;
      ret.complete = wref.substr(winp.size());
      ret.fullword = fullword;
      return ret;
      }

    if(ref[0]=='%' && argc<4) {
      ret.argv[argc] = wref;
      ++argc;
      }

    if(ref.size()==wr) {
      if(inp.size()!=winp.size())
        return C_Extra;
      break;
      }

    if(inp.size()==wr)
      return C_Incomplete;

    ref = ref.substr(wr+1);
    inp = inp.substr(std::min(wi+1,inp.size()));
    while(inp.size()>0 && inp[0]==' ')
      inp = inp.substr(1);
    }

  ret.cmd = cmd;
  return ret;
  }

Marvin::CmdVal Marvin::recognize(std::string_view inp) {
  while(inp.size()>0 && inp.back()==' ')
    inp = inp.substr(0,inp.size()-1);
  while(inp.size()>0 && inp[0]==' ')
    inp = inp.substr(1);

  if(inp.size()==0)
    return C_None;

  CmdVal suggestion = C_Invalid;

  for(auto& i:cmd) {
    auto m = isMatch(inp,i);
    if(m.cmd.type==C_Invalid)
      continue;
    if(m.cmd.type!=C_Incomplete)
      return m;
    if(suggestion.cmd.type==C_Incomplete) {
      if(suggestion.cmd.cmd!=m.cmd.cmd && suggestion.complete!=m.complete)
        return C_Incomplete; // multiple distinct commands do match - no suggestion
      suggestion = m;
      continue;
      }
    suggestion = m;
    }

  return suggestion;
  }

bool Marvin::autoComplete(std::string& v) {
  auto ret = recognize(v);
  if(ret.cmd.type==C_Incomplete && !ret.complete.empty()) {
    for(auto& i:ret.complete)
      v.push_back(i);
    if(ret.fullword)
      v.push_back(' ');
    return true;
    }
  return false;
  }

bool Marvin::exec(std::string_view v) {
  auto ret = recognize(v);
  switch(ret.cmd.type) {
    case C_None:
      return true;
    case C_Incomplete:
    case C_Invalid:
    case C_Extra:
      return false;
    case C_CheatFull:{
      if(auto pl = Gothic::inst().player()) {
        pl->changeAttribute(ATR_HITPOINTS,pl->attribute(ATR_HITPOINTSMAX),false);
        }
      return true;
      }
    case C_CheatGod: {
      auto& fnt = Resources::font(1.0);
      if(Gothic::inst().isGodMode()) {
        Gothic::inst().setGodMode(false);
        Gothic::inst().onPrintScreen("Godmode off",2,4,1,fnt);
        } else {
        Gothic::inst().setGodMode(true);
        Gothic::inst().onPrintScreen("Godmode on",2,4,1,fnt);
        }
      return true;
      }
    case C_Kill: {
      Npc* player = Gothic::inst().player();
      if(player==nullptr || player->target()==nullptr)
        return false;
      auto target = player->target();
      target->changeAttribute(ATR_HITPOINTS,-target->attribute(ATR_HITPOINTSMAX),false);
      return true;
      }
    case C_AiGoTo: {
      World* world  = Gothic::inst().world();
      Npc*   player = Gothic::inst().player();
      if(world==nullptr || player==nullptr || !player->setInteraction(nullptr))
        return false;
      auto wpoint = world->findPoint(ret.argv[0]);
      if(wpoint==nullptr)
        return false;
      player->aiPush(AiQueue::aiGoToPoint(*wpoint));
      return true;
      }
    case C_GoToPos: {
      Npc* player = Gothic::inst().player();
      if(player==nullptr || !player->setInteraction(nullptr))
        return false;
      int c[3] = {};
      for(int i=0; i<3; ++i) {
        if(!fromString(ret.argv[i], c[i]))
          return false;
        }
      player->setPosition(float(c[0]),float(c[1]),float(c[2]));
      player->updateTransform();
      Gothic::inst().camera()->reset(player);
      return true;
      }
    case C_GoToWayPoint: {
      World* world  = Gothic::inst().world();
      Npc*   player = Gothic::inst().player();
      if(world==nullptr || player==nullptr || !player->setInteraction(nullptr))
        return false;
      auto wpoint = world->findPoint(ret.argv[0]);
      if(wpoint==nullptr)
        return false;
      player->setPosition(wpoint->position());
      player->updateTransform();
      Gothic::inst().camera()->reset(player);
      return true;
      }
    case C_GoToVob: {
      World* world  = Gothic::inst().world();
      Npc*   player = Gothic::inst().player();
      auto   c      = Gothic::inst().camera();
      if(world==nullptr || c==nullptr || player==nullptr)
        return false;
      size_t n = 1;
      if(!ret.argv[1].empty() && !fromString(ret.argv[1], n)) {
        return false;
        }
      return goToVob(*world,*player,*c,ret.argv[0],--n);
      }
    case C_GoToCamera: {
      auto c      = Gothic::inst().camera();
      Npc* player = Gothic::inst().player();
      if(c==nullptr || player==nullptr)
        return false;
      auto pos = c->destPosition();
      player->setPosition(pos.x,pos.y,pos.z);
      player->updateTransform();
      c->reset();
      return true;
      }
    case C_ToggleFrame:{
      Gothic::inst().setFRate(!Gothic::inst().doFrate());
      return true;
      }
    case C_ToggleTime:{
      Gothic::inst().setClock(!Gothic::inst().doClock());
      return true;
      }
    case C_TimeMultiplyer: {
      if(auto g = Gothic::inst().gameSession()) {
        float mul = 1;
        if(!fromString(ret.argv[0], mul))
          return false;
        mul = std::max(mul, 0.f);
        g->setTimeMultiplyer(mul);
        return true;
        }
      return false;
      }
    case C_TimeRealtime:{
      if(auto g = Gothic::inst().gameSession()) {
        g->setTimeMultiplyer(1);
        return true;
        }
      return false;
      }
    case C_CamAutoswitch:
      return true;
    case C_CamMode:
      return true;
    case C_ToggleCamDebug:
      if(auto c = Gothic::inst().camera())
        c->toggleDebug();
      return true;
    case C_ToggleCamera: {
      if(auto c = Gothic::inst().camera())
        c->setToggleEnable(!c->isToggleEnabled());
      return true;
      }
    case C_ToggleInertia: {
      if(auto c = Gothic::inst().camera())
        c->setInertiaTargetEnable(!c->isInertiaTargetEnabled());
      return true;
      }
    case C_ZToggleTimeDemo: {
      World* world = Gothic::inst().world();
      if(world==nullptr)
        return false;
      const TriggerEvent evt("TIMEDEMO","",world->tickCount(),TriggerEvent::T_Trigger);
      world->triggerEvent(evt);
      return true;
      }
    case C_ToggleDesktop: {
      Gothic::inst().toggleDesktop();
      return true;
      }
    case C_ZTrigger: {
      World* world = Gothic::inst().world();
      if(world==nullptr)
        return false;
      const TriggerEvent evt(std::string(ret.argv[0]),"",world->tickCount(),TriggerEvent::T_Trigger);
      world->triggerEvent(evt);
      return true;
      }
    case C_ZUntrigger: {
      World* world = Gothic::inst().world();
      if(world==nullptr)
        return false;
      const TriggerEvent evt(std::string(ret.argv[0]),"",world->tickCount(),TriggerEvent::T_Untrigger);
      world->triggerEvent(evt);
      return true;
      }
    case C_Insert: {
      World* world  = Gothic::inst().world();
      Npc*   player = Gothic::inst().player();
      if(world==nullptr || player==nullptr)
        return false;
      return addItemOrNpcBySymbolName(world, ret.argv[0], player->position());
      }
    case C_SetTime: {
      World* world = Gothic::inst().world();
      if(world==nullptr)
        return false;
      return setTime(*world, ret.argv[0], ret.argv[1]);
      }
    case C_PrintVar: {
      World* world  = Gothic::inst().world();
      if(world==nullptr)
        return false;
      return printVariable(world, ret.argv[0]);
      }

    case C_SetVar: {
      World* world  = Gothic::inst().world();
      Npc*   player = Gothic::inst().player();
      if(world==nullptr || player==nullptr)
        return false;
      return setVariable(world, ret.argv[0], ret.argv[1]);
      }

    case C_PlayAni: {
      Npc*   player = Gothic::inst().player();
      if(player==nullptr)
        return false;
      return player->playAnimByName(ret.argv[0], BS_NONE) != nullptr;
      }

    case C_ToggleGI:
      Gothic::inst().toggleGi();
      return true;
    case C_ToggleVsm:
      Gothic::inst().toggleVsm();
      return true;
    case C_ToggleRtsm:
      Gothic::inst().toggleRtsm();
      return true;
    }

  return true;
  }

bool Marvin::addItemOrNpcBySymbolName(World* world, std::string_view name, const Tempest::Vec3& at) {
  auto&  sc  = world->script();
  size_t id  = sc.findSymbolIndex(name);
  if(id==size_t(-1))
    return false;

  auto*  sym = sc.findSymbol(id);
  if(sym==nullptr || sym->parent()==uint32_t(-1))
    return false;

  if(sym->type()!=zenkit::DaedalusDataType::INSTANCE || sym->address()==0)
    return false;

  const auto* cls = sym;
  while(cls!=nullptr && cls->parent()!=uint32_t(-1)) {
    cls = sc.findSymbol(cls->parent());
    }

  if(cls==nullptr)
    return false;

  if(cls->name()=="C_NPC")
    return (world->addNpc(id, at)!=nullptr);
  if(cls->name()=="C_ITEM")
    return (world->addItem(id, at)!=nullptr);
  return false;
  }

bool Marvin::printVariable(World* world, std::string_view name) {
  string_frm buf;
  auto&  sc  = world->script();
  auto*  sym = sc.findSymbol(name);
  if(sym==nullptr)
    return false;
  switch(sym->type()) {
    case zenkit::DaedalusDataType::INT:
      buf = string_frm(name," = ",sym->get_int(0));
      break;
    case zenkit::DaedalusDataType::FLOAT:
      buf = string_frm(name," = ",sym->get_float(0));
      break;
    case zenkit::DaedalusDataType::STRING:
      buf = string_frm(name," = ",sym->get_string(0));
      break;
    case zenkit::DaedalusDataType::INSTANCE:
      buf = string_frm(name," = ",sym->get_instance().get());
      break;
    default:
      break;
    }
  print(buf);
  return true;
  }

bool Marvin::setVariable(World* world, std::string_view name, std::string_view value) {
  auto&  sc  = world->script();
  auto*  sym = sc.findSymbol(name);
  if(sym==nullptr)
    return false;
  switch(sym->type()) {
    case zenkit::DaedalusDataType::INT: {
      int32_t valueI = 0;
      if(!fromString(value, valueI))
        return false;
      sym->set_int(valueI);
      break;
      }
    case zenkit::DaedalusDataType::FLOAT: {
      float valueF = 0;
      if(!fromString(value, valueF))
        return false;
      sym->set_float(valueF);
      break;
      }
    case zenkit::DaedalusDataType::STRING:
      sym->set_string(value);
      break;
    case zenkit::DaedalusDataType::INSTANCE:
      print("unable to override instance variable");
      break;
    default:
      return false;
    }
  printVariable(world, name);
  return true;
  }

bool Marvin::setTime(World& world, std::string_view hh, std::string_view mm) {
  int hv = 0, mv = 0;

  auto err = std::from_chars(hh.data(), hh.data()+hh.size(), hv, 10).ec;
  if(err!=std::errc())
    return false;

  err = std::from_chars(mm.data(), mm.data()+mm.size(), mv, 10).ec;
  if(err!=std::errc()) {
    mv = 0;
    }

  if(hv<0 || hv>=24 || mv<0 || mv>=60)
    return false;

  world.setDayTime(hv,mv);
  return true;
  }

bool Marvin::goToVob(World& world, Npc& player, Camera& c, std::string_view name, size_t n) {
  auto&  sc = world.script();
  size_t id = sc.findSymbolIndex(name);
  if(id==size_t(-1))
    return false;

  Tempest::Vec3 pos;
  if(auto npc = world.findNpcByInstance(id,n))
     pos = npc->position();
  else if(auto it = world.findItemByInstance(id,n))
     pos = it->position();
  else
    return false;

  if(!player.setInteraction(nullptr))
    return false;
  player.setPosition(pos);
  player.updateTransform();
  c.reset();
  return true;
  }

std::string_view Marvin::completeInstanceName(std::string_view inp, bool& fullword) const {
  World* world  = Gothic::inst().world();
  if(world==nullptr || inp.size()==0)
    return "";

  auto&            sc    = world->script();
  std::string_view match = "";
  for(size_t i=0; i<sc.symbolsCount(); ++i) {
    auto*  sym  = sc.findSymbol(i);
    auto   name = std::string_view(sym->name());
    if(!startsWith(name,inp))
      continue;

    if(match.empty()) {
      match   = name;
      fullword = true;
      continue;
      }
    if(name.size()>match.size())
      name = name.substr(0,match.size());
    for(size_t i=0; i<name.size(); ++i) {
      int n = std::tolower(name [i]);
      int m = std::tolower(match[i]);
      if(m!=n) {
        name     = name.substr(0,i);
        fullword = false;
        break;
        }
      }
    match = name;
    }
  return match;
  }
