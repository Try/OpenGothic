#pragma once

#include <phoenix/vm.hh>

#include "scriptplugin.h"

class GameScript;
class Ikarus;

class LeGo : public ScriptPlugin {
  public:
    LeGo(GameScript& owner, Ikarus& ikarus, phoenix::vm& vm);

    static bool isRequired(phoenix::vm& vm);

  private:
    int  create(int inst);
    void tick(uint64_t dt) override;

    // ## FRAMEFUNCTIONS
    void _FF_Create   (int function, int delay, int cycles, int hasData, int data, bool gametime);
    void FF_Remove    (int function);
    void FF_RemoveAll (int function);
    void FF_RemoveData(int function, int data);
    bool FF_ActiveData(int function, int data);
    bool FF_Active    (int function);
    struct FFItem {
      uint32_t fncID    = 0;
      uint64_t next     = 0;
      int      delay    = 0;
      int      cycles   = 0;
      int      data     = 0;
      bool     hasData  = 0;
      bool     gametime = 0;
      };

    GameScript&  owner;
    Ikarus&      ikarus;
    phoenix::vm& vm;

    std::vector<FFItem> frameFunc;
  };

