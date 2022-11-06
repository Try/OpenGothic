#pragma once

#include <string>
#include <vector>
#include <Tempest/Vec>
#include "world/world.h"

class Marvin {
  public:
    Marvin();

    Tempest::Signal<void(std::string_view)> print;

    void autoComplete(std::string& v);
    bool exec(std::string_view v);

  private:
    enum CmdType {
      C_None,
      C_Incomplete,
      C_Invalid,

      // gdb-like
      C_PrintVar,

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
      CmdVal(Cmd cmd):cmd(cmd) {};

      Cmd              cmd;
      std::string_view complete = "";
      bool             fullword = false;

      std::string_view argv[4]  = {};
      };

    CmdVal recognize(std::string_view v);
    CmdVal isMatch(std::string_view inp, const Cmd& cmd) const;
    auto   completeInstanceName(std::string_view inp, bool& fullword) const -> std::string_view;

    bool   addItemOrNpcBySymbolName(World* world, std::string_view name, const Tempest::Vec3& at);
    bool   printVariable           (World* world, std::string_view name);

    std::vector<Cmd> cmd;
  };

