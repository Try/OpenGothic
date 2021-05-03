#pragma once

#include <string>
#include <vector>
#include <Tempest/Vec>
#include "world/world.h"

class Gothic;

class Marvin {
  public:
    Marvin();

    void autoComplete(std::string& v);
    bool exec(Gothic& gothic, const std::string& v);

  private:
    enum CmdType {
      C_None,
      C_Incomplete,
      C_Invalid,

      // npc
      C_CheatFull,
      // camera
      C_CamAutoswitch,
      C_CamMode,
      C_ToogleCamDebug,
      C_ToogleCamera,

      C_Insert,
      };

    struct Cmd {
      const char* cmd  = nullptr;
      CmdType     type = C_None;
      };

    struct CmdVal {
      CmdVal() = default;
      CmdVal(CmdType t){ cmd.type = t; };
      CmdVal(Cmd cmd, size_t cmdOffset):cmd(cmd), cmdOffset(cmdOffset) {};

      Cmd     cmd;
      size_t  cmdOffset = 0;
      };

    CmdVal recognize(const std::string& v);
    bool addItemOrNpcBySymbolName (World* world, const std::string& name, const Tempest::Vec3& at);

    std::vector<Cmd> cmd;
  };

