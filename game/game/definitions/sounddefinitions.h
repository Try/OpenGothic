#pragma once

#include <daedalus/DaedalusStdlib.h>
#include <unordered_map>

class SoundDefinitions final {
  public:
    SoundDefinitions();

    const Daedalus::GEngineClasses::C_SFX& operator[](const char* name) const;

  private:
    std::unordered_map<std::string,Daedalus::GEngineClasses::C_SFX> sfx;
  };

