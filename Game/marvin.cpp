#include "marvin.h"

#include <cstdint>

#include "world/objects/npc.h"
#include "gothic.h"

Marvin::Marvin() {
  cmd = {
    {"cheat full",        C_CheatFull},

    {"camera autoswitch", C_CamAutoswitch},
    {"camera mode",       C_CamMode},
    {"toogle camdebug",   C_ToogleCamDebug},
    {"toogle camera",     C_ToogleCamera},
    };
  }

Marvin::CmdVal Marvin::recognize(const std::string& v) {
  char clean[256] = {};

  for(size_t i=0, r=0; r<255 && i<v.size(); ++i) {
    char c = v[i];
    if('A'<=c && c<='Z')
      c = char(c+'a'-'A');
    if(r==0 && c==' ')
      continue;
    if(c==' ') {
      while(v[i+1]==' ')
        ++i;
      }

    clean[r] = c;
    ++r;
    }

  size_t cmdLen = std::strlen(clean);
  size_t prefix = cmdLen;
  if(prefix==0)
    return C_None;

  while(prefix>0) {
    if(clean[prefix]==' ')
      break;
    --prefix;
    }

  size_t  cnt           = 0;
  size_t  suggestionLen = 0;
  Cmd     suggestion;

  for(auto& i:cmd) {
    const size_t len = std::strlen(i.cmd);
    if(len<prefix)
      continue;
    if(std::memcmp(clean,i.cmd,prefix)!=0)
      continue;

    if(cmdLen<=len && std::memcmp(clean,i.cmd,cmdLen)==0) {
      size_t len = cmdLen;
      while(i.cmd[len]!='\0' && i.cmd[len]!=' ')
        ++len;
      if(len!=suggestionLen || std::memcmp(suggestion.cmd,i.cmd,len)!=0) {
        suggestion    = i;
        suggestionLen = len;
        cnt++;
        }
      }
    }

  if(cnt==0)
    return C_Invalid;

  if(cnt!=1)
    return C_Incomplete;

  if(suggestion.cmd[cmdLen]=='\0')
    return CmdVal{suggestion,cmdLen};

  suggestion.type = C_Incomplete;
  for(size_t i=cmdLen;; ++i) {
    if(suggestion.cmd[i]=='\0')
      return CmdVal{suggestion,cmdLen};
    if(suggestion.cmd[i]==' ' || i>=suggestionLen)
      return CmdVal{suggestion,cmdLen};
    }
  }

void Marvin::autoComplete(std::string& v) {
  auto ret = recognize(v);
  if(ret.cmd.type==C_Incomplete&& ret.cmd.cmd!=nullptr) {
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

bool Marvin::exec(Gothic& gothic, const std::string& v) {
  //auto w = gothic.world();
  //auto c = gothic.gameCamera();

  auto ret = recognize(v);
  switch(ret.cmd.type) {
    case C_None:
      return true;
    case C_Incomplete:
    case C_Invalid:
      return false;
    case C_CheatFull:{
      if(auto pl = gothic.player()) {
        pl->changeAttribute(Npc::ATR_HITPOINTS,pl->attribute(Npc::ATR_HITPOINTSMAX),false);
        }
      break;
      }
    case C_CamAutoswitch:
    case C_CamMode:
    case C_ToogleCamDebug:
    case C_ToogleCamera:
      return true;
    }

  return true;
  }

