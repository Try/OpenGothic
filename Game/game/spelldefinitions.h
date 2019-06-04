#pragma once

#include <daedalus/DaedalusStdlib.h>

class Gothic;

class SpellDefinitions final {
  public:
    SpellDefinitions(Daedalus::DaedalusVM &vm);

    const Daedalus::GEngineClasses::C_Spell &find(const std::string &instanceName) const;

  private:
    struct Spell:Daedalus::GEngineClasses::C_Spell {
      std::string instName;
      };
    std::vector<Spell> spl;
  };
