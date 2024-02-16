#pragma once

#include <zenkit/DaedalusVm.hh>

#include "scriptplugin.h"

class GameScript;
class Ikarus;

class LeGo : public ScriptPlugin {
  public:
    LeGo(GameScript& owner, Ikarus& ikarus, zenkit::DaedalusVm& vm);

    static bool isRequired(zenkit::DaedalusVm& vm);

  private:
    int  create(int inst);
    void tick(uint64_t dt) override;

    // ## FRAMEFUNCTIONS
    void _FF_Create   (zenkit::DaedalusFunction function, int delay, int cycles, int hasData, int data, bool gametime);
    void FF_Remove    (zenkit::DaedalusFunction function);
    void FF_RemoveAll (zenkit::DaedalusFunction function);
    void FF_RemoveData(zenkit::DaedalusFunction function, int data);
    bool FF_ActiveData(zenkit::DaedalusFunction function, int data);
    bool FF_Active    (zenkit::DaedalusFunction function);
    struct FFItem {
      uint32_t fncID    = 0;
      uint64_t next     = 0;
      int      delay    = 0;
      int      cycles   = 0;
      int      data     = 0;
      bool     hasData  = 0;
      bool     gametime = 0;
      };

    GameScript&         owner;
    Ikarus&             ikarus;
    zenkit::DaedalusVm& vm;

    std::vector<FFItem> frameFunc;
  };

