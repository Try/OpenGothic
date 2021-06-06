#pragma once

#include <daedalus/DaedalusStdlib.h>

class SpellDefinitions final {
  public:
    SpellDefinitions(Daedalus::DaedalusVM &vm);
    ~SpellDefinitions();

    const Daedalus::GEngineClasses::C_Spell &find(const char* instanceName) const;

  private:
    struct Spell:Daedalus::GEngineClasses::C_Spell {
      std::string instName;
      };
    Daedalus::DaedalusVM& vm;
    std::vector<Spell>    spl;
  };
