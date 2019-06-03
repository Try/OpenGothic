#pragma once

#include <daedalus/DaedalusStdlib.h>
#include <memory>

class Gothic;

class MusicDefinitions final {
  public:
    MusicDefinitions(Gothic &gothic);

    const Daedalus::GEngineClasses::C_MusicTheme& get(const char* name);

  private:
    std::unique_ptr<Daedalus::DaedalusVM>  vm;
    Daedalus::GEngineClasses::C_MusicTheme mm;
  };
