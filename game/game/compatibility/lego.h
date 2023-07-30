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
    int create(int inst);

    Ikarus&      ikarus;
    phoenix::vm& vm;
  };

