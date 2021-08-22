#pragma once

#include <string>
#include <vector>
#include <Tempest/Vec>
#include "world/world.h"

class Marvin {
  public:
    Marvin();

    void autoComplete(std::string& v);
    bool exec(const std::string& v);

  private:
    enum CmdType {
      C_None,
      C_Incomplete,
      C_Invalid,

      // rendering
      C_ToogleFrame,
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
      std::string_view cmd  = "";
      CmdType          type = C_None;
      };

    struct CmdVal {
      CmdVal() = default;
      CmdVal(CmdType t){ cmd.type = t; };
      CmdVal(Cmd cmd, size_t cmdOffset):cmd(cmd), cmdOffset(cmdOffset) {};

      Cmd         cmd;
      size_t      cmdOffset = 0;
      std::string argCls    = "";
      };

    CmdVal recognize(std::string_view v);
    CmdVal isMatch(std::string_view inp, const Cmd& cmd) const;
    bool   addItemOrNpcBySymbolName (World* world, const std::string& name, const Tempest::Vec3& at);
    auto   completeInstanceName(std::string_view inp) const -> std::string_view;

    std::vector<Cmd> cmd;
  };

