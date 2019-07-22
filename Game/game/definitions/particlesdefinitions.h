#pragma once

#include <daedalus/DaedalusStdlib.h>
#include <memory>

class Gothic;

class ParticlesDefinitions final {
  public:
    ParticlesDefinitions(Gothic &gothic);
    ~ParticlesDefinitions();

    const Daedalus::GEngineClasses::C_ParticleFX& get(const char* name);

  private:
    std::unique_ptr<Daedalus::DaedalusVM>  vm;
    Daedalus::GEngineClasses::C_ParticleFX pfx;
  };
