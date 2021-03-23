#pragma once

#include <string>
#include <vector>

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
      // Insert
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

    std::vector<Cmd> cmd;
  };

