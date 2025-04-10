#pragma once

#include <Tempest/Vec>
#include <Tempest/Signal>
#include <string>
#include <vector>
#include "world/world.h"

class Marvin {
  public:
    Marvin();

    Tempest::Signal<void(std::string_view)> print;

    bool autoComplete(std::string& v);
    bool exec(std::string_view v);

  private:
    enum CmdType {
      C_None,
      C_Incomplete,
      C_Extra,
      C_Invalid,

      // gdb-like
      C_PrintVar,

      // rendering
      C_ToggleFrame,
      // game
      C_ToggleDesktop,
      C_ToggleTime,
      C_TimeMultiplyer,
      C_TimeRealtime,
      // npc
      C_CheatFull,
      C_CheatGod,
      C_Kill,
      // camera
      C_CamAutoswitch,
      C_CamMode,
      C_ToggleCamDebug,
      C_ToggleCamera,
      C_ToggleInertia,
      C_ZToggleTimeDemo,

      C_AiGoTo,
      C_GoToPos,
      C_GoToVob,
      C_GoToWayPoint,
      C_GoToCamera,

      C_SetTime,

      C_Insert,

      // opengothic specific
      C_ToggleGI,
      C_ToggleVsm,
      C_ToggleRtsm,
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
    bool   setTime                 (World& world, std::string_view hh, std::string_view mm);
    bool   goToVob                 (World& world, Npc& player, Camera& c, std::string_view name, size_t n);

    std::vector<Cmd> cmd;
  };

