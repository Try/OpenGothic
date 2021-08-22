#include "marvin.h"

#include <initializer_list>
#include <cstdint>

#include "world/objects/npc.h"
#include "camera.h"
#include "gothic.h"

Marvin::Marvin() {
  cmd = std::vector<Cmd>{
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
    {"set time %d %d",             C_Invalid},
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
  size_t           part = 0;
  std::string_view ref  = cmd.cmd;
  for(size_t i=0; !ref.empty(); ++i) {
    size_t wr = ref.find(' ');
    if(wr==std::string_view::npos)
      wr = ref.size();
    size_t wi = inp.find(' ');
    if(wi==std::string_view::npos)
      wi = inp.size();

    auto wref = ref.substr(0,wr);
    auto winp = inp.substr(0,wi);
    part += wr;

    if(wref=="%c") {
      wref = completeInstanceName(winp);
      }

    if(wref!=winp) {
      if(wref.find(winp)==std::string::npos)
        return C_Invalid;
      CmdVal v = {cmd,0};
      v.cmd.type = C_Incomplete;
      v.cmd.cmd  = v.cmd.cmd.substr(0,part);
      return v;
      }

    if(ref.size()==wr)
      break;

    part++;
    ref = ref.substr(wr+1);
    inp = inp.substr(wr+1);
    while(inp.size()>0 && inp[0]==' ')
      inp = inp.substr(1);
    }

  return CmdVal{cmd,part};
  }

Marvin::CmdVal Marvin::recognize(std::string_view inp) {
  if(inp.size()==0)
    return C_None;

  size_t prefix = 0;
  for(size_t i=0; i<inp.size(); ++i) {
    ++prefix;
    if(inp[i]==' ') {
      while(i<inp.size() && inp[i]==' ')
        ++i;
      --i;
      }
    }

  CmdVal suggestion = C_Invalid;

  for(auto& i:cmd) {
    auto m = isMatch(inp,i);
    if(m.cmd.type==C_Invalid)
      continue;
    if(m.cmd.type!=C_Incomplete)
      return CmdVal{m.cmd,prefix};
    if(suggestion.cmd.type==C_Incomplete) {
      if(suggestion.cmd.cmd!=m.cmd.cmd)
        return C_Incomplete; // multiple distinct commands do match - no suggestion
      suggestion = m;
      continue;
      }
    suggestion = m;
    }

  suggestion.cmdOffset = prefix;
  return suggestion;
  }

void Marvin::autoComplete(std::string& v) {
  auto ret = recognize(v);
  if(ret.cmd.type==C_Incomplete && ret.cmd.cmd!=nullptr) {
    for(size_t i=ret.cmdOffset;; ++i) {
      if(ret.cmd.cmd[i]=='\0')
        return;
      if(ret.cmd.cmd[i]==' ') {
        v.push_back(' ');
        return;
        }
      v.push_back(ret.cmd.cmd[i]);
      }
    }
  }

bool Marvin::exec(const std::string& v) {
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

      auto   spacePos = v.find(" ");
      if(spacePos==std::string::npos)
        return false;

      std::string arguments = v.substr(spacePos + 1);
      return addItemOrNpcBySymbolName(world, arguments, player->position());
      }
    }

  return true;
  }

bool Marvin::addItemOrNpcBySymbolName (World* world, const std::string& name, const Tempest::Vec3& at) {
  auto&  sc  = world->script();
  size_t id  = sc.getSymbolIndex(name);
  if(id==size_t(-1))
    return false;

  auto&  sym = sc.getSymbol(id);
  if(sym.parent==uint32_t(-1))
    return false;

  const auto* cls = &sym;
  while(cls->parent!=uint32_t(-1)) {
    cls = &sc.getSymbol(cls->parent);
    }

  if(cls->name=="C_NPC")
    return (world->addNpc(id, at)!=nullptr);
  if(cls->name=="C_ITEM")
    return (world->addItem(id, at)!=nullptr);
  return false;
  }

std::string_view Marvin::completeInstanceName(std::string_view inp) const {
  World* world  = Gothic::inst().world();
  if(world==nullptr || inp.size()==0)
    return "";

  auto&            sc    = world->script();
  std::string_view match = "";
  for(size_t i=0; i<sc.getSymbolCount(); ++i) {
    auto&  sym  = sc.getSymbol(i);
    auto   name = std::string_view(sym.name);
    if(name.find(inp)!=0)
      continue;
    if(match.empty()) {
      match = name;
      continue;
      }
    if(name.size()>match.size())
      name = name.substr(0,match.size());
    for(size_t i=0; i<match.size(); ++i) {
      if(match[i]!=name[i]) {
        name = name.substr(0,i);
        break;
        }
      }
    match = name;
    }
  return match;
  }
