#include "marvin.h"

#include <charconv>
#include <initializer_list>
#include <cstdint>
#include <cctype>

#include "utils/string_frm.h"
#include "world/objects/npc.h"
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

    // animation
    {"play ani %s",                C_Invalid},
    {"play faceani %s",            C_Invalid},
    {"remove overlaymds %s",       C_Invalid},
    {"toogle aniinfo",             C_Invalid},
    {"zoverlaymds apply",          C_Invalid},
    {"zoverlaymds remove",         C_Invalid},
    {"zstartani %s %s",            C_Invalid},
    {"ztoogle modelskeleton",      C_Invalid},
    {"ztoogle rootsmoothnode",     C_Invalid},
    {"ztoogle vobmorph",           C_Invalid},

    // rendering
    {"set clipfactor %d",          C_Invalid},
    {"toogle frame",               C_ToogleFrame},
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
    {"ztoogle ambientvobs",        C_Invalid},
    {"ztoogle flushambientcol",    C_Invalid},
    {"ztoogle lightstat",          C_Invalid},
    {"ztoogle markpmeshmaterials", C_Invalid},
    {"ztoogle matmorph",           C_Invalid},
    {"ztoogle renderorder",        C_Invalid},
    {"ztoogle renderportals",      C_Invalid},
    {"ztoogle rendervob",          C_Invalid},
    {"ztoogle showportals",        C_Invalid},
    {"ztoogle showtraceray",       C_Invalid},
    {"ztoogle tnl",                C_Invalid},
    {"ztoogle vobbox",             C_Invalid},
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
    {"toogle descktop",            C_Invalid},
    {"toogle freepoints",          C_Invalid},
    {"toogle screen",              C_Invalid},
    {"toogle time",                C_Invalid},
    {"toogle wayboxes",            C_Invalid},
    {"toogle waynet",              C_Invalid},
    {"version",                    C_Invalid},
    {"zstartrain",                 C_Invalid},
    {"zstartsnow",                 C_Invalid},
    {"ztimer multiplyer",          C_Invalid},
    {"ztimer realtime",            C_Invalid},
    {"ztoogle helpervisuals",      C_Invalid},
    {"ztoogle showzones",          C_Invalid},
    {"ztrigger %s",                C_Invalid},
    {"zuntrigger %s",              C_Invalid},

    {"cheat full",        C_CheatFull},


    {"camera autoswitch", C_CamAutoswitch},
    {"camera mode",       C_CamMode},
    {"toogle camdebug",   C_ToogleCamDebug},
    {"toogle camera",     C_ToogleCamera},
    {"insert %c",         C_Insert},
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
        return C_Invalid; // extra stuff
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

void Marvin::autoComplete(std::string& v) {
  auto ret = recognize(v);
  if(ret.cmd.type==C_Incomplete && !ret.complete.empty()) {
    for(auto& i:ret.complete)
      v.push_back(i);
    if(ret.fullword)
      v.push_back(' ');
    }
  }

bool Marvin::exec(std::string_view v) {
  auto ret = recognize(v);
  switch(ret.cmd.type) {
    case C_None:
      return true;
    case C_Incomplete:
    case C_Invalid:
      return false;
    case C_CheatFull:{
      if(auto pl = Gothic::inst().player()) {
        pl->changeAttribute(ATR_HITPOINTS,pl->attribute(ATR_HITPOINTSMAX),false);
        }
      return true;
      }
    case C_ToogleFrame:{
      Gothic::inst().setFRate(!Gothic::inst().doFrate());
      return true;
      }
    case C_CamAutoswitch:
      return true;
    case C_CamMode:
      return true;
    case C_ToogleCamDebug:
      if(auto c = Gothic::inst().camera())
        c->toogleDebug();
      return true;
    case C_ToogleCamera: {
      if(auto c = Gothic::inst().camera())
        c->setToogleEnable(!c->isToogleEnabled());
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
      Npc*   player = Gothic::inst().player();
      if(world==nullptr || player==nullptr)
        return false;
      return printVariable(world, ret.argv[0]);
      }
    }

  return true;
  }

bool Marvin::addItemOrNpcBySymbolName(World* world, std::string_view name, const Tempest::Vec3& at) {
  auto&  sc  = world->script();
  size_t id  = sc.getSymbolIndex(name);
  if(id==size_t(-1))
    return false;

  auto*  sym = sc.getSymbol(id);
  if(sym==nullptr||sym->parent()==uint32_t(-1))
    return false;

  if(sym->type()!=phoenix::datatype::instance)
    return false;

  const auto* cls = sym;
  while(cls!=nullptr&&cls->parent()!=uint32_t(-1)) {
    cls = sc.getSymbol(cls->parent());
    }

  if (cls==nullptr)
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
  auto*  sym = sc.getSymbol(name);
  if(sym==nullptr)
    return false;
  switch(sym->type()) {
    case phoenix::datatype::integer:
      buf = string_frm(name," = ",sym->get_int(0));
      break;
    case phoenix::datatype::float_:
      buf = string_frm(name," = ",sym->get_float(0));
      break;
    case phoenix::datatype::string:
      buf = string_frm(name," = ",sym->get_string(0));
      break;
    case phoenix::datatype::instance:
      buf = string_frm(name," = ",sym->get_instance().get());
      break;
    default:
      break;
    }
  print(buf);
  return true;
  }

bool Marvin::setTime(World& world, std::string_view hh, std::string_view mm) {
  int hv = 0, mv = 0;

  auto err = std::from_chars(hh.begin(), hh.end(), hv, 10).ec;
  if(err!=std::errc())
    return false;

  err = std::from_chars(mm.begin(), mm.end(), mv, 10).ec;
  if(err!=std::errc())
    return false;

  if(hv<0 || hv>=24 || mv<0 || mv>=60)
    return false;

  world.setDayTime(hv,mv);
  return true;
  }

std::string_view Marvin::completeInstanceName(std::string_view inp, bool& fullword) const {
  World* world  = Gothic::inst().world();
  if(world==nullptr || inp.size()==0)
    return "";

  auto&            sc    = world->script();
  std::string_view match = "";
  for(size_t i=0; i<sc.getSymbolCount(); ++i) {
    auto*  sym  = sc.getSymbol(i);
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
    for(size_t i=0; i<match.size(); ++i) {
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
