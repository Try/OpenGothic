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
      C_SetVar,

      // rendering
      C_ToggleFrame,
      C_ToggleShowRay,
      C_ToggleVobBox,

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

      C_ZTrigger,
      C_ZUntrigger,

      C_AiGoTo,
      C_GoToPos,
      C_GoToVob,
      C_GoToWayPoint,
      C_GoToCamera,

      C_SetTime,

      C_Insert,
      C_PlayAni,
      C_PlayTheme,

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
      std::string_view complete    = "";
      bool             fullword    = false;
      // Theme-slot state. `inThemeSlot` is true whenever the cursor is at or
      // past the %t token for a "play theme %t" command, even if the typed
      // prefix is empty (e.g. user typed "play theme " and hit Tab). The
      // prefix itself may be empty; autoComplete uses inThemeSlot to decide
      // whether to trigger theme cycling.
      bool             inThemeSlot = false;
      std::string_view themePrefix = "";

      std::string_view argv[4]  = {};
      };

    CmdVal recognize(std::string_view v);
    CmdVal isMatch(std::string_view inp, const Cmd& cmd) const;
    auto   completeInstanceName(std::string_view inp, bool& fullword) const -> std::string_view;
    auto   completeThemeName   (std::string_view inp, bool& fullword) const -> std::string_view;

    bool   addItemOrNpcBySymbolName(World* world, std::string_view name, const Tempest::Vec3& at);
    bool   printVariable           (World* world, std::string_view name);
    bool   setVariable             (World* world, std::string_view name, std::string_view value);
    bool   setTime                 (World& world, std::string_view hh, std::string_view mm);
    bool   goToVob                 (World& world, Npc& player, Camera& c, std::string_view name, size_t n);

    std::vector<Cmd> cmd;

    // Theme-name cycling state (Tab rotates through all matching theme names).
    std::vector<std::string> cycleList;
    size_t                   cycleIdx  = 0;
    std::string              cycleBase; // prefix of the input line before the theme token
  };

